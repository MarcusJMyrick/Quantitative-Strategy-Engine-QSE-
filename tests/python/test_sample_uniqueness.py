"""Unit tests for scripts/research/meta/sample_uniqueness.py (QR5.2).

Done-when: heavily-overlapping samples receive lower weight than isolated ones.
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "research" / "meta"))

from sample_uniqueness import (  # noqa: E402
    average_uniqueness,
    concurrency,
    sample_weights,
    sequential_bootstrap,
    weights_for_labels,
)


# ---------------------------------------------------------------------------
# Done-when: overlapping < isolated
# ---------------------------------------------------------------------------


def test_overlapping_samples_get_lower_weight():
    # event 0 is isolated [0,2]; events 1,2,3 are a triple-overlap cluster [10,20]
    starts = np.array([0, 10, 10, 10])
    ends = np.array([2, 20, 20, 20])
    u = average_uniqueness(starts, ends, n_bars=30)
    assert u[0] == pytest.approx(1.0)  # isolated → fully unique
    assert np.allclose(u[1:], 1 / 3)  # each cluster member shares with 2 others
    assert (u[0] > u[1:]).all()  # the done-when


def test_identical_and_partial_overlap():
    # two identical windows → each 1/2
    u = average_uniqueness([5, 5], [10, 10], n_bars=20)
    assert np.allclose(u, 0.5)
    # partial overlap sits between full-overlap (0.5) and isolated (1.0)
    # A=[0,10], B=[5,15]: they share bars 5..10 (c=2) and are alone elsewhere (c=1)
    u2 = average_uniqueness([0, 5], [10, 15], n_bars=20)
    assert (0.5 < u2).all() and (u2 < 1.0).all()


# ---------------------------------------------------------------------------
# Concurrency + uniqueness bounds
# ---------------------------------------------------------------------------


def test_concurrency_counts_active_windows():
    c = concurrency([0, 2], [3, 5], n_bars=6)
    # window A spans 0..3, B spans 2..5; overlap at 2,3
    assert list(c) == [1, 1, 2, 2, 1, 1]


def test_average_uniqueness_in_unit_interval():
    rng = np.random.default_rng(0)
    starts = rng.integers(0, 90, size=50)
    ends = starts + rng.integers(1, 15, size=50)
    u = average_uniqueness(starts, ends, n_bars=int(ends.max()) + 1)
    assert (u > 0).all() and (u <= 1.0 + 1e-12).all()


def test_average_uniqueness_infers_n_bars():
    u_explicit = average_uniqueness([0, 5], [4, 9], n_bars=10)
    u_inferred = average_uniqueness([0, 5], [4, 9])  # n_bars = max end + 1
    assert np.allclose(u_explicit, u_inferred)


# ---------------------------------------------------------------------------
# sample_weights: return-scaling + normalization
# ---------------------------------------------------------------------------


def test_sample_weights_normalized_to_average_one():
    w = sample_weights([0, 10, 10, 10], [2, 20, 20, 20], n_bars=30)
    assert w.mean() == pytest.approx(1.0)
    assert w[0] > w[1]  # isolated still weighted more after normalization


def test_return_attribution_scales_weight():
    # equal uniqueness, but the bigger |return| carries more weight
    starts, ends = [0, 10], [2, 12]  # both isolated → ū = 1
    w = sample_weights(starts, ends, n_bars=20, returns=[0.01, 0.05], normalize=False)
    assert w[1] == pytest.approx(5 * w[0])


# ---------------------------------------------------------------------------
# Sequential bootstrap: draws the unique event more than uniform
# ---------------------------------------------------------------------------


def test_sequential_bootstrap_over_samples_unique_events():
    # one isolated event + a cluster of 5 that all overlap
    starts = np.array([0, 50, 50, 50, 50, 50])
    ends = np.array([1, 60, 60, 60, 60, 60])
    draws = sequential_bootstrap(starts, ends, n_bars=61, size=6000, seed=1)
    iso_freq = (draws == 0).mean()
    # under uniform sampling the isolated event would be 1/6 ≈ 0.167; sequential
    # bootstrap favours it because the cluster members lose uniqueness fast
    assert iso_freq > 0.25


def test_sequential_bootstrap_shape_and_range():
    d = sequential_bootstrap([0, 5, 10], [3, 8, 13], n_bars=14, size=20, seed=2)
    assert len(d) == 20
    assert set(np.unique(d)).issubset({0, 1, 2})


# ---------------------------------------------------------------------------
# Labels-frame convenience + grouping (the QR5.1 → QR5.3 handoff)
# ---------------------------------------------------------------------------


def test_weights_for_labels_uses_windows():
    labels = pd.DataFrame(
        {"t0_idx": [0, 10, 10], "t1_idx": [2, 20, 20], "ret": [0.03, 0.02, 0.02]},
        index=["a", "b", "c"],
    )
    w = weights_for_labels(labels, n_bars=30)
    assert w.loc["a"] > w.loc["b"]  # isolated 'a' weighted above the overlapping pair
    assert w.mean() == pytest.approx(1.0)  # normalized


def test_weights_for_labels_handles_duplicate_index():
    # a pooled multi-name frame repeats the same t0 date across names → the index
    # is not unique; weighting must still assign positionally (regression test)
    labels = pd.DataFrame(
        {
            "t0_idx": [0, 0, 10, 10],
            "t1_idx": [3, 3, 20, 20],
            "ret": [0.02, 0.02, 0.02, 0.02],
            "name": ["AAA", "BBB", "AAA", "BBB"],
        },
        index=pd.to_datetime(["2024-01-01", "2024-01-01", "2024-01-15", "2024-01-15"]),
    )
    w = weights_for_labels(labels, n_bars=30, group_col="name")
    assert len(w) == 4 and w.mean() == pytest.approx(1.0)
    assert not w.isna().any()


def test_grouping_isolates_concurrency_per_name():
    # same time window on two different names: not concurrent when grouped by name
    labels = pd.DataFrame(
        {
            "t0_idx": [0, 0],
            "t1_idx": [10, 10],
            "ret": [0.02, 0.02],
            "name": ["AAA", "BBB"],
        },
        index=["x", "y"],
    )
    pooled = weights_for_labels(labels, n_bars=20)  # treated as concurrent → each 1/2
    grouped = weights_for_labels(labels, n_bars=20, group_col="name")  # each unique
    # grouping removes the cross-name "overlap", so both are fully unique (equal,
    # normalized to 1.0); pooled also equal but from a lower raw uniqueness
    assert np.allclose(grouped.to_numpy(), 1.0)
    assert np.allclose(pooled.to_numpy(), 1.0)  # both normalize to 1, but...
    # ...the raw (unnormalized) uniqueness differs: grouped 1.0 vs pooled 0.5
    raw_pooled = average_uniqueness(labels["t0_idx"], labels["t1_idx"], 20)
    assert np.allclose(raw_pooled, 0.5)
