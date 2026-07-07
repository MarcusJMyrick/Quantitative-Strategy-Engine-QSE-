"""QR2.1 — Purge + embargo primitives for leak-free cross-validation.

Financial CV leaks the future into the past two ways, and vanilla k-fold
ignores both. This module implements the López de Prado fixes (AFML ch. 7) as
pure functions on integer bar positions, so QR2.2's CPCV and QR5's
triple-barrier labels can both reuse them.

Each observation i carries an information window [start_i, end_i]: the span of
bars whose data determine that observation's label. For a daily strategy with a
one-bar label the window is a single bar (end == start); a triple-barrier label
held h bars spans [i, i+h].

  Purging. Two intervals overlap iff a_start <= b_end AND b_start <= a_end. A
  training observation is dropped ("purged") if its window overlaps ANY test
  observation's window — otherwise the label shares bars across the split and
  the test is not truly out of sample. The check is symmetric, so it also
  catches a test label that reaches back into the training region.

  Embargo. Serial correlation lets information bleed forward even across
  non-overlapping windows, so an additional block of training samples
  immediately AFTER each test region is dropped. The embargo size is a fixed
  fraction of the sample count, embargo_size = int(n * embargo_pct) (AFML's
  convention), applied to the bars following each test observation's window end.

Together: for a given test set, start from every non-test bar, purge the
overlaps, then embargo the forward neighbourhood.
"""

import numpy as np


def default_windows(n: int, holding: int = 0) -> tuple[np.ndarray, np.ndarray]:
    """Information windows for a fixed `holding`-bar label: obs i spans
    [i, i + holding]. holding=0 is the one-bar (daily) label default."""
    starts = np.arange(n)
    return starts, starts + holding


def embargo_size(n: int, embargo_pct: float) -> int:
    """Number of post-test bars to embargo — int(n * embargo_pct), AFML-style."""
    return int(n * embargo_pct)


def purge(train: np.ndarray, test: np.ndarray, starts: np.ndarray, ends: np.ndarray) -> np.ndarray:
    """Drop training indices whose information window overlaps any test window."""
    train = np.asarray(train, dtype=int)
    if len(train) == 0 or len(test) == 0:
        return train
    t_starts, t_ends = starts[test], ends[test]
    keep = []
    for j in train:
        # [starts[j], ends[j]] overlaps [t_starts, t_ends] anywhere?
        if not np.any((starts[j] <= t_ends) & (t_starts <= ends[j])):
            keep.append(j)
    return np.array(keep, dtype=int)


def embargo(
    train: np.ndarray, test: np.ndarray, ends: np.ndarray, n: int, embargo_pct: float
) -> np.ndarray:
    """Drop training indices in the embargo zone: the `embargo_size` bars
    immediately after each test observation's window end."""
    size = embargo_size(n, embargo_pct)
    if size <= 0 or len(test) == 0:
        return np.asarray(train, dtype=int)
    banned = np.zeros(n, dtype=bool)
    for t in test:
        lo = ends[t] + 1
        hi = min(ends[t] + size, n - 1)
        if lo <= hi:
            banned[lo : hi + 1] = True
    return np.array([j for j in train if not banned[j]], dtype=int)


def purged_train_indices(
    n: int,
    test: np.ndarray,
    starts: np.ndarray | None = None,
    ends: np.ndarray | None = None,
    embargo_pct: float = 0.0,
) -> np.ndarray:
    """Leak-free training indices for a given test set: every non-test bar,
    minus purged overlaps, minus the embargoed forward neighbourhood.

    Defaults to one-bar labels (starts == ends == arange(n)); pass explicit
    windows for multi-bar labels (e.g. triple-barrier holds in QR5)."""
    if starts is None or ends is None:
        starts, ends = default_windows(n)
    test = np.asarray(sorted(set(int(t) for t in test)), dtype=int)
    train = np.setdiff1d(np.arange(n), test, assume_unique=True)
    train = purge(train, test, starts, ends)
    train = embargo(train, test, ends, n, embargo_pct)
    return train


def windows_overlap(a_start: int, a_end: int, b_start: int, b_end: int) -> bool:
    """Whether two information windows share any bar."""
    return a_start <= b_end and b_start <= a_end
