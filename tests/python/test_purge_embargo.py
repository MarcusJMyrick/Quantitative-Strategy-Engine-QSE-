"""Unit tests for scripts/research/validation/purge_embargo.py (QR2.1).

Done-when coverage: no training index's information window overlaps any test
window after purging, and the embargo removes exactly the configured fraction.
"""

import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "research" / "validation"))

from purge_embargo import (  # noqa: E402
    default_windows,
    embargo,
    embargo_size,
    purge,
    purged_train_indices,
    windows_overlap,
)


# ---------------------------------------------------------------------------
# Done-when #1: purging leaves no train/test window overlap
# ---------------------------------------------------------------------------


def test_no_overlap_after_purge_multibar_labels():
    n, holding = 100, 5  # each label spans 5 bars forward -> real overlap risk
    starts, ends = default_windows(n, holding)
    test = np.arange(40, 50)
    train = purged_train_indices(n, test, starts, ends)

    # brute-force: every surviving train window must be disjoint from every test window
    for j in train:
        for t in test:
            assert not windows_overlap(starts[j], ends[j], starts[t], ends[t])


def test_purge_drops_the_boundary_neighbours():
    # holding=3: train samples in [37,39] reach forward into test bar 40+, and
    # test labels reach back, so both sides of the block get purged
    n, holding = 60, 3
    starts, ends = default_windows(n, holding)
    test = np.arange(40, 45)
    train = set(purged_train_indices(n, test, starts, ends))
    assert 39 not in train and 38 not in train and 37 not in train  # lead-in purged
    assert 36 in train  # far enough back to be safe (36..39 vs test start 40)


def test_one_bar_labels_purge_nothing_extra():
    # holding=0: windows are single bars, so only the test bars themselves are
    # excluded; no neighbour is purged (and with no embargo, train = complement)
    n = 50
    starts, ends = default_windows(n, 0)
    test = np.arange(20, 30)
    train = purged_train_indices(n, test, starts, ends, embargo_pct=0.0)
    assert set(train) == set(range(n)) - set(range(20, 30))


# ---------------------------------------------------------------------------
# Done-when #2: embargo removes exactly the configured fraction
# ---------------------------------------------------------------------------


def test_embargo_removes_exact_fraction():
    n = 100
    starts, ends = default_windows(n, 0)
    test = np.arange(40, 50)  # contiguous block ending at 49
    for pct in (0.05, 0.10, 0.20):
        size = embargo_size(n, pct)
        no_emb = purged_train_indices(n, test, starts, ends, embargo_pct=0.0)
        with_emb = purged_train_indices(n, test, starts, ends, embargo_pct=pct)
        removed = set(no_emb) - set(with_emb)
        assert removed == set(range(50, 50 + size))  # exactly the block after test
        assert len(removed) == size


def test_embargo_size_formula():
    assert embargo_size(1000, 0.01) == 10
    assert embargo_size(1432, 0.02) == 28
    assert embargo_size(100, 0.0) == 0


def test_embargo_zero_removes_nothing():
    n = 80
    starts, ends = default_windows(n, 0)
    test = np.arange(30, 40)
    a = purged_train_indices(n, test, starts, ends, embargo_pct=0.0)
    assert set(a) == set(range(n)) - set(range(30, 40))


def test_embargo_clipped_at_series_end():
    # test block near the end: the embargo zone runs off the array and is clipped
    n = 100
    starts, ends = default_windows(n, 0)
    test = np.arange(93, 98)  # ends at 97; embargo of 10 would reach 107
    no_emb = purged_train_indices(n, test, starts, ends, embargo_pct=0.0)
    with_emb = purged_train_indices(n, test, starts, ends, embargo_pct=0.10)
    removed = set(no_emb) - set(with_emb)
    assert removed == {98, 99}  # only bars that exist


# ---------------------------------------------------------------------------
# Composition + multi-block test sets (CPCV feeds unions of blocks)
# ---------------------------------------------------------------------------


def test_purge_and_embargo_compose():
    n, holding, pct = 200, 4, 0.05
    starts, ends = default_windows(n, holding)
    test = np.arange(80, 100)
    train = purged_train_indices(n, test, starts, ends, embargo_pct=pct)
    train_set = set(train)
    # nothing in the test set, nothing overlapping, nothing embargoed
    assert train_set.isdisjoint(set(test))
    for j in train:
        assert not np.any((starts[j] <= ends[test]) & (starts[test] <= ends[j]))
    size = embargo_size(n, pct)  # embargo after end-of-block window (99+holding)
    assert train_set.isdisjoint(set(range(100 + holding, 100 + holding + size)))


def test_two_test_blocks_each_purged_and_embargoed():
    n, pct = 300, 0.02
    starts, ends = default_windows(n, 0)
    test = np.concatenate([np.arange(50, 60), np.arange(200, 210)])
    train = set(purged_train_indices(n, test, starts, ends, embargo_pct=pct))
    size = embargo_size(n, pct)
    assert train.isdisjoint(set(range(60, 60 + size)))  # after block 1
    assert train.isdisjoint(set(range(210, 210 + size)))  # after block 2
    assert train.isdisjoint(set(test))


def test_purge_and_embargo_direct_helpers():
    # the lower-level purge/embargo compose to the same thing as the wrapper
    n, holding, pct = 120, 2, 0.05
    starts, ends = default_windows(n, holding)
    test = np.arange(40, 50)
    all_train = np.setdiff1d(np.arange(n), test)
    manual = embargo(purge(all_train, test, starts, ends), test, ends, n, pct)
    wrapped = purged_train_indices(n, test, starts, ends, embargo_pct=pct)
    assert set(manual) == set(wrapped)


def test_windows_overlap_truth_table():
    assert windows_overlap(0, 5, 5, 10)  # touch at 5
    assert windows_overlap(0, 5, 3, 4)  # contained
    assert not windows_overlap(0, 5, 6, 10)  # disjoint
    assert not windows_overlap(6, 10, 0, 5)  # disjoint, reversed


def test_empty_test_returns_all():
    n = 30
    starts, ends = default_windows(n, 0)
    train = purged_train_indices(n, np.array([], dtype=int), starts, ends, embargo_pct=0.1)
    assert set(train) == set(range(n))
