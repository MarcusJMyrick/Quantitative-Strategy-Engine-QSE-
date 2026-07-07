"""QR4.7 — analyze the multi-symbol A/B stat-arb audit (Engine A vs Engine B).

Reads the paired equity curves written by the `statarb_audit` C++ tool and
reports, per strategy and order-size regime, the net Sharpe under Engine B (the
full-depth book) alongside the naive Sharpe, plus the phantom profit the naive
fills hallucinate. This is the QR-P1 capstone: the first time the cost-free
QR4.6 floor meets realistic fills.

The reported Sharpe is PROVISIONAL — a candidate, not a result — until QR2.5
deflates it for the parameter search (bands, windows, factor count, depth
assumptions) and the tearsheet carries the DSR and trial count.

Usage:
  python scripts/analysis/statarb_audit.py            # reads results/statarb_audit/
Outputs:
  docs/research/statarb/statarb_ab_summary.md
  docs/research/statarb/statarb_ab_audit.png
"""

import argparse
import sys
from pathlib import Path

import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parent))

from tearsheet import annualized_sharpe, daily_returns  # noqa: E402

STRATEGIES = ["stat_arb", "momentum", "reversal"]


def load_equity(path: Path) -> pd.Series:
    """Load a `timestamp_ms,equity` curve indexed by date."""
    df = pd.read_csv(path)
    df["date"] = pd.to_datetime(df["timestamp"], unit="ms")
    return df.set_index("date")["equity"]


def audit_one(run_dir: Path, size: int) -> dict:
    """Metrics for one (strategy, size): naive vs depth Sharpe + phantom."""
    naive = load_equity(run_dir / f"naive_{size}_equity.csv")
    depth = load_equity(run_dir / f"depth_{size}_equity.csv")
    naive_pnl = float(naive.iloc[-1] - naive.iloc[0])
    depth_pnl = float(depth.iloc[-1] - depth.iloc[0])
    phantom = naive_pnl - depth_pnl
    return {
        "size": size,
        "naive_pnl": naive_pnl,
        "depth_pnl": depth_pnl,
        "phantom": phantom,
        "phantom_pct": 100.0 * phantom / abs(naive_pnl) if naive_pnl != 0 else float("nan"),
        "naive_sharpe": annualized_sharpe(daily_returns(naive)),
        "depth_sharpe": annualized_sharpe(daily_returns(depth)),
    }


def collect(audit_dir: Path, sizes: list[int]) -> pd.DataFrame:
    rows = []
    for strat in STRATEGIES:
        run_dir = audit_dir / strat
        if not run_dir.exists():
            continue
        for size in sizes:
            if not (run_dir / f"depth_{size}_equity.csv").exists():
                continue
            row = audit_one(run_dir, size)
            row["strategy"] = strat
            rows.append(row)
    return pd.DataFrame(rows)


def write_summary(df: pd.DataFrame, path: Path):
    lines = [
        "# QR4.7 — Stat arb vs baselines under Engine B (A/B audit)",
        "",
        "Net Sharpe under the full-depth book (**depth**) vs the naive "
        "infinite-liquidity fills (**naive**), per order-size regime. "
        "Phantom profit is the dollar PnL the naive fills hallucinate "
        "(naive − depth).",
        "",
        "> **Provisional.** These Sharpes are candidates, not results, until "
        "QR2.5 deflates them for the configurations tried (QR-P2). Engine B's "
        "daily depth is synthesized from IEX-partial volume (a stated "
        "approximation); the A-vs-B gap growing with size is the robust "
        "finding.",
        "",
    ]
    for strat in STRATEGIES:
        sub = df[df["strategy"] == strat]
        if sub.empty:
            continue
        lines += [
            f"## {strat}",
            "",
            "| Size | Naive PnL | Depth PnL | Phantom $ | Phantom % | Naive Sharpe | **Depth Sharpe** |",
            "|---|---|---|---|---|---|---|",
        ]
        for _, r in sub.iterrows():
            lines.append(
                f"| {int(r['size'])}× | {r['naive_pnl']:,.0f} | {r['depth_pnl']:,.0f} | "
                f"{r['phantom']:,.0f} | {r['phantom_pct']:.1f}% | {r['naive_sharpe']:.2f} | "
                f"**{r['depth_sharpe']:.2f}** |"
            )
        lines.append("")

    # Headline at the most impactful regime
    big = df[df["size"] == df["size"].max()].set_index("strategy")
    lines += ["## Headline (largest size regime)", ""]
    order = [s for s in STRATEGIES if s in big.index]
    ranked = ", ".join(f"{s} {big.loc[s, 'depth_sharpe']:.2f}" for s in order)
    lines.append(
        f"Under Engine B at {int(df['size'].max())}× size, net Sharpe: {ranked}. "
        "The elaborate stat arb does not clearly beat cheap 12-1 momentum once "
        "realistic fills are charged — and both clear the reversal floor. "
        "Whether *anything* survives is settled only after QR-P2 deflation."
    )
    lines.append("")
    path.write_text("\n".join(lines))


def plot_audit(df: pd.DataFrame, path: Path):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    sizes = sorted(df["size"].unique())
    strategies = [s for s in STRATEGIES if s in set(df["strategy"])]
    x = np.arange(len(sizes))
    width = 0.8 / max(1, len(strategies))
    colors = {"stat_arb": "tab:green", "momentum": "tab:orange", "reversal": "tab:blue"}

    fig, (left, right) = plt.subplots(1, 2, figsize=(12, 5))
    for i, strat in enumerate(strategies):
        sub = df[df["strategy"] == strat].set_index("size").reindex(sizes)
        offset = (i - (len(strategies) - 1) / 2) * width
        left.bar(x + offset, sub["depth_sharpe"], width, label=strat, color=colors.get(strat))
        right.plot(sizes, sub["phantom_pct"], "o-", label=strat, color=colors.get(strat))

    left.axhline(0, color="gray", lw=0.8)
    left.set_xticks(x)
    left.set_xticklabels([f"{s}×" for s in sizes])
    left.set_ylabel("net Sharpe under Engine B (depth)")
    left.set_title("Net-of-cost Sharpe by size")
    left.legend(fontsize=8)

    right.set_xscale("log")
    right.set_xticks(sizes)
    right.set_xticklabels([f"{s}×" for s in sizes])
    right.set_ylabel("phantom profit (% of naive PnL)")
    right.set_xlabel("order-size regime")
    right.set_title("Phantom profit grows with size")
    right.legend(fontsize=8)

    fig.suptitle(
        "QR4.7 — Engine B reality check (PROVISIONAL: DSR deflation still to come)", fontsize=11
    )
    fig.tight_layout()
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=150)
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--audit-dir", type=Path, default=Path("results/statarb_audit"))
    parser.add_argument("--sizes", default="1,10,50")
    parser.add_argument(
        "--summary", type=Path, default=Path("docs/research/statarb/statarb_ab_summary.md")
    )
    parser.add_argument(
        "--plot", type=Path, default=Path("docs/research/statarb/statarb_ab_audit.png")
    )
    args = parser.parse_args()

    sizes = [int(s) for s in args.sizes.split(",")]
    df = collect(args.audit_dir, sizes)
    if df.empty:
        print(f"No audit results found under {args.audit_dir}; run ./build/statarb_audit first")
        return 1

    args.summary.parent.mkdir(parents=True, exist_ok=True)
    write_summary(df, args.summary)
    plot_audit(df, args.plot)
    print(df.to_string(index=False))
    print(f"\nSummary -> {args.summary}\nPlot -> {args.plot}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
