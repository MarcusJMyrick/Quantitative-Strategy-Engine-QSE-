"""Unit tests for scripts/research/statarb/residuals.py (QR4.3).

Done-when coverage: residuals from the retained-factor regression are
(approximately) orthogonal to those factors in-window — max abs correlation
below tolerance. Plus: known-beta recovery, the cumulative-residual identity
and its sum-to-zero property, R2 behavior, and causality.
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "research" / "statarb"))

from residuals import (  # noqa: E402
    in_window_r2,
    max_abs_resid_factor_corr,
    regress_on_factors,
    residuals_for_window,
    rolling_residuals,
)

BDAYS = pd.bdate_range("2024-01-01", periods=600)


def factor_model_returns(
    seed: int, t: int = 120, n: int = 8, m: int = 2, noise: float = 0.01
) -> tuple[pd.DataFrame, np.ndarray, np.ndarray]:
    """Build returns R = alpha + F @ beta.T + noise with known alpha, beta and
    m common factors. Returns (returns_df, betas [N x m], factors [t x m])."""
    rng = np.random.default_rng(seed)
    factors = rng.normal(0, 0.02, (t, m))
    betas = rng.normal(1.0, 0.4, (n, m))
    alphas = rng.normal(0, 0.001, n)
    eps = rng.normal(0, noise, (t, n))
    r = alphas + factors @ betas.T + eps
    cols = [f"S{i}" for i in range(n)]
    return pd.DataFrame(r, index=BDAYS[:t], columns=cols), betas, factors


# ---------------------------------------------------------------------------
# Done-when: OLS residuals orthogonal to the regressors
# ---------------------------------------------------------------------------


def test_residuals_orthogonal_to_factors():
    returns, _, factors = factor_model_returns(seed=1, m=2)
    _, _, residuals = regress_on_factors(returns, factors)
    assert max_abs_resid_factor_corr(residuals, factors) < 1e-9


def test_residuals_orthogonal_through_the_pca_path():
    """The real path: factors come from window PCA, not handed in. Residuals
    must still be orthogonal to whatever factors were retained."""
    returns, _, _ = factor_model_returns(seed=2, t=90, n=10, m=2, noise=0.008)
    res = residuals_for_window(returns, mode="mp")
    assert res["num_factors"] >= 1  # a real factor exists to retain
    assert res["max_orthogonality_corr"] < 1e-9


# ---------------------------------------------------------------------------
# Regression correctness
# ---------------------------------------------------------------------------


def test_recovers_known_betas():
    returns, betas, factors = factor_model_returns(seed=3, t=400, n=6, m=2, noise=1e-4)
    est_betas, alphas, _ = regress_on_factors(returns, factors)
    assert np.abs(est_betas - betas).max() < 1e-2
    assert np.abs(alphas).max() < 1e-2  # alphas were ~0


def test_no_factor_case_is_demeaned_returns():
    """m == 0: the model is the intercept alone, residual = demeaned return,
    orthogonality vacuously zero."""
    returns = pd.DataFrame(
        {"A": [0.01, -0.02, 0.03, 0.00], "B": [-0.01, 0.02, 0.00, 0.01]}, index=BDAYS[:4]
    )
    empty = np.empty((4, 0))
    betas, alphas, residuals = regress_on_factors(returns, empty)
    assert betas.shape == (2, 0)
    np.testing.assert_allclose(alphas, returns.mean().to_numpy())
    np.testing.assert_allclose(residuals, (returns - returns.mean()).to_numpy(), atol=1e-12)
    assert max_abs_resid_factor_corr(residuals, empty) == 0.0


# ---------------------------------------------------------------------------
# Cumulative residual: identity + sum-to-zero (the QR4.4-facing contract)
# ---------------------------------------------------------------------------


def test_cumulative_residual_is_cumsum():
    returns, _, _ = factor_model_returns(seed=4, t=80, n=5, m=2)
    res = residuals_for_window(returns, mode="mp")
    np.testing.assert_allclose(
        res["cumulative_residuals"], np.cumsum(res["residuals"], axis=0), atol=1e-15
    )


def test_residuals_sum_to_zero_so_cumulative_ends_at_zero():
    """OLS-with-intercept residuals sum to zero in-window, so X_i(t) ~ 0 at the
    window's right edge — the property QR4.4 must build the s-score around."""
    returns, _, _ = factor_model_returns(seed=5, t=100, n=7, m=2)
    res = residuals_for_window(returns, mode="mp")
    assert np.abs(res["residuals"].sum(axis=0)).max() < 1e-10
    assert np.abs(res["cumulative_residuals"][-1]).max() < 1e-10


# ---------------------------------------------------------------------------
# R^2 behavior
# ---------------------------------------------------------------------------


def test_r2_high_for_factor_driven_low_for_noise():
    # strongly factor-driven -> most variance explained
    driven, _, factors = factor_model_returns(seed=6, t=200, n=6, m=2, noise=0.002)
    _, _, resid = regress_on_factors(driven, factors)
    assert np.nanmin(in_window_r2(driven, resid)) > 0.8

    # returns independent of the regressor factor -> ~0 explained
    rng = np.random.default_rng(7)
    noise = pd.DataFrame(rng.normal(0, 0.02, (200, 4)), columns=list("abcd"))
    unrelated = rng.normal(0, 0.02, (200, 1))
    _, _, resid_n = regress_on_factors(noise, unrelated)
    assert np.nanmax(in_window_r2(noise, resid_n)) < 0.15


# ---------------------------------------------------------------------------
# Rolling driver: shapes, diagnostics, causality
# ---------------------------------------------------------------------------


def test_rolling_shapes_and_orthogonality():
    returns, _, _ = factor_model_returns(seed=8, t=200, n=6, m=2, noise=0.01)
    window = 60
    out = rolling_residuals(returns, window=window)
    n_windows = 200 - window + 1

    assert out["r2"].shape == (n_windows, 6)
    assert out["market_beta"].shape == (n_windows, 6)
    assert list(out["diagnostics"].columns) == [
        "num_factors",
        "mean_r2",
        "max_orthogonality_corr",
    ]
    assert out["r2"].index[0] == returns.index[window - 1]
    # every window's residuals are orthogonal to its factors
    assert out["diagnostics"]["max_orthogonality_corr"].max() < 1e-8
    assert out["diagnostics"]["num_factors"].min() >= 1


def test_rolling_residuals_is_causal():
    """Appending future data must not change any already-emitted window."""
    returns, _, _ = factor_model_returns(seed=9, t=200, n=6, m=2)
    full = rolling_residuals(returns, window=50)
    truncated = rolling_residuals(returns.iloc[:120], window=50)
    for key in ("r2", "market_beta", "diagnostics"):
        pd.testing.assert_frame_equal(full[key].loc[truncated[key].index], truncated[key])


def test_market_beta_is_positive_for_a_common_factor():
    """Factor 1 is the market mode; with a positive common factor every name
    loads positively on it (the deterministic PCA sign convention)."""
    returns, _, _ = factor_model_returns(seed=10, t=120, n=8, m=1, noise=0.006)
    out = rolling_residuals(returns, window=60)
    # median market beta across names/windows should be clearly positive
    assert np.nanmedian(out["market_beta"].to_numpy()) > 0
