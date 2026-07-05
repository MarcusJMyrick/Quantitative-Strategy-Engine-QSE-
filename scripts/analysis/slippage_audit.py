#!/usr/bin/env python3
"""A/B slippage audit (roadmap H1).

Compares the exact same SMA signal stream executed by two engine
configurations (produced by the `ab_audit` C++ driver):

    Engine A (naive): fills at the tick mid - no spread, no impact, no queue.
    Engine B (depth): full-depth order book - fills pay the touch and walk
                      the seeded depth profile, receiving the true VWAP.

The gap between the paired equity curves is the "phantom profit" - PnL the
naive backtester hallucinates by assuming infinite liquidity.

Outputs (docs/research/microstructure/):
    slippage_audit.png   equity overlays per size regime + phantom bar chart
    slippage_audit.pdf   the same figures plus a summary table page
    ab_audit_summary.md  headline number, per-regime table, methodology

Usage (from the repo root):
    python scripts/analysis/slippage_audit.py [--rerun-audit]
"""

import argparse
import datetime
import subprocess
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd
from matplotlib.backends.backend_pdf import PdfPages

sys.path.insert(0, str(Path(__file__).resolve().parent))
import tearsheet  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[2]
AUDIT_TOOL = REPO_ROOT / "build" / "ab_audit"
AUDIT_DIR = REPO_ROOT / "results" / "ab_audit"
OUT_DIR = REPO_ROOT / "docs" / "research" / "microstructure"
SIZES = [1000, 5000, 25000]


def run_audit() -> None:
    if not AUDIT_TOOL.exists():
        print("Building ab_audit tool...")
        subprocess.run(
            ["cmake", "--build", "build", "--target", "ab_audit", "-j4"],
            cwd=REPO_ROOT,
            check=True,
        )
    print("Running A/B audit...")
    subprocess.run([str(AUDIT_TOOL)], cwd=REPO_ROOT, check=True)


def vwap(trades: pd.DataFrame, side: str) -> float:
    rows = trades[trades["type"] == side]
    qty = rows["quantity"].abs()
    if qty.sum() == 0:
        return float("nan")
    return float((qty * rows["price"]).sum() / qty.sum())


def analyze_regime(size: int) -> dict:
    naive_eq = tearsheet.load_equity(str(AUDIT_DIR / f"naive_{size}_equity.csv"))
    depth_eq = tearsheet.load_equity(str(AUDIT_DIR / f"depth_{size}_equity.csv"))
    naive_tl = tearsheet.load_tradelog(str(AUDIT_DIR / f"naive_{size}_tradelog.csv"))
    depth_tl = tearsheet.load_tradelog(str(AUDIT_DIR / f"depth_{size}_tradelog.csv"))

    start = naive_eq.iloc[0]
    naive_pnl = float(naive_eq.iloc[-1] - start)
    depth_pnl = float(depth_eq.iloc[-1] - start)
    phantom = naive_pnl - depth_pnl
    shares_traded = float(naive_tl["quantity"].abs().sum())

    naive_sharpe = tearsheet.annualized_sharpe(
        tearsheet.daily_returns(tearsheet.to_daily(naive_eq)))
    depth_sharpe = tearsheet.annualized_sharpe(
        tearsheet.daily_returns(tearsheet.to_daily(depth_eq)))

    return {
        "size": size,
        "naive_eq": naive_eq,
        "depth_eq": depth_eq,
        "naive_pnl": naive_pnl,
        "depth_pnl": depth_pnl,
        "phantom": phantom,
        "phantom_per_share": phantom / shares_traded if shares_traded else float("nan"),
        "naive_sharpe": naive_sharpe,
        "depth_sharpe": depth_sharpe,
        "buy_gap": vwap(depth_tl, "BUY") - vwap(naive_tl, "BUY"),
        "sell_gap": vwap(naive_tl, "SELL") - vwap(depth_tl, "SELL"),
        "n_fills_naive": len(naive_tl),
        "n_fills_depth": len(depth_tl),
    }


def make_overlay_figure(regimes: list[dict]):
    fig, axes = plt.subplots(len(regimes) + 1, 1, figsize=(8.5, 12))
    for ax, r in zip(axes, regimes):
        ax.plot(r["naive_eq"].index, r["naive_eq"].values,
                color="tab:gray", label="Engine A - naive (mid fills)")
        ax.plot(r["depth_eq"].index, r["depth_eq"].values,
                color="tab:blue", label="Engine B - full-depth book")
        ax.fill_between(r["naive_eq"].index, r["naive_eq"].values,
                        r["depth_eq"].reindex(r["naive_eq"].index).values,
                        color="tab:red", alpha=0.15)
        ax.set_title(
            f"{r['size']:,} shares/signal - phantom profit ${r['phantom']:,.0f}")
        ax.legend(fontsize=8)
        ax.grid(alpha=0.3)

    bar_ax = axes[-1]
    sizes = [f"{r['size']:,}" for r in regimes]
    bar_ax.bar(sizes, [r["phantom"] for r in regimes], color="tab:red", alpha=0.7)
    bar_ax.set_title("Phantom profit vs order size (superlinear in size)")
    bar_ax.set_ylabel("$")
    bar_ax.grid(alpha=0.3, axis="y")
    fig.suptitle("A/B Slippage Audit - same signals, two fill models", fontsize=13)
    fig.tight_layout(rect=(0, 0, 1, 0.97))
    return fig


def make_table_figure(regimes: list[dict]):
    fig, ax = plt.subplots(figsize=(8.5, 11))
    ax.axis("off")
    columns = ["Size", "Naive PnL", "Real PnL", "Phantom $", "Phantom $/sh",
               "Naive Sharpe", "Real Sharpe", "Buy gap $/sh", "Sell gap $/sh"]
    rows = [[
        f"{r['size']:,}",
        f"${r['naive_pnl']:,.0f}",
        f"${r['depth_pnl']:,.0f}",
        f"${r['phantom']:,.0f}",
        f"{r['phantom_per_share']:.4f}",
        f"{r['naive_sharpe']:.2f}",
        f"{r['depth_sharpe']:.2f}",
        f"{r['buy_gap']:.4f}",
        f"{r['sell_gap']:.4f}",
    ] for r in regimes]
    table = ax.table(cellText=rows, colLabels=columns, loc="center", cellLoc="center")
    table.auto_set_font_size(False)
    table.set_fontsize(8)
    table.scale(1.0, 1.6)
    ax.set_title("A/B audit summary", pad=30)
    return fig


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rerun-audit", action="store_true",
                        help="Re-run the C++ audit even if outputs exist")
    args = parser.parse_args()

    expected = [AUDIT_DIR / f"{m}_{s}_equity.csv"
                for s in SIZES for m in ("naive", "depth")]
    if args.rerun_audit or not all(p.exists() for p in expected):
        run_audit()

    regimes = [analyze_regime(size) for size in SIZES]
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    overlay_fig = make_overlay_figure(regimes)
    png_path = OUT_DIR / "slippage_audit.png"
    overlay_fig.savefig(png_path, dpi=150)
    print(f"Wrote {png_path}")

    pdf_path = OUT_DIR / "slippage_audit.pdf"
    with PdfPages(pdf_path) as pdf:
        pdf.savefig(overlay_fig)
        table_fig = make_table_figure(regimes)
        pdf.savefig(table_fig)
        plt.close(table_fig)
    plt.close(overlay_fig)
    print(f"Wrote {pdf_path}")

    worst = max(regimes, key=lambda r: r["phantom"])
    today = datetime.date.today().isoformat()
    lines = [
        "# A/B Slippage Audit",
        "",
        f"*Generated {today} by `scripts/analysis/slippage_audit.py`*",
        "",
        "## Headline",
        "",
        f"At **{worst['size']:,} shares per signal**, the naive backtester reports",
        f"**${worst['naive_pnl']:,.0f}** PnL while the full-depth engine reports",
        f"**${worst['depth_pnl']:,.0f}** — **${worst['phantom']:,.0f} of phantom",
        "profit** that exists only under the infinite-liquidity assumption",
        f"(naive Sharpe {worst['naive_sharpe']:.2f} vs real {worst['depth_sharpe']:.2f}).",
        "",
        "## Per-regime results",
        "",
        "| Size/signal | Naive PnL | Real PnL | Phantom $ | Phantom $/share | Naive Sharpe | Real Sharpe |",
        "|---|---|---|---|---|---|---|",
    ]
    for r in regimes:
        lines.append(
            f"| {r['size']:,} | ${r['naive_pnl']:,.0f} | ${r['depth_pnl']:,.0f} "
            f"| ${r['phantom']:,.0f} | {r['phantom_per_share']:.4f} "
            f"| {r['naive_sharpe']:.2f} | {r['depth_sharpe']:.2f} |")
    lines += [
        "",
        "## Method",
        "",
        "- Identical SMA 20/50 crossover signals (455 per run) on the AAPL",
        "  minute-tick file, long/flat, executed as market orders",
        "- **Engine A (naive):** fills at the tick mid; no spread, no impact,",
        "  no queue — the standard tutorial-backtester assumption",
        "- **Engine B (institutional):** full-depth book with a one-tick",
        "  half-spread and a 12-level uniform depth profile behind the touch",
        "  (each level as thick as displayed volume, the profile validated in",
        "  the [impact study](results_summary.md)); market orders walk levels",
        "  and pay the consumed-liquidity VWAP",
        "- Both engines share signals, data, cash, and mark-to-market — the",
        "  only difference is the fill model, so the entire PnL gap is",
        "  execution cost",
        "- Fully deterministic: no randomness anywhere, reproducible",
        "  run-to-run",
        "",
        "## Interpretation",
        "",
        "Per-share phantom cost grows with order size (spread cost plus",
        "book-walk impact), so the distortion is superlinear in size: the",
        "regime where the naive backtester is most confidently wrong is",
        "exactly the size a profitable-looking strategy would scale into.",
        "",
        "![Slippage audit](slippage_audit.png)",
    ]
    md_path = OUT_DIR / "ab_audit_summary.md"
    md_path.write_text("\n".join(lines) + "\n")
    print(f"Wrote {md_path}")

    for r in regimes:
        print(f"size {r['size']:>6,}: phantom ${r['phantom']:>12,.0f} "
              f"({r['phantom_per_share']:.4f} $/sh), "
              f"Sharpe {r['naive_sharpe']:.2f} -> {r['depth_sharpe']:.2f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
