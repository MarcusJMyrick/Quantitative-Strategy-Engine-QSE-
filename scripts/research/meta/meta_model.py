"""QR5.3 — Meta-model trained and validated under Combinatorial Purged CV.

The whole track converges here. QR4's s-score decides the *side*; this classifier
predicts **P(the primary bet is profitable)** from features that never encode the
direction of returns — s-score magnitude, regime state (QR3), recent vol, a
daily order-flow proxy — trained on the QR5.1 triple-barrier labels, weighted by
QR5.2 sample uniqueness, and validated **only** through purged CPCV so no leakage
can inflate it.

The meta-CPCV composes the QR2 machinery on the *events'* information windows:
events (each with a bar window [t0, t1] from QR5.1) are ordered by entry time and
partitioned into N groups; every k-subset of groups is a test fold (C(N,k)
splits); the training complement is **purged** (QR2.1 — drop train events whose
window overlaps any test event's window) and **embargoed** on the bar axis (drop
train events whose entry falls just after a test window). QR2.1's own embargo
assumes observation-index == bar-index, which is false for events with custom
windows, so the bar-space embargo lives here.

A dependency-free weighted logistic regression is the "simple, not a deep net"
classifier; gradient-boosted trees are a drop-in upgrade behind the same harness.
"""

import itertools
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "validation"))

from cpcv import num_paths, num_splits, partition_groups  # noqa: E402,F401
from purge_embargo import purge  # noqa: E402


# ---------------------------------------------------------------------------
# Weighted logistic regression (numpy; standardizes internally per fit)
# ---------------------------------------------------------------------------


def _sigmoid(z: np.ndarray) -> np.ndarray:
    return 1.0 / (1.0 + np.exp(-np.clip(z, -30.0, 30.0)))


class LogisticRegression:
    """L2-regularized logistic regression with per-sample weights, fit by
    gradient descent. Features are standardized using the training moments, so
    each CPCV fold scales from its own train set (no leakage)."""

    def __init__(self, l2: float = 1.0, lr: float = 0.3, n_iter: int = 800, seed: int = 0):
        self.l2 = l2
        self.lr = lr
        self.n_iter = n_iter
        self.seed = seed

    def fit(self, X: np.ndarray, y: np.ndarray, sample_weight: np.ndarray | None = None):
        X = np.asarray(X, dtype=float)
        y = np.asarray(y, dtype=float)
        self.mu_ = X.mean(axis=0)
        self.sd_ = X.std(axis=0)
        self.sd_[self.sd_ == 0] = 1.0
        Xs = (X - self.mu_) / self.sd_

        n, d = Xs.shape
        w = np.zeros(d)
        b = 0.0
        sw = np.ones(n) if sample_weight is None else np.asarray(sample_weight, dtype=float)
        sw = sw * (n / sw.sum()) if sw.sum() > 0 else sw  # normalize to mean 1

        for _ in range(self.n_iter):
            p = _sigmoid(Xs @ w + b)
            err = sw * (p - y)
            w -= self.lr * (Xs.T @ err / n + self.l2 * w)
            b -= self.lr * (err.sum() / n)
        self.coef_ = w
        self.intercept_ = b
        return self

    def predict_proba(self, X: np.ndarray) -> np.ndarray:
        Xs = (np.asarray(X, dtype=float) - self.mu_) / self.sd_
        return _sigmoid(Xs @ self.coef_ + self.intercept_)


# ---------------------------------------------------------------------------
# Combinatorial Purged CV over event windows
# ---------------------------------------------------------------------------


def _bar_embargo(
    train: np.ndarray,
    test: np.ndarray,
    starts: np.ndarray,
    ends: np.ndarray,
    n_bars: int,
    embargo_pct: float,
) -> np.ndarray:
    """Drop train events whose entry bar falls in the embargo zone — the
    `int(n_bars·embargo_pct)` bars immediately after any test event's window end
    (serial-correlation guard, on the BAR axis)."""
    size = int(n_bars * embargo_pct)
    if size <= 0 or len(test) == 0:
        return train
    banned = np.zeros(n_bars + size + 1, dtype=bool)
    for t in test:
        lo, hi = int(ends[t]) + 1, int(ends[t]) + size
        banned[lo : hi + 1] = True
    return np.array([j for j in train if not banned[int(starts[j])]], dtype=int)


def purged_cpcv_splits(
    starts: np.ndarray,
    ends: np.ndarray,
    n_groups: int,
    k: int,
    embargo_pct: float = 0.0,
    n_bars: int | None = None,
) -> list[tuple[np.ndarray, np.ndarray]]:
    """C(N, k) CPCV splits over events. Events are ordered by entry time and
    partitioned into `n_groups` contiguous groups; each k-subset is a test fold
    with a purged + bar-embargoed training complement. Returns
    [(train_event_idx, test_event_idx)]."""
    starts = np.asarray(starts, dtype=int)
    ends = np.asarray(ends, dtype=int)
    n_events = len(starts)
    if not 1 <= k < n_groups:
        raise ValueError(f"need 1 <= k < n_groups; got k={k}, n_groups={n_groups}")
    if n_bars is None:
        n_bars = int(ends.max()) + 1 if n_events else 0

    order = np.argsort(starts, kind="stable")  # events in entry-time order
    groups = [order[g] for g in partition_groups(n_events, n_groups)]  # time-contiguous

    splits = []
    for combo in itertools.combinations(range(n_groups), k):
        test = np.sort(np.concatenate([groups[g] for g in combo]))
        train = np.setdiff1d(np.arange(n_events), test, assume_unique=True)
        train = purge(train, test, starts, ends)
        train = _bar_embargo(train, test, starts, ends, n_bars, embargo_pct)
        splits.append((train, test))
    return splits


def pooled_oos_proba(results: list[dict], n_events: int) -> np.ndarray:
    """One out-of-sample P per event: the mean over the CPCV splits that tested
    it (each event is tested C(N−1,k−1) times). NaN if never tested."""
    total = np.zeros(n_events)
    count = np.zeros(n_events)
    for r in results:
        total[r["test"]] += r["proba"]
        count[r["test"]] += 1
    with np.errstate(invalid="ignore", divide="ignore"):
        return np.where(count > 0, total / count, np.nan)


def train_cpcv(
    X: np.ndarray,
    y: np.ndarray,
    splits: list[tuple[np.ndarray, np.ndarray]],
    sample_weight: np.ndarray | None = None,
    seed: int = 0,
    **model_kwargs,
) -> list[dict]:
    """Fit a fresh model on each split's purged train set and predict its test
    events. Returns [{split, test, proba}] — the per-split out-of-sample
    predictions QR5.5 recombines into CPCV paths."""
    X = np.asarray(X, dtype=float)
    y = np.asarray(y, dtype=float)
    results = []
    for sid, (train, test) in enumerate(splits):
        sw = None if sample_weight is None else np.asarray(sample_weight)[train]
        model = LogisticRegression(seed=seed, **model_kwargs).fit(X[train], y[train], sw)
        results.append({"split": sid, "test": test, "proba": model.predict_proba(X[test])})
    return results
