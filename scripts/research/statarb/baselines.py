"""QR4.6 — Cheap baselines (the floor): cross-sectional reversal + 12-1 momentum.

The eigenportfolio stat arb (QR4.1-4.5) is elaborate: rolling PCA, RMT factor
selection, OU residual modeling. This module builds the two dumb strategies it
has to beat to justify that complexity. If the fancy version can't clear naive
*reversal* net of Engine B costs, that is itself a finding — the machinery is
not earning its keep.

  cross-sectional short-term reversal: buy recent losers, sell recent winners.
      signal_i = -(trailing `reversal_lookback`-day return); long the lowest
      raw returns, short the highest. The stat arb is reversal on *idiosyncratic
      residuals*; this is reversal on *raw* returns, so beating it measures what
      the factor removal buys.

  12-1 momentum: buy 12-month winners, sell losers, skipping the most recent
      month (the standard Jegadeesh-Titman / Carhart construction — the skip
      avoids contaminating momentum with short-term reversal).
      signal_i = cumulative return from t-`mom_lookback` to t-`mom_skip`.

Same harness by construction. Both baselines reuse QR4.5's dollar-neutral
construction (`weights_from_positions`) and weight-file emission
(`write_weight_files`) unchanged, so all three strategies emit the identical
`weights_YYYYMMDD.csv` format, on the same universe, with the same execution
lag — the apples-to-apples comparison QR4.7 then runs through Engine B. (The
QR-track proposal framed these as additions to the C++ MultiFactorCalculator;
they live here instead precisely so they share the stat-arb weight path rather
than a separate one.)

As-of alignment. The reversal signal at date t uses returns r_{t-k+1}..r_t; the
momentum signal uses r_{t-lookback+1}..r_{t-skip}. Both use data <= t only, and
weights are executed at t+execution_lag (inherited from QR4.5). Appending future
data never changes an emitted row (tested).

The paper-PnL comparison is COST-FREE and PROVISIONAL. `paper_pnl` marks each
book to the next available return with no slippage — the optimistic view the
whole track exists to puncture. These Sharpes are candidates, not results:
QR4.7 replaces them with Engine B fills and QR-P2 deflates them for the search.

Outputs (per strategy `name` in {reversal, momentum}):
  data/universe/weights_<name>/weights_YYYYMMDD.csv   (gitignored)
  data/universe/baseline_<name>_weights.parquet
  data/universe/baseline_<name>_positions.parquet
  data/universe/baseline_manifest.json                 params + paper Sharpes
Plot (committed): docs/research/statarb/baseline_comparison.png
"""

import argparse
import json
import logging
import sys
from datetime import date
from pathlib import Path

import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parent))

from signals import (
    LONG,
    SHORT,
    diagnostics,
    weights_from_positions,
    write_weight_files,
)  # noqa: E402

logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")
logger = logging.getLogger(__name__)

TRADING_DAYS = 252.0


# ---------------------------------------------------------------------------
# Signals (pure functions, unit-tested)
# ---------------------------------------------------------------------------


def reversal_signal(returns: pd.DataFrame, lookback: int = 5) -> pd.DataFrame:
    """Short-term reversal: signal = -(trailing `lookback`-day log return), so
    the biggest recent losers get the highest signal (long them). Uses data
    through t; the first `lookback` rows are NaN (warm-up)."""
    cum = np.log1p(returns).cumsum()
    trailing = cum - cum.shift(lookback)  # sum of log returns over (t-lookback, t]
    return -trailing


def momentum_signal(returns: pd.DataFrame, lookback: int = 252, skip: int = 21) -> pd.DataFrame:
    """12-1 momentum: cumulative log return from t-lookback to t-skip (skip the
    most recent `skip` days). Winners get the highest signal (long them). Uses
    data through t-skip only; the first `lookback` rows are NaN."""
    cum = np.log1p(returns).cumsum()
    return cum.shift(skip) - cum.shift(lookback)  # log return over (t-lookback, t-skip]


def signal_to_positions(signal: pd.DataFrame, frac: float = 1.0 / 3.0) -> pd.DataFrame:
    """Cross-sectional long/short: each day, long the top `frac` of names by
    signal and short the bottom `frac`, flat in between. Long and short counts
    are kept equal (and disjoint) so the book is dollar-neutralizable. Rows with
    fewer than two valid signals stay flat."""
    positions = pd.DataFrame(0, index=signal.index, columns=signal.columns, dtype=int)
    for t in signal.index:
        row = signal.loc[t].dropna()
        if len(row) < 2:
            continue
        k = max(1, int(round(len(row) * frac)))
        k = min(k, len(row) // 2)  # disjoint long/short sets
        order = row.sort_values()
        positions.loc[t, order.index[:k]] = SHORT  # lowest signal
        positions.loc[t, order.index[-k:]] = LONG  # highest signal
    return positions


# ---------------------------------------------------------------------------
# Cost-free paper PnL (provisional — Engine B replaces this in QR4.7)
# ---------------------------------------------------------------------------


def paper_pnl(weights: pd.DataFrame, returns: pd.DataFrame, execution_lag: int = 1) -> pd.Series:
    """Cost-free daily portfolio return. A book chosen from the signal at t is
    filed for t+execution_lag and established at that close, so it earns the
    return of the following day: realized PnL lags the signal by
    execution_lag + 1. No slippage, no impact — the optimistic view QR4.7
    corrects."""
    total_lag = execution_lag + 1
    held = weights.reindex(returns.index).shift(total_lag)
    return (held * returns).sum(axis=1, min_count=1).fillna(0.0)


def annualized_sharpe(daily: pd.Series, periods: float = TRADING_DAYS) -> float:
    active = daily[daily != 0.0]
    if len(active) < 2 or active.std(ddof=1) == 0.0:
        return 0.0
    return float(active.mean() / active.std(ddof=1) * np.sqrt(periods))


# ---------------------------------------------------------------------------
# Per-strategy driver
# ---------------------------------------------------------------------------


def generate_baseline(
    returns: pd.DataFrame, kind: str, frac: float = 1.0 / 3.0, gross: float = 1.0, **params
) -> dict:
    """Build one baseline: signal -> cross-sectional positions -> dollar-neutral
    weights (reusing the QR4.5 construction). `kind` in {reversal, momentum}."""
    if kind == "reversal":
        signal = reversal_signal(returns, lookback=params.get("reversal_lookback", 5))
    elif kind == "momentum":
        signal = momentum_signal(
            returns,
            lookback=params.get("mom_lookback", 252),
            skip=params.get("mom_skip", 21),
        )
    else:
        raise ValueError(f"unknown baseline {kind!r}; expected reversal | momentum")

    positions = signal_to_positions(signal, frac=frac)
    weights = weights_from_positions(positions, gross=gross)
    return {
        "signal": signal,
        "positions": positions,
        "weights": weights,
        "diagnostics": diagnostics(weights, positions),
    }


# ---------------------------------------------------------------------------
# Artifacts
# ---------------------------------------------------------------------------


def plot_comparison(equities: dict, sharpes: dict, path: Path):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(10, 6))
    for name, eq in equities.items():
        ax.plot(eq.index, eq, lw=1.1, label=f"{name} (paper Sharpe {sharpes[name]:.2f})")
    ax.axhline(1.0, color="gray", ls=":", lw=0.8)
    ax.set_ylabel("cost-free cumulative return (start = 1.0)")
    ax.set_xlabel("date")
    ax.set_title(
        "Stat arb vs cheap baselines — COST-FREE paper PnL "
        "(provisional: Engine B + DSR judge the real result)"
    )
    ax.legend(loc="upper left", fontsize=9)
    fig.tight_layout()
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=150)
    plt.close(fig)
    logger.info(f"Comparison plot written to {path}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--returns", type=Path, default=Path("data/universe/universe_returns.parquet")
    )
    parser.add_argument(
        "--frac", type=float, default=1.0 / 3.0, help="fraction long/short per side"
    )
    parser.add_argument("--gross", type=float, default=1.0)
    parser.add_argument("--execution-lag", type=int, default=1)
    parser.add_argument("--reversal-lookback", type=int, default=5)
    parser.add_argument("--mom-lookback", type=int, default=252)
    parser.add_argument("--mom-skip", type=int, default=21)
    parser.add_argument("--out-dir", type=Path, default=Path("data/universe"))
    parser.add_argument(
        "--statarb-weights",
        type=Path,
        default=Path("data/universe/signal_weights.parquet"),
        help="QR4.5 stat-arb weights, for the 3-way paper-PnL comparison (optional)",
    )
    parser.add_argument(
        "--plot", type=Path, default=Path("docs/research/statarb/baseline_comparison.png")
    )
    args = parser.parse_args()

    returns = pd.read_parquet(args.returns)
    args.out_dir.mkdir(parents=True, exist_ok=True)

    books, equities, sharpes = {}, {}, {}
    manifest = {
        "generated": date.today().isoformat(),
        "frac_per_side": args.frac,
        "gross_cap": args.gross,
        "execution_lag_days": args.execution_lag,
        "reversal_lookback": args.reversal_lookback,
        "momentum": {"lookback": args.mom_lookback, "skip": args.mom_skip},
        "paper_pnl_caveat": (
            "Sharpes below are COST-FREE and PROVISIONAL (candidates, not results); "
            "QR4.7 runs Engine B fills and QR-P2 deflates for the search."
        ),
        "strategies": {},
    }

    for kind in ("reversal", "momentum"):
        out = generate_baseline(
            returns,
            kind,
            frac=args.frac,
            gross=args.gross,
            reversal_lookback=args.reversal_lookback,
            mom_lookback=args.mom_lookback,
            mom_skip=args.mom_skip,
        )
        books[kind] = out
        out["positions"].to_parquet(args.out_dir / f"baseline_{kind}_positions.parquet")
        out["weights"].to_parquet(args.out_dir / f"baseline_{kind}_weights.parquet")
        written = write_weight_files(
            out["weights"], args.out_dir / f"weights_{kind}", args.execution_lag
        )

        pnl = paper_pnl(out["weights"], returns, args.execution_lag)
        equities[kind] = (1.0 + pnl).cumprod()
        sharpes[kind] = annualized_sharpe(pnl)
        diag = out["diagnostics"]
        manifest["strategies"][kind] = {
            "weight_files_written": len(written),
            "days_with_a_book": int((diag["gross"] > 0).sum()),
            "mean_daily_turnover": float(diag["turnover"].mean()),
            "max_abs_net_exposure": float(diag["net"].abs().max()),
            "paper_sharpe": sharpes[kind],
        }
        logger.info(
            f"{kind}: {len(written)} files, paper Sharpe {sharpes[kind]:.2f}, "
            f"turnover {diag['turnover'].mean():.3f}, max|net| {diag['net'].abs().max():.1e}"
        )

    # Stat arb from QR4.5, if available, for the 3-way comparison
    if args.statarb_weights.exists():
        sa = pd.read_parquet(args.statarb_weights)
        pnl = paper_pnl(sa, returns, args.execution_lag)
        equities["stat_arb"] = (1.0 + pnl).cumprod()
        sharpes["stat_arb"] = annualized_sharpe(pnl)
        manifest["strategies"]["stat_arb"] = {"paper_sharpe": sharpes["stat_arb"]}
        logger.info(f"stat_arb (QR4.5): paper Sharpe {sharpes['stat_arb']:.2f}")
    else:
        logger.warning(f"{args.statarb_weights} not found; run signals.py for the 3-way compare")

    (args.out_dir / "baseline_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    if equities:
        plot_comparison(equities, sharpes, args.plot)
    logger.info(
        "Baselines done (paper Sharpes, COST-FREE): "
        + ", ".join(f"{k} {v:.2f}" for k, v in sharpes.items())
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
