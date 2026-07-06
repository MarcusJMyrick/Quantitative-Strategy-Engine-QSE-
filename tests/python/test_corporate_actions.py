"""Unit tests for scripts/data/corporate_actions.py (B2).

Flagship case per the roadmap done-when: the AAPL 4-for-1 split effective
2020-08-31 — pre-split prices ÷4, volumes ×4, and a buy-and-hold equity curve
that is continuous across the split date on adjusted data (and crashes 75% on
raw data).
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "data"))

from corporate_actions import adjust_for_corporate_actions, load_actions  # noqa: E402

AAPL_SPLIT = pd.DataFrame({
    "symbol": ["AAPL"],
    "date": [pd.Timestamp("2020-08-31")],
    "action": ["split"],
    "value": [4.0],
})


def bars(dates, closes, volumes):
    idx = pd.DatetimeIndex(pd.to_datetime(dates), name="timestamp")
    return pd.DataFrame({
        "open": closes, "high": closes, "low": closes, "close": closes,
        "volume": volumes,
    }, index=idx)


class TestSplitAdjustment:
    def test_aapl_4_for_1_prices_divided_volumes_multiplied(self):
        # Raw closes: $500 pre-split, $125 post-split (economically flat)
        df = bars(["2020-08-27", "2020-08-28", "2020-08-31", "2020-09-01"],
                  [500.0, 500.0, 125.0, 125.0],
                  [1000, 1000, 4000, 4000])
        adjusted, report = adjust_for_corporate_actions(df, AAPL_SPLIT, "AAPL")

        # Pre-split rows: prices / 4, volume * 4
        assert adjusted.loc["2020-08-27", "close"] == pytest.approx(125.0)
        assert adjusted.loc["2020-08-28", "close"] == pytest.approx(125.0)
        assert adjusted.loc["2020-08-27", "volume"] == pytest.approx(4000)
        # Ex-date and later rows: untouched
        assert adjusted.loc["2020-08-31", "close"] == pytest.approx(125.0)
        assert adjusted.loc["2020-09-01", "volume"] == pytest.approx(4000)
        # All OHLC columns adjusted, not just close
        for col in ("open", "high", "low"):
            assert adjusted.loc["2020-08-28", col] == pytest.approx(125.0)

        assert len(report) == 1
        assert report[0]["rows_adjusted"] == 2
        assert report[0]["factor"] == pytest.approx(0.25)

    def test_buy_and_hold_equity_continuous_across_split(self):
        # Economically the stock is flat; the raw series halves per share
        # count bookkeeping. Buy-and-hold on ADJUSTED data must show a flat
        # equity curve; on RAW data it shows a fake -75% crash.
        df = bars(["2020-08-26", "2020-08-27", "2020-08-28",
                   "2020-08-31", "2020-09-01", "2020-09-02"],
                  [500.0, 500.0, 500.0, 125.0, 125.0, 125.0],
                  [1000] * 6)
        adjusted, _ = adjust_for_corporate_actions(df, AAPL_SPLIT, "AAPL")

        def buy_and_hold_equity(closes, cash=10_000.0):
            shares = cash / closes.iloc[0]
            return shares * closes

        raw_equity = buy_and_hold_equity(df["close"])
        adj_equity = buy_and_hold_equity(adjusted["close"])

        # Raw: phantom crash at the split date
        assert raw_equity.iloc[-1] / raw_equity.iloc[0] == pytest.approx(0.25)
        # Adjusted: perfectly continuous (constant) across the split
        assert adj_equity.max() == pytest.approx(adj_equity.min())
        assert adj_equity.iloc[-1] == pytest.approx(10_000.0)

    def test_other_symbols_are_untouched(self):
        df = bars(["2020-08-28", "2020-08-31"], [500.0, 500.0], [1000, 1000])
        adjusted, report = adjust_for_corporate_actions(df, AAPL_SPLIT, "MSFT")
        pd.testing.assert_frame_equal(adjusted, df)
        assert report == []

    def test_series_starting_after_event_is_untouched(self):
        df = bars(["2021-01-04", "2021-01-05"], [130.0, 131.0], [1000, 1000])
        adjusted, report = adjust_for_corporate_actions(df, AAPL_SPLIT, "AAPL")
        pd.testing.assert_frame_equal(adjusted, df)
        assert report == []

    def test_multiple_splits_compound(self):
        # AAPL 7:1 (2014-06-09) then 4:1 (2020-08-31): rows before 2014 get
        # both factors (28x), rows between get only the 4x
        actions = pd.DataFrame({
            "symbol": ["AAPL", "AAPL"],
            "date": [pd.Timestamp("2014-06-09"), pd.Timestamp("2020-08-31")],
            "action": ["split", "split"],
            "value": [7.0, 4.0],
        })
        df = bars(["2014-06-06", "2020-08-28", "2020-08-31"],
                  [644.0, 500.0, 125.0],
                  [700, 1000, 4000])
        adjusted, report = adjust_for_corporate_actions(df, actions, "AAPL")

        assert adjusted.loc["2014-06-06", "close"] == pytest.approx(644.0 / 28)
        assert adjusted.loc["2014-06-06", "volume"] == pytest.approx(700 * 28)
        assert adjusted.loc["2020-08-28", "close"] == pytest.approx(125.0)
        assert adjusted.loc["2020-08-31", "close"] == pytest.approx(125.0)
        assert len(report) == 2


class TestDividendAdjustment:
    def test_proportional_backadjustment(self):
        # $1 dividend, reference close $100 -> pre-ex prices x 0.99
        actions = pd.DataFrame({
            "symbol": ["XYZ"], "date": [pd.Timestamp("2024-03-15")],
            "action": ["dividend"], "value": [1.0],
        })
        df = bars(["2024-03-13", "2024-03-14", "2024-03-15"],
                  [99.0, 100.0, 99.5], [1000] * 3)
        adjusted, report = adjust_for_corporate_actions(df, actions, "XYZ")

        assert adjusted.loc["2024-03-14", "close"] == pytest.approx(99.0)
        assert adjusted.loc["2024-03-13", "close"] == pytest.approx(98.01)
        assert adjusted.loc["2024-03-15", "close"] == pytest.approx(99.5)
        # Dividends do not touch volume
        assert adjusted.loc["2024-03-13", "volume"] == pytest.approx(1000)
        assert report[0]["factor"] == pytest.approx(0.99)

    def test_implausible_dividend_is_skipped(self):
        actions = pd.DataFrame({
            "symbol": ["XYZ"], "date": [pd.Timestamp("2024-03-15")],
            "action": ["dividend"], "value": [200.0],  # exceeds the price
        })
        df = bars(["2024-03-14", "2024-03-15"], [100.0, 99.5], [1000, 1000])
        adjusted, report = adjust_for_corporate_actions(df, actions, "XYZ")
        pd.testing.assert_frame_equal(adjusted, df)
        assert report == []


class TestTickDataAndEpochIndexes:
    def test_tick_frame_with_epoch_seconds_index(self):
        # Tick schema (price/volume) with an epoch-seconds index spanning the
        # AAPL split: 2020-08-28 -> 1598572800, 2020-08-31 -> 1598832000
        df = pd.DataFrame(
            {"price": [500.0, 125.0], "volume": [1000, 4000]},
            index=pd.Index([1598572800, 1598832000], name="timestamp"))
        adjusted, report = adjust_for_corporate_actions(df, AAPL_SPLIT, "AAPL")

        assert adjusted["price"].iloc[0] == pytest.approx(125.0)
        assert adjusted["volume"].iloc[0] == pytest.approx(4000)
        assert adjusted["price"].iloc[1] == pytest.approx(125.0)
        assert report[0]["rows_adjusted"] == 1


class TestLoadActions:
    def test_reference_file_loads_and_validates(self):
        actions = load_actions(
            Path(__file__).resolve().parents[2] / "config" / "corporate_actions.csv")
        aapl = actions[actions["symbol"] == "AAPL"]
        assert len(aapl) == 2
        assert set(actions["action"]) == {"split"}

    def test_missing_columns_raise(self, tmp_path):
        bad = tmp_path / "bad.csv"
        bad.write_text("symbol,date\nAAPL,2020-08-31\n")
        with pytest.raises(ValueError, match="missing columns"):
            load_actions(bad)
