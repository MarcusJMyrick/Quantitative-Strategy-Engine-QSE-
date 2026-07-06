#!/usr/bin/env python3
"""Institutional performance tearsheet (roadmap B3).

Pure metric functions (Sharpe, max drawdown, CAGR, Calmar, turnover, rolling
Sharpe, alpha/beta vs a benchmark) over engine output CSVs, plus a multi-page
PDF report built with matplotlib PdfPages.

Engine formats supported:
    equity:   timestamp,equity        (current)  or timestamp,portfolio_value
    tradelog: timestamp,symbol,type,quantity,price,cash  (current)
              or timestamp,type,price,quantity,commission (legacy)
    benchmark: date-indexed OHLCV CSV (e.g. data/raw_AAPL.csv); close is used

Usage (from the repo root):
    python scripts/analysis/tearsheet.py --equity equity_curve.csv \
        --tradelog tradelog.csv --benchmark data/raw_AAPL.csv \
        --out results/tearsheet.pdf --title "SMA 20/50 AAPL"
"""

import argparse
import datetime
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from matplotlib.backends.backend_pdf import PdfPages

TRADING_DAYS = 252
ROLLING_WINDOW = 63


# ---------------------------------------------------------------------------
# Metrics (pure functions, unit-tested in tests/python/test_tearsheet.py)
# ---------------------------------------------------------------------------


def daily_returns(equity: pd.Series) -> pd.Series:
    """Simple returns between consecutive equity observations."""
    return equity.pct_change().dropna()


def annualized_sharpe(
    returns: pd.Series, risk_free_rate: float = 0.0, periods: int = TRADING_DAYS
) -> float:
    """Annualized Sharpe ratio of per-period returns (sample std, ddof=1)."""
    if len(returns) < 2:
        return 0.0
    excess = returns - risk_free_rate / periods
    std = excess.std(ddof=1)
    if std == 0 or np.isnan(std):
        return 0.0
    return float(excess.mean() / std * np.sqrt(periods))


def max_drawdown(equity: pd.Series) -> float:
    """Largest peak-to-trough decline as a negative fraction (0 = no loss)."""
    if len(equity) == 0:
        return 0.0
    peak = equity.cummax()
    return float(((equity - peak) / peak).min())


def cagr(equity: pd.Series, periods: int = TRADING_DAYS) -> float:
    """Compound annual growth rate, with len-1 observations = one period."""
    n = len(equity) - 1
    if n <= 0 or equity.iloc[0] <= 0:
        return 0.0
    years = n / periods
    return float((equity.iloc[-1] / equity.iloc[0]) ** (1.0 / years) - 1.0)


def calmar_ratio(equity: pd.Series, periods: int = TRADING_DAYS) -> float:
    """CAGR over absolute max drawdown; 0.0 when there is no drawdown."""
    mdd = max_drawdown(equity)
    if mdd == 0:
        return 0.0
    return cagr(equity, periods) / abs(mdd)


def annualized_turnover(
    trades: pd.DataFrame, equity: pd.Series, periods: int = TRADING_DAYS
) -> float:
    """Traded notional (|quantity| * price, single-counted) over mean equity,
    per year of the equity history."""
    n = len(equity) - 1
    if n <= 0 or trades is None or len(trades) == 0:
        return 0.0
    notional = float((trades["quantity"].abs() * trades["price"]).sum())
    years = n / periods
    return notional / float(equity.mean()) / years


def rolling_sharpe(
    returns: pd.Series, window: int = ROLLING_WINDOW, periods: int = TRADING_DAYS
) -> pd.Series:
    """Annualized Sharpe over a rolling window of per-period returns."""
    mean = returns.rolling(window).mean()
    std = returns.rolling(window).std(ddof=1)
    return mean / std * np.sqrt(periods)


def alpha_beta(
    strategy_returns: pd.Series, benchmark_returns: pd.Series, periods: int = TRADING_DAYS
) -> tuple[float, float]:
    """OLS regression strategy = alpha + beta * benchmark on the aligned
    overlap. Returns (annualized alpha, beta); (nan, nan) if the overlap is
    too short to regress."""
    df = pd.concat([strategy_returns, benchmark_returns], axis=1, join="inner").dropna()
    if len(df) < 3:
        return (float("nan"), float("nan"))
    y = df.iloc[:, 0].to_numpy(dtype=float)
    x = df.iloc[:, 1].to_numpy(dtype=float)
    beta, alpha_per_period = np.polyfit(x, y, 1)
    return (float(alpha_per_period * periods), float(beta))


# ---------------------------------------------------------------------------
# Loaders
# ---------------------------------------------------------------------------


def _parse_timestamps(ts: pd.Series) -> pd.Index:
    """Epoch timestamps of any resolution -> DatetimeIndex; non-numeric values
    are parsed as date strings; small integers are kept as a plain index
    (bar counters from old runs)."""
    if pd.api.types.is_numeric_dtype(ts):
        mx = float(ts.max())
        for threshold, unit in ((1e17, "ns"), (1e14, "us"), (1e11, "ms"), (1e8, "s")):
            if mx > threshold:
                return pd.to_datetime(ts, unit=unit)
        return pd.Index(ts)
    return pd.to_datetime(ts)


def load_equity(path: str) -> pd.Series:
    df = pd.read_csv(path)
    value_col = next((c for c in ("equity", "portfolio_value", "value") if c in df.columns), None)
    if value_col is None:
        raise ValueError(f"No equity column in {path}; columns: {list(df.columns)}")
    series = pd.Series(
        df[value_col].to_numpy(dtype=float), index=_parse_timestamps(df["timestamp"]), name="equity"
    )
    # Keep the last observation per timestamp (intraday duplicates)
    series = series[~series.index.duplicated(keep="last")].sort_index()
    return series


def load_tradelog(path: str) -> pd.DataFrame:
    df = pd.read_csv(path)
    if "quantity" not in df.columns or "price" not in df.columns:
        raise ValueError(f"Tradelog {path} needs quantity and price columns")
    return df


def load_benchmark(path: str) -> pd.Series:
    df = pd.read_csv(path, index_col=0, parse_dates=True)
    if "close" not in df.columns:
        raise ValueError(f"Benchmark {path} needs a close column")
    return df["close"].dropna().sort_index()


def to_daily(equity: pd.Series) -> pd.Series:
    """Last observation per calendar day for datetime-indexed curves;
    non-datetime curves are returned unchanged."""
    if isinstance(equity.index, pd.DatetimeIndex):
        return equity.resample("1D").last().dropna()
    return equity


# ---------------------------------------------------------------------------
# Tearsheet PDF
# ---------------------------------------------------------------------------


def compute_summary(
    equity: pd.Series,
    trades: pd.DataFrame | None,
    benchmark: pd.Series | None,
    periods: int = TRADING_DAYS,
) -> dict:
    returns = daily_returns(equity)
    summary = {
        "Observations": len(equity),
        "Total return": equity.iloc[-1] / equity.iloc[0] - 1.0,
        "CAGR": cagr(equity, periods),
        "Volatility (ann.)": (
            float(returns.std(ddof=1) * np.sqrt(periods)) if len(returns) > 1 else 0.0
        ),
        "Sharpe (ann.)": annualized_sharpe(returns, periods=periods),
        "Max drawdown": max_drawdown(equity),
        "Calmar": calmar_ratio(equity, periods),
    }
    if trades is not None and len(trades) > 0:
        summary["Trades"] = len(trades)
        summary["Turnover (ann.)"] = annualized_turnover(trades, equity, periods)
    if benchmark is not None:
        bench_returns = daily_returns(benchmark)
        alpha, beta = alpha_beta(returns, bench_returns, periods)
        summary["Alpha (ann.)"] = alpha
        summary["Beta"] = beta
    return summary


def _format_value(key: str, value) -> str:
    if isinstance(value, int):
        return f"{value:,}"
    percent_keys = ("return", "CAGR", "Volatility", "drawdown", "Alpha")
    if any(k.lower() in key.lower() for k in percent_keys):
        return f"{value * 100:.2f}%"
    return f"{value:.3f}"


def build_tearsheet(
    equity: pd.Series,
    trades: pd.DataFrame | None,
    benchmark: pd.Series | None,
    out_path: str,
    title: str = "Strategy",
) -> dict:
    """Render the multi-page PDF and return the summary metrics."""
    daily = to_daily(equity)
    returns = daily_returns(daily)
    summary = compute_summary(daily, trades, benchmark)

    bench_daily = to_daily(benchmark) if benchmark is not None else None
    if bench_daily is not None and isinstance(daily.index, pd.DatetimeIndex):
        bench_daily = bench_daily.loc[
            (bench_daily.index >= daily.index.min()) & (bench_daily.index <= daily.index.max())
        ]
        if bench_daily.empty:
            bench_daily = None

    with PdfPages(out_path) as pdf:
        # Page 1: equity, drawdown, summary table
        fig, axes = plt.subplots(3, 1, figsize=(8.5, 11), gridspec_kw={"height_ratios": [3, 2, 2]})
        fig.suptitle(f"Performance Tearsheet — {title}", fontsize=14)

        axes[0].plot(daily.index, daily.values, color="tab:blue")
        axes[0].set_title("Equity curve")
        axes[0].grid(alpha=0.3)

        drawdown = (daily - daily.cummax()) / daily.cummax()
        axes[1].fill_between(daily.index, drawdown.values, 0, color="tab:red", alpha=0.4)
        axes[1].set_title("Drawdown")
        axes[1].grid(alpha=0.3)

        axes[2].axis("off")
        rows = [[k, _format_value(k, v)] for k, v in summary.items()]
        table = axes[2].table(
            cellText=rows, colLabels=["Metric", "Value"], loc="center", cellLoc="left"
        )
        table.auto_set_font_size(False)
        table.set_fontsize(9)
        table.scale(1.0, 1.3)
        fig.tight_layout(rect=(0, 0, 1, 0.96))
        pdf.savefig(fig)
        plt.close(fig)

        # Page 2: rolling Sharpe and return distribution
        fig, axes = plt.subplots(2, 1, figsize=(8.5, 11))
        window = ROLLING_WINDOW
        if len(returns) < 2 * window:
            window = max(5, len(returns) // 4)
        rs = rolling_sharpe(returns, window=window)
        axes[0].plot(rs.index, rs.values, color="tab:green")
        axes[0].axhline(0.0, color="gray", lw=0.8)
        axes[0].set_title(f"Rolling Sharpe ({window}-period window, annualized)")
        axes[0].grid(alpha=0.3)

        axes[1].hist(returns.values, bins=50, color="tab:blue", alpha=0.7)
        axes[1].axvline(
            float(returns.mean()),
            color="tab:red",
            lw=1.0,
            label=f"mean {returns.mean() * 1e4:.1f} bps",
        )
        axes[1].set_title("Daily return distribution")
        axes[1].legend()
        axes[1].grid(alpha=0.3)
        fig.tight_layout()
        pdf.savefig(fig)
        plt.close(fig)

        # Page 3: benchmark-relative view
        if bench_daily is not None and len(bench_daily) >= 3:
            bench_returns = daily_returns(bench_daily)
            fig, axes = plt.subplots(2, 1, figsize=(8.5, 11))

            strat_rebased = daily / daily.iloc[0]
            bench_rebased = bench_daily / bench_daily.iloc[0]
            axes[0].plot(
                strat_rebased.index, strat_rebased.values, label="strategy", color="tab:blue"
            )
            axes[0].plot(
                bench_rebased.index,
                bench_rebased.values,
                label="benchmark (buy & hold)",
                color="tab:gray",
            )
            axes[0].set_title("Growth of $1 vs benchmark")
            axes[0].legend()
            axes[0].grid(alpha=0.3)

            aligned = pd.concat([returns, bench_returns], axis=1, join="inner").dropna()
            if len(aligned) >= 3:
                y = aligned.iloc[:, 0].to_numpy(dtype=float)
                x = aligned.iloc[:, 1].to_numpy(dtype=float)
                beta, alpha_daily = np.polyfit(x, y, 1)
                axes[1].scatter(x, y, s=12, alpha=0.6, color="tab:blue")
                grid = np.linspace(x.min(), x.max(), 50)
                axes[1].plot(
                    grid,
                    alpha_daily + beta * grid,
                    color="tab:red",
                    label=(
                        f"alpha {alpha_daily * TRADING_DAYS * 100:.2f}%/yr, " f"beta {beta:.2f}"
                    ),
                )
                axes[1].set_xlabel("Benchmark daily return")
                axes[1].set_ylabel("Strategy daily return")
                axes[1].set_title("Return regression vs benchmark")
                axes[1].legend()
                axes[1].grid(alpha=0.3)
            fig.tight_layout()
            pdf.savefig(fig)
            plt.close(fig)

        meta = pdf.infodict()
        meta["Title"] = f"Tearsheet - {title}"
        meta["CreationDate"] = datetime.datetime.now()

    return summary


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--equity", required=True, help="Equity curve CSV")
    parser.add_argument("--tradelog", help="Trade log CSV")
    parser.add_argument("--benchmark", help="Benchmark OHLCV CSV (close used)")
    parser.add_argument("--out", default="tearsheet.pdf", help="Output PDF path")
    parser.add_argument("--title", default="Strategy", help="Report title")
    args = parser.parse_args()

    equity = load_equity(args.equity)
    if len(equity) < 2:
        print(f"Equity curve {args.equity} has fewer than 2 points", file=sys.stderr)
        return 1
    trades = load_tradelog(args.tradelog) if args.tradelog else None
    benchmark = load_benchmark(args.benchmark) if args.benchmark else None

    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    summary = build_tearsheet(equity, trades, benchmark, args.out, args.title)

    print(f"Wrote {args.out}")
    width = max(len(k) for k in summary)
    for key, value in summary.items():
        print(f"  {key:<{width}}  {_format_value(key, value)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
