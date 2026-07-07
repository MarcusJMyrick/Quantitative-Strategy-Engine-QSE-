"""Unit tests for scripts/analysis/statarb_audit.py (QR4.7 analysis).

The C++ statarb_audit tool drives the weight files through the real
OrderManager fill models (VWAP walk / shorting, already covered by the
A1-A3 gtests); this suite covers the analysis that turns its paired equity
curves into the net-Sharpe + phantom-profit summary.
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "analysis"))

from statarb_audit import audit_one, collect, load_equity, write_summary  # noqa: E402

DAY_MS = 86_400_000


def write_equity(path: Path, daily_pnl: list[float], start: float = 1e9):
    """Write a timestamp_ms,equity curve from a list of daily dollar PnLs."""
    path.parent.mkdir(parents=True, exist_ok=True)
    equity = start + np.cumsum([0.0] + daily_pnl)
    ts = [i * DAY_MS for i in range(len(equity))]
    pd.DataFrame({"timestamp": ts, "equity": equity}).to_csv(path, index=False)


def make_run(run_dir: Path, size: int, naive_pnl: list[float], depth_pnl: list[float]):
    write_equity(run_dir / f"naive_{size}_equity.csv", naive_pnl)
    write_equity(run_dir / f"depth_{size}_equity.csv", depth_pnl)


# ---------------------------------------------------------------------------
# load + per-run metrics
# ---------------------------------------------------------------------------


def test_load_equity_indexes_by_date(tmp_path):
    write_equity(tmp_path / "e.csv", [1.0, 2.0, 3.0])
    eq = load_equity(tmp_path / "e.csv")
    assert isinstance(eq.index, pd.DatetimeIndex)
    assert eq.iloc[0] == 1e9 and eq.iloc[-1] == 1e9 + 6.0


def test_audit_one_phantom_and_sharpe(tmp_path):
    run = tmp_path / "stat_arb"
    # depth earns less each day than naive -> positive phantom, lower Sharpe
    naive = [100.0, 120.0, 90.0, 110.0] * 50
    depth = [80.0, 100.0, 70.0, 90.0] * 50
    make_run(run, 10, naive, depth)

    m = audit_one(run, 10)
    assert m["naive_pnl"] == pytest.approx(sum(naive))
    assert m["depth_pnl"] == pytest.approx(sum(depth))
    assert m["phantom"] == pytest.approx(sum(naive) - sum(depth))
    assert m["phantom"] > 0  # depth costs money
    assert m["phantom_pct"] == pytest.approx(100.0 * m["phantom"] / m["naive_pnl"])
    assert m["naive_sharpe"] > m["depth_sharpe"] > 0  # impact drags Sharpe down


def test_audit_one_losing_strategy(tmp_path):
    run = tmp_path / "reversal"
    naive = [-50.0, 20.0, -40.0, 10.0] * 50  # net negative
    depth = [-90.0, 10.0, -70.0, 0.0] * 50  # worse under impact
    make_run(run, 1, naive, depth)

    m = audit_one(run, 1)
    assert m["naive_pnl"] < 0 and m["depth_pnl"] < 0
    assert m["depth_pnl"] < m["naive_pnl"]  # depth is worse
    assert m["phantom"] > 0
    assert m["naive_sharpe"] < 0 and m["depth_sharpe"] < 0


# ---------------------------------------------------------------------------
# collect + summary
# ---------------------------------------------------------------------------


def test_collect_skips_missing(tmp_path):
    make_run(tmp_path / "stat_arb", 1, [10.0] * 20, [5.0] * 20)
    # momentum + reversal absent, size 50 absent
    df = collect(tmp_path, [1, 50])
    assert set(df["strategy"]) == {"stat_arb"}
    assert set(df["size"]) == {1}


def test_write_summary_reports_depth_sharpe_and_headline(tmp_path):
    for strat, mult in [("stat_arb", 0.7), ("momentum", 0.9), ("reversal", -0.6)]:
        run = tmp_path / strat
        rng = np.random.default_rng(hash(strat) % 1000)
        base = rng.normal(mult, 1.0, 400)
        make_run(run, 50, list(base * 100), list(base * 80))
    df = collect(tmp_path, [50])
    out = tmp_path / "summary.md"
    write_summary(df, out)
    text = out.read_text()

    assert "Depth Sharpe" in text  # net-of-cost column present
    assert "Headline" in text
    for strat in ("stat_arb", "momentum", "reversal"):
        assert strat in text
    assert "Provisional" in text  # the candidates-not-results caveat
