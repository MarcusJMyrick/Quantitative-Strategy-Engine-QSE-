"""QR2.5 — Wire QR4 through CPCV + Deflated Sharpe (the QR-P2 capstone).

The bands and window of QR4.5 are exactly the overfitting-prone knobs the whole
truth-serum exists to protect. This sweeps them, logs every configuration and
its return series to the trial registry (QR2.3), and reports the **Deflated
Sharpe** of the best config — its Sharpe corrected for how many configurations
were tried (QR2.4). It also runs the chosen config's returns through CPCV
(QR2.2) to report the out-of-sample Sharpe spread across time blocks.

Honest scope. The per-config return series here is the QR4.6 *cost-free* paper
PnL (cheap to sweep). Engine B fills are a further, separately-quantified
haircut (QR4.7: the stat arb's ~0.97 paper Sharpe becomes ~0.69 net). So the
true "survives?" answer is the DSR below, minus the Engine B cost — this chunk
adds the *search* correction on top of the *cost* correction already measured.

QR4 is a rolling, re-fit-free signal (each day's s-score uses only trailing
data), so CPCV's per-split model re-fitting does not apply — the active
correction is the **trial-count deflation** (V[SR] across the sweep). CPCV is
used here for the temporal-stability read (per-block out-of-sample Sharpe).

Output: docs/research/statarb/qr4_dsr_summary.md  (the DSR line + trial count)
        docs/research/statarb/qr4_dsr.png
        data/universe/qr4_trials/  (registry, gitignored)
"""

import argparse
import sys
from pathlib import Path

import numpy as np
import pandas as pd

_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE))
sys.path.insert(0, str(_HERE.parent / "validation"))

from baselines import paper_pnl  # noqa: E402
from cpcv import cpcv_splits  # noqa: E402
from deflated_sharpe import deflate_registry, sharpe_ratio  # noqa: E402
from signals import Bands, generate  # noqa: E402
from trial_registry import TrialRegistry, run_sweep  # noqa: E402

TRADING_DAYS = 252.0

# The QR-P2-protected knobs: entry/exit bands x estimation window. This IS the
# search the DSR deflates against.
BAND_SETS = [
    (-1.25, 1.25, -0.50, 0.75),  # Avellaneda-Lee default
    (-1.00, 1.00, -0.25, 0.50),
    (-1.50, 1.50, -0.75, 1.00),
    (-2.00, 2.00, -0.50, 0.50),
]
WINDOWS = [40, 60, 80]


def build_grid() -> list[dict]:
    return [{"bands": list(b), "window": w} for b in BAND_SETS for w in WINDOWS]


def run_config(returns: pd.DataFrame, params: dict) -> pd.Series:
    """One QR4 configuration -> its cost-free daily return series."""
    out = generate(returns, window=params["window"], bands=Bands(*params["bands"]))
    return paper_pnl(out["weights"], returns, execution_lag=1)


def block_sharpes(series: pd.Series, n_groups: int = 6, k: int = 2) -> list[float]:
    """Per-period Sharpe on each CPCV group (temporal out-of-sample stability)."""
    r = series.to_numpy()
    _, groups = cpcv_splits(len(r), n_groups, k, embargo_pct=0.01, holding=1)
    return [sharpe_ratio(r[g]) for g in groups]


def deflate(returns: pd.DataFrame, registry: TrialRegistry) -> dict:
    grid = build_grid()
    run_sweep(registry, grid, lambda p: run_config(returns, p))
    result = deflate_registry(registry)

    best = registry.load(result["selected_trial_id"])
    blk = block_sharpes(best.returns)
    result["annualized_selected_sharpe"] = result["selected_sharpe"] * np.sqrt(TRADING_DAYS)
    result["block_sharpes"] = blk
    result["block_sharpe_mean"] = float(np.mean(blk))
    result["block_sharpe_std"] = float(np.std(blk, ddof=1))
    result["best_params"] = best.params
    return result


def write_summary(result: dict, path: Path):
    lines = [
        "# QR4 under CPCV + Deflated Sharpe (QR2.5)",
        "",
        f"Swept **{result['n_trials']} configurations** of QR4's entry/exit bands "
        f"× estimation window — the overfitting-prone knobs — logging each to the "
        f"trial registry with its cost-free paper-PnL return series.",
        "",
        "## The deflated result",
        "",
        f"- Best config: `{result['best_params']}`",
        f"- Best annualized Sharpe (cost-free): **{result['annualized_selected_sharpe']:.2f}**",
        f"- Trials deflated against: **N = {result['n_trials']}**",
        f"- Expected max Sharpe under the null (per-period): "
        f"SR*₀ = {result['expected_max_sharpe_null']:.3f}",
        f"- Undeflated PSR(0): {result['psr_vs_zero']:.3f}",
        f"- **Deflated Sharpe Ratio: DSR = {result['deflated_sharpe_ratio']:.3f}**",
        "",
        "## Temporal stability (CPCV blocks)",
        "",
        f"Per-block out-of-sample per-period Sharpe of the chosen config across 6 "
        f"time blocks: mean {result['block_sharpe_mean']:.3f}, "
        f"std {result['block_sharpe_std']:.3f} "
        f"(min {min(result['block_sharpes']):.3f}, max {max(result['block_sharpes']):.3f}).",
        "",
        "## Reading it",
        "",
        f"DSR = {result['deflated_sharpe_ratio']:.2f} is the probability the chosen "
        "config's *true* Sharpe beats what the luckiest of "
        f"{result['n_trials']} skill-less configurations would have produced. "
        + (
            "It clears 0.5, so the paper edge is not purely an artifact of the "
            "band/window search — "
            if result["deflated_sharpe_ratio"] > 0.5
            else "It does not clear 0.5, so the paper edge is within what the search "
            "alone would produce — "
        )
        + "but this is the *cost-free* series; the Engine B haircut (QR4.7) applies "
        "on top, so the net-of-cost, deflated verdict is lower still.",
        "",
    ]
    path.write_text("\n".join(lines))


def plot(result: dict, registry: TrialRegistry, path: Path):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    per_period = [sharpe_ratio(t.returns) for t in registry.load_all()]
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.hist(
        per_period,
        bins=max(6, len(per_period) // 2),
        color="tab:green",
        alpha=0.7,
        label="per-config Sharpe (sweep)",
    )
    ax.axvline(
        result["selected_sharpe"],
        color="tab:green",
        lw=1.8,
        label=f"best {result['selected_sharpe']:.3f}",
    )
    ax.axvline(
        result["expected_max_sharpe_null"],
        color="red",
        ls="--",
        lw=1.6,
        label=f"E[max | null] SR*₀ = {result['expected_max_sharpe_null']:.3f}",
    )
    ax.set_xlabel("per-period Sharpe")
    ax.set_ylabel("count")
    ax.set_title(
        f"QR4 band/window sweep (N={result['n_trials']}): "
        f"DSR = {result['deflated_sharpe_ratio']:.2f}"
    )
    ax.legend(fontsize=9)
    fig.tight_layout()
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=150)
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--returns", type=Path, default=Path("data/universe/universe_returns.parquet")
    )
    parser.add_argument("--registry", type=Path, default=Path("data/universe/qr4_trials"))
    parser.add_argument(
        "--summary", type=Path, default=Path("docs/research/statarb/qr4_dsr_summary.md")
    )
    parser.add_argument("--plot", type=Path, default=Path("docs/research/statarb/qr4_dsr.png"))
    args = parser.parse_args()

    returns = pd.read_parquet(args.returns)
    registry = TrialRegistry(args.registry)
    result = deflate(returns, registry)
    write_summary(result, args.summary)
    plot(result, registry, args.plot)
    print(
        f"QR4 DSR: swept N={result['n_trials']} configs, best annualized Sharpe "
        f"{result['annualized_selected_sharpe']:.2f}, DSR "
        f"{result['deflated_sharpe_ratio']:.3f} -> {args.summary}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
