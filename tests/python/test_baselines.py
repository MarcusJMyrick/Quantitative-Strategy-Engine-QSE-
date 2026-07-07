"""Unit tests for scripts/research/statarb/baselines.py (QR4.6).

Done-when coverage: reversal and momentum run through the SAME harness as the
stat arb (identical dollar-neutral weights + weight-file format) and produce
comparable paper tearsheets. Plus signal correctness, cross-sectional
selection, the look-ahead-safe lags, and causality.
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "research" / "statarb"))

from baselines import (  # noqa: E402
    annualized_sharpe,
    generate_baseline,
    momentum_signal,
    paper_pnl,
    reversal_signal,
    signal_to_positions,
)
from signals import write_weight_files  # noqa: E402

BDAYS = pd.bdate_range("2024-01-01", periods=400)


# ---------------------------------------------------------------------------
# Signal correctness + as-of horizon
# ---------------------------------------------------------------------------


def test_reversal_buys_the_loser():
    # A rose, B fell over the last 3 days -> reversal longs B (higher signal)
    returns = pd.DataFrame(
        {"A": [0.02, 0.02, 0.02, 0.02], "B": [-0.02, -0.02, -0.02, -0.02]}, index=BDAYS[:4]
    )
    sig = reversal_signal(returns, lookback=3)
    assert sig.iloc[-1]["B"] > sig.iloc[-1]["A"]  # loser B is more attractive
    # warm-up: first `lookback` rows are NaN, the row at index==lookback is valid
    assert np.isnan(sig.iloc[0]["A"]) and not np.isnan(sig.iloc[3]["A"])


def test_momentum_buys_the_winner_and_skips_recent_month():
    # A trends up for a year then crashes in the last few days; momentum with a
    # skip should still see A as a winner (it ignores the recent crash).
    n = 300
    a = np.r_[np.full(280, 0.01), np.full(20, -0.05)]  # up 280d, then crash
    b = -a
    returns = pd.DataFrame({"A": a, "B": b}, index=BDAYS[:n])
    mom = momentum_signal(returns, lookback=250, skip=20)
    assert mom.iloc[-1]["A"] > mom.iloc[-1]["B"]  # A the 12-1 winner despite the crash

    # short-term reversal, by contrast, sees the recent crash and prefers A too
    # for the OPPOSITE reason (A just fell) -> the two signals rank differently
    rev = reversal_signal(returns, lookback=5)
    assert rev.iloc[-1]["A"] > rev.iloc[-1]["B"]  # A just crashed -> reversal longs A


def test_momentum_uses_only_data_through_t_minus_skip():
    rng = np.random.default_rng(0)
    returns = pd.DataFrame(rng.normal(0, 0.02, (300, 3)), index=BDAYS[:300], columns=list("abc"))
    mom = momentum_signal(returns, lookback=100, skip=20)
    # perturbing a return inside the skip window must NOT change that day's signal
    bumped = returns.copy()
    bumped.iloc[290] += 0.5  # inside the last-20-day skip for t = 299
    mom_bumped = momentum_signal(bumped, lookback=100, skip=20)
    assert mom.iloc[299].equals(mom_bumped.iloc[299])


# ---------------------------------------------------------------------------
# Cross-sectional selection
# ---------------------------------------------------------------------------


def test_signal_to_positions_long_top_short_bottom():
    signal = pd.DataFrame([[5.0, 4.0, 3.0, 2.0, 1.0, 0.0]], index=BDAYS[:1], columns=list("abcdef"))
    pos = signal_to_positions(signal, frac=1.0 / 3.0)  # 6 names -> k=2 each side
    row = pos.iloc[0]
    assert row["a"] == 1 and row["b"] == 1  # top 2 long
    assert row["e"] == -1 and row["f"] == -1  # bottom 2 short
    assert row["c"] == 0 and row["d"] == 0  # middle flat
    assert row.sum() == 0  # equal counts


def test_positions_flat_when_too_few_valid_signals():
    signal = pd.DataFrame([[np.nan, 1.0], [np.nan, np.nan]], index=BDAYS[:2], columns=list("ab"))
    pos = signal_to_positions(signal)
    assert (pos == 0).all().all()  # <2 valid -> flat


def test_long_short_sets_stay_disjoint():
    rng = np.random.default_rng(1)
    signal = pd.DataFrame(rng.normal(size=(20, 15)), index=BDAYS[:20])
    pos = signal_to_positions(signal, frac=0.5)  # aggressive frac
    # a name is never both long and short; counts balance
    assert ((pos == 1) & (pos == -1)).sum().sum() == 0
    assert ((pos == 1).sum(axis=1) == (pos == -1).sum(axis=1)).all()


# ---------------------------------------------------------------------------
# Same harness: dollar-neutral weights + loader-compatible files
# ---------------------------------------------------------------------------


def _synthetic_universe(seed=1, t=350, n=9):
    rng = np.random.default_rng(seed)
    factor = rng.normal(0, 0.02, t)
    cols = [f"S{i}" for i in range(n)]
    return pd.DataFrame(
        {c: 0.7 * factor + rng.normal(0, 0.015, t) for c in cols}, index=BDAYS[:t], columns=cols
    )


@pytest.mark.parametrize("kind", ["reversal", "momentum"])
def test_baseline_weights_are_dollar_neutral(kind):
    returns = _synthetic_universe()
    out = generate_baseline(returns, kind, frac=1.0 / 3.0, gross=1.0, mom_lookback=200, mom_skip=20)
    w = out["weights"]
    assert w.sum(axis=1).abs().max() < 1e-12  # net ~ 0 every day
    active = out["diagnostics"][out["diagnostics"]["gross"] > 0]
    assert np.allclose(active["gross"], 1.0)  # gross cap on active days
    assert set(np.unique(out["positions"].to_numpy())) <= {-1, 0, 1}


def test_baseline_files_match_the_statarb_format(tmp_path):
    """Same weight-file emission as QR4.5 -> same loader constraints."""
    returns = _synthetic_universe()
    out = generate_baseline(returns, "reversal", mom_lookback=200, mom_skip=20)
    written = write_weight_files(out["weights"], tmp_path, execution_lag=1)
    assert len(written) > 0
    for _, path in written:
        lines = path.read_text().splitlines()
        assert lines[0] == "symbol,weight"
        net = 0.0
        for line in lines[1:]:
            _, w = line.split(",")
            val = float(w)
            assert np.isfinite(val) and abs(val) <= 10.0
            net += val
        assert abs(net) < 1e-9


# ---------------------------------------------------------------------------
# Paper PnL (cost-free, provisional) + lag correctness
# ---------------------------------------------------------------------------


def test_paper_pnl_lag_and_value():
    # long A on the signal date; with execution_lag=1 the position is held two
    # days later, so it earns A's return on that day (and nothing before).
    idx = BDAYS[:6]
    weights = pd.DataFrame(0.0, index=idx, columns=["A", "B"])
    weights.loc[idx[0], "A"] = 1.0
    returns = pd.DataFrame(0.0, index=idx, columns=["A", "B"])
    returns.loc[idx[2], "A"] = 0.03  # the day the position is in force

    pnl = paper_pnl(weights, returns, execution_lag=1)
    assert pnl.loc[idx[2]] == pytest.approx(0.03)
    assert pnl.drop(idx[2]).abs().sum() == pytest.approx(0.0)  # no PnL any other day


def test_annualized_sharpe_basic():
    flat = pd.Series([0.0] * 300)  # a book that never trades -> no active days -> 0
    assert annualized_sharpe(flat) == 0.0
    rng = np.random.default_rng(3)
    noisy = pd.Series(rng.normal(0.0005, 0.01, 500))
    s = annualized_sharpe(noisy)
    assert np.isfinite(s)
    # sign/scale sanity: a clean positive-drift, low-vol series is strongly positive
    assert annualized_sharpe(pd.Series([0.01, 0.011, 0.009, 0.0105] * 50)) > 5


# ---------------------------------------------------------------------------
# End-to-end + causality
# ---------------------------------------------------------------------------


def test_generate_baseline_causal():
    """Appending future data must not change already-emitted rows."""
    returns = _synthetic_universe(seed=2, t=340)
    full = generate_baseline(returns, "reversal", reversal_lookback=5)
    trunc = generate_baseline(returns.iloc[:300], "reversal", reversal_lookback=5)
    for key in ("positions", "weights"):
        pd.testing.assert_frame_equal(
            full[key].loc[trunc[key].index], trunc[key], check_dtype=False
        )


def test_momentum_warmup_is_flat():
    returns = _synthetic_universe(t=300)
    out = generate_baseline(returns, "momentum", mom_lookback=252, mom_skip=21)
    # nothing tradeable until the 252-day lookback fills
    assert (out["positions"].iloc[:252] == 0).all().all()
    assert out["positions"].iloc[252:].abs().to_numpy().sum() > 0
