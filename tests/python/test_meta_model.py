"""Unit tests for scripts/research/meta/meta_model.py (QR5.3).

Done-when: the model trains through the purged CPCV harness, and no train/test
fold overlaps — neither the event sets nor their information windows.
"""

import sys
from pathlib import Path

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "research" / "meta"))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "research" / "validation"))

from meta_model import (  # noqa: E402
    LogisticRegression,
    num_splits,
    purged_cpcv_splits,
    train_cpcv,
)
from purge_embargo import windows_overlap  # noqa: E402


def synthetic_events(n=120, span=8, n_bars=400, seed=0):
    """n events with entry bars spread over [0, n_bars) and `span`-bar windows."""
    rng = np.random.default_rng(seed)
    starts = np.sort(rng.integers(0, n_bars - span, size=n))
    ends = starts + span
    return starts, ends


# ---------------------------------------------------------------------------
# Weighted logistic regression
# ---------------------------------------------------------------------------


def test_logreg_learns_a_separable_pattern():
    rng = np.random.default_rng(0)
    X = rng.normal(size=(400, 3))
    y = (X[:, 0] + 0.5 * X[:, 1] > 0).astype(float)  # linearly separable-ish
    model = LogisticRegression(l2=0.01).fit(X, y)
    acc = ((model.predict_proba(X) > 0.5) == y).mean()
    assert acc > 0.9


def test_logreg_probabilities_in_unit_interval():
    rng = np.random.default_rng(1)
    X = rng.normal(size=(50, 4))
    p = LogisticRegression().fit(X, rng.integers(0, 2, 50).astype(float)).predict_proba(X)
    assert (p >= 0).all() and (p <= 1).all()


def test_sample_weights_shift_the_fit():
    # duplicate one class heavily via weights → decision boundary moves toward it
    X = np.array([[-1.0], [-1.0], [1.0], [1.0]])
    y = np.array([0.0, 0.0, 1.0, 1.0])
    base = LogisticRegression(n_iter=2000).fit(X, y).predict_proba([[0.0]])[0]
    up = (
        LogisticRegression(n_iter=2000)
        .fit(X, y, sample_weight=[10, 10, 1, 1])
        .predict_proba([[0.0]])[0]
    )
    assert up < base  # weighting the negative class pushes P(+) at 0 down


# ---------------------------------------------------------------------------
# Done-when: purged CPCV has no train/test overlap in any fold
# ---------------------------------------------------------------------------


@pytest.mark.parametrize("n_groups,k", [(6, 2), (8, 3), (5, 2)])
def test_no_train_test_overlap_in_any_fold(n_groups, k):
    starts, ends = synthetic_events()
    splits = purged_cpcv_splits(starts, ends, n_groups, k, embargo_pct=0.02, n_bars=400)
    assert len(splits) == num_splits(n_groups, k)
    for train, test in splits:
        train_set, test_set = set(train.tolist()), set(test.tolist())
        assert train_set.isdisjoint(test_set)  # event sets disjoint
        # and — the real guarantee — no train WINDOW overlaps a test window
        for j in train:
            for t in test:
                assert not windows_overlap(starts[j], ends[j], starts[t], ends[t])


def test_every_event_tested_the_expected_number_of_times():
    starts, ends = synthetic_events(seed=3)
    n_groups, k = 6, 2
    splits = purged_cpcv_splits(starts, ends, n_groups, k, n_bars=400)
    counts = np.zeros(len(starts), dtype=int)
    for _, test in splits:
        counts[test] += 1
    # every event lands in the test set of exactly C(N-1, k-1) splits
    from meta_model import num_paths

    assert np.all(counts == num_paths(n_groups, k))


def test_embargo_drops_train_after_test_windows():
    # one test window, generous embargo → the immediately-following entries drop
    starts = np.array([0, 10, 20, 30, 40, 50, 60, 70, 80, 90])
    ends = starts + 3
    splits = purged_cpcv_splits(starts, ends, n_groups=5, k=1, embargo_pct=0.1, n_bars=100)
    for train, test in splits:
        for t in test:
            embargo_zone = range(ends[t] + 1, ends[t] + int(100 * 0.1) + 1)
            assert not any(starts[j] in embargo_zone for j in train)


def test_k_out_of_range_raises():
    starts, ends = synthetic_events()
    with pytest.raises(ValueError):
        purged_cpcv_splits(starts, ends, n_groups=5, k=5, n_bars=400)


# ---------------------------------------------------------------------------
# Training through the harness end to end
# ---------------------------------------------------------------------------


def test_train_cpcv_trains_and_predicts_every_test_event():
    rng = np.random.default_rng(4)
    starts, ends = synthetic_events(n=100, seed=4)
    n = len(starts)
    X = rng.normal(size=(n, 3))
    y = (X[:, 0] > 0).astype(float)
    weights = rng.uniform(0.3, 1.0, n)
    splits = purged_cpcv_splits(starts, ends, n_groups=6, k=2, embargo_pct=0.02, n_bars=400)

    results = train_cpcv(X, y, splits, sample_weight=weights, l2=0.1)
    assert len(results) == len(splits)
    for r, (_, test) in zip(results, splits):
        assert np.array_equal(r["test"], test)
        assert len(r["proba"]) == len(test)
        assert (r["proba"] >= 0).all() and (r["proba"] <= 1).all()


def test_train_cpcv_is_deterministic():
    rng = np.random.default_rng(5)
    starts, ends = synthetic_events(n=80, seed=5)
    X = rng.normal(size=(len(starts), 2))
    y = rng.integers(0, 2, len(starts)).astype(float)
    splits = purged_cpcv_splits(starts, ends, n_groups=5, k=2, n_bars=400)
    a = train_cpcv(X, y, splits, seed=0)
    b = train_cpcv(X, y, splits, seed=0)
    for ra, rb in zip(a, b):
        assert np.allclose(ra["proba"], rb["proba"])
