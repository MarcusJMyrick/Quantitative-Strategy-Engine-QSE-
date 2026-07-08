"""Unit tests for scripts/research/regime/regime_features.py (QR3.1).

Done-when coverage: a clean feature frame with a documented no-look-ahead
alignment, and a test that the feature at time t uses only data <= t.
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "research" / "regime"))

from regime_features import build, compute_features, load_market  # noqa: E402

DAYS = pd.bdate_range("2020-01-01", periods=400)


def synthetic_ohlcv(seed=0, n=400) -> pd.DataFrame:
    """A market series with a deliberate low-vol -> high-vol regime shift so the
    features have something real to pick up."""
    rng = np.random.default_rng(seed)
    vol = np.where(np.arange(n) < n // 2, 0.008, 0.025)  # calm then turbulent
    ret = rng.normal(0.0003, 1.0, n) * vol
    close = 100 * np.exp(np.cumsum(ret))
    intraday = np.abs(rng.normal(0, 1, n)) * vol * close
    high = close + intraday
    low = close - intraday
    volume = rng.lognormal(15, 0.4, n)
    return pd.DataFrame(
        {"open": close, "high": high, "low": low, "close": close, "volume": volume},
        index=DAYS[:n],
    )


# ---------------------------------------------------------------------------
# Done-when #1: clean feature frame, expected columns, no NaNs
# ---------------------------------------------------------------------------


def test_features_clean_and_named():
    feats = compute_features(synthetic_ohlcv())
    assert list(feats.columns) == ["rv_21", "rv_5", "vov_21", "range_5", "vol_ratio_63"]
    assert not feats.isna().any().any()  # warm-up dropped
    # the largest warm-up is the 63-day volume window: rolling(63) is first valid
    # at 0-based row 62, so that is where the feature frame begins
    assert feats.index[0] == DAYS[62]


def test_features_pick_up_the_regime_shift():
    feats = compute_features(synthetic_ohlcv())
    calm = feats.loc[feats.index < DAYS[200], "rv_21"].mean()
    turbulent = feats.loc[feats.index >= DAYS[200], "rv_21"].mean()
    assert turbulent > 2 * calm  # realized vol clearly separates the two regimes


# ---------------------------------------------------------------------------
# Done-when #2: strict as-of — feature at t uses only data <= t
# ---------------------------------------------------------------------------


def test_features_are_causal_appending_future_does_not_change_them():
    ohlcv = synthetic_ohlcv(seed=1)
    full = compute_features(ohlcv)
    truncated = compute_features(ohlcv.iloc[:250])
    common = truncated.index
    pd.testing.assert_frame_equal(full.loc[common], truncated, check_dtype=False)


def test_perturbing_the_future_leaves_todays_feature_unchanged():
    ohlcv = synthetic_ohlcv(seed=2)
    base = compute_features(ohlcv)
    t = ohlcv.index[100]  # a RAW row past warm-up (>= 62), so it's in the frame
    assert t in base.index
    bumped = ohlcv.copy()
    bumped.iloc[130:] *= 1.5  # mangle raw rows strictly after t's position (100)
    after = compute_features(bumped)
    pd.testing.assert_series_equal(base.loc[t], after.loc[t], check_names=True)


def test_feature_at_t_matches_manual_trailing_window():
    ohlcv = synthetic_ohlcv(seed=3)
    feats = compute_features(ohlcv)
    t_pos = 120
    t = ohlcv.index[t_pos]
    ret = ohlcv["close"].pct_change()
    # rv_21 at t must equal the std of exactly the trailing 21 returns ending at t
    manual = ret.iloc[t_pos - 20 : t_pos + 1].std(ddof=1) * np.sqrt(252)
    assert feats.loc[t, "rv_21"] == pytest.approx(manual)


# ---------------------------------------------------------------------------
# Build artifact + validation
# ---------------------------------------------------------------------------


def test_build_writes_parquet_and_manifest(tmp_path):
    csv = tmp_path / "MKT.csv"
    synthetic_ohlcv(seed=4).to_csv(csv, index_label="date")
    manifest = build(csv, tmp_path / "out")

    feats = pd.read_parquet(tmp_path / "out" / "regime_features.parquet")
    assert not feats.isna().any().any()
    assert manifest["rows"] == len(feats)
    assert manifest["market_proxy"] == "MKT"
    assert "t+1" in manifest["as_of_alignment"]  # the execution-lag contract
    assert manifest["features"] == list(feats.columns)


def test_load_market_requires_ohlcv(tmp_path):
    bad = tmp_path / "bad.csv"
    pd.DataFrame({"close": [1, 2, 3]}, index=DAYS[:3]).to_csv(bad, index_label="date")
    with pytest.raises(ValueError, match="missing columns"):
        load_market(bad)
