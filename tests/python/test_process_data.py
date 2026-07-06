"""Unit tests for forward_fill_ticks in scripts/data/process_data.py (B1).

Fixtures are synthetic gappy tick data; every filled value and every report
count is asserted exactly.
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "data"))

from process_data import forward_fill_ticks  # noqa: E402


def make_df(timestamps, prices, volumes):
    return pd.DataFrame(
        {"price": prices, "volume": volumes}, index=pd.Index(timestamps, name="timestamp")
    )


class TestForwardFill:
    def test_missing_price_is_forward_filled(self):
        df = make_df([1000, 1060, 1120, 1180], [100.0, np.nan, np.nan, 101.0], [500, 400, 300, 200])
        clean, report = forward_fill_ticks(df)

        assert clean.loc[1060, "price"] == pytest.approx(100.0)
        assert clean.loc[1120, "price"] == pytest.approx(100.0)
        assert clean.loc[1180, "price"] == pytest.approx(101.0)
        assert report["prices_filled"] == 2
        assert len(clean) == 4

    def test_missing_volume_becomes_zero(self):
        df = make_df([1000, 1060, 1120], [100.0, 100.5, 101.0], [500, np.nan, 300])
        clean, report = forward_fill_ticks(df)

        assert clean.loc[1060, "volume"] == 0
        assert report["volumes_filled"] == 1
        assert report["prices_filled"] == 0

    def test_leading_rows_without_price_are_dropped(self):
        df = make_df([1000, 1060, 1120, 1180], [np.nan, np.nan, 100.0, 100.5], [500, 400, 300, 200])
        clean, report = forward_fill_ticks(df)

        assert len(clean) == 2
        assert clean.index.tolist() == [1120, 1180]
        assert report["leading_rows_dropped"] == 2
        assert report["prices_filled"] == 0

    def test_non_numeric_values_are_coerced_and_filled(self):
        df = make_df([1000, 1060, 1120], [100.0, "bad_value", 101.0], [500, "also_bad", 300])
        clean, report = forward_fill_ticks(df)

        assert clean.loc[1060, "price"] == pytest.approx(100.0)
        assert clean.loc[1060, "volume"] == 0
        assert report["prices_filled"] == 1
        assert report["volumes_filled"] == 1

    def test_grid_gaps_are_counted(self):
        # 60s grid with 1180 and 1240 missing -> the 1120->1300 delta spans
        # 3 spacings -> 2 missing rows
        df = make_df(
            [1000, 1060, 1120, 1300, 1360, 1420],
            [100.0, 100.1, 100.2, 100.3, 100.4, 100.5],
            [500, 500, 500, 500, 500, 500],
        )
        clean, report = forward_fill_ticks(df)

        assert report["grid_gaps"] == 2
        assert len(clean) == 6  # gaps are reported, not fabricated

    def test_unsorted_input_is_sorted(self):
        df = make_df([1120, 1000, 1060], [101.0, 100.0, np.nan], [300, 500, 400])
        clean, _ = forward_fill_ticks(df)

        assert clean.index.tolist() == [1000, 1060, 1120]
        # After sorting, the 1060 NaN fills from 1000's price, not 1120's
        assert clean.loc[1060, "price"] == pytest.approx(100.0)

    def test_clean_data_reports_all_zero(self):
        df = make_df([1000, 1060, 1120], [100.0, 100.5, 101.0], [500, 400, 300])
        clean, report = forward_fill_ticks(df)

        assert report == {
            "prices_filled": 0,
            "volumes_filled": 0,
            "leading_rows_dropped": 0,
            "grid_gaps": 0,
        }
        pd.testing.assert_frame_equal(clean, df, check_dtype=False)
