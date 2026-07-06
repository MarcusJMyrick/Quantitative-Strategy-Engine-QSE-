"""QR4.1 — Universe + standardized-returns matrix for eigenportfolio stat arb.

Builds the input to the QR4 pipeline: a T×N daily returns matrix `R` over a
liquid single-sector universe, and its rolling-standardized counterpart `Y`,
emitted as Parquet with a manifest documenting every repair and adjustment.

Pipeline (per the roadmap: adjusted daily bars through the existing
corporate-actions (B2) + ffill (B1) machinery):

    raw daily bars (Alpaca, adjustment=raw, or local CSVs)
      -> B2 back-adjustment from config/corporate_actions.csv
      -> close panel on the union trading-day grid
      -> B1-style forward-fill of interior gaps (every repair counted)
      -> simple returns  R_t = P_t / P_{t-1} - 1
      -> rolling standardization  Y_t = (R_t - mean_w(t)) / std_w(t)

As-of alignment contract (the no-look-ahead guarantee QR4.2+ depend on):
row t of `Y` uses ONLY data at dates <= t — the trailing window covers
returns r_{t-w+1} .. r_t inclusive, all observable at the close of day t.
Rows before the window is full are dropped, never partially standardized.
Appending future data can never change an already-emitted row (tested).
A consumer of row t must not execute before the next session (t+1).

Known limitations (documented, not hidden — see docs/research/statarb/):
  * survivorship/selection bias: the universe is today's large-cap tech
    names applied retroactively;
  * dividends are only adjusted for events listed in the actions file, so
    ex-dividend days carry a small artificial negative return (~0.2-0.7%
    quarterly for the payers here, second-order vs ~2% daily vol);
  * a single-day |return| above --extreme-return is flagged loudly as a
    suspected missing corporate action.
"""

import argparse
import json
import logging
import os
import sys
from datetime import date
from pathlib import Path

import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "data"))

from corporate_actions import adjust_for_corporate_actions, load_actions  # noqa: E402

logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")
logger = logging.getLogger(__name__)

# One sector (large-cap US technology) so the correlation matrix has real
# factor structure for QR4.2's PCA; all names trade $1B+/day so QR4.7's
# Engine B size sweeps are meaningful. 15 names per the roadmap's
# "prove it on 10-15 before scaling to 100".
UNIVERSE = [
    "AAPL",
    "MSFT",
    "GOOG",
    "AMZN",
    "META",
    "NVDA",
    "AVGO",
    "AMD",
    "INTC",
    "QCOM",
    "TXN",
    "MU",
    "ADBE",
    "CRM",
    "ORCL",
]

ALPACA_BARS_URL = "https://data.alpaca.markets/v2/stocks/bars"
BAR_COLUMNS = ["open", "high", "low", "close", "volume"]


# ---------------------------------------------------------------------------
# Fetch (raw bars -> per-symbol CSVs, same daily-bar shape as data/raw_*.csv)
# ---------------------------------------------------------------------------


def _default_get_json(params: dict) -> dict:
    """GET the Alpaca bars endpoint with credentials from the environment."""
    import requests

    key_id = os.environ.get("APCA_API_KEY_ID")
    secret = os.environ.get("APCA_API_SECRET_KEY")
    if not key_id or not secret:
        raise RuntimeError(
            "APCA_API_KEY_ID / APCA_API_SECRET_KEY not set; run `set -a; source .env; set +a`"
        )
    response = requests.get(
        ALPACA_BARS_URL,
        params=params,
        headers={"APCA-API-KEY-ID": key_id, "APCA-API-SECRET-KEY": secret},
        timeout=30,
    )
    response.raise_for_status()
    return response.json()


def fetch_alpaca_daily(symbols, start, end, feed="sip", get_json=None) -> dict:
    """Fetch RAW (unadjusted) daily bars for `symbols`, paginating until done.

    Bars are requested unadjusted on purpose: split/dividend adjustment is
    the job of the audited B2 pipeline, not the vendor. Returns
    {symbol -> DataFrame(open, high, low, close, volume)} indexed by date.
    """
    get_json = get_json or _default_get_json
    params = {
        "symbols": ",".join(symbols),
        "timeframe": "1Day",
        "start": str(start),
        "end": str(end),
        "adjustment": "raw",
        "feed": feed,
        "limit": 10000,
        "sort": "asc",
    }
    rows: dict[str, list] = {sym: [] for sym in symbols}
    while True:
        payload = get_json(params)
        for sym, bars in (payload.get("bars") or {}).items():
            rows[sym].extend(bars)
        token = payload.get("next_page_token")
        if not token:
            break
        params["page_token"] = token

    out = {}
    for sym in symbols:
        if not rows[sym]:
            raise RuntimeError(f"No bars returned for {sym} in [{start}, {end}] (feed={feed})")
        df = pd.DataFrame(rows[sym])
        df["date"] = pd.to_datetime(df["t"]).dt.tz_localize(None).dt.normalize()
        df = (
            df.rename(columns={"o": "open", "h": "high", "l": "low", "c": "close", "v": "volume"})[
                ["date"] + BAR_COLUMNS
            ]
            .set_index("date")
            .sort_index()
        )
        df = df[~df.index.duplicated(keep="last")]
        out[sym] = df
    return out


def save_bars(bars: dict, bars_dir: Path) -> None:
    bars_dir.mkdir(parents=True, exist_ok=True)
    for sym, df in bars.items():
        df.to_csv(bars_dir / f"{sym}.csv", index_label="date")
        logger.info(f"Saved {len(df)} raw daily bars for {sym} to {bars_dir / f'{sym}.csv'}")


def load_universe_bars(bars_dir: Path, symbols) -> dict:
    """Load per-symbol daily-bar CSVs written by `save_bars` (or hand-placed
    files in the same shape as data/raw_*.csv: date index + OHLCV columns)."""
    bars = {}
    for sym in symbols:
        path = bars_dir / f"{sym}.csv"
        if not path.exists():
            raise FileNotFoundError(f"{path} not found; run with --fetch first")
        df = pd.read_csv(path, index_col=0, parse_dates=True).sort_index()
        missing = [c for c in ("close",) if c not in df.columns]
        if missing:
            raise ValueError(f"{path} is missing columns: {missing}")
        bars[sym] = df
    return bars


# ---------------------------------------------------------------------------
# Build (B2 adjust -> panel -> B1-style ffill -> returns -> standardize)
# ---------------------------------------------------------------------------


def apply_actions(bars: dict, actions_path: Path) -> tuple[dict, list]:
    """Back-adjust every symbol through the B2 corporate-actions handler."""
    actions = load_actions(actions_path)
    adjusted, events = {}, []
    for sym, df in bars.items():
        adjusted[sym], report = adjust_for_corporate_actions(df, actions, sym)
        events.extend(report)
        for event in report:
            logger.info(f"Corporate action applied: {event}")
    return adjusted, events


def build_close_panel(bars: dict) -> tuple[pd.DataFrame, dict]:
    """Assemble the T×N close-price panel on the union trading-day grid.

    Interior gaps (a symbol missing a day others traded) are forward-filled
    B1-style and counted per symbol; rows before every symbol has printed at
    least once are dropped (nothing to fill from), also counted.
    """
    panel = pd.DataFrame({sym: df["close"] for sym, df in bars.items()}).sort_index()

    live = panel.notna().cummax()
    all_live = live.all(axis=1)
    leading_dropped = int((~all_live).sum())
    panel = panel[all_live]

    fills = {sym: int(panel[sym].isna().sum()) for sym in panel.columns}
    panel = panel.ffill()

    for sym, n in sorted(fills.items(), key=lambda kv: -kv[1]):
        if n:
            logger.warning(f"Data quality: forward-filled {n} missing close(s) for {sym}")

    report = {
        "rows": len(panel),
        "start": panel.index[0].date().isoformat() if len(panel) else None,
        "end": panel.index[-1].date().isoformat() if len(panel) else None,
        "leading_rows_dropped": leading_dropped,
        "fills_per_symbol": fills,
    }
    return panel, report


def compute_returns(panel: pd.DataFrame) -> pd.DataFrame:
    """Simple daily returns R_t = P_t / P_{t-1} - 1; the first row (no prior
    close) is dropped, so the result is NaN-free by construction."""
    returns = panel.pct_change(fill_method=None).iloc[1:]
    if returns.isna().any().any():
        bad = returns.columns[returns.isna().any()].tolist()
        raise ValueError(f"NaN returns after cleaning for {bad}; upstream repair failed")
    return returns


def rolling_standardize(returns: pd.DataFrame, window: int) -> pd.DataFrame:
    """Per-name rolling standardization Y_t = (R_t - mean) / std over the
    TRAILING window of `window` observations ending at (and including) t.

    As-of safe: row t sees only rows <= t, so appending future data never
    changes an already-emitted row. The first window-1 rows are dropped
    (window not yet full) rather than standardized against short samples.
    std uses ddof=1.
    """
    mean = returns.rolling(window, min_periods=window).mean()
    std = returns.rolling(window, min_periods=window).std(ddof=1)

    degenerate = (std == 0.0) & std.notna()
    if degenerate.any().any():
        sym = degenerate.any()[degenerate.any()].index.tolist()
        raise ValueError(
            f"Zero return variance inside a {window}-day window for {sym}; "
            "degenerate input data (constant prices)"
        )

    return ((returns - mean) / std).dropna(how="any")


def flag_extreme_returns(returns: pd.DataFrame, threshold: float) -> list[dict]:
    """Single-day |return| > threshold is almost always an unadjusted
    corporate action at daily frequency, not a price move. Flag, loudly."""
    flagged = []
    mask = returns.abs() > threshold
    for sym in returns.columns[mask.any()]:
        for ts in returns.index[mask[sym]]:
            flagged.append(
                {"symbol": sym, "date": ts.date().isoformat(), "return": float(returns.at[ts, sym])}
            )
    for f in flagged:
        logger.warning(
            f"SUSPECTED MISSING CORPORATE ACTION: {f['symbol']} {f['date']} "
            f"return {f['return']:+.1%} exceeds |{threshold:.0%}| — check "
            "config/corporate_actions.csv before trusting this matrix"
        )
    return flagged


def build(
    bars_dir: Path,
    out_dir: Path,
    symbols,
    window: int,
    actions_path: Path,
    extreme_return: float = 0.45,
) -> dict:
    """Run the full QR4.1 build; write Parquets + manifest; return the manifest."""
    bars = load_universe_bars(bars_dir, symbols)
    bars, ca_events = apply_actions(bars, actions_path)
    panel, panel_report = build_close_panel(bars)
    returns = compute_returns(panel)
    flagged = flag_extreme_returns(returns, extreme_return)
    standardized = rolling_standardize(returns, window)

    assert not returns.isna().any().any(), "returns matrix contains NaNs"
    assert not standardized.isna().any().any(), "standardized matrix contains NaNs"

    out_dir.mkdir(parents=True, exist_ok=True)
    returns_path = out_dir / "universe_returns.parquet"
    standardized_path = out_dir / "universe_standardized.parquet"
    returns.to_parquet(returns_path)
    standardized.to_parquet(standardized_path)

    manifest = {
        "generated": date.today().isoformat(),
        "symbols": list(symbols),
        "window": window,
        "panel": panel_report,
        "returns_rows": len(returns),
        "standardized_rows": len(standardized),
        "standardized_start": standardized.index[0].date().isoformat(),
        "standardized_end": standardized.index[-1].date().isoformat(),
        "corporate_actions_applied": ca_events,
        "extreme_returns_flagged": flagged,
        "as_of_alignment": (
            f"Row t standardizes r_t against the trailing {window}-day window "
            "r_{t-w+1}..r_t (inclusive), all observable at the close of day t; "
            "warm-up rows are dropped. Consumers must not execute before t+1."
        ),
        "files": {"returns": str(returns_path), "standardized": str(standardized_path)},
    }
    manifest_path = out_dir / "universe_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    logger.info(
        f"Universe built: {len(standardized)} standardized rows x "
        f"{len(symbols)} names -> {standardized_path}"
    )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--fetch", action="store_true", help="fetch raw daily bars from Alpaca")
    parser.add_argument("--start", default="2018-01-01", help="fetch start date (YYYY-MM-DD)")
    parser.add_argument("--end", default=str(date.today()), help="fetch end date (YYYY-MM-DD)")
    parser.add_argument("--feed", default="sip", choices=["sip", "iex"], help="Alpaca data feed")
    parser.add_argument("--symbols", nargs="*", default=UNIVERSE)
    parser.add_argument("--window", type=int, default=60, help="rolling standardization window")
    parser.add_argument("--bars-dir", type=Path, default=Path("data/universe"))
    parser.add_argument("--out-dir", type=Path, default=Path("data/universe"))
    parser.add_argument("--actions", type=Path, default=Path("config/corporate_actions.csv"))
    parser.add_argument("--extreme-return", type=float, default=0.45)
    args = parser.parse_args()

    if args.fetch:
        try:
            bars = fetch_alpaca_daily(args.symbols, args.start, args.end, feed=args.feed)
        except Exception as exc:
            if args.feed == "sip" and "403" in str(exc):
                logger.warning("SIP feed forbidden on this plan; retrying with IEX feed")
                bars = fetch_alpaca_daily(args.symbols, args.start, args.end, feed="iex")
            else:
                raise
        save_bars(bars, args.bars_dir)

    build(
        bars_dir=args.bars_dir,
        out_dir=args.out_dir,
        symbols=args.symbols,
        window=args.window,
        actions_path=args.actions,
        extreme_return=args.extreme_return,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
