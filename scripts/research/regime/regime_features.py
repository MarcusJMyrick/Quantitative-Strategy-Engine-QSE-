"""QR3.1 — Causal market-regime features for the HMM overlay.

The HMM (QR3.2) clusters market *states* from these features; it never sees
returns directly. Every feature is a TRAILING-window statistic of the market
proxy (SPY), so the value at date t depends only on data at dates <= t — the
strict as-of contract the whole regime overlay rests on. A look-ahead here would
silently leak the future into the regime label and inflate every downstream
backtest, so it is the property the tests hammer.

Features (windows configurable; defaults in trading days):
  rv_21          21-day annualized realized volatility — the primary regime axis
                 (low / high / crash separate mostly on this)
  rv_5           5-day annualized realized vol — fast moves that lead rv_21
  vov_21         21-day std of rv_21 — vol-of-vol, i.e. how unstable vol itself is
  range_5        5-day mean of (high − low) / close — an intraday-range /
                 spread-expansion proxy (we have daily bars, not quotes)
  vol_ratio_63   log(volume / 63-day mean volume) — the volume profile; surges
                 accompany regime shifts

Computed on unadjusted SPY daily bars: dividends (~0.3–0.5%/quarter) are
negligible for volatility/range/volume features, and the volume RATIO is robust
to the IEX-partial feed (numerator and denominator scale together).

Output: data/regime/regime_features.parquet (date × the features above)
        data/regime/regime_manifest.json (windows, span, as-of contract)
"""

import argparse
import json
import logging
import sys
from datetime import date
from pathlib import Path

import numpy as np
import pandas as pd

logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")
logger = logging.getLogger(__name__)

TRADING_DAYS = 252.0


def load_market(path: Path) -> pd.DataFrame:
    """Load a market-proxy OHLCV CSV (date index + open/high/low/close/volume)."""
    df = pd.read_csv(path, index_col=0, parse_dates=True).sort_index()
    missing = {"high", "low", "close", "volume"} - set(df.columns)
    if missing:
        raise ValueError(f"{path} missing columns: {sorted(missing)}")
    return df[~df.index.duplicated(keep="last")]


def compute_features(
    ohlcv: pd.DataFrame,
    rv_fast: int = 5,
    rv_slow: int = 21,
    vov_window: int = 21,
    range_window: int = 5,
    volume_window: int = 63,
) -> pd.DataFrame:
    """Trailing-window regime features. Every column at row t uses only rows
    <= t (rolling(...) is right-aligned), so the frame is causal by construction.
    Warm-up rows (any NaN) are dropped."""
    close = ohlcv["close"]
    ret = close.pct_change(fill_method=None)

    rv_slow_s = ret.rolling(rv_slow).std(ddof=1) * np.sqrt(TRADING_DAYS)
    feats = pd.DataFrame(index=ohlcv.index)
    feats["rv_21"] = rv_slow_s
    feats["rv_5"] = ret.rolling(rv_fast).std(ddof=1) * np.sqrt(TRADING_DAYS)
    feats["vov_21"] = rv_slow_s.rolling(vov_window).std(ddof=1)
    feats["range_5"] = ((ohlcv["high"] - ohlcv["low"]) / close).rolling(range_window).mean()
    feats["vol_ratio_63"] = np.log(ohlcv["volume"] / ohlcv["volume"].rolling(volume_window).mean())

    return feats.dropna(how="any")


def build(
    market_csv: Path,
    out_dir: Path,
    windows: dict | None = None,
) -> dict:
    """Build regime features from a market CSV; write parquet + manifest."""
    windows = windows or {}
    ohlcv = load_market(market_csv)
    feats = compute_features(ohlcv, **windows)
    if feats.isna().any().any():
        raise ValueError("regime features contain NaNs after warm-up drop")

    out_dir.mkdir(parents=True, exist_ok=True)
    feats_path = out_dir / "regime_features.parquet"
    feats.to_parquet(feats_path)

    manifest = {
        "generated": date.today().isoformat(),
        "market_proxy": market_csv.stem,
        "features": list(feats.columns),
        "windows": {
            "rv_fast": windows.get("rv_fast", 5),
            "rv_slow": windows.get("rv_slow", 21),
            "vov_window": windows.get("vov_window", 21),
            "range_window": windows.get("range_window", 5),
            "volume_window": windows.get("volume_window", 63),
        },
        "rows": len(feats),
        "span": [feats.index[0].date().isoformat(), feats.index[-1].date().isoformat()],
        "as_of_alignment": (
            "Every feature at date t is a trailing-window statistic using only "
            "rows <= t; warm-up rows are dropped. The HMM (QR3.2) must consume the "
            "filtered feature at t and act no earlier than t+1."
        ),
    }
    (out_dir / "regime_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    logger.info(
        f"Regime features: {len(feats)} rows x {feats.shape[1]} features "
        f"({manifest['span'][0]} -> {manifest['span'][1]}) -> {feats_path}"
    )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--market", type=Path, default=Path("data/regime/SPY.csv"))
    parser.add_argument("--out-dir", type=Path, default=Path("data/regime"))
    args = parser.parse_args()
    build(args.market, args.out_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
