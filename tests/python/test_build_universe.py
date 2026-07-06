"""Unit tests for scripts/research/statarb/build_universe.py (QR4.1).

Done-when coverage: the build emits a clean standardized-returns Parquet with
no NaNs, and the as-of alignment is enforced — the causality test proves that
appending future data never changes an already-emitted standardized row.
"""

import json
import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "research" / "statarb"))

from build_universe import (  # noqa: E402
    build,
    build_close_panel,
    compute_returns,
    fetch_alpaca_daily,
    flag_extreme_returns,
    rolling_standardize,
)

BDAYS = pd.bdate_range("2024-01-01", periods=250)


def write_bar_csv(bars_dir: Path, symbol: str, closes: pd.Series) -> None:
    """Write a per-symbol daily-bar CSV in the shape build_universe loads."""
    df = pd.DataFrame(
        {
            "open": closes.values,
            "high": closes.values,
            "low": closes.values,
            "close": closes.values,
            "volume": 1_000_000,
        },
        index=closes.index,
    )
    bars_dir.mkdir(parents=True, exist_ok=True)
    df.to_csv(bars_dir / f"{symbol}.csv", index_label="date")


def write_actions_csv(path: Path, rows: list[str]) -> Path:
    path.write_text("symbol,date,action,value\n" + "".join(r + "\n" for r in rows))
    return path


def random_walk(seed: int, n: int = 250, start: float = 100.0) -> pd.Series:
    rng = np.random.default_rng(seed)
    prices = start * np.exp(np.cumsum(rng.normal(0.0005, 0.02, n)))
    return pd.Series(prices, index=BDAYS[:n])


# ---------------------------------------------------------------------------
# Returns
# ---------------------------------------------------------------------------


def test_returns_hand_computed():
    panel = pd.DataFrame({"A": [100.0, 110.0, 99.0]}, index=BDAYS[:3])
    returns = compute_returns(panel)
    assert len(returns) == 2  # first row has no prior close
    assert returns["A"].iloc[0] == pytest.approx(0.10)
    assert returns["A"].iloc[1] == pytest.approx(-0.10)


# ---------------------------------------------------------------------------
# Rolling standardization: correctness, warm-up, causality, degeneracy
# ---------------------------------------------------------------------------


def test_rolling_standardize_matches_hand_computation():
    r = pd.DataFrame({"A": [0.01, -0.02, 0.03, 0.00, -0.01]}, index=BDAYS[:5])
    y = rolling_standardize(r, window=3)
    # Row t must use exactly the trailing 3 returns ending at t, ddof=1
    for i in range(2, 5):
        window = r["A"].iloc[i - 2 : i + 1]
        expected = (r["A"].iloc[i] - window.mean()) / window.std(ddof=1)
        assert y["A"].loc[r.index[i]] == pytest.approx(expected)


def test_warmup_rows_dropped_and_no_nans():
    r = pd.DataFrame(
        {"A": np.random.default_rng(1).normal(0, 0.02, 100)},
        index=BDAYS[:100],
    )
    y = rolling_standardize(r, window=10)
    assert len(y) == 100 - 9  # first window-1 rows dropped, never partial
    assert y.index[0] == r.index[9]
    assert not y.isna().any().any()


def test_rolling_standardize_is_causal():
    """The as-of guarantee: appending future data must not change any
    already-emitted standardized row (trailing window, no smoothing)."""
    r = pd.DataFrame(
        {
            "A": np.random.default_rng(2).normal(0, 0.02, 200),
            "B": np.random.default_rng(3).normal(0, 0.03, 200),
        },
        index=BDAYS[:200],
    )
    full = rolling_standardize(r, window=20)
    truncated = rolling_standardize(r.iloc[:120], window=20)
    pd.testing.assert_frame_equal(full.loc[truncated.index], truncated)


def test_zero_variance_window_raises():
    r = pd.DataFrame({"A": [0.0] * 30}, index=BDAYS[:30])
    with pytest.raises(ValueError, match="Zero return variance"):
        rolling_standardize(r, window=10)


# ---------------------------------------------------------------------------
# Panel assembly: interior gaps ffilled + counted, leading rows dropped
# ---------------------------------------------------------------------------


def test_interior_gap_ffilled_and_reported():
    a = pd.Series([100.0, 101.0, 102.0, 103.0, 104.0], index=BDAYS[:5])
    b = a.drop(BDAYS[2])  # B misses one day A traded
    panel, report = build_close_panel(
        {"A": pd.DataFrame({"close": a}), "B": pd.DataFrame({"close": b})}
    )
    assert report["fills_per_symbol"] == {"A": 0, "B": 1}
    assert panel.loc[BDAYS[2], "B"] == 101.0  # filled from the prior close
    assert not panel.isna().any().any()


def test_leading_rows_dropped_until_all_symbols_live():
    a = pd.Series(np.linspace(100, 110, 10), index=BDAYS[:10])
    b = a.iloc[5:]  # B starts trading 5 days later
    panel, report = build_close_panel(
        {"A": pd.DataFrame({"close": a}), "B": pd.DataFrame({"close": b})}
    )
    assert report["leading_rows_dropped"] == 5
    assert panel.index[0] == BDAYS[5]
    assert report["fills_per_symbol"] == {"A": 0, "B": 0}  # leading != interior


# ---------------------------------------------------------------------------
# Corporate actions through the B2 pipeline
# ---------------------------------------------------------------------------


def test_split_adjustment_removes_fake_crash(tmp_path):
    """A 4:1 split shows a -75% 'return' on raw prices; through B2 the
    adjusted matrix must show a quiet day instead."""
    base = pd.Series([400.0] * 30 + [100.0] * 30, index=BDAYS[:60])  # 4:1 split before day 30
    closes = base * (1 + np.random.default_rng(5).normal(0, 0.001, 60))  # non-degenerate
    bars_dir = tmp_path / "bars"
    write_bar_csv(bars_dir, "SPLT", closes)
    actions = write_actions_csv(tmp_path / "actions.csv", [f"SPLT,{BDAYS[30].date()},split,4"])

    raw_returns = closes.pct_change().dropna()
    assert raw_returns.min() == pytest.approx(-0.75, abs=0.01)  # the fake crash exists in raw

    manifest = build(
        bars_dir=bars_dir,
        out_dir=tmp_path / "out",
        symbols=["SPLT"],
        window=10,
        actions_path=actions,
    )
    returns = pd.read_parquet(tmp_path / "out" / "universe_returns.parquet")
    assert returns["SPLT"].abs().max() < 0.05  # crash gone after adjustment, noise remains
    assert manifest["corporate_actions_applied"][0]["action"] == "split"
    assert manifest["extreme_returns_flagged"] == []


def test_missed_split_is_flagged(tmp_path):
    """The same split with an empty actions file must be flagged loudly as a
    suspected missing corporate action, not silently standardized away."""
    closes = pd.Series([400.0] * 30 + [100.0] * 30, index=BDAYS[:60])
    closes = closes * (1 + np.random.default_rng(4).normal(0, 0.001, 60))  # non-degenerate
    bars_dir = tmp_path / "bars"
    write_bar_csv(bars_dir, "SPLT", closes)
    actions = write_actions_csv(tmp_path / "actions.csv", [])

    manifest = build(
        bars_dir=bars_dir,
        out_dir=tmp_path / "out",
        symbols=["SPLT"],
        window=10,
        actions_path=actions,
    )
    flagged = manifest["extreme_returns_flagged"]
    assert len(flagged) == 1
    assert flagged[0]["symbol"] == "SPLT"
    assert flagged[0]["date"] == BDAYS[30].date().isoformat()
    assert flagged[0]["return"] == pytest.approx(-0.75, abs=0.01)


def test_flag_extreme_returns_threshold():
    r = pd.DataFrame({"A": [0.10, -0.44, 0.46]}, index=BDAYS[:3])
    assert flag_extreme_returns(r, threshold=0.45) == [
        {"symbol": "A", "date": BDAYS[2].date().isoformat(), "return": 0.46}
    ]


# ---------------------------------------------------------------------------
# End-to-end build: the done-when artifact
# ---------------------------------------------------------------------------


def test_end_to_end_build_emits_clean_parquets(tmp_path):
    bars_dir = tmp_path / "bars"
    symbols = ["AA", "BB", "CC"]
    for i, sym in enumerate(symbols):
        write_bar_csv(bars_dir, sym, random_walk(seed=10 + i))
    actions = write_actions_csv(tmp_path / "actions.csv", [])

    window = 60
    manifest = build(
        bars_dir=bars_dir,
        out_dir=tmp_path / "out",
        symbols=symbols,
        window=window,
        actions_path=actions,
    )

    returns = pd.read_parquet(tmp_path / "out" / "universe_returns.parquet")
    standardized = pd.read_parquet(tmp_path / "out" / "universe_standardized.parquet")
    assert list(returns.columns) == symbols
    assert list(standardized.columns) == symbols
    assert not returns.isna().any().any()
    assert not standardized.isna().any().any()
    assert len(returns) == 249  # 250 bars -> 249 returns
    assert len(standardized) == 249 - (window - 1)

    on_disk = json.loads((tmp_path / "out" / "universe_manifest.json").read_text())
    assert on_disk["window"] == window
    assert on_disk["symbols"] == symbols
    assert "t+1" in on_disk["as_of_alignment"]  # the contract is documented
    assert manifest["standardized_rows"] == len(standardized)

    # standardized columns should look standardized over the sample
    assert standardized.mean().abs().max() < 0.5
    assert (standardized.std() - 1.0).abs().max() < 0.5


# ---------------------------------------------------------------------------
# Alpaca fetch parsing (fake get_json — zero network)
# ---------------------------------------------------------------------------


def test_fetch_parses_and_paginates():
    pages = [
        {
            "bars": {
                "AA": [{"t": "2024-01-02T05:00:00Z", "o": 1, "h": 2, "l": 0.5, "c": 1.5, "v": 100}],
                "BB": [
                    {"t": "2024-01-02T05:00:00Z", "o": 10, "h": 11, "l": 9, "c": 10.5, "v": 200}
                ],
            },
            "next_page_token": "tok",
        },
        {
            "bars": {
                "AA": [{"t": "2024-01-03T05:00:00Z", "o": 1.5, "h": 2, "l": 1, "c": 1.8, "v": 150}]
            },
            "next_page_token": None,
        },
    ]
    calls = []

    def fake_get_json(params):
        calls.append(dict(params))
        return pages[len(calls) - 1]

    bars = fetch_alpaca_daily(["AA", "BB"], "2024-01-01", "2024-01-05", get_json=fake_get_json)

    assert len(calls) == 2
    assert "page_token" not in calls[0] and calls[1]["page_token"] == "tok"
    assert calls[0]["adjustment"] == "raw"  # adjustment belongs to B2, not the vendor
    assert list(bars["AA"].index) == [pd.Timestamp("2024-01-02"), pd.Timestamp("2024-01-03")]
    assert bars["AA"]["close"].tolist() == [1.5, 1.8]
    assert list(bars["AA"].columns) == ["open", "high", "low", "close", "volume"]
    assert bars["BB"]["volume"].tolist() == [200]


def test_fetch_raises_on_empty_symbol():
    def fake_get_json(params):
        return {
            "bars": {"AA": [{"t": "2024-01-02T05:00:00Z", "o": 1, "h": 1, "l": 1, "c": 1, "v": 1}]}
        }

    with pytest.raises(RuntimeError, match="No bars returned for BB"):
        fetch_alpaca_daily(["AA", "BB"], "2024-01-01", "2024-01-05", get_json=fake_get_json)
