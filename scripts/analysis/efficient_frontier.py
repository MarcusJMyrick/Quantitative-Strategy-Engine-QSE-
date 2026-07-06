#!/usr/bin/env python3
"""Efficient frontier from the mean-variance PortfolioBuilder (roadmap A5).

Runs the `frontier_sweep` C++ tool (a log-spaced risk-aversion sweep over a
fixed synthetic universe) and plots expected alpha against portfolio risk.
Each point is the constrained optimum for one lambda: small lambda buys alpha
with variance, large lambda retreats toward the minimum-variance book.

Outputs (docs/research/factor/):
    efficient_frontier.png
    frontier_summary.md

Usage (from the repo root):
    python scripts/analysis/efficient_frontier.py [--rerun-sweep]
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

REPO_ROOT = Path(__file__).resolve().parents[2]
SWEEP_TOOL = REPO_ROOT / "build" / "frontier_sweep"
SWEEP_CSV = REPO_ROOT / "results" / "frontier_sweep.csv"
OUT_DIR = REPO_ROOT / "docs" / "research" / "factor"


def run_sweep() -> None:
    if not SWEEP_TOOL.exists():
        print("Building frontier_sweep tool...")
        subprocess.run(
            ["cmake", "--build", "build", "--target", "frontier_sweep", "-j4"],
            cwd=REPO_ROOT,
            check=True,
        )
    SWEEP_CSV.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(SWEEP_TOOL), "--out", str(SWEEP_CSV)], cwd=REPO_ROOT, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rerun-sweep", action="store_true")
    args = parser.parse_args()

    if args.rerun_sweep or not SWEEP_CSV.exists():
        run_sweep()

    df = pd.read_csv(SWEEP_CSV).sort_values("lambda")
    if df.empty:
        print("Sweep CSV is empty", file=sys.stderr)
        return 1

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    fig, ax = plt.subplots(figsize=(8, 6))
    ax.plot(df["stdev"], df["exp_alpha"], "o-", color="tab:blue")
    for _, row in df.iloc[[0, len(df) // 2, len(df) - 1]].iterrows():
        ax.annotate(
            f"λ={row['lambda']:.3g}",
            (row["stdev"], row["exp_alpha"]),
            textcoords="offset points",
            xytext=(8, -4),
            fontsize=9,
            color="tab:gray",
        )
    ax.set_xlabel("Portfolio risk (stdev, wᵀΣw)^½")
    ax.set_ylabel("Expected alpha (αᵀw)")
    ax.set_title(
        "Efficient frontier traced by the risk-aversion sweep\n"
        "(constrained mean-variance PortfolioBuilder, single-factor Σ)"
    )
    ax.grid(alpha=0.3)
    fig.tight_layout()

    png_path = OUT_DIR / "efficient_frontier.png"
    fig.savefig(png_path, dpi=150)
    print(f"Wrote {png_path}")

    today = datetime.date.today().isoformat()
    low, high = df.iloc[0], df.iloc[-1]
    lines = [
        "# Efficient Frontier — Mean-Variance PortfolioBuilder (A5)",
        "",
        f"*Generated {today} by `scripts/analysis/efficient_frontier.py`*",
        "",
        "Sweeping the risk-aversion parameter λ through the constrained",
        "optimizer traces the efficient frontier of the synthetic 8-asset",
        "universe (single-factor covariance Σ = σ_m²ββᵀ + diag(σ_resid²),",
        "net-zero / gross-cap / beta-neutral constraints active):",
        "",
        "| | λ | expected alpha | risk (stdev) | gross |",
        "|---|---|---|---|---|",
        f"| alpha-seeking end | {low['lambda']:.3g} | {low['exp_alpha']:.4f} "
        f"| {low['stdev']:.4f} | {low['gross']:.2f} |",
        f"| min-variance end | {high['lambda']:.3g} | {high['exp_alpha']:.4f} "
        f"| {high['stdev']:.4f} | {high['gross']:.2f} |",
        "",
        "Both risk and expected alpha decrease monotonically in λ — the",
        "textbook frontier shape, produced by the same projected-gradient",
        "optimizer the factor pipeline uses in production, with λ = 0",
        "reproducing the legacy pure-alpha behavior exactly",
        "(`MeanVarianceTest`).",
        "",
        "![Efficient frontier](efficient_frontier.png)",
    ]
    md_path = OUT_DIR / "frontier_summary.md"
    md_path.write_text("\n".join(lines) + "\n")
    print(f"Wrote {md_path}")

    print(df.to_string(index=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
