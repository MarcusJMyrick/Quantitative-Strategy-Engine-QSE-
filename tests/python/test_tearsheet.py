"""Unit tests for scripts/analysis/tearsheet.py (roadmap B3).

Every expected value is hand-computed from the metric definition and asserted
to 4 decimal places; fixtures are built so the arithmetic stays exact.
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "analysis"))

import tearsheet  # noqa: E402

TOL = 1e-4


def equity_from_returns(returns, start=100_000.0):
    values = [start]
    for r in returns:
        values.append(values[-1] * (1.0 + r))
    return pd.Series(values)


class TestSharpe:
    def test_known_value(self):
        # returns [0.01, -0.005, 0.02]: mean = 0.025/3, std(ddof=1) of the
        # deviations {1/600, -2/150, 7/600} -> sqrt(9.5/60000);
        # sharpe = mean/std * sqrt(252) = 10.51314966...
        returns = pd.Series([0.01, -0.005, 0.02])
        assert tearsheet.annualized_sharpe(returns) == pytest.approx(10.5131, abs=TOL)

    def test_zero_mean_is_zero(self):
        # +1% then -1% alternating has mean ~ -0.005% but symmetric case:
        # use exactly offsetting arithmetic returns -> mean 0, sharpe 0
        returns = pd.Series([0.01, -0.01, 0.01, -0.01])
        assert tearsheet.annualized_sharpe(returns) == pytest.approx(0.0, abs=TOL)

    def test_constant_returns_guard(self):
        # zero variance must not divide by zero
        returns = pd.Series([0.01, 0.01, 0.01])
        assert tearsheet.annualized_sharpe(returns) == 0.0


class TestDrawdownAndCalmar:
    def test_max_drawdown_exact(self):
        # peak 120 -> trough 90: drawdown = (90-120)/120 = -0.25 exactly
        equity = pd.Series([100.0, 120.0, 90.0, 100.0, 130.0])
        assert tearsheet.max_drawdown(equity) == pytest.approx(-0.25, abs=TOL)

    def test_monotonic_curve_has_no_drawdown(self):
        equity = pd.Series([100.0, 101.0, 102.0, 103.0])
        assert tearsheet.max_drawdown(equity) == pytest.approx(0.0, abs=TOL)

    def test_calmar_exact(self):
        # One 252-return year ending +20% with a single dip 110 -> 99:
        # CAGR = 0.2, max drawdown = (99-110)/110 = -0.1, Calmar = 2.0
        up = np.linspace(100.0, 110.0, 101)          # days 0..100
        dip = np.array([99.0])                        # day 101
        recover = np.linspace(99.0, 120.0, 151)       # days 102..252
        equity = pd.Series(np.concatenate([up, dip, recover]))
        assert len(equity) == 253  # 252 returns = exactly one year
        assert tearsheet.cagr(equity) == pytest.approx(0.2, abs=TOL)
        assert tearsheet.calmar_ratio(equity) == pytest.approx(2.0, abs=TOL)

    def test_cagr_compounds_over_two_years(self):
        # 504 returns = 2 years, total growth 1.21 -> CAGR = sqrt(1.21)-1 = 0.1
        equity = pd.Series(np.linspace(0.0, 1.0, 505) ** 2 * 21_000.0 + 100_000.0)
        assert equity.iloc[-1] / equity.iloc[0] == pytest.approx(1.21, abs=1e-12)
        assert tearsheet.cagr(equity) == pytest.approx(0.1, abs=TOL)


class TestTurnover:
    def test_exact_turnover(self):
        # Flat 100k equity for one year (253 points = 252 returns);
        # notional = |100|*500 + |-100|*500 + |250|*400 = 200,000
        # turnover = 200,000 / 100,000 / 1yr = 2.0
        equity = pd.Series(np.full(253, 100_000.0))
        trades = pd.DataFrame({
            "quantity": [100, -100, 250],
            "price": [500.0, 500.0, 400.0],
        })
        assert tearsheet.annualized_turnover(trades, equity) == pytest.approx(2.0, abs=TOL)

    def test_no_trades_is_zero(self):
        equity = pd.Series(np.full(253, 100_000.0))
        trades = pd.DataFrame({"quantity": [], "price": []})
        assert tearsheet.annualized_turnover(trades, equity) == 0.0


class TestRollingSharpe:
    def test_window_matches_full_sample_sharpe(self):
        # The final rolling value over window w equals the plain Sharpe of the
        # last w returns
        rng = np.random.default_rng(7)
        returns = pd.Series(rng.normal(0.0005, 0.01, 100))
        rolling = tearsheet.rolling_sharpe(returns, window=63)
        assert len(rolling) == len(returns)
        assert rolling.iloc[:62].isna().all()
        expected = tearsheet.annualized_sharpe(returns.iloc[-63:])
        assert rolling.iloc[-1] == pytest.approx(expected, abs=TOL)


class TestAlphaBeta:
    def test_exact_linear_relationship(self):
        # strategy = 0.001 + 0.5 * benchmark exactly ->
        # beta = 0.5, alpha = 0.001 * 252 = 0.252
        rng = np.random.default_rng(11)
        bench = pd.Series(rng.normal(0.0, 0.01, 60))
        strat = 0.001 + 0.5 * bench
        alpha, beta = tearsheet.alpha_beta(strat, bench)
        assert beta == pytest.approx(0.5, abs=TOL)
        assert alpha == pytest.approx(0.252, abs=TOL)

    def test_short_overlap_returns_nan(self):
        alpha, beta = tearsheet.alpha_beta(pd.Series([0.01]), pd.Series([0.02]))
        assert np.isnan(alpha) and np.isnan(beta)


class TestLoaders:
    def test_load_equity_current_format_ms_epoch(self, tmp_path):
        path = tmp_path / "equity.csv"
        path.write_text(
            "timestamp,equity\n"
            "1748318400000,100000\n"
            "1748404800000,101000\n"
        )
        equity = tearsheet.load_equity(str(path))
        assert isinstance(equity.index, pd.DatetimeIndex)
        assert equity.iloc[-1] == pytest.approx(101000.0)

    def test_load_equity_legacy_portfolio_value(self, tmp_path):
        path = tmp_path / "equity.csv"
        path.write_text(
            "timestamp,portfolio_value\n"
            "1700000000,100000\n"
            "1700086400,100500\n"
        )
        equity = tearsheet.load_equity(str(path))
        assert len(equity) == 2
        assert equity.iloc[-1] == pytest.approx(100500.0)

    def test_duplicate_timestamps_keep_last(self, tmp_path):
        path = tmp_path / "equity.csv"
        path.write_text(
            "timestamp,equity\n"
            "1700000000,100000\n"
            "1700000000,100700\n"
        )
        equity = tearsheet.load_equity(str(path))
        assert len(equity) == 1
        assert equity.iloc[0] == pytest.approx(100700.0)


class TestPdfGeneration:
    def test_build_tearsheet_writes_pdf(self, tmp_path):
        rng = np.random.default_rng(3)
        returns = rng.normal(0.0004, 0.01, 300)
        equity = equity_from_returns(returns)
        equity.index = pd.bdate_range("2024-01-02", periods=len(equity))
        bench = equity_from_returns(rng.normal(0.0003, 0.008, 300))
        bench.index = pd.bdate_range("2024-01-02", periods=len(bench))
        trades = pd.DataFrame({"quantity": [10, -10], "price": [100.0, 101.0]})

        out = tmp_path / "tearsheet.pdf"
        summary = tearsheet.build_tearsheet(equity, trades, bench, str(out), "test")

        assert out.exists() and out.stat().st_size > 10_000
        assert out.read_bytes()[:5] == b"%PDF-"
        for key in ("Sharpe (ann.)", "Calmar", "Turnover (ann.)", "Alpha (ann.)", "Beta"):
            assert key in summary
