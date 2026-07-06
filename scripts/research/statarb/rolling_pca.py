"""QR4.2 — Rolling PCA + principled eigenvalue count for eigenportfolio stat arb.

Consumes the QR4.1 returns matrix and, on each trailing window, eigendecomposes
the correlation matrix of the window's returns. (The correlation matrix of raw
returns over a window IS the covariance of within-window standardized returns,
so the Avellaneda-Lee standardization is implicit here and needs no separate
matrix.) The number of retained factors is principled, not arbitrary:

  Marchenko-Pastur (default): for aspect ratio q = N/T, the eigenvalues of a
  pure-noise correlation matrix fall below lambda+ = (1 + sqrt(q))^2 almost
  surely as T grows. Eigenvalues above lambda+ carry structure; keep those.

  Comparison modes: --mode fixed (Avellaneda-Lee's fixed count) and
  --mode variance (smallest m explaining a target share, e.g. ~55%).

Eigenportfolio j weights each name inversely to its in-window volatility,
  Q_i^(j) = v_i^(j) / sigma_i,
and the factor return is F_j = sum_i Q_i^(j) * R_i. Eigenvector signs are
arbitrary out of the solver, so they are fixed deterministically (sum of
loadings positive; tie broken by the largest-|loading| component). Residuals
regressed on these factors (QR4.3) are invariant to the sign choice.

As-of alignment (inherited from QR4.1): the window ending at date t uses
returns r_{t-w+1} .. r_t only — weights, eigenvalues, and the date-t factor
return are all observable at the close of t; consumers trade no earlier than
t+1. Appending future data never changes an emitted row (tested).

Outputs (data/universe/, Parquets regenerated not committed):
  eigen_spectrum.parquet   date x eigenvalue_1..N of each window
  factor_counts.parquet    date x retained-count under mp / variance modes
  factor_returns.parquet   long format (date, factor, ret) for MP-retained
                           factors — no NaN padding
  rolling_pca_manifest.json  parameters, lambda+, count stats, as-of contract
Plot (committed): docs/research/statarb/eigen_spectrum.png
"""

import argparse
import json
import logging
import sys
from datetime import date
from pathlib import Path

import numpy as np
import pandas as pd

logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")
logger = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Window-level primitives (pure functions, unit-tested)
# ---------------------------------------------------------------------------


def mp_lambda_plus(n_assets: int, window: int) -> float:
    """Marchenko-Pastur upper edge (1 + sqrt(q))^2 for q = N/T: the largest
    eigenvalue a pure-noise correlation matrix produces asymptotically."""
    q = n_assets / window
    return (1.0 + np.sqrt(q)) ** 2


def window_pca(returns_window: pd.DataFrame) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Eigendecompose the correlation matrix of one returns window.

    Returns (eigenvalues, eigenvectors, sigmas): eigenvalues descending,
    eigenvectors as columns in the same order with deterministic signs,
    sigmas the per-name window stds (ddof=1) for eigenportfolio weights.
    """
    values = returns_window.to_numpy(dtype=float)
    sigmas = values.std(axis=0, ddof=1)
    if (sigmas == 0).any():
        dead = returns_window.columns[sigmas == 0].tolist()
        raise ValueError(f"Zero return variance in window for {dead}; degenerate input")

    corr = np.corrcoef(values, rowvar=False)
    eigenvalues, eigenvectors = np.linalg.eigh(corr)  # ascending
    order = np.argsort(eigenvalues)[::-1]
    eigenvalues = eigenvalues[order]
    eigenvectors = eigenvectors[:, order]

    # eigh's sign is arbitrary; fix it so results are reproducible run-to-run
    for j in range(eigenvectors.shape[1]):
        v = eigenvectors[:, j]
        total = v.sum()
        if total < 0 or (total == 0 and v[np.argmax(np.abs(v))] < 0):
            eigenvectors[:, j] = -v

    return eigenvalues, eigenvectors, sigmas


def select_num_factors(
    eigenvalues: np.ndarray,
    n_assets: int,
    window: int,
    mode: str = "mp",
    fixed_k: int | None = None,
    variance_target: float = 0.55,
) -> int:
    """How many eigenvalues carry structure, under the chosen rule.

    mp: count above the Marchenko-Pastur edge (can be 0 on pure noise —
        that is the point of the test, not an error).
    fixed: Avellaneda-Lee style constant, clamped to N.
    variance: smallest m whose eigenvalues explain >= variance_target of
        total variance (correlation eigenvalues sum to N).
    """
    if mode == "mp":
        return int((eigenvalues > mp_lambda_plus(n_assets, window)).sum())
    if mode == "fixed":
        if fixed_k is None:
            raise ValueError("mode='fixed' requires fixed_k")
        return min(fixed_k, n_assets)
    if mode == "variance":
        share = np.cumsum(eigenvalues) / n_assets
        return int(np.searchsorted(share, variance_target) + 1)
    raise ValueError(f"Unknown mode {mode!r}; expected mp | fixed | variance")


def eigenportfolio_weights(
    eigenvectors: np.ndarray, sigmas: np.ndarray, num_factors: int
) -> np.ndarray:
    """Q_i^(j) = v_i^(j) / sigma_i for the top `num_factors` eigenvectors —
    each name weighted inversely to its own volatility (N x m)."""
    return eigenvectors[:, :num_factors] / sigmas[:, None]


def factor_returns_window(returns_window: pd.DataFrame, weights: np.ndarray) -> np.ndarray:
    """In-window factor return series F = R @ Q (w x m)."""
    return returns_window.to_numpy(dtype=float) @ weights


def pca_for_window(
    returns_window: pd.DataFrame,
    mode: str = "mp",
    fixed_k: int | None = None,
    variance_target: float = 0.55,
) -> dict:
    """One window, everything QR4.3 needs: spectrum, retained count, weights,
    and the in-window factor return series."""
    eigenvalues, eigenvectors, sigmas = window_pca(returns_window)
    n_assets, window = returns_window.shape[1], len(returns_window)
    m = select_num_factors(eigenvalues, n_assets, window, mode, fixed_k, variance_target)
    weights = eigenportfolio_weights(eigenvectors, sigmas, m)
    return {
        "eigenvalues": eigenvalues,
        "eigenvectors": eigenvectors,
        "sigmas": sigmas,
        "num_factors": m,
        "weights": weights,
        "factor_returns": factor_returns_window(returns_window, weights),
    }


# ---------------------------------------------------------------------------
# Rolling driver
# ---------------------------------------------------------------------------


def rolling_pca(
    returns: pd.DataFrame,
    window: int = 60,
    mode: str = "mp",
    fixed_k: int | None = None,
    variance_target: float = 0.55,
) -> dict:
    """Slide the trailing window over the returns matrix.

    For each date t (from the first full window on): the eigenvalue spectrum,
    the retained count under the MP and variance rules side by side, and the
    date-t return of each MP-retained eigenportfolio (weights from the window
    ending at t applied to the date-t cross-section — data <= t only).
    """
    n_assets = returns.shape[1]
    dates, spectra, counts, factor_rows = [], [], [], []

    for end in range(window, len(returns) + 1):
        win = returns.iloc[end - window : end]
        t = returns.index[end - 1]
        eigenvalues, eigenvectors, sigmas = window_pca(win)
        m_mp = select_num_factors(eigenvalues, n_assets, window, "mp")
        m_var = select_num_factors(
            eigenvalues, n_assets, window, "variance", variance_target=variance_target
        )
        m_used = select_num_factors(eigenvalues, n_assets, window, mode, fixed_k, variance_target)

        weights = eigenportfolio_weights(eigenvectors, sigmas, m_used)
        f_t = returns.iloc[end - 1].to_numpy(dtype=float) @ weights

        dates.append(t)
        spectra.append(eigenvalues)
        counts.append({"mp": m_mp, f"variance_{int(variance_target * 100)}": m_var})
        factor_rows.extend(
            {"date": t, "factor": j + 1, "ret": float(f_t[j])} for j in range(m_used)
        )

    index = pd.DatetimeIndex(dates, name="date")
    spectrum = pd.DataFrame(
        spectra, index=index, columns=[f"lambda_{i + 1}" for i in range(n_assets)]
    )
    counts_df = pd.DataFrame(counts, index=index)
    factor_returns = pd.DataFrame(factor_rows, columns=["date", "factor", "ret"])
    return {"spectrum": spectrum, "counts": counts_df, "factor_returns": factor_returns}


# ---------------------------------------------------------------------------
# Artifacts
# ---------------------------------------------------------------------------


def plot_spectrum(spectrum: pd.DataFrame, counts: pd.DataFrame, lambda_plus: float, path: Path):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, (top, bottom) = plt.subplots(2, 1, figsize=(10, 7), sharex=True)
    for i in range(min(3, spectrum.shape[1])):
        top.plot(spectrum.index, spectrum.iloc[:, i], lw=0.9, label=f"$\\lambda_{{{i + 1}}}$")
    top.axhline(
        lambda_plus, color="red", ls="--", lw=1.2, label=f"MP $\\lambda_+$={lambda_plus:.2f}"
    )
    top.set_ylabel("eigenvalue (corr matrix)")
    top.set_title("Rolling eigenvalue spectrum vs Marchenko-Pastur noise edge")
    top.legend(loc="upper left", fontsize=8)

    for col in counts.columns:
        bottom.step(counts.index, counts[col], where="post", lw=1.0, label=col)
    bottom.set_ylabel("retained factors")
    bottom.set_xlabel("window end date")
    bottom.legend(loc="upper left", fontsize=8)

    fig.tight_layout()
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=150)
    plt.close(fig)
    logger.info(f"Spectrum plot written to {path}")


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
        "--plot", type=Path, default=Path("docs/research/statarb/eigen_spectrum.png")
    )
    args = parser.parse_args()

    returns = pd.read_parquet(args.returns)
    result = rolling_pca(
        returns,
        window=args.window,
        mode=args.mode,
        fixed_k=args.fixed_k,
        variance_target=args.variance_target,
    )
    spectrum, counts, factor_returns = (
        result["spectrum"],
        result["counts"],
        result["factor_returns"],
    )
    lam_plus = mp_lambda_plus(returns.shape[1], args.window)

    args.out_dir.mkdir(parents=True, exist_ok=True)
    spectrum.to_parquet(args.out_dir / "eigen_spectrum.parquet")
    counts.to_parquet(args.out_dir / "factor_counts.parquet")
    factor_returns.to_parquet(args.out_dir / "factor_returns.parquet")

    mp_counts = counts["mp"]
    manifest = {
        "generated": date.today().isoformat(),
        "window": args.window,
        "n_assets": returns.shape[1],
        "mode": args.mode,
        "lambda_plus": lam_plus,
        "windows_evaluated": len(spectrum),
        "span": [spectrum.index[0].date().isoformat(), spectrum.index[-1].date().isoformat()],
        "mp_count": {
            "min": int(mp_counts.min()),
            "median": float(mp_counts.median()),
            "max": int(mp_counts.max()),
        },
        "top_eigenvalue": {
            "median": float(spectrum["lambda_1"].median()),
            "median_variance_share": float((spectrum["lambda_1"] / returns.shape[1]).median()),
        },
        "as_of_alignment": (
            f"The window ending at date t covers r_{{t-{args.window}+1}}..r_t; eigenvalues, "
            "weights, and the date-t factor return use data <= t only. Consumers must not "
            "execute before t+1."
        ),
    }
    (args.out_dir / "rolling_pca_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

    plot_spectrum(spectrum, counts, lam_plus, args.plot)
    logger.info(
        f"Rolling PCA done: {len(spectrum)} windows, lambda+={lam_plus:.2f}, "
        f"MP count min/median/max = {manifest['mp_count']['min']}/"
        f"{manifest['mp_count']['median']:.0f}/{manifest['mp_count']['max']}, "
        f"median top-eigenvalue share = {manifest['top_eigenvalue']['median_variance_share']:.1%}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
