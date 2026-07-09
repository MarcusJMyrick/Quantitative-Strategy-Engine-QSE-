"""QR5.2 — Sample uniqueness for meta-labeling (López de Prado, AFML ch. 4).

The QR5.1 triple-barrier labels span overlapping information windows [t0, t1]:
two entries a day apart, each held ten bars, share nine bars of outcome, so
their labels are not independent. Training a classifier on them as if they were
IID lets clusters of overlapping (redundant) samples dominate. The fix is to
down-weight each sample by how much of its window it shares with others.

  concurrency c_t     = number of label windows active at bar t
  uniqueness  u_{i,t} = 1 / c_t for a bar t in event i's window (0 otherwise)
  average uniqueness  ū_i = mean over event i's span of 1/c_t  ∈ (0, 1]

An event that never overlaps anything has ū = 1; one buried in a cluster of k
concurrent events has ū ≈ 1/k. `sample_weights` uses ū (optionally scaled by the
event's realized |return|, AFML's return-attribution weighting) as the training
weight, so overlapping samples get less say.

`sequential_bootstrap` is AFML's complement: draw samples one at a time with
probability proportional to their uniqueness *given what's already drawn*, so the
bootstrap sample is more diverse (higher average uniqueness) than a uniform one.

All functions take integer bar positions — exactly the `t0_idx`/`t1_idx` the
QR5.1 `label_windows` emits.
"""

import numpy as np
import pandas as pd


def concurrency(starts: np.ndarray, ends: np.ndarray, n_bars: int) -> np.ndarray:
    """c_t: number of label windows [start, end] (inclusive) active at each bar."""
    starts = np.asarray(starts, dtype=int)
    ends = np.asarray(ends, dtype=int)
    c = np.zeros(n_bars, dtype=float)
    for s, e in zip(starts, ends):
        c[s : e + 1] += 1.0
    return c


def average_uniqueness(
    starts: np.ndarray, ends: np.ndarray, n_bars: int | None = None
) -> np.ndarray:
    """ū_i for each event: the mean of 1/c_t over its window. c_t ≥ 1 on any
    spanned bar (the event counts itself), so this is always in (0, 1]."""
    starts = np.asarray(starts, dtype=int)
    ends = np.asarray(ends, dtype=int)
    if n_bars is None:
        n_bars = int(ends.max()) + 1 if len(ends) else 0
    c = concurrency(starts, ends, n_bars)
    out = np.empty(len(starts))
    for i, (s, e) in enumerate(zip(starts, ends)):
        out[i] = np.mean(1.0 / c[s : e + 1])
    return out


def sample_weights(
    starts: np.ndarray,
    ends: np.ndarray,
    n_bars: int | None = None,
    returns: np.ndarray | None = None,
    normalize: bool = True,
) -> np.ndarray:
    """Training weights from average uniqueness, optionally scaled by |return|
    (return attribution). Normalized to average 1.0 by default so they can drop
    into a classifier's `sample_weight` without changing the effective N."""
    w = average_uniqueness(starts, ends, n_bars)
    if returns is not None:
        w = w * np.abs(np.asarray(returns, dtype=float))
    if normalize and w.sum() > 0:
        w = w * (len(w) / w.sum())
    return w


def sequential_bootstrap(
    starts: np.ndarray,
    ends: np.ndarray,
    n_bars: int | None = None,
    size: int | None = None,
    seed: int = 0,
) -> np.ndarray:
    """AFML sequential bootstrap: draw `size` event indices (with replacement),
    each step favoring events whose window overlaps least with what's already
    drawn — yielding a more-unique (less redundant) sample than uniform."""
    starts = np.asarray(starts, dtype=int)
    ends = np.asarray(ends, dtype=int)
    n = len(starts)
    if n_bars is None:
        n_bars = int(ends.max()) + 1 if len(ends) else 0
    if size is None:
        size = n
    rng = np.random.default_rng(seed)
    occupancy = np.zeros(n_bars, dtype=float)  # concurrency of already-drawn events
    drawn = []
    for _ in range(size):
        avg_u = np.empty(n)
        for i in range(n):
            s, e = starts[i], ends[i]
            # if event i were added its bars would sit at occupancy+1
            avg_u[i] = np.mean(1.0 / (occupancy[s : e + 1] + 1.0))
        j = int(rng.choice(n, p=avg_u / avg_u.sum()))
        drawn.append(j)
        occupancy[starts[j] : ends[j] + 1] += 1.0
    return np.array(drawn, dtype=int)


def weights_for_labels(
    labels: pd.DataFrame, n_bars: int, use_returns: bool = False, group_col: str | None = None
) -> pd.Series:
    """Convenience over a QR5.1 labels frame (`t0_idx`, `t1_idx`, optional `ret`,
    optional group column e.g. `name`). With `group_col`, uniqueness is computed
    within each group on the shared bar axis (overlaps across different names are
    on different residual paths, so they are not concurrent for this purpose)."""
    # assign positionally (boolean masks), so a duplicated index — the same t0
    # date across different names in a pooled frame — is handled correctly
    w = np.empty(len(labels))
    if group_col is None:
        rets = labels["ret"].to_numpy() if use_returns else None
        w[:] = sample_weights(
            labels["t0_idx"].to_numpy(),
            labels["t1_idx"].to_numpy(),
            n_bars,
            returns=rets,
            normalize=False,
        )
    else:
        codes = labels[group_col].to_numpy()
        for grp in pd.unique(codes):
            mask = codes == grp
            g = labels[mask]
            rets = g["ret"].to_numpy() if use_returns else None
            w[mask] = sample_weights(
                g["t0_idx"].to_numpy(),
                g["t1_idx"].to_numpy(),
                n_bars,
                returns=rets,
                normalize=False,
            )
    total = w.sum()
    if total > 0:
        w = w * (len(w) / total)  # normalize the pooled weights to average 1.0
    return pd.Series(w, index=labels.index)
