"""QR5.1 — Triple-barrier labels for meta-labeling (López de Prado, AFML ch. 3).

The primary model (QR4's s-score) decides the SIDE of each bet; meta-labeling
trains a second model to decide *whether to act and how big*. That needs a
binary label per entry: **would the primary bet have been profitable?**

For each entry event at t0 (entry price p0, side s ∈ {+1 long, −1 short}) three
barriers are placed on the return *in the bet's direction*, `signed = s·(pₜ/p0 −
1)`:

  * upper (profit-take): first bar with signed ≥ `pt`   → label 1 (win)
  * lower (stop-loss):   first bar with signed ≤ −`sl`  → label 0 (loss)
  * vertical (time):     neither within `max_holding` bars → label by the sign
                         of signed at the horizon (profitable → 1, else 0)

Whichever barrier is touched first wins. The touch time `t1` is not just for the
label: `[t0, t1]` is the observation's **information window**, so purging/embargo
(QR2.1) and sample uniqueness (QR5.2) consume the emitted `t0_idx`/`t1_idx`
directly.

`apply_triple_barrier` is generic over the input series — feed raw prices to
label the realized bet, or a residual/cumulative series to label the
idiosyncratic reversion the s-score actually targets (QR5.3's choice).
"""

import numpy as np
import pandas as pd


def extract_entry_events(positions: pd.Series) -> pd.DataFrame:
    """Entry events from a {-1,0,+1} position series for one name: a bar whose
    position is non-zero and differs from the previous bar (a fresh open, or a
    same-bar long↔short flip). Returns a frame indexed by t0 with `side`."""
    prev = positions.shift(1).fillna(0)
    entries = positions[(positions != 0) & (positions != prev)]
    return pd.DataFrame({"side": entries.astype(int)})


def apply_triple_barrier(
    prices: pd.Series,
    events: pd.DataFrame,
    pt: float,
    sl: float,
    max_holding: int,
) -> pd.DataFrame:
    """Label each event by which barrier its path touches first.

    prices: value series indexed by bar/date. events: frame indexed by entry
    times (a subset of `prices.index`) with a `side` column (+1/−1). pt, sl:
    positive take-profit / stop-loss thresholds (fractional return). max_holding:
    vertical-barrier horizon in bars. Returns a frame indexed by t0 with columns
    side, t0_idx, t1, t1_idx, ret, barrier ∈ {pt, sl, time}, label ∈ {0, 1}."""
    if pt <= 0 or sl <= 0:
        raise ValueError("pt and sl must be positive")
    if max_holding < 1:
        raise ValueError("max_holding must be >= 1")

    idx = prices.index
    pos_of = {d: i for i, d in enumerate(idx)}
    vals = prices.to_numpy(dtype=float)
    n = len(vals)

    rows = []
    for t0, ev in events.iterrows():
        s = int(ev["side"])
        i0 = pos_of[t0]
        p0 = vals[i0]
        end = min(i0 + max_holding, n - 1)

        barrier, i1, ret, label = "time", end, None, None
        for i in range(i0 + 1, end + 1):
            signed = s * (vals[i] / p0 - 1.0)
            if signed >= pt:
                barrier, i1, ret, label = "pt", i, signed, 1
                break
            if signed <= -sl:
                barrier, i1, ret, label = "sl", i, signed, 0
                break
        if barrier == "time":
            ret = s * (vals[end] / p0 - 1.0)
            i1 = end
            label = 1 if ret > 0 else 0

        rows.append(
            {
                "t0": t0,
                "side": s,
                "t0_idx": i0,
                "t1": idx[i1],
                "t1_idx": i1,
                "ret": ret,
                "barrier": barrier,
                "label": label,
            }
        )

    cols = ["side", "t0_idx", "t1", "t1_idx", "ret", "barrier", "label"]
    if not rows:
        return pd.DataFrame(columns=cols)
    return pd.DataFrame(rows).set_index("t0")[cols]


def label_windows(labels: pd.DataFrame, n: int) -> tuple[np.ndarray, np.ndarray]:
    """Information windows [start, end] (integer bar positions) for the labelled
    events — the input QR2.1 purge/embargo expects (starts == t0_idx,
    ends == t1_idx). `n` is the total number of bars (for validation)."""
    starts = labels["t0_idx"].to_numpy(dtype=int)
    ends = labels["t1_idx"].to_numpy(dtype=int)
    if len(ends) and (ends.max() >= n or starts.min() < 0):
        raise ValueError("label window out of range")
    return starts, ends
