"""Unit tests for scripts/research/statarb/rolling_pca.py (QR4.2).

Done-when coverage: the top eigenvector of a synthetic block-correlated
matrix is recovered, and the Marchenko-Pastur cutoff drops pure-noise
eigenvalues on a simulated noise matrix (while detecting a planted factor).
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "research" / "statarb"))

from rolling_pca import (  # noqa: E402
    eigenportfolio_weights,
    factor_returns_window,
    mp_lambda_plus,
    pca_for_window,
    rolling_pca,
    select_num_factors,
    window_pca,
)

BDAYS = pd.bdate_range("2024-01-01", periods=600)


def make_block_returns(
    seed: int, t: int = 500, block_a: int = 6, block_b: int = 6, beta_a: float = 0.9
) -> pd.DataFrame:
    """Block-correlated returns: block A names share a strong common factor,
    block B names are pure idiosyncratic noise, zero cross-correlation."""
    rng = np.random.default_rng(seed)
    factor = rng.normal(0, 0.02, t)
    a = beta_a * factor[:, None] + rng.normal(0, 0.008, (t, block_a))
    b = rng.normal(0, 0.02, (t, block_b))
    cols = [f"A{i}" for i in range(block_a)] + [f"B{i}" for i in range(block_b)]
    return pd.DataFrame(np.hstack([a, b]), index=BDAYS[:t], columns=cols)


# ---------------------------------------------------------------------------
# Done-when #1: recover the known top eigenvector of a block-correlated matrix
# ---------------------------------------------------------------------------


def test_recovers_analytic_top_eigenvector_of_equicorrelated_block():
    """For an equicorrelated block (all pairwise rho), the top eigenvector is
    analytically uniform: v = (1,...,1)/sqrt(N), eigenvalue 1 + (N-1)*rho.
    Simulate a one-factor block and check both to tolerance."""
    n = 8
    rng = np.random.default_rng(7)
    factor = rng.normal(0, 0.02, 4000)
    returns = pd.DataFrame(
        factor[:, None] + rng.normal(0, 0.01, (4000, n)),
        columns=[f"S{i}" for i in range(n)],
    )
    eigenvalues, eigenvectors, _ = window_pca(returns)

    # implied rho = var(f) / (var(f) + var(eps)) = 4 / (4 + 1) = 0.8
    assert eigenvalues[0] == pytest.approx(1 + (n - 1) * 0.8, rel=0.05)
    uniform = np.ones(n) / np.sqrt(n)
    assert np.abs(eigenvectors[:, 0] - uniform).max() < 0.05
    assert eigenvectors[:, 0].sum() > 0  # deterministic sign convention


def test_top_eigenvector_loads_on_the_correlated_block_only():
    returns = make_block_returns(seed=11)
    eigenvalues, eigenvectors, _ = window_pca(returns)
    v1 = eigenvectors[:, 0]

    a_loadings, b_loadings = v1[:6], v1[6:]
    assert a_loadings.min() > 0.3  # near-uniform positive weight inside block A
    assert np.abs(b_loadings).max() < 0.15  # ~zero weight on the noise block
    # block A is ~perfectly correlated internally -> lambda_1 ~ block size
    assert eigenvalues[0] == pytest.approx(6, rel=0.2)


# ---------------------------------------------------------------------------
# Done-when #2: the MP cutoff drops pure noise (and keeps a planted factor)
# ---------------------------------------------------------------------------


def test_mp_lambda_plus_formula():
    assert mp_lambda_plus(15, 60) == pytest.approx((1 + np.sqrt(0.25)) ** 2)  # 2.25
    assert mp_lambda_plus(10, 1000) == pytest.approx((1 + 0.1) ** 2)


def test_mp_cutoff_drops_pure_noise():
    rng = np.random.default_rng(42)
    noise = pd.DataFrame(rng.normal(0, 0.02, (1000, 10)), columns=[f"N{i}" for i in range(10)])
    eigenvalues, _, _ = window_pca(noise)
    m = select_num_factors(eigenvalues, n_assets=10, window=1000, mode="mp")
    assert m == 0  # every eigenvalue of iid noise sits below lambda+
    assert eigenvalues[0] < mp_lambda_plus(10, 1000)


def test_mp_cutoff_keeps_a_planted_factor():
    returns = make_block_returns(seed=13)
    eigenvalues, _, _ = window_pca(returns)
    m = select_num_factors(eigenvalues, n_assets=12, window=500, mode="mp")
    assert m == 1  # the planted block factor survives; the noise does not


# ---------------------------------------------------------------------------
# Comparison modes (pure-function checks on a hand-built spectrum)
# ---------------------------------------------------------------------------


def test_fixed_and_variance_modes():
    eigenvalues = np.array([6.0, 3.0, 2.0, 1.0, 1.0, 1.0, 1.0])  # sums to N=15
    assert select_num_factors(eigenvalues, 15, 60, mode="fixed", fixed_k=3) == 3
    assert select_num_factors(eigenvalues, 15, 60, mode="fixed", fixed_k=99) == 15  # clamped
    # cumulative shares: 0.40, 0.60 -> first m reaching 55% is 2
    assert select_num_factors(eigenvalues, 15, 60, mode="variance", variance_target=0.55) == 2
    assert select_num_factors(eigenvalues, 15, 60, mode="variance", variance_target=0.40) == 1
    with pytest.raises(ValueError, match="requires fixed_k"):
        select_num_factors(eigenvalues, 15, 60, mode="fixed")
    with pytest.raises(ValueError, match="Unknown mode"):
        select_num_factors(eigenvalues, 15, 60, mode="bogus")


# ---------------------------------------------------------------------------
# Eigenportfolio weights + factor returns
# ---------------------------------------------------------------------------


def test_eigenportfolio_weights_are_inverse_vol():
    """Two perfectly correlated names, B twice A's vol: the top eigenvector is
    (1,1)/sqrt(2), so Q_A / Q_B must equal sigma_B / sigma_A = 2."""
    rng = np.random.default_rng(3)
    a = rng.normal(0, 0.01, 300)
    returns = pd.DataFrame({"A": a, "B": 2 * a}, index=BDAYS[:300])
    eigenvalues, eigenvectors, sigmas = window_pca(returns)
    q = eigenportfolio_weights(eigenvectors, sigmas, 1)

    assert eigenvalues[0] == pytest.approx(2.0)
    assert q[0, 0] / q[1, 0] == pytest.approx(2.0)

    f = factor_returns_window(returns, q)
    # F_t = (r_A/sigma_A + r_B/sigma_B)/sqrt(2); with r_B = 2 r_A this is
    # sqrt(2) * r_A / sigma_A
    expected = np.sqrt(2) * a / a.std(ddof=1)
    assert np.abs(f[:, 0] - expected).max() < 1e-10


def test_pca_for_window_shapes():
    returns = make_block_returns(seed=17, t=120)
    out = pca_for_window(returns, mode="mp")
    n = returns.shape[1]
    assert out["eigenvalues"].shape == (n,)
    assert out["eigenvectors"].shape == (n, n)
    assert out["weights"].shape == (n, out["num_factors"])
    assert out["factor_returns"].shape == (len(returns), out["num_factors"])
    assert out["num_factors"] >= 1


def test_window_pca_rejects_degenerate_column():
    returns = pd.DataFrame({"A": [0.01, -0.01, 0.02, 0.0], "B": [0.0, 0.0, 0.0, 0.0]})
    with pytest.raises(ValueError, match="Zero return variance"):
        window_pca(returns)


# ---------------------------------------------------------------------------
# Rolling driver: shapes, spectrum sanity, causality
# ---------------------------------------------------------------------------


def test_rolling_outputs_shapes_and_counts():
    returns = make_block_returns(seed=19, t=200)
    window = 60
    out = rolling_pca(returns, window=window)
    n_windows = 200 - window + 1

    assert len(out["spectrum"]) == n_windows
    assert out["spectrum"].shape[1] == returns.shape[1]
    assert out["spectrum"].index[0] == returns.index[window - 1]
    assert not out["spectrum"].isna().any().any()
    # eigenvalues of a correlation matrix sum to N in every window
    assert out["spectrum"].sum(axis=1).round(8).eq(returns.shape[1]).all()

    counts = out["counts"]
    assert set(counts.columns) == {"mp", "variance_55"}
    assert counts["mp"].between(0, returns.shape[1]).all()

    fr = out["factor_returns"]
    assert list(fr.columns) == ["date", "factor", "ret"]
    assert not fr.isna().any().any()
    # one row per retained factor per date
    per_date = fr.groupby("date").size()
    assert (per_date.values == counts.loc[per_date.index, "mp"].values).all()


def test_rolling_pca_is_causal():
    """Appending future data must not change any already-emitted window."""
    returns = make_block_returns(seed=23, t=200)
    full = rolling_pca(returns, window=50)
    truncated = rolling_pca(returns.iloc[:120], window=50)

    pd.testing.assert_frame_equal(
        full["spectrum"].loc[truncated["spectrum"].index], truncated["spectrum"]
    )
    pd.testing.assert_frame_equal(
        full["counts"].loc[truncated["counts"].index], truncated["counts"]
    )
    cutoff = truncated["factor_returns"]["date"].max()
    pd.testing.assert_frame_equal(
        full["factor_returns"][full["factor_returns"]["date"] <= cutoff].reset_index(drop=True),
        truncated["factor_returns"].reset_index(drop=True),
    )
