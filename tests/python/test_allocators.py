"""Unit tests for QR-X — MVO vs HRP allocators and the walk-forward comparison.

Done-when: both allocators run on the same universe/windows; a tearsheet compares
out-of-sample Sharpe and turnover, judged under CPCV. The real numbers live in
docs/research/portfolio/hrp_vs_mvo_summary.md; here we pin the allocator math and
the no-lookahead backtest on synthetic inputs.
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(_ROOT / "scripts" / "research" / "portfolio"))
sys.path.insert(0, str(_ROOT / "scripts" / "research" / "validation"))

from allocators import (  # noqa: E402
    equal_weights,
    hrp_weights,
    inverse_variance_weights,
    mvo_weights,
)
from compare_allocators import block_sharpes, rebalance_backtest  # noqa: E402


# ---------------------------------------------------------------------------
# Allocator invariants
# ---------------------------------------------------------------------------


def test_all_allocators_sum_to_one():
    rng = np.random.default_rng(0)
    A = rng.normal(size=(200, 5))
    cov = np.cov(A, rowvar=False)
    for w in (equal_weights(5), inverse_variance_weights(cov), mvo_weights(cov), hrp_weights(cov)):
        assert w.sum() == pytest.approx(1.0)
        assert len(w) == 5


def test_ivp_is_inverse_variance():
    cov = np.diag([1.0, 4.0, 16.0])  # variances 1, 4, 16
    w = inverse_variance_weights(cov)
    # weights ∝ 1, 1/4, 1/16 = 16:4:1 of 21
    assert w == pytest.approx(np.array([16, 4, 1]) / 21.0)


def test_mvo_equals_ivp_when_uncorrelated():
    # diagonal Σ ⇒ Σ⁻¹1 ∝ 1/σ²ᵢ ⇒ min-variance == inverse-variance
    cov = np.diag([0.5, 2.0, 8.0])
    assert mvo_weights(cov) == pytest.approx(inverse_variance_weights(cov))


def test_hrp_reduces_to_inverse_variance_when_uncorrelated():
    # with no correlation HRP's bisection allocates purely by variance
    cov = np.diag([1.0, 3.0, 9.0, 27.0])
    assert hrp_weights(cov) == pytest.approx(inverse_variance_weights(cov), rel=1e-6)


def test_hrp_is_long_only_and_diversifies_a_correlated_cluster():
    # assets 0,1 nearly identical (a redundant pair), asset 2 independent.
    # HRP clusters {0,1} and should not dump the whole book into the pair the
    # way an inverting optimizer can; both the cluster and the loner get capital.
    cov = np.array(
        [
            [1.0, 0.99, 0.0],
            [0.99, 1.0, 0.0],
            [0.0, 0.0, 1.0],
        ]
    )
    w = hrp_weights(cov)
    assert np.all(w > 0)  # long-only
    assert w.sum() == pytest.approx(1.0)
    # the independent asset carries ~half the risk budget, the correlated pair
    # splits the other half (so neither of 0,1 dominates the loner)
    assert w[2] == pytest.approx(0.5, abs=0.1)
    assert w[0] == pytest.approx(w[1], abs=1e-6)  # symmetric pair


def test_mvo_can_go_short_when_hrp_cannot():
    # covariance between the legs exceeds asset 0's own variance, so the
    # min-variance optimum shorts asset 1 (w₂ = (σ₁²−σ₁₂)/(σ₁²+σ₂²−2σ₁₂) < 0)
    cov = np.array([[1.0, 1.2], [1.2, 2.0]])  # PSD: eigenvalues 2.8, 0.2
    w_mvo = mvo_weights(cov)
    assert w_mvo.min() < 0  # inversion produces a short
    assert np.all(hrp_weights(cov) > 0)  # HRP stays long-only


def test_mvo_singular_cov_degrades_gracefully():
    cov = np.ones((3, 3))  # perfectly collinear ⇒ singular
    w = mvo_weights(cov)
    assert np.isfinite(w).all() and w.sum() == pytest.approx(1.0)


# ---------------------------------------------------------------------------
# Walk-forward backtest — no lookahead, turnover, CPCV blocks
# ---------------------------------------------------------------------------


def _toy_returns(n=300, d=4, seed=1):
    rng = np.random.default_rng(seed)
    idx = pd.bdate_range("2020-01-01", periods=n)
    return pd.DataFrame(rng.normal(0, 0.01, size=(n, d)), index=idx, columns=list("ABCD"))


def test_backtest_has_no_lookahead():
    # zero out returns after a cutoff; OOS returns before the cutoff must be
    # unchanged whether or not the future exists (weights use only trailing data)
    r = _toy_returns()
    r2 = r.copy()
    r2.iloc[200:] = 0.0
    a = rebalance_backtest(r, inverse_variance_weights, lookback=60, hold=20)["returns"]
    b = rebalance_backtest(r2, inverse_variance_weights, lookback=60, hold=20)["returns"]
    common = a.index[a.index < r.index[200]]
    pd.testing.assert_series_equal(a.loc[common], b.loc[common])


def test_backtest_turnover_and_length():
    r = _toy_returns()
    bt = rebalance_backtest(r, hrp_weights, lookback=60, hold=20)
    # rebalances tile the post-lookback span in `hold`-sized held periods
    assert len(bt["returns"]) == ((len(r) - 60) // 20) * 20
    assert bt["turnover"] >= 0.0
    assert bt["n_rebalances"] >= 2


def test_equal_weight_backtest_has_zero_turnover():
    r = _toy_returns()
    bt = rebalance_backtest(r, lambda cov: equal_weights(cov.shape[0]), lookback=60, hold=20)
    assert bt["turnover"] == pytest.approx(0.0)  # 1/N never changes → no churn


def test_block_sharpes_partition_the_series():
    r = _toy_returns()
    s = rebalance_backtest(r, mvo_weights, lookback=60, hold=20)["returns"]
    blk = block_sharpes(s, n_groups=5, k=2)
    assert len(blk) == 5 and all(np.isfinite(blk))
