"""Unit tests for scripts/research/statarb/signals.py (QR4.5).

Done-when coverage: the pipeline emits daily target-weight files in the exact
`weights_YYYYMMDD.csv` / `symbol,weight` format the C++ WeightsLoader consumes
(finite values, |w| <= 10), dated one day after the signal (no look-ahead),
with net exposure ~ 0 each day. Plus the entry/exit hysteresis state machine
and the dollar-neutral construction.
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "research" / "statarb"))

from signals import (  # noqa: E402
    Bands,
    dollar_neutral_row,
    generate,
    next_position,
    positions_from_sscores,
    scan_positions,
    weights_from_positions,
    write_weight_files,
)

BANDS = Bands()
BDAYS = pd.bdate_range("2024-01-01", periods=300)


# ---------------------------------------------------------------------------
# Bands validation
# ---------------------------------------------------------------------------


def test_bands_reject_misordered_thresholds():
    with pytest.raises(ValueError, match="open_long < close_long"):
        Bands(open_long=0.5, open_short=1.25, close_long=-0.5, close_short=0.75)


# ---------------------------------------------------------------------------
# The entry/exit state machine (hysteresis)
# ---------------------------------------------------------------------------


def test_open_and_close_transitions():
    # flat -> long only past -1.25; then held through the dead band; closed above -0.50
    assert next_position(0, -1.30, BANDS) == 1
    assert next_position(0, -1.00, BANDS) == 0  # inside the band, no entry from flat
    assert next_position(1, -0.80, BANDS) == 1  # hold long in the dead band
    assert next_position(1, -0.40, BANDS) == 0  # close long above -0.50
    # symmetric short side
    assert next_position(0, 1.30, BANDS) == -1
    assert next_position(0, 1.00, BANDS) == 0
    assert next_position(-1, 0.90, BANDS) == -1  # hold short
    assert next_position(-1, 0.60, BANDS) == 0  # close short below 0.75


def test_nan_forces_flat():
    assert next_position(1, np.nan, BANDS) == 0
    assert next_position(-1, np.nan, BANDS) == 0


def test_hysteresis_holds_through_dead_band():
    # cross -1.25 to open, drift up into (-1.25, -0.50): must stay long
    s = np.array([-1.5, -1.0, -0.7, -0.55, -0.49, -0.40])
    pos = scan_positions(s, BANDS)
    assert list(pos) == [1, 1, 1, 1, 0, 0]  # closes the first time s > -0.50


def test_same_bar_flip_long_to_short():
    # a jump from deep-long territory straight past +1.25 exits long and opens short
    s = np.array([-1.5, 1.6])
    assert list(scan_positions(s, BANDS)) == [1, -1]


def test_positions_frame_shape_and_values():
    sscore = pd.DataFrame({"A": [-1.5, -0.4, 1.5], "B": [1.5, 0.6, -1.5]}, index=BDAYS[:3])
    pos = positions_from_sscores(sscore, BANDS)
    assert pos.shape == (3, 2)
    assert set(np.unique(pos.to_numpy())) <= {-1, 0, 1}
    assert list(pos["A"]) == [1, 0, -1]
    assert list(pos["B"]) == [-1, 0, 1]


# ---------------------------------------------------------------------------
# Dollar-neutral construction
# ---------------------------------------------------------------------------


def test_dollar_neutral_row_nets_to_zero():
    pos = np.array([1, 1, -1, 0])  # 2 long, 1 short
    w = dollar_neutral_row(pos, gross=1.0)
    assert w.sum() == pytest.approx(0.0)
    assert np.abs(w).sum() == pytest.approx(1.0)  # gross
    assert w[0] == pytest.approx(0.25) and w[1] == pytest.approx(0.25)  # 0.5 / 2 longs
    assert w[2] == pytest.approx(-0.5)  # 0.5 / 1 short
    assert w[3] == 0.0


def test_one_sided_book_goes_flat():
    # cannot neutralize with only longs (or only shorts)
    assert np.all(dollar_neutral_row(np.array([1, 1, 0]), gross=1.0) == 0.0)
    assert np.all(dollar_neutral_row(np.array([-1, 0, -1]), gross=1.0) == 0.0)


def test_gross_scales_weights():
    pos = np.array([1, -1])
    w = dollar_neutral_row(pos, gross=2.0)
    assert np.abs(w).sum() == pytest.approx(2.0)
    assert w[0] == pytest.approx(1.0) and w[1] == pytest.approx(-1.0)


def test_weights_frame_net_zero_every_day():
    rng = np.random.default_rng(0)
    pos = pd.DataFrame(
        rng.choice([-1, 0, 1], size=(50, 6)), index=BDAYS[:50], columns=list("abcdef")
    )
    w = weights_from_positions(pos, gross=1.0)
    assert w.sum(axis=1).abs().max() < 1e-12  # net ~ 0 every day
    assert (w.abs() <= 10.0).all().all()  # within the loader's cap


# ---------------------------------------------------------------------------
# Weight-file emission: format + look-ahead-safe dating (the C++ handoff)
# ---------------------------------------------------------------------------


def test_weight_files_format_and_execution_lag(tmp_path):
    dates = BDAYS[:4]
    weights = pd.DataFrame(
        {"AAA": [0.5, 0.0, -0.5, 0.5], "BBB": [-0.5, 0.0, 0.5, -0.5]}, index=dates
    )
    written = write_weight_files(weights, tmp_path, execution_lag=1)

    # one file per signal date except the last (no in-sample execution day)
    assert len(written) == 3
    exec_dates = [d for d, _ in written]
    assert exec_dates == list(dates[1:])  # shifted forward one trading day

    # the file dated t+1 carries the signal from t (look-ahead-safe)
    first_path = written[0][1]
    assert first_path.name == f"weights_{dates[1].strftime('%Y%m%d')}.csv"
    lines = first_path.read_text().splitlines()
    assert lines[0] == "symbol,weight"  # exact header the loader expects
    parsed = dict(line.split(",") for line in lines[1:])
    assert set(parsed) == {"AAA", "BBB"}
    assert float(parsed["AAA"]) == pytest.approx(0.5)  # == signal row for dates[0]
    assert float(parsed["BBB"]) == pytest.approx(-0.5)


def test_emitted_files_satisfy_loader_constraints(tmp_path):
    """Re-parse every emitted file exactly as WeightsLoader does and assert its
    invariants: header, finite doubles, |w| <= 10, net ~ 0 per day."""
    returns = _synthetic_universe()
    out = generate(returns, window=60, gross=1.0, execution_lag=1)
    written = write_weight_files(out["weights"], tmp_path / "w", execution_lag=1)
    assert len(written) > 0

    for _, path in written:
        assert path.name.startswith("weights_") and path.name.endswith(".csv")
        lines = path.read_text().splitlines()
        assert lines[0] == "symbol,weight"
        net = 0.0
        for line in lines[1:]:
            sym, w = line.split(",")
            val = float(w)  # must parse as a double
            assert np.isfinite(val) and abs(val) <= 10.0
            net += val
        assert abs(net) < 1e-9  # dollar-neutral each day


# ---------------------------------------------------------------------------
# Diagnostics + end-to-end on a synthetic universe
# ---------------------------------------------------------------------------


def _synthetic_universe(seed=1, t=220, n=8):
    """Common factor + mean-reverting idiosyncratic parts, enough structure for
    the PCA -> residual -> OU chain to produce real signals."""
    rng = np.random.default_rng(seed)
    factor = rng.normal(0, 0.02, t)
    cols = [f"S{i}" for i in range(n)]
    data = {c: 0.8 * factor + rng.normal(0, 0.012, t) for c in cols}
    return pd.DataFrame(data, index=BDAYS[:t], columns=cols)


def test_diagnostics_net_zero_and_turnover_bounds():
    returns = _synthetic_universe()
    out = generate(returns, window=60, gross=1.0)
    diag = out["diagnostics"]
    assert diag["net"].abs().max() < 1e-12
    assert (diag["gross"].between(0.0, 1.0 + 1e-9)).all()
    assert (diag["turnover"] >= -1e-12).all()
    # on days with a book, gross is exactly the cap
    active = diag[diag["gross"] > 0]
    assert np.allclose(active["gross"], 1.0)


def test_generate_end_to_end_shapes():
    returns = _synthetic_universe()
    out = generate(returns, window=60, gross=1.0)
    n_windows = len(returns) - 60 + 1
    assert out["positions"].shape == (n_windows, returns.shape[1])
    assert out["weights"].shape == (n_windows, returns.shape[1])
    assert set(np.unique(out["positions"].to_numpy())) <= {-1, 0, 1}
    # weights are zero exactly where positions are flat or the book is one-sided
    nonzero_weight = out["weights"].to_numpy() != 0
    active_pos = out["positions"].to_numpy() != 0
    assert np.all(nonzero_weight <= active_pos)  # no weight without a position


def test_generate_is_causal():
    """Appending future data must not change already-emitted signal rows."""
    returns = _synthetic_universe(seed=2, t=200)
    full = generate(returns, window=50)
    truncated = generate(returns.iloc[:120], window=50)
    for key in ("positions", "weights"):
        pd.testing.assert_frame_equal(
            full[key].loc[truncated[key].index], truncated[key], check_dtype=False
        )
