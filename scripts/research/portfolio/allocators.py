"""QR-X — risk-based portfolio allocators: MVO (min-variance) vs HRP.

The self-contained research question behind A5: given a covariance matrix over
the QR4 universe, how should capital be split across names? Two philosophies:

  MVO (mean-variance / minimum-variance) — the textbook optimum inverts the
    covariance: w ∝ Σ⁻¹·1. It is optimal *in sample* but the inversion is
    unstable for near-singular Σ (highly correlated names, T not ≫ N): tiny
    estimation errors are amplified into extreme, concentrated, often-short
    weights that churn from rebalance to rebalance. This is the risk-allocation
    core of A5's −λ/2·wᵀΣw objective with alpha = 0.

  HRP (Hierarchical Risk Parity, López de Prado AFML ch. 16) — never inverts Σ.
    It (1) clusters the correlation matrix into a tree, (2) reorders Σ so similar
    assets sit adjacently (quasi-diagonalization), then (3) splits capital
    top-down by recursive bisection, allocating between two sub-clusters in
    inverse proportion to their variance. Hierarchical clustering is the only
    "ML" here — and it predicts nothing about returns.

IVP (inverse-variance) and equal-weight are naive baselines. All allocators
return long-only-or-signed weights that sum to 1 (fully invested).
"""

import numpy as np
from scipy.cluster.hierarchy import linkage
from scipy.spatial.distance import squareform


def equal_weights(n: int) -> np.ndarray:
    """1/N — the assumption-free baseline that is famously hard to beat OOS."""
    return np.full(n, 1.0 / n)


def inverse_variance_weights(cov: np.ndarray) -> np.ndarray:
    """IVP: w ∝ 1/σ²ᵢ, ignoring all correlations. Long-only, sums to 1. A
    degenerate (zero-variance, e.g. no-data) asset gets zero weight; an all-
    degenerate window falls back to equal weight."""
    var = np.diag(cov).astype(float)
    ivar = np.where(var > 0, 1.0 / np.where(var > 0, var, 1.0), 0.0)
    total = ivar.sum()
    return equal_weights(len(var)) if total <= 0 else ivar / total


def mvo_weights(cov: np.ndarray) -> np.ndarray:
    """MVO minimum-variance optimum w = Σ⁻¹·1 / (1ᵀΣ⁻¹·1) — the inversion HRP
    avoids. Uses the pseudo-inverse so a singular Σ degrades gracefully rather
    than raising, but the instability (extreme/negative weights) is preserved on
    purpose: that fragility is the whole point of the comparison. Signed
    (may short), gross-normalized so the weights sum to 1."""
    ones = np.ones(cov.shape[0])
    inv_dot_one = np.linalg.pinv(cov) @ ones
    denom = ones @ inv_dot_one
    if abs(denom) < 1e-300:  # degenerate Σ⁻¹1 ⟂ 1
        return equal_weights(cov.shape[0])
    return inv_dot_one / denom


# ---------------------------------------------------------------------------
# Hierarchical Risk Parity (López de Prado)
# ---------------------------------------------------------------------------


def _cov_to_corr(cov: np.ndarray) -> np.ndarray:
    std = np.sqrt(np.diag(cov))
    denom = np.outer(std, std)
    corr = np.divide(cov, denom, out=np.zeros_like(cov), where=denom > 0)
    return np.clip(corr, -1.0, 1.0)


def _quasi_diag(link: np.ndarray) -> list[int]:
    """Depth-first leaf order from a SciPy linkage matrix — places the most
    correlated assets adjacently so the recursive bisection splits real
    clusters. Iterative expansion of the merge tree (López de Prado)."""
    link = link.astype(int)
    n = link[-1, 3]  # total original leaves in the final merge
    order = [link[-1, 0], link[-1, 1]]
    while max(order) >= n:  # any entry still a merged cluster? expand it
        expanded = []
        for item in order:
            if item < n:
                expanded.append(item)
            else:
                row = link[item - n]
                expanded.extend([row[0], row[1]])
        order = expanded
    return order


def _cluster_var(cov: np.ndarray, items: list[int]) -> float:
    """Variance of an inverse-variance-weighted sub-portfolio over `items` —
    the risk each cluster contributes, used to split capital between siblings."""
    sub = cov[np.ix_(items, items)]
    w = inverse_variance_weights(sub)
    return float(w @ sub @ w)


def _rec_bipart(cov: np.ndarray, order: list[int]) -> np.ndarray:
    """Recursive bisection: start every asset at weight 1, repeatedly split each
    contiguous cluster in the quasi-diagonal order into two halves and scale them
    by 1 − (var share) so the lower-variance half gets more capital."""
    w = np.ones(cov.shape[0])
    clusters = [order]
    while clusters:
        clusters = [
            c[j:k]
            for c in clusters
            if len(c) > 1
            for j, k in ((0, len(c) // 2), (len(c) // 2, len(c)))
        ]
        for i in range(0, len(clusters), 2):
            left, right = clusters[i], clusters[i + 1]
            v_left, v_right = _cluster_var(cov, left), _cluster_var(cov, right)
            alpha = 1.0 - v_left / (v_left + v_right)
            w[left] *= alpha
            w[right] *= 1.0 - alpha
    return w


def hrp_weights(cov: np.ndarray) -> np.ndarray:
    """Hierarchical Risk Parity weights: cluster → quasi-diagonalize → recursive
    bisection. Long-only, sums to 1, and never inverts Σ. A single asset returns
    weight 1; two assets reduce to inverse-variance."""
    n = cov.shape[0]
    if n == 1:
        return np.ones(1)
    corr = _cov_to_corr(cov)
    dist = np.sqrt(np.clip((1.0 - corr) / 2.0, 0.0, 1.0))  # correlation distance
    np.fill_diagonal(dist, 0.0)
    link = linkage(squareform(dist, checks=False), method="single")
    order = _quasi_diag(link)
    return _rec_bipart(cov, order)
