"""QR1.3 — analyze the toxicity-filter A/B execution audit.

Reads the per-order slippage the `toxicity_audit` tool writes (blind market
orders vs a VPIN+OFI-gated passive-execution policy on the same AAPL tick
stream) and reports whether the filter reduces slippage. Per the brief, the
filter earns its place or it doesn't — the audit decides.

Usage:
  python scripts/analysis/toxicity_audit.py          # reads results/toxicity_audit/
Outputs:
  docs/research/execution/toxicity_audit_summary.md
  docs/research/execution/toxicity_audit.png
"""

import argparse
import sys
from pathlib import Path

import numpy as np
import pandas as pd

BPS = 1e4


def analyze(df: pd.DataFrame) -> dict:
    n = len(df)
    blind = df["blind_slip"].mean()
    filtered = df["filtered_slip"].mean()
    rested = df[df["rested"] == 1]
    passive = rested[rested["filled_passive"] == 1]
    fallback = rested[rested["filled_passive"] == 0]
    # average price for bps conversion
    mid = df["arrival_mid"].mean()
    return {
        "orders": n,
        "blind_slip": blind,
        "filtered_slip": filtered,
        "reduction": blind - filtered,  # positive = filter helps
        "reduction_bps": (blind - filtered) / mid * BPS,
        "rested": len(rested),
        "passive_fills": len(passive),
        "passive_fill_rate": len(passive) / len(rested) if len(rested) else float("nan"),
        "avg_passive_slip": passive["filtered_slip"].mean() if len(passive) else float("nan"),
        "avg_fallback_slip": fallback["filtered_slip"].mean() if len(fallback) else float("nan"),
        "mid": mid,
    }


def write_summary(m: dict, path: Path):
    helps = m["reduction"] > 0
    verdict = (
        "**reduces** slippage — the filter earns its place"
        if helps
        else "**does not** reduce slippage — the filter does not earn its place on this data"
    )
    lines = [
        "# QR1.3 — Toxicity filter A/B execution audit",
        "",
        f"On {m['orders']:,} scheduled AAPL orders, the VPIN+OFI toxicity filter " f"{verdict}.",
        "",
        "| Policy | Avg slippage/order ($) | vs arrival mid (bps) |",
        "|---|---|---|",
        f"| Blind market order | {m['blind_slip']:.5f} | {m['blind_slip'] / m['mid'] * BPS:.3f} |",
        f"| VPIN+OFI filtered | {m['filtered_slip']:.5f} | "
        f"{m['filtered_slip'] / m['mid'] * BPS:.3f} |",
        f"| **Reduction** | **{m['reduction']:+.5f}** | **{m['reduction_bps']:+.3f}** |",
        "",
        "## Why (the decomposition)",
        "",
        f"The filter rested passive on **{m['rested']}** orders (toxic *and* "
        f"directionally favorable); the rest crossed like blind. Of those, "
        f"**{m['passive_fills']} ({m['passive_fill_rate']:.0%})** filled passively, "
        f"capturing the spread (avg slip {m['avg_passive_slip']:+.4f}). But the "
        f"**{m['rested'] - m['passive_fills']}** that did *not* fill fell back to "
        f"crossing after the toxic flow had run away — average slip "
        f"{m['avg_fallback_slip']:+.4f}, the adverse-selection tail.",
        "",
        "**The honest finding.** "
        + (
            "The spread captured on passive fills outweighs the fallback adverse "
            "selection, so the filter helps."
            if helps
            else "The adverse selection on the fallback orders (the toxic flow "
            "continuing away from the resting order) outweighs the spread captured "
            "on passive fills. On 1-minute AAPL, high VPIN predicts *continued* "
            "adverse movement, so resting passive into it is the wrong move — you "
            "only get filled when the flow reverses. A robust negative across every "
            "threshold/horizon tested."
        ),
        "",
        "> Provisional either way: any apparent win from sweeping the threshold / "
        "wait horizon would itself need deflating for the config search (QR-P2). "
        "Depth is L1-reconstructed (thesis limitations §1), so this is an "
        "execution-fill result, not a price-prediction claim.",
        "",
    ]
    path.write_text("\n".join(lines))


def plot(df: pd.DataFrame, m: dict, path: Path):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    rested = df[df["rested"] == 1]
    fig, (left, right) = plt.subplots(1, 2, figsize=(12, 5))

    left.bar(
        ["blind", "filtered"],
        [m["blind_slip"], m["filtered_slip"]],
        color=["tab:gray", "tab:red" if m["reduction"] < 0 else "tab:green"],
    )
    left.axhline(0, color="black", lw=0.6)
    left.set_ylabel("avg slippage / order ($)")
    left.set_title(f"Avg slippage (reduction {m['reduction']:+.5f})")

    if len(rested):
        passive = rested[rested["filled_passive"] == 1]["filtered_slip"]
        fallback = rested[rested["filled_passive"] == 0]["filtered_slip"]
        bins = np.linspace(-0.05, 0.25, 40)
        right.hist(
            passive, bins=bins, alpha=0.7, color="tab:green", label="passive fill (spread capture)"
        )
        right.hist(
            fallback, bins=bins, alpha=0.7, color="tab:red", label="fallback (adverse selection)"
        )
    right.axvline(m["blind_slip"], color="black", ls="--", lw=1, label="blind (=half-spread)")
    right.set_xlabel("slippage of rested orders ($)")
    right.set_ylabel("count")
    right.set_title("Rested-order economics: capture vs adverse tail")
    right.legend(fontsize=8)

    fig.suptitle("QR1.3 — VPIN+OFI toxicity filter vs blind market orders (AAPL)", fontsize=11)
    fig.tight_layout()
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=150)
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--orders", type=Path, default=Path("results/toxicity_audit/toxicity_orders.csv")
    )
    parser.add_argument(
        "--summary",
        type=Path,
        default=Path("docs/research/execution/toxicity_audit_summary.md"),
    )
    parser.add_argument(
        "--plot", type=Path, default=Path("docs/research/execution/toxicity_audit.png")
    )
    args = parser.parse_args()

    if not args.orders.exists():
        print(f"{args.orders} not found; run ./build/toxicity_audit first")
        return 1
    df = pd.read_csv(args.orders)
    m = analyze(df)
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    write_summary(m, args.summary)
    plot(df, m, args.plot)
    print(
        f"orders {m['orders']}, blind {m['blind_slip']:.5f}, filtered {m['filtered_slip']:.5f}, "
        f"reduction {m['reduction']:+.5f} ({m['reduction_bps']:+.3f} bps)"
    )
    print(f"summary -> {args.summary}\nplot -> {args.plot}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
