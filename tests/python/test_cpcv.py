"""Unit tests for scripts/research/validation/cpcv.py (QR2.2).

Done-when coverage: the correct number of splits and paths for small (N, k),
and every observation appears in the test set the expected number of times.
"""

import itertools
import sys
from math import comb
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "research" / "validation"))

from cpcv import (  # noqa: E402
    assemble_paths,
    cpcv_splits,
    num_paths,
    num_splits,
    partition_groups,
    path_assignments,
)


# ---------------------------------------------------------------------------
# Done-when #1: split and path counts
# ---------------------------------------------------------------------------


def test_split_and_path_counts_small_cases():
    # N=6, k=2 (AFML's worked example): 15 splits, 5 paths
    assert num_splits(6, 2) == 15 == comb(6, 2)
    assert num_paths(6, 2) == 5 == comb(5, 1)
    # a couple more
    assert num_splits(10, 3) == comb(10, 3)  # 120
    assert num_paths(10, 3) == comb(9, 2)  # 36
    assert num_paths(5, 1) == 1  # k=1 -> plain purged k-fold, one path


def test_cpcv_splits_produce_c_n_k_splits():
    splits, groups = cpcv_splits(n=120, n_groups=6, k=2)
    assert len(splits) == num_splits(6, 2)
    assert len(groups) == 6
    assert sum(len(g) for g in groups) == 120
    for s in splits:
        assert len(s.test_groups) == 2
        # test set is exactly the union of the two test groups
        expected = np.sort(np.concatenate([groups[g] for g in s.test_groups]))
        assert np.array_equal(s.test_idx, expected)


# ---------------------------------------------------------------------------
# Done-when #2: every observation tested exactly phi times
# ---------------------------------------------------------------------------


@pytest.mark.parametrize("n_groups,k", [(6, 2), (8, 3), (5, 2), (7, 1)])
def test_every_observation_tested_phi_times(n_groups, k):
    n = 200
    splits, groups = cpcv_splits(n, n_groups, k)
    counts = np.zeros(n, dtype=int)
    for s in splits:
        counts[s.test_idx] += 1
    phi = num_paths(n_groups, k)
    assert np.all(counts == phi)  # every bar tested exactly phi times


def test_each_group_tested_in_phi_splits():
    n_groups, k = 6, 2
    splits, _ = cpcv_splits(200, n_groups, k)
    phi = num_paths(n_groups, k)
    for g in range(n_groups):
        appearances = sum(1 for s in splits if g in s.test_groups)
        assert appearances == phi


# ---------------------------------------------------------------------------
# Train/test integrity: disjoint, purged, embargoed
# ---------------------------------------------------------------------------


def test_train_test_disjoint_and_purged():
    # holding=3 so labels overlap at block boundaries -> purge must bite
    splits, groups = cpcv_splits(n=180, n_groups=6, k=2, embargo_pct=0.02, holding=3)
    for s in splits:
        train, test = set(s.train_idx), set(s.test_idx)
        assert train.isdisjoint(test)
        # no train bar within `holding` of a test bar (would overlap the label window)
        test_arr = np.array(sorted(test))
        for j in s.train_idx:
            assert not np.any(np.abs(test_arr - j) <= 3 - 0)  # |j - t| <= holding => overlap
        # train is a subset of the non-test complement (purge/embargo only remove)
        assert train.issubset(set(range(180)) - test)


def test_k_must_be_less_than_n_groups():
    with pytest.raises(ValueError):
        cpcv_splits(100, n_groups=5, k=5)
    with pytest.raises(ValueError):
        partition_groups(10, 20)  # more groups than samples


# ---------------------------------------------------------------------------
# Path assignment + reconstruction
# ---------------------------------------------------------------------------


def test_path_assignments_tile_the_splits():
    n_groups, k = 6, 2
    paths = path_assignments(n_groups, k)
    assert len(paths) == num_paths(n_groups, k)
    # every path covers all N groups exactly once
    for assign in paths:
        assert set(assign.keys()) == set(range(n_groups))
    # every (split, test-group) pair used exactly once across all paths
    combos = list(itertools.combinations(range(n_groups), k))
    used = [(sid, g) for assign in paths for g, sid in assign.items()]
    assert len(used) == len(set(used))  # no pair reused
    assert len(used) == num_paths(n_groups, k) * n_groups
    # and each used split actually tests that group
    for sid, g in used:
        assert g in combos[sid]


def test_assemble_paths_full_coverage_and_sourcing():
    n, n_groups, k = 60, 6, 2
    splits, groups = cpcv_splits(n, n_groups, k)
    # each split "predicts" its own split_id on its test bars -> we can trace sourcing
    per_split = {s.split_id: np.full(n, np.nan) for s in splits}
    for s in splits:
        per_split[s.split_id][s.test_idx] = s.split_id
    paths = assemble_paths(groups, per_split, n_groups, k)

    assert len(paths) == num_paths(n_groups, k)
    for series in paths:
        assert not np.isnan(series).any()  # full-length, every bar filled
    # each bar's value is a split that actually tested that bar's group
    combos = list(itertools.combinations(range(n_groups), k))
    group_of = np.empty(n, dtype=int)
    for g, idx in enumerate(groups):
        group_of[idx] = g
    for series in paths:
        for bar in range(n):
            sid = int(series[bar])
            assert group_of[bar] in combos[sid]


def test_paths_are_distinct_out_of_sample_curves():
    # sanity: with real per-split values the phi paths are genuinely different
    n, n_groups, k = 90, 6, 2
    splits, groups = cpcv_splits(n, n_groups, k)
    rng = np.random.default_rng(0)
    per_split = {s.split_id: rng.normal(size=n) for s in splits}
    paths = assemble_paths(groups, per_split, n_groups, k)
    stacked = np.vstack(paths)
    # not all paths identical (they draw from different splits per group)
    assert not np.allclose(stacked, stacked[0])
