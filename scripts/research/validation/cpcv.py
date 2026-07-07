"""QR2.2 — Combinatorial Purged Cross-Validation (CPCV).

Standard k-fold gives one out-of-sample estimate; CPCV (AFML ch. 12) gives a
*distribution*. Partition the series into N contiguous groups, use every
k-subset of groups as the test set — C(N, k) splits — and purge+embargo the
training complement of each (QR2.1). Because each group is tested in
C(N-1, k-1) different splits, the per-split test predictions recombine into
C(N-1, k-1) full-length **backtest paths**, each a complete out-of-sample
equity curve. The spread of Sharpes across those paths is exactly the variance
QR2.4's Deflated Sharpe consumes.

Counts (the invariants QR2.2 is verified against):
  splits = C(N, k)                 one per k-subset of groups
  paths  = C(N-1, k-1) = phi       times each group (hence each bar) is tested
  every observation appears in the test set of exactly phi splits.

The path assignment tiles the C(N, k) splits into phi paths x N groups: for each
group, its phi testing-splits are handed out one per path, so every
(split, test-group) pair is used exactly once and every path covers all N
groups exactly once.
"""

import itertools
from dataclasses import dataclass
from math import comb

import numpy as np

from purge_embargo import default_windows, purged_train_indices


@dataclass(frozen=True)
class CPCVSplit:
    """One CPCV split: k groups tested, the purged/embargoed remainder trained."""

    split_id: int
    test_groups: tuple[int, ...]
    test_idx: np.ndarray
    train_idx: np.ndarray


def num_splits(n_groups: int, k: int) -> int:
    return comb(n_groups, k)


def num_paths(n_groups: int, k: int) -> int:
    """phi = C(N-1, k-1): how many times each group is tested = number of paths."""
    return comb(n_groups - 1, k - 1)


def partition_groups(n: int, n_groups: int) -> list[np.ndarray]:
    """N contiguous, near-equal groups of bar indices (time order preserved)."""
    if not 1 <= n_groups <= n:
        raise ValueError(f"n_groups must be in [1, n]; got {n_groups} for n={n}")
    return [g for g in np.array_split(np.arange(n), n_groups)]


def cpcv_splits(
    n: int, n_groups: int, k: int, embargo_pct: float = 0.0, holding: int = 0
) -> tuple[list[CPCVSplit], list[np.ndarray]]:
    """Every k-of-N test combination with its purged+embargoed training set.
    Returns (splits, groups)."""
    if not 1 <= k < n_groups:
        raise ValueError(f"need 1 <= k < n_groups; got k={k}, n_groups={n_groups}")
    groups = partition_groups(n, n_groups)
    starts, ends = default_windows(n, holding)
    splits = []
    for sid, combo in enumerate(itertools.combinations(range(n_groups), k)):
        test_idx = np.sort(np.concatenate([groups[g] for g in combo]))
        train_idx = purged_train_indices(n, test_idx, starts, ends, embargo_pct)
        splits.append(CPCVSplit(sid, combo, test_idx, train_idx))
    return splits, groups


def path_assignments(n_groups: int, k: int) -> list[dict[int, int]]:
    """Tile the splits into phi paths. Each path is {group -> split_id}: the
    split whose test prediction supplies that group's slice of the path."""
    combos = list(itertools.combinations(range(n_groups), k))
    per_group = {g: [sid for sid, c in enumerate(combos) if g in c] for g in range(n_groups)}
    phi = num_paths(n_groups, k)
    return [{g: per_group[g][j] for g in range(n_groups)} for j in range(phi)]


def assemble_paths(
    groups: list[np.ndarray],
    per_split_test_values: dict[int, np.ndarray] | list[np.ndarray],
    n_groups: int,
    k: int,
) -> list[np.ndarray]:
    """Recombine per-split test predictions into phi full-length backtest paths.

    `per_split_test_values[sid]` is a full-length array carrying split `sid`'s
    predictions on its test bars (values elsewhere are ignored). Each returned
    path is a length-n array with every bar filled exactly once, from the split
    assigned to that bar's group."""
    n = sum(len(g) for g in groups)
    paths = []
    for assign in path_assignments(n_groups, k):
        series = np.full(n, np.nan)
        for g, sid in assign.items():
            idx = groups[g]
            series[idx] = np.asarray(per_split_test_values[sid])[idx]
        paths.append(series)
    return paths
