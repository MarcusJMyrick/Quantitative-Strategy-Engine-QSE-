"""F3 — the QSE one-pager: architecture + key results + repo link on one PDF page.

A single-page, print-ready summary of the whole project, built the same way the
tearsheet is (matplotlib's PDF backend, no extra dependencies). It draws a compact
architecture pipeline, tabulates the marquee findings from every track, embeds the
flagship A/B slippage figure, and links the repo — the artifact you hand someone
who has 60 seconds.

    venv/bin/python scripts/analysis/onepager.py       # -> docs/QSE_one_pager.pdf
"""

import argparse
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.image as mpimg  # noqa: E402
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch  # noqa: E402

REPO_URL = "github.com/MarcusJMyrick/Quantitative-Strategy-Engine-QSE-"

INK = "#1a1a2e"
ACCENT = "#0f4c81"
HILITE = "#b03a2e"
MUTED = "#5a5a6e"
BOX_BG = "#eef2f7"

# (label, value) — the marquee numbers, grouped engine-first then research.
RESULTS = [
    ("Phantom profit @ 25k sh/signal", "$813,700  (naive Sharpe +1.93 -> real -5.26)"),
    ("Market-impact exponent", "b = 0.569 vs sqrt-law 0.5  (R2 = 0.999)"),
    ("Arena allocator vs new/delete", "3.5 ns vs 57-70 ns  (16-20x)"),
    ("Lock-free SPSC ring, tail latency", "p99 42 ns vs 16,334 ns  (389x); TSan-clean"),
    ("Cross-platform determinism", "Docker == native: Sharpe -2.404, 456 trades"),
    ("Live paper trading (Alpaca)", "5 signals -> 5 fills -> 5/5 reconciled"),
    ("Stat arb vs momentum, Engine B", "credible negative: momentum 0.84 > stat arb 0.69"),
    ("Deflated Sharpe (multiple testing)", "100 noise: PSR 0.99 but DSR 0.47"),
    ("HMM regime overlay", "calm 44% / elevated 33% / turbulent 23% -> A5 lambda"),
    ("VPIN/OFI toxicity filter", "robust negative: raises slippage 0.0117 vs 0.0100"),
    ("Meta-labeling, judged", "rejected: Engine B 0.69 -> 0.17; DSR 0.94 > 0.77"),
    ("HRP vs MVO out-of-sample", "MVO -0.35 collapse, HRP 0.65, 1/N still wins 0.90"),
]

# Architecture pipeline stages: (title, sublines, highlight?)
STAGES = [
    ("Data layer", ["CSV / Parquet readers", "Python pipeline", "ZeroMQ tick feed"], False),
    (
        "C++ engine",
        ["SPSC ring -> BarBuilder", "Strategies -> OrderManager", "FULL-DEPTH ORDER BOOK"],
        True,
    ),
    ("Analysis", ["equity + tradelog", "tearsheet.py", "research artifacts"], False),
]


def _box(ax, x, y, w, h, title, lines, highlight):
    edge = HILITE if highlight else ACCENT
    ax.add_patch(
        FancyBboxPatch(
            (x, y),
            w,
            h,
            boxstyle="round,pad=0.008,rounding_size=0.015",
            linewidth=2.0 if highlight else 1.3,
            edgecolor=edge,
            facecolor="#fdece9" if highlight else BOX_BG,
            mutation_aspect=0.5,
        )
    )
    ax.text(
        x + w / 2,
        y + h - 0.055,
        title,
        ha="center",
        va="top",
        fontsize=11,
        fontweight="bold",
        color=edge,
    )
    for i, ln in enumerate(lines):
        bold = ln.isupper()
        ax.text(
            x + w / 2,
            y + h - 0.11 - i * 0.052,
            ln.title() if bold else ln,
            ha="center",
            va="top",
            fontsize=8.2,
            color=HILITE if bold else INK,
            fontweight="bold" if bold else "normal",
        )


def draw_architecture(ax):
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis("off")
    w, h, gap = 0.28, 0.62, 0.06
    xs = [0.02, 0.02 + w + gap, 0.02 + 2 * (w + gap)]
    for x, (title, lines, hi) in zip(xs, STAGES):
        _box(ax, x, 0.30, w, h, title, lines, hi)
    for x in xs[:-1]:
        ax.add_patch(
            FancyArrowPatch(
                (x + w + 0.006, 0.61),
                (x + w + gap - 0.006, 0.61),
                arrowstyle="-|>",
                mutation_scale=16,
                linewidth=1.6,
                color=MUTED,
            )
        )
    ax.text(
        0.5,
        0.16,
        "Live mode:  same strategy code -> Alpaca paper venue " "(order + fill reconciliation)",
        ha="center",
        fontsize=8.4,
        color=ACCENT,
    )
    ax.text(
        0.5,
        0.06,
        "Research track (QR):  eigen stat arb  |  HMM regime -> lambda  |  "
        "meta-labeling  |  CPCV + Deflated Sharpe + MDA (the judge)",
        ha="center",
        fontsize=8.4,
        color=ACCENT,
    )


def build(out_path: Path, fig_path: Path) -> Path:
    fig = plt.figure(figsize=(8.5, 11))
    fig.patch.set_facecolor("white")

    # --- Header ---
    fig.text(
        0.5,
        0.965,
        "QSE - Quantitative Strategy Engine",
        ha="center",
        fontsize=21,
        fontweight="bold",
        color=INK,
    )
    fig.text(
        0.5,
        0.938,
        "A C++ event-driven backtesting & execution-research engine:",
        ha="center",
        fontsize=11,
        color=MUTED,
    )
    fig.text(
        0.5,
        0.920,
        "how much of a backtest's profit is real, and how much is an "
        "artifact of ignoring market microstructure?",
        ha="center",
        fontsize=11,
        color=MUTED,
        style="italic",
    )
    fig.text(0.5, 0.898, REPO_URL, ha="center", fontsize=10.5, fontweight="bold", color=ACCENT)

    # --- Architecture ---
    fig.text(0.06, 0.868, "ARCHITECTURE", fontsize=12, fontweight="bold", color=INK)
    draw_architecture(fig.add_axes([0.05, 0.66, 0.90, 0.19]))

    # --- Key results (two columns) ---
    fig.text(0.06, 0.632, "KEY RESULTS", fontsize=12, fontweight="bold", color=INK)
    fig.text(
        0.30,
        0.632,
        "(engine + low-latency, then research track - honest negatives included)",
        fontsize=8.5,
        color=MUTED,
        style="italic",
    )
    n = len(RESULTS)
    half = (n + 1) // 2
    cols = [(0.06, RESULTS[:half]), (0.53, RESULTS[half:])]
    for x0, items in cols:
        y = 0.610
        for label, value in items:
            fig.text(x0, y, label, fontsize=8.7, fontweight="bold", color=ACCENT)
            fig.text(x0, y - 0.019, value, fontsize=8.2, color=INK)
            y -= 0.049

    # --- Flagship figure ---
    fig.text(
        0.06, 0.318, "FLAGSHIP: THE A/B SLIPPAGE AUDIT", fontsize=12, fontweight="bold", color=INK
    )
    fig.text(
        0.06,
        0.300,
        "Same 455 signals, two fill models - at 25k sh the naive engine's "
        "fundable Sharpe-1.9 becomes a heavy loser.",
        fontsize=8.6,
        color=MUTED,
    )
    if fig_path.exists():
        img_ax = fig.add_axes([0.08, 0.075, 0.84, 0.215])
        img_ax.imshow(mpimg.imread(str(fig_path)))
        img_ax.axis("off")
    else:
        fig.text(0.5, 0.18, "[ figure unavailable ]", ha="center", color=MUTED)

    # --- Footer ---
    fig.add_artist(plt.Line2D([0.06, 0.94], [0.055, 0.055], color="#cccccc", lw=0.8))
    fig.text(
        0.06,
        0.036,
        "302 C++ / 258 Python tests   |   3 CI gates (build+test, "
        "format/lint, clang-tidy)   |   two compilers x standards x Arrow   |   "
        "ThreadSanitizer-clean",
        fontsize=7.4,
        color=MUTED,
    )
    fig.text(
        0.06,
        0.020,
        "Whitepaper: docs/PROJECT_PHASES.md   |   "
        "Log: docs/TASK_BREAKDOWN.md   |   Walkthrough: notebooks/qse_walkthrough.ipynb",
        fontsize=8,
        color=ACCENT,
    )

    out_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(str(out_path))  # format inferred from suffix (.pdf, or .png to preview)
    plt.close(fig)
    return out_path


def main() -> int:
    here = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", type=Path, default=here / "docs" / "QSE_one_pager.pdf")
    parser.add_argument(
        "--figure",
        type=Path,
        default=here / "docs" / "research" / "microstructure" / "slippage_audit.png",
    )
    args = parser.parse_args()
    out = build(args.out, args.figure)
    print(f"one-pager -> {out}  ({out.stat().st_size:,} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
