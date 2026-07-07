"""QR4.3 — Idiosyncratic residuals for eigenportfolio stat arb.

Given the QR4.2 rolling PCA, each name's return is decomposed into a systematic
part (its exposure to the retained eigenportfolio factors) and an idiosyncratic
residual. Per trailing window ending at date t:

    R_i = alpha_i + sum_j beta_ij * F_j + eps_i           (OLS, one design for
                                                            all names)
    X_i = cumsum(eps_i)                                    (the traded process)

eps_i is the idiosyncratic return; the cumulative residual X_i is the
mean-reverting auxiliary process QR4.4 fits an OU/AR(1) to. Betas are estimated
in-window, so a single pseudo-inverse of the design [1 | F] solves every name
at once.

Two structural facts, both enforced by tests, that QR4.4 depends on:

  * Orthogonality (the QR4.3 done-when). OLS residuals with an intercept are
    orthogonal to every regressor, so the in-window correlation between eps_i
    and each retained factor is zero to numerical precision. This is the
    correctness check — forget the intercept and it breaks.

  * Sum-to-zero. That same intercept forces sum_n eps_i(n) = 0 over the fit
    window, so the cumulative residual returns to ~0 at the window's right
    edge: X_i(t) ~ 0. The tradeable information is the *path* of X across the
    window (its excursions and mean-reversion speed), which the OU fit in
    QR4.4 captures via the estimated equilibrium m and speed kappa — not the
    endpoint level. Flagged here so QR4.4 constructs the s-score from the OU
    parameters, not from a naive "current X".

As-of alignment (inherited from QR4.1/QR4.2): the window ending at date t uses
returns r_{t-w+1} .. r_t only, so betas, residuals, and diagnostics at t are
observable at the close of t; consumers trade no earlier than t+1. Appending
future data never changes an emitted row (tested).

Outputs (data/universe/, Parquets regenerated not committed):
  residual_r2.parquet           date x name — in-window variance explained by
                                the retained factors (1 - SSR/SST)
  residual_market_beta.parquet  date x name — loading on factor 1 (market mode)
  residual_diagnostics.parquet  date x [num_factors, mean_r2,
                                max_orthogonality_corr]
  residual_manifest.json        parameters, summary stats, as-of contract
Plot (committed): docs/research/statarb/residual_diagnostics.png
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

from rolling_pca import pca_for_window  # noqa: E402

logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")
logger = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Window-level primitives (pure functions, unit-tested)
# ---------------------------------------------------------------------------


def regress_on_factors(
    returns_window: pd.DataFrame, factor_returns: np.ndarray
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """OLS-regress every name's returns on the retained factor returns.

    Design is [intercept | F], shared across names, so one least-squares solve
    handles the whole cross-section. Returns (betas [N x m], alphas [N],
    residuals [w x N]). With m == 0 (no retained factors) the model is the
    intercept alone and residuals are the demeaned returns.
    """
    r = returns_window.to_numpy(dtype=float)  # w x N
    w, n = r.shape
    factor_returns = np.asarray(factor_returns, dtype=float).reshape(w, -1)
    m = factor_returns.shape[1]

    design = np.column_stack([np.ones(w), factor_returns]) if m else np.ones((w, 1))
    coef, *_ = np.linalg.lstsq(design, r, rcond=None)  # (m+1) x N
    residuals = r - design @ coef
    alphas = coef[0]
    betas = coef[1:].T if m else np.zeros((n, 0))
    return betas, alphas, residuals


def max_abs_resid_factor_corr(residuals: np.ndarray, factor_returns: np.ndarray) -> float:
    """Largest |Pearson corr| between any residual column and any factor column
    — the orthogonality diagnostic. Zero to precision for correct OLS."""
    factor_returns = np.asarray(factor_returns, dtype=float)
    if factor_returns.ndim == 1:
        factor_returns = factor_returns[:, None]
    if factor_returns.shape[1] == 0:
        return 0.0  # nothing to be orthogonal to

    rz = residuals - residuals.mean(axis=0)
    fz = factor_returns - factor_returns.mean(axis=0)
    rz /= np.linalg.norm(rz, axis=0) + 1e-300
    fz /= np.linalg.norm(fz, axis=0) + 1e-300
    return float(np.abs(rz.T @ fz).max())


def in_window_r2(returns_window: pd.DataFrame, residuals: np.ndarray) -> np.ndarray:
    """Per-name variance explained by the factors: 1 - SSR/SST (SST about the
    in-window mean). NaN for a name with zero in-window variance."""
    r = returns_window.to_numpy(dtype=float)
    ss_res = (residuals**2).sum(axis=0)
    ss_tot = ((r - r.mean(axis=0)) ** 2).sum(axis=0)
    return 1.0 - ss_res / np.where(ss_tot == 0.0, np.nan, ss_tot)


def residuals_for_window(
    returns_window: pd.DataFrame,
    mode: str = "mp",
    fixed_k: int | None = None,
    variance_target: float = 0.55,
) -> dict:
    """Everything QR4.4 needs for one window: the retained-factor regression,
    the idiosyncratic residuals, and the cumulative residual X = cumsum(eps)
    (the OU process), plus in-window diagnostics."""
    pca = pca_for_window(returns_window, mode, fixed_k, variance_target)
    factor_returns = pca["factor_returns"]  # w x m
    betas, alphas, residuals = regress_on_factors(returns_window, factor_returns)
    return {
        "columns": list(returns_window.columns),
        "num_factors": pca["num_factors"],
        "factor_returns": factor_returns,
        "betas": betas,
        "alphas": alphas,
        "residuals": residuals,
        "cumulative_residuals": np.cumsum(residuals, axis=0),  # w x N — X_i(t)
        "r2": in_window_r2(returns_window, residuals),
        "max_orthogonality_corr": max_abs_resid_factor_corr(residuals, factor_returns),
    }


# ---------------------------------------------------------------------------
# Rolling driver
# ---------------------------------------------------------------------------


def rolling_residuals(
    returns: pd.DataFrame,
    window: int = 60,
    mode: str = "mp",
    fixed_k: int | None = None,
    variance_target: float = 0.55,
) -> dict:
    """Slide the trailing window; per date t emit the per-name R2 and market
    beta (loading on factor 1) plus window diagnostics. The heavy cumulative
    residuals are recomputed on demand by QR4.4 via residuals_for_window, not
    stored per date."""
    cols = list(returns.columns)
    dates, r2_rows, beta_rows, diag_rows = [], [], [], []

    for end in range(window, len(returns) + 1):
        win = returns.iloc[end - window : end]
        res = residuals_for_window(win, mode, fixed_k, variance_target)
        dates.append(returns.index[end - 1])
        r2_rows.append(res["r2"])
        beta1 = res["betas"][:, 0] if res["betas"].shape[1] else np.full(len(cols), np.nan)
        beta_rows.append(beta1)
        diag_rows.append(
            {
                "num_factors": res["num_factors"],
                "mean_r2": float(np.nanmean(res["r2"])),
                "max_orthogonality_corr": res["max_orthogonality_corr"],
            }
        )

    index = pd.DatetimeIndex(dates, name="date")
    return {
        "r2": pd.DataFrame(r2_rows, index=index, columns=cols),
        "market_beta": pd.DataFrame(beta_rows, index=index, columns=cols),
        "diagnostics": pd.DataFrame(diag_rows, index=index),
    }


# ---------------------------------------------------------------------------
# Artifacts
# ---------------------------------------------------------------------------


def plot_diagnostics(r2: pd.DataFrame, diagnostics: pd.DataFrame, path: Path):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, (top, bottom) = plt.subplots(2, 1, figsize=(10, 7), sharex=True)

    lo, hi = r2.quantile(0.1, axis=1), r2.quantile(0.9, axis=1)
    top.fill_between(r2.index, lo, hi, alpha=0.2, label="10-90th pct across names")
    top.plot(diagnostics.index, diagnostics["mean_r2"], lw=1.0, label="cross-name mean $R^2$")
    top.set_ylabel("in-window $R^2$")
    top.set_title("Factor-explained variance and residual orthogonality")
    top.legend(loc="upper left", fontsize=8)
    top.set_ylim(0, 1)

    worst = float(diagnostics["max_orthogonality_corr"].max())
    bottom.plot(diagnostics.index, diagnostics["max_orthogonality_corr"], lw=0.8, color="tab:red")
    bottom.set_ylabel("max |corr(resid, factor)|")
    bottom.set_xlabel("window end date")
    bottom.annotate(
        f"global max = {worst:.1e}  (residuals orthogonal to factors by construction)",
        xy=(0.02, 0.85),
        xycoords="axes fraction",
        fontsize=8,
    )

    fig.tight_layout()
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=150)
    plt.close(fig)
    logger.info(f"Diagnostics plot written to {path}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--returns", type=Path, default=Path("data/universe/universe_returns.parquet")
    )
    parser.add_argument("--window", type=int, default=60)
    parser.add_argument("--mode", choices=["mp", "fixed", "variance"], default="mp")
    parser.add_argument("--fixed-k", type=int, default=None)
    parser.add_argument("--variance-target", type=float, default=0.55)
    parser.add_argument("--out-dir", type=Path, default=Path("data/universe"))
    parser.add_argument(
        "--plot", type=Path, default=Path("docs/research/statarb/residual_diagnostics.png")
    )
    args = parser.parse_args()

    returns = pd.read_parquet(args.returns)
    out = rolling_residuals(
        returns,
        window=args.window,
        mode=args.mode,
        fixed_k=args.fixed_k,
        variance_target=args.variance_target,
    )
    r2, market_beta, diagnostics = out["r2"], out["market_beta"], out["diagnostics"]

    args.out_dir.mkdir(parents=True, exist_ok=True)
    r2.to_parquet(args.out_dir / "residual_r2.parquet")
    market_beta.to_parquet(args.out_dir / "residual_market_beta.parquet")
    diagnostics.to_parquet(args.out_dir / "residual_diagnostics.parquet")

    worst_orth = float(diagnostics["max_orthogonality_corr"].max())
    manifest = {
        "generated": date.today().isoformat(),
        "window": args.window,
        "mode": args.mode,
        "n_assets": returns.shape[1],
        "windows_evaluated": len(diagnostics),
        "span": [diagnostics.index[0].date().isoformat(), diagnostics.index[-1].date().isoformat()],
        "mean_r2": {
            "median": float(diagnostics["mean_r2"].median()),
            "min": float(diagnostics["mean_r2"].min()),
            "max": float(diagnostics["mean_r2"].max()),
        },
        "market_beta_median": float(np.nanmedian(market_beta.to_numpy())),
        "max_orthogonality_corr_over_all_windows": worst_orth,
        "as_of_alignment": (
            f"The window ending at date t regresses r_{{t-{args.window}+1}}..r_t on the "
            "same-window factor returns; betas, residuals, and diagnostics at t use data "
            "<= t only. Consumers must not execute before t+1."
        ),
        "note": (
            "OLS-with-intercept residuals sum to zero in-window, so the cumulative "
            "residual X returns to ~0 at the window's right edge; QR4.4 builds the "
            "s-score from the OU fit of the X path, not the endpoint level."
        ),
    }
    (args.out_dir / "residual_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

    plot_diagnostics(r2, diagnostics, args.plot)
    logger.info(
        f"Residuals done: {len(diagnostics)} windows, median factor-explained variance "
        f"{manifest['mean_r2']['median']:.1%}, worst residual-factor |corr| {worst_orth:.1e}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
