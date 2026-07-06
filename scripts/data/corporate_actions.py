"""Corporate actions handler (roadmap B2).

Back-adjusts historical price series for stock splits and cash dividends so
returns computed across event dates reflect economics, not bookkeeping:

    split (ratio s, ex-date D):    price[t < D] /= s,  volume[t < D] *= s
    dividend (amount d, ex-date D): price[t < D] *= (1 - d / close_before_D)

Rows strictly before the ex-date are adjusted; the series stays expressed in
the currently traded price (standard back-adjustment). Adjustment factors are
computed from the RAW series per event and applied as a cumulative product,
so multiple events compound correctly regardless of order — applying events
sequentially would mis-scale dividend factors once a later split rescales the
reference close.

Actions CSV format (see config/corporate_actions.csv):
    symbol,date,action,value
    AAPL,2020-08-31,split,4        # 4-for-1 split
    AAPL,2020-08-07,dividend,0.82  # $0.82/share, date = ex-date
"""

import logging

import pandas as pd

logger = logging.getLogger(__name__)

PRICE_COLUMNS = ("open", "high", "low", "close", "price")
VALID_ACTIONS = {"split", "dividend"}


def load_actions(path) -> pd.DataFrame:
    """Load and validate an actions CSV."""
    actions = pd.read_csv(path, parse_dates=["date"])
    required = {"symbol", "date", "action", "value"}
    missing = required - set(actions.columns)
    if missing:
        raise ValueError(f"Actions file {path} is missing columns: {sorted(missing)}")
    bad = set(actions["action"]) - VALID_ACTIONS
    if bad:
        raise ValueError(f"Unknown action types in {path}: {sorted(bad)}")
    return actions


def _cutoffs_for_index(index: pd.Index, ex_date: pd.Timestamp):
    """Return a boolean mask of rows strictly before the ex-date, handling
    datetime indexes and numeric epoch indexes (seconds or milliseconds)."""
    if isinstance(index, pd.DatetimeIndex):
        return index < ex_date
    cutoff = ex_date.timestamp()
    if len(index) > 0 and float(pd.Series(index).max()) > 1e11:
        cutoff *= 1000.0  # epoch milliseconds
    return index < cutoff


def adjust_for_corporate_actions(
    df: pd.DataFrame, actions: pd.DataFrame, symbol: str
) -> tuple[pd.DataFrame, list[dict]]:
    """Back-adjust `df` (bar OHLCV or tick price/volume, timestamp-indexed
    and sorted ascending) for `symbol`'s actions.

    Returns (adjusted_df, report) where the report lists every applied event
    with the factor used and the number of rows it touched.
    """
    price_cols = [c for c in PRICE_COLUMNS if c in df.columns]
    if not price_cols:
        raise ValueError(f"No price columns found; expected one of {PRICE_COLUMNS}")
    ref_col = "close" if "close" in df.columns else price_cols[0]

    df = df.copy()
    report: list[dict] = []
    price_factor = pd.Series(1.0, index=df.index)
    volume_factor = pd.Series(1.0, index=df.index)

    relevant = actions[actions["symbol"] == symbol].sort_values("date")
    for row in relevant.itertuples(index=False):
        ex_date = pd.Timestamp(row.date)
        mask = _cutoffs_for_index(df.index, ex_date)
        n_rows = int(mask.sum())
        if n_rows == 0:
            continue  # the series starts after this event

        if row.action == "split":
            ratio = float(row.value)
            if ratio <= 0:
                logger.warning(f"Skipping non-positive split ratio for {symbol}: {ratio}")
                continue
            price_factor[mask] /= ratio
            volume_factor[mask] *= ratio
            factor = 1.0 / ratio
        else:  # dividend
            amount = float(row.value)
            # Reference close: last RAW price before the ex-date
            prev_close = float(df.loc[mask, ref_col].iloc[-1])
            if amount <= 0 or amount >= prev_close:
                logger.warning(
                    f"Skipping implausible dividend for {symbol}: {amount} "
                    f"against reference close {prev_close}")
                continue
            factor = 1.0 - amount / prev_close
            price_factor[mask] *= factor

        report.append({
            "symbol": symbol,
            "date": ex_date.date().isoformat(),
            "action": row.action,
            "value": float(row.value),
            "factor": factor,
            "rows_adjusted": n_rows,
        })

    if not report:
        return df, report  # nothing applied - preserve the frame (and dtypes)

    for col in price_cols:
        df[col] = df[col] * price_factor
    if "volume" in df.columns:
        df["volume"] = df["volume"] * volume_factor

    return df, report
