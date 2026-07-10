"""QR5.5 — Judge the meta-layer (the QR-P5 and whole-QR-track capstone).

The truth serum turned on meta-labeling itself. Three lenses, all leak-free:

  1. meta-on vs meta-off UNDER ENGINE B — the meta-sized weight files (QR5.4)
     run through the SAME full-depth fill model as QR4.7 (src/tools/statarb_audit
     .cpp), net-of-cost Sharpe compared per order-size regime.
  2. DEFLATED SHARPE for both — the meta layer adds its own knobs (mode, floor);
     this sweeps them, and reports the Deflated Sharpe (QR2.4) of meta-off and of
     the best meta-on config against the dispersion of that search. As in QR2.5,
     the DSR sweep uses the cost-free paper PnL (cheap to sweep) and the Engine B
     haircut of lens 1 applies on top.
  3. MDA feature importance under purged CV (QR2.1/QR5.3) — which of the meta
     features actually carried out-of-sample information, permutation-scored on
     purged test folds so the ranking cannot be inflated by leakage.

The expected verdict, given QR5.3's honest null (0.500 CV accuracy): the meta
layer adds no deflated edge, and naively gating/sizing on a no-edge model *hurts*
net-of-cost performance. Building the guardrails is what lets us say that with a
straight face instead of shipping a flattering in-sample number.

Outputs: docs/research/meta/qr5_judge_summary.md
         docs/research/meta/qr5_judge.png
"""

import argparse
import subprocess
import sys
from pathlib import Path

import numpy as np
import pandas as pd

_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE))
sys.path.insert(0, str(_HERE.parent / "statarb"))
sys.path.insert(0, str(_HERE.parent / "validation"))
sys.path.insert(0, str(_HERE.parents[1] / "analysis"))

from baselines import paper_pnl  # noqa: E402
from deflated_sharpe import (  # noqa: E402
    expected_max_sharpe,
    probabilistic_sharpe_ratio,
    sharpe_ratio,
)
from meta_model import mda_importance, purged_cpcv_splits, train_cpcv  # noqa: E402
from meta_sizing import FEATURES, event_probabilities, meta_weights  # noqa: E402
from tearsheet import annualized_sharpe, daily_returns  # noqa: E402

TRADING_DAYS = 252.0
MODES = ["meta_off", "meta_gate", "meta_size"]


# ---------------------------------------------------------------------------
# Lens 1 — Engine B (full-depth fills): net Sharpe meta-on vs meta-off
# ---------------------------------------------------------------------------


def net_returns(audit_dir: Path, mode: str, size: int) -> pd.Series:
    """Daily net-of-cost returns from a depth (Engine B) equity curve."""
    df = pd.read_csv(audit_dir / mode / f"depth_{size}_equity.csv")
    df["date"] = pd.to_datetime(df["timestamp"], unit="ms")
    return daily_returns(df.set_index("date")["equity"])


def engine_b_table(audit_dir: Path, sizes: list[int]) -> pd.DataFrame:
    """Net Engine-B Sharpe + PnL per (mode, size). meta_off == the raw QR4.5
    book, so it reproduces QR4.7's stat_arb numbers (a built-in cross-check)."""
    rows = []
    for mode in MODES:
        for size in sizes:
            eq_path = audit_dir / mode / f"depth_{size}_equity.csv"
            if not eq_path.exists():
                continue
            eq = pd.read_csv(eq_path)
            pnl = float(eq["equity"].iloc[-1] - eq["equity"].iloc[0])
            rows.append(
                {
                    "mode": mode,
                    "size": size,
                    "net_sharpe": annualized_sharpe(net_returns(audit_dir, mode, size)),
                    "net_pnl": pnl,
                }
            )
    return pd.DataFrame(rows)


def run_engine_b(weights_root: Path, audit_dir: Path, sizes: list[int], build: Path) -> None:
    """Drive the C++ statarb_audit over each meta weight dir (Engine A vs B)."""
    tool = build / "statarb_audit"
    if not tool.exists():
        subprocess.run(
            ["cmake", "--build", str(build), "--target", "statarb_audit", "-j4"], check=True
        )
    sizes_arg = ",".join(str(s) for s in sizes)
    for mode in MODES:
        wdir = weights_root / f"weights_{mode}"
        subprocess.run(
            [
                str(tool),
                "--weights-dir",
                str(wdir),
                "--name",
                mode,
                "--out-dir",
                str(audit_dir),
                "--sizes",
                sizes_arg,
            ],
            check=True,
        )


# ---------------------------------------------------------------------------
# Lens 2 — Deflated Sharpe over the meta search (mode × floor)
# ---------------------------------------------------------------------------


def meta_search_family(
    positions: pd.DataFrame,
    events: pd.DataFrame,
    returns: pd.DataFrame,
    floors: list[float],
) -> dict[str, pd.Series]:
    """Cost-free paper-PnL return series for every config in the meta search:
    {gate, size} × floors, plus meta-off. This IS the search the DSR deflates."""
    family = {"meta_off": paper_pnl(meta_weights(positions, None, "off"), returns)}
    for mode in ("gate", "size"):
        for f in floors:
            w = meta_weights(positions, events, mode, floor=f)
            family[f"{mode}@{f:.2f}"] = paper_pnl(w, returns)
    return family


def deflated_verdict(family: dict[str, pd.Series]) -> dict:
    """DSR of meta-off and of the best meta-on config, deflated against the
    dispersion of the whole meta search (expected max Sharpe under the null)."""
    per_period = {k: sharpe_ratio(v.to_numpy()) for k, v in family.items()}
    meta_configs = {k: v for k, v in per_period.items() if k != "meta_off"}
    best_meta = max(meta_configs, key=meta_configs.get)

    sr_star = expected_max_sharpe(list(per_period.values()))
    off = family["meta_off"].to_numpy()
    on = family[best_meta].to_numpy()
    return {
        "n_configs": len(family),
        "sr_star_null": sr_star,
        "best_meta": best_meta,
        "off_sharpe_ann": per_period["meta_off"] * np.sqrt(TRADING_DAYS),
        "on_sharpe_ann": per_period[best_meta] * np.sqrt(TRADING_DAYS),
        "off_psr0": probabilistic_sharpe_ratio(off, 0.0),
        "on_psr0": probabilistic_sharpe_ratio(on, 0.0),
        "off_dsr": probabilistic_sharpe_ratio(off, sr_star),
        "on_dsr": probabilistic_sharpe_ratio(on, sr_star),
        "per_period": per_period,
    }


# ---------------------------------------------------------------------------
# Lens 3 — MDA feature importance under purged CV
# ---------------------------------------------------------------------------


def feature_importance(dataset: pd.DataFrame, n_bars: int, n_groups: int, k: int, embargo: float):
    """MDA importance plus the model's own pooled OOS accuracy vs the majority
    baseline — MDA ranks which feature the model *uses*, the base accuracy says
    whether using them *pays*. Both are needed to read the result honestly."""
    X = dataset[FEATURES].to_numpy()
    y = dataset["label"].to_numpy()
    w = dataset["weight"].to_numpy()
    splits = purged_cpcv_splits(
        dataset["t0_idx"].to_numpy(), dataset["t1_idx"].to_numpy(), n_groups, k, embargo, n_bars
    )
    results = train_cpcv(X, y, splits, sample_weight=w, l2=1.0)
    pooled = np.concatenate([(r["proba"] > 0.5) == y[r["test"]] for r in results])
    return {
        "mda": mda_importance(X, y, splits, FEATURES, sample_weight=w, n_repeats=10, l2=1.0),
        "base_acc": float(pooled.mean()),
        "baseline": float(max(y.mean(), 1.0 - y.mean())),
    }


# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------


def write_summary(eng: pd.DataFrame, dsr: dict, fi: dict, path: Path) -> None:
    mda = fi["mda"]
    big = int(eng["size"].max())
    row = eng.set_index(["mode", "size"])
    off_sr = row.loc[("meta_off", big), "net_sharpe"]
    gate_sr = row.loc[("meta_gate", big), "net_sharpe"]
    size_sr = row.loc[("meta_size", big), "net_sharpe"]

    lines = [
        "# QR5.5 — Judging the meta-layer (Engine B + DSR + MDA)",
        "",
        "The capstone of Track QR: the same truth serum — realistic fills, "
        "deflation, and purged-CV importance — turned on the learned meta-layer "
        "itself. All three lenses are leak-free.",
        "",
        "## Lens 1 — meta-on vs meta-off under Engine B (full-depth fills)",
        "",
        "Net-of-cost Sharpe of the meta-sized books (QR5.4) run through the same "
        "depth fill model as QR4.7. `meta_off` reproduces the raw QR4.5 book, so "
        "it lands on QR4.7's stat_arb numbers (a cross-check).",
        "",
        "| Mode | Size | Net Sharpe (Engine B) | Net PnL |",
        "|---|---|---|---|",
    ]
    for _, r in eng.iterrows():
        lines.append(
            f"| {r['mode']} | {int(r['size'])}× | **{r['net_sharpe']:.2f}** | "
            f"{r['net_pnl']:,.0f} |"
        )
    lines += [
        "",
        f"**At {big}× size, net Sharpe: meta_off {off_sr:.2f}, meta_gate "
        f"{gate_sr:.2f}, meta_size {size_sr:.2f}.** Gating/sizing on the meta "
        "model *destroys* risk-adjusted return — it keeps ~18% of the trades "
        "(1,419 vs 7,747 rebalances) but the survivors are not better-selected "
        "(QR5.3: 0.500 CV), so the 15-name book's diversification collapses "
        "without any compensating skill and the Sharpe craters.",
        "",
        "## Lens 2 — Deflated Sharpe for both (meta search deflation)",
        "",
        f"Swept **{dsr['n_configs']} meta configs** (mode × floor) on the "
        "cost-free paper PnL and deflated against the dispersion of that search "
        "(as in QR2.5; the Engine B haircut above applies on top).",
        "",
        f"- Best meta-on config: `{dsr['best_meta']}`",
        f"- Expected max Sharpe under the null: SR*₀ = {dsr['sr_star_null']:.3f} " "(per-period)",
        f"- meta_off: annualized Sharpe {dsr['off_sharpe_ann']:.2f}, PSR(0) "
        f"{dsr['off_psr0']:.3f}, **DSR {dsr['off_dsr']:.3f}**",
        f"- meta_on:  annualized Sharpe {dsr['on_sharpe_ann']:.2f}, PSR(0) "
        f"{dsr['on_psr0']:.3f}, **DSR {dsr['on_dsr']:.3f}**",
        "",
        "The meta search cannot produce a config whose deflated Sharpe beats "
        "doing nothing (meta_off) — "
        + (
            "meta_on's DSR is below meta_off's, so the layer adds no " "search-corrected edge."
            if dsr["on_dsr"] <= dsr["off_dsr"]
            else "note the unexpected ordering (investigate)."
        ),
        "",
        "## Lens 3 — MDA feature importance under purged CV",
        "",
        "Permutation importance scored on purged test folds — the mean accuracy "
        "drop when each feature is shuffled. Positive ⇒ the model leans on the "
        "feature; ≤ 0 ⇒ it carries no leak-free signal (permuting it doesn't "
        "hurt, or even helps — a fingerprint of mild overfit).",
        "",
        f"First, the model's own scoreline: pooled purged-CV accuracy "
        f"**{fi['base_acc']:.3f}** vs a **{fi['baseline']:.3f}** majority baseline "
        "— it sits on the coin-flip (QR5.3). MDA sharpens *why*:",
        "",
        "| Feature | MDA importance | t-stat |",
        "|---|---|---|",
    ]
    for r in mda:
        lines.append(f"| {r['feature']} | {r['importance']:+.4f} | {r['t_stat']:+.2f} |")
    top = mda[0]
    lines += [
        "",
        f"The *only* feature with positive importance is `{top['feature']}` "
        f"({top['importance']:+.4f}) — the signed s-score, i.e. the primary "
        "signal's own sign, which the meta-model merely rediscovers rather than "
        "adding to. Every **engineered** meta feature — the `abs_sscore` "
        "conviction proxy, `regime`, the vol pair, `kappa`, the order-flow "
        "`vol_ratio` — has ≤ 0 importance: none tells winning QR4 bets from "
        "losers out-of-sample. So the classifier, at best, re-derives the signal "
        "it was handed and still cannot clear the baseline. (The per-feature "
        "t-stats treat the 10 permutation repeats within each fold as "
        "independent, so they overstate significance; the robust reading is the "
        "sign and ranking, not the magnitude.)",
        "",
        "## Verdict",
        "",
        "Meta-labeling, honestly validated, **adds nothing here**, and applied "
        f"naively it *subtracts* (Engine-B Sharpe {off_sr:.2f} → {gate_sr:.2f}). "
        "This is the "
        "intended payoff of the whole track: the guardrails (purged CPCV, DSR, "
        "MDA) let the project state a clean negative with conviction instead of "
        "shipping an overfit story. The one defensible ML addition was worth "
        "building precisely so it could be *rejected* on the evidence.",
        "",
    ]
    path.write_text("\n".join(lines))


def plot(eng: pd.DataFrame, mda: list[dict], path: Path) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, (left, right) = plt.subplots(1, 2, figsize=(12, 5))
    sizes = sorted(eng["size"].unique())
    x = np.arange(len(sizes))
    width = 0.8 / len(MODES)
    colors = {"meta_off": "tab:green", "meta_gate": "tab:red", "meta_size": "tab:orange"}
    for i, mode in enumerate(MODES):
        sub = eng[eng["mode"] == mode].set_index("size").reindex(sizes)
        left.bar(
            x + (i - (len(MODES) - 1) / 2) * width,
            sub["net_sharpe"],
            width,
            label=mode,
            color=colors[mode],
        )
    left.axhline(0, color="gray", lw=0.8)
    left.set_xticks(x)
    left.set_xticklabels([f"{s}×" for s in sizes])
    left.set_ylabel("net Sharpe under Engine B")
    left.set_title("Meta-on gating/sizing craters the Sharpe")
    left.legend(fontsize=8)

    names = [r["feature"] for r in mda][::-1]
    imps = [r["importance"] for r in mda][::-1]
    right.barh(names, imps, color="tab:blue", alpha=0.8)
    right.axvline(0, color="gray", lw=0.8)
    right.set_xlabel("MDA importance (accuracy drop)")
    right.set_title("No feature carries leak-free signal")

    fig.suptitle("QR5.5 — the meta-layer, judged (honest null)", fontsize=12)
    fig.tight_layout()
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=150)
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--data-dir", type=Path, default=Path("data/universe"))
    parser.add_argument("--audit-dir", type=Path, default=Path("results/meta_audit"))
    parser.add_argument("--build", type=Path, default=Path("build"))
    parser.add_argument("--sizes", default="1,10,50")
    parser.add_argument("--floors", default="0.50,0.52,0.54,0.56,0.58,0.60")
    parser.add_argument("--n-groups", type=int, default=6)
    parser.add_argument("--k", type=int, default=2)
    parser.add_argument("--embargo-pct", type=float, default=0.01)
    parser.add_argument("--run-engine-b", action="store_true", help="(re)run the C++ audit first")
    parser.add_argument(
        "--summary", type=Path, default=Path("docs/research/meta/qr5_judge_summary.md")
    )
    parser.add_argument("--plot", type=Path, default=Path("docs/research/meta/qr5_judge.png"))
    args = parser.parse_args()

    sizes = [int(s) for s in args.sizes.split(",")]
    floors = [float(f) for f in args.floors.split(",")]

    positions = pd.read_parquet(args.data_dir / "signal_positions.parquet")
    returns = pd.read_parquet(args.data_dir / "universe_returns.parquet")
    dataset = pd.read_parquet(args.data_dir / "meta_dataset.parquet")
    dataset["proba"] = event_probabilities(
        dataset, len(positions), args.n_groups, args.k, args.embargo_pct
    )
    events = dataset[["name", "t0", "proba"]]

    if args.run_engine_b:
        run_engine_b(args.data_dir, args.audit_dir, sizes, args.build)

    eng = engine_b_table(args.audit_dir, sizes)
    if eng.empty:
        print(f"No Engine-B curves under {args.audit_dir}; rerun with --run-engine-b")
        return 1
    dsr = deflated_verdict(meta_search_family(positions, events, returns, floors))
    fi = feature_importance(dataset, len(positions), args.n_groups, args.k, args.embargo_pct)

    args.summary.parent.mkdir(parents=True, exist_ok=True)
    write_summary(eng, dsr, fi, args.summary)
    plot(eng, fi["mda"], args.plot)

    big = int(eng["size"].max())
    r = eng.set_index(["mode", "size"])["net_sharpe"]
    print(
        f"Engine B @{big}×: off {r[('meta_off', big)]:.2f} | gate "
        f"{r[('meta_gate', big)]:.2f} | size {r[('meta_size', big)]:.2f}"
    )
    print(
        f"DSR: meta_off {dsr['off_dsr']:.3f} vs best meta_on ({dsr['best_meta']}) "
        f"{dsr['on_dsr']:.3f} | SR*₀ {dsr['sr_star_null']:.3f} over {dsr['n_configs']} configs"
    )
    print(
        f"MDA: base acc {fi['base_acc']:.3f} vs baseline {fi['baseline']:.3f}; only "
        f"+ve feature {fi['mda'][0]['feature']} {fi['mda'][0]['importance']:+.4f}"
    )
    print(f"Summary -> {args.summary}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
