#!/usr/bin/env python3
"""Empirical market impact study (roadmap A4).

Sweeps market-order sizes through the C++ full-depth order book (via the
`impact_sweep` tool) against synthetic depth reconstructed behind real AAPL
tick prices, then fits the impact power law

    slippage_bps = a * Q^b

per depth profile and compares the fitted exponent b with the square-root
impact law (b = 0.5) widely reported in the empirical literature.

Outputs (docs/research/microstructure/):
    impact_curve.png    log-log impact curves with fitted exponents
    results_summary.md  fitted exponents, R^2, and methodology notes

Usage (from the repo root):
    python scripts/analysis/impact_study.py [--rerun-sweep]
"""

import argparse
import datetime
import subprocess
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

REPO_ROOT = Path(__file__).resolve().parents[2]
SWEEP_TOOL = REPO_ROOT / "build" / "impact_sweep"
SWEEP_CSV = REPO_ROOT / "results" / "impact_sweep.csv"
OUT_DIR = REPO_ROOT / "docs" / "research" / "microstructure"

# Theoretical exponents implied by each cumulative-depth profile
THEORY = {"uniform": 1.0, "linear": 0.5}

# Orders filling inside the touch level have zero slippage up to floating-point
# dust (~1e-12 bps); anything below this threshold carries no walk information
MIN_SLIP_BPS = 1e-6

# The power law describes the continuum limit; below this size the walk only
# reaches a handful of discrete levels and the local slope is biased upward,
# so the exponent is fitted on the asymptotic tail
TAIL_MIN_SIZE = 1600


def run_sweep() -> None:
    """Build (if needed) and run the C++ sweep tool."""
    if not SWEEP_TOOL.exists():
        print("Building impact_sweep tool...")
        subprocess.run(
            ["cmake", "--build", "build", "--target", "impact_sweep", "-j4"],
            cwd=REPO_ROOT,
            check=True,
        )
    SWEEP_CSV.parent.mkdir(parents=True, exist_ok=True)
    print("Running impact sweep...")
    subprocess.run(
        [str(SWEEP_TOOL), "--out", str(SWEEP_CSV)],
        cwd=REPO_ROOT,
        check=True,
    )


def fit_power_law(sizes: np.ndarray, slips: np.ndarray) -> tuple[float, float, float]:
    """OLS fit of ln(slip) = ln(a) + b*ln(Q). Returns (a, b, r_squared)."""
    x = np.log(sizes)
    y = np.log(slips)
    b, ln_a = np.polyfit(x, y, 1)
    y_hat = ln_a + b * x
    ss_res = float(np.sum((y - y_hat) ** 2))
    ss_tot = float(np.sum((y - y.mean()) ** 2))
    r2 = 1.0 - ss_res / ss_tot if ss_tot > 0 else float("nan")
    return float(np.exp(ln_a)), float(b), r2


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--rerun-sweep",
        action="store_true",
        help="Re-run the C++ sweep even if the sweep CSV already exists",
    )
    args = parser.parse_args()

    if args.rerun_sweep or not SWEEP_CSV.exists():
        run_sweep()

    df = pd.read_csv(SWEEP_CSV)
    if df.empty:
        print("Sweep CSV is empty", file=sys.stderr)
        return 1

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    # Mean slippage past the touch per (profile, size); zero-slippage points
    # (orders inside the first level) carry no information about the walk
    curves = (
        df.groupby(["profile", "size"], as_index=False)["slip_bps_vs_touch"]
        .mean()
        .rename(columns={"slip_bps_vs_touch": "slip_bps"})
    )

    fits = {}
    fig, ax = plt.subplots(figsize=(8, 6))
    colors = {"uniform": "tab:blue", "linear": "tab:orange"}

    for profile, group in curves.groupby("profile"):
        positive = group[group["slip_bps"] > MIN_SLIP_BPS]
        tail = positive[positive["size"] >= TAIL_MIN_SIZE]
        a, b, r2 = fit_power_law(
            tail["size"].to_numpy(dtype=float),
            tail["slip_bps"].to_numpy(dtype=float),
        )
        fits[profile] = {"a": a, "b": b, "r2": r2, "n": len(tail)}

        color = colors.get(profile, None)
        ax.plot(
            group["size"],
            group["slip_bps"].where(group["slip_bps"] > MIN_SLIP_BPS),
            "o",
            color=color,
            label=f"{profile} depth (fit b={b:.3f}, theory {THEORY[profile]:.1f})",
        )
        grid = np.geomspace(positive["size"].min(), positive["size"].max(), 100)
        ax.plot(grid, a * grid ** b, "-", color=color, alpha=0.7)

    # Square-root law reference, anchored to the linear-profile fit level
    if "linear" in fits:
        anchor = fits["linear"]
        grid = np.geomspace(curves["size"].min(), curves["size"].max(), 100)
        ref = anchor["a"] * grid ** 0.5
        ax.plot(grid, ref, "--", color="gray", label="square-root law (b=0.5)")

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlabel("Order size Q (shares)")
    ax.set_ylabel("Mean slippage past the touch (bps)")
    ax.set_title("Realized market impact vs order size\n(full-depth book walk, AAPL tick replay)")
    ax.legend()
    ax.grid(True, which="both", alpha=0.3)
    fig.tight_layout()

    curve_path = OUT_DIR / "impact_curve.png"
    fig.savefig(curve_path, dpi=150)
    print(f"Wrote {curve_path}")

    today = datetime.date.today().isoformat()
    n_samples = df["sample"].nunique()
    lines = [
        "# Market Impact Study",
        "",
        f"*Generated {today} by `scripts/analysis/impact_study.py`*",
        "",
        "## Setup",
        "",
        f"- {n_samples} price samples drawn evenly from `data/raw_ticks_AAPL.csv`",
        "- Synthetic quote (one-tick half-spread) with 800 depth levels behind the touch",
        "- Market buy orders swept from 50 to 51,200 shares through",
        "  `OrderBookFullDepth::fill_market`; slippage measured as the VWAP's",
        "  distance past the touch, in bps of the mid price",
        f"- Power law `slippage = a * Q^b` fitted by OLS on log-log means over the",
        f"  asymptotic tail (Q >= {TAIL_MIN_SIZE:,}); smaller orders reach only a",
        "  few discrete levels, which biases the local slope upward",
        "",
        "## Fitted impact exponents",
        "",
        "| Depth profile | Fitted b | Theory | R² | Points |",
        "|---|---|---|---|---|",
    ]
    for profile in sorted(fits):
        f = fits[profile]
        lines.append(
            f"| {profile} | {f['b']:.3f} | {THEORY[profile]:.1f} | {f['r2']:.4f} | {f['n']} |"
        )
    lines += [
        "",
        "## Interpretation",
        "",
        "The impact exponent is a property of how liquidity is distributed",
        "through the book, and the walked-book simulation recovers the",
        "theoretical value for each profile:",
        "",
        "- **uniform** depth (equal size at every level) makes cumulative depth",
        "  grow linearly with distance, so cost grows linearly with order size",
        "  (b ≈ 1) — the assumption implicitly made by the legacy linear",
        "  slippage coefficient.",
        "- **linear** depth (size growing with distance from the touch) makes",
        "  cumulative depth grow quadratically, which yields the square-root",
        "  law (b ≈ 0.5) that empirical studies consistently report for real",
        "  markets.",
        "",
        "This motivates the `fill_model: full_depth` backtest mode: a flat or",
        "linear cost model mis-prices exactly the large orders where impact",
        "matters most, while the book walk prices them by construction.",
        "",
        "![Impact curves](impact_curve.png)",
    ]
    summary_path = OUT_DIR / "results_summary.md"
    summary_path.write_text("\n".join(lines) + "\n")
    print(f"Wrote {summary_path}")

    for profile, f in fits.items():
        print(f"{profile}: b={f['b']:.3f} (theory {THEORY[profile]}), R²={f['r2']:.4f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
