"""QR5.4 — map the meta-model's P(profitable) to a bet size / gate.

The primary model (QR4's s-score) already chose the side; the meta-model
(QR5.3) says how confident to be. This turns that P into the size of each bet
and re-emits the book in the SAME `weights_YYYYMMDD.csv` format the C++ engine
consumes (QR4.5), so meta-sizing is a drop-in the A/B audit (QR5.5) can toggle:

  off   size = 1 for every bet  → reproduces the raw QR4.5 book exactly (the
        A/B baseline)
  gate  size = 1 if P ≥ floor else 0  → skip low-confidence bets
  size  size = clip((P − floor)/(1 − floor), 0, 1)  → ramp from 0 at the floor
        to full size at P = 1 (confidence-weighted)

The meta-decision is made at entry and held for the life of the position, so a
per-event size is propagated over its whole run in the QR4.5 position frame. The
sized (now fractional) positions are made dollar-neutral by allocating each side
in proportion to its sizes — which collapses to QR4.5's equal weighting when all
sizes are 1, guaranteeing meta-off == baseline.
"""

import argparse
import sys
from pathlib import Path

import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "statarb"))

from meta_model import pooled_oos_proba, purged_cpcv_splits, train_cpcv  # noqa: E402
from signals import write_weight_files  # noqa: E402
from triple_barrier import extract_entry_events  # noqa: E402


def size_from_proba(proba: np.ndarray, mode: str = "size", floor: float = 0.5) -> np.ndarray:
    """Per-event bet size in [0, 1] from P(profitable)."""
    proba = np.asarray(proba, dtype=float)
    if mode == "off":
        return np.ones_like(proba)
    if mode == "gate":
        return (proba >= floor).astype(float)
    if mode == "size":
        return np.clip((proba - floor) / (1.0 - floor), 0.0, 1.0)
    raise ValueError(f"unknown mode {mode!r}; expected off | gate | size")


def apply_meta_sizes(positions: pd.DataFrame, events: pd.DataFrame) -> pd.DataFrame:
    """Scale each held position run by its entry event's size. `events` has
    columns name, t0 (entry date), size. Returns fractional signed positions —
    a bar not covered by any event keeps its raw ±1 (full size), so entries we
    could not meta-score are taken at full size rather than dropped."""
    factor = pd.DataFrame(
        (positions != 0).astype(float).to_numpy(),  # default full size on held bars
        index=positions.index,
        columns=positions.columns,
    )
    pos_of = {d: i for i, d in enumerate(positions.index)}
    col_of = {c: i for i, c in enumerate(positions.columns)}
    arr = positions.to_numpy()
    fac = factor.to_numpy()
    n = len(positions)
    for _, ev in events.iterrows():
        j = col_of[ev["name"]]
        i0 = pos_of[ev["t0"]]
        s = arr[i0, j]
        if s == 0:
            continue
        i = i0
        while i < n and arr[i, j] == s:  # the contiguous held run
            fac[i, j] = ev["size"]
            i += 1
    return positions * pd.DataFrame(fac, index=positions.index, columns=positions.columns)


def dollar_neutral_from_sizes(sized: pd.DataFrame, gross: float = 1.0) -> pd.DataFrame:
    """Dollar-neutral weights from fractional signed positions: each side is
    allocated in proportion to its sizes and scaled to ±gross/2 (net 0, gross
    cap). A one-sided day cannot be neutralized → flat. Uniform sizes reproduce
    QR4.5's equal weighting exactly."""
    arr = sized.to_numpy(dtype=float)
    out = np.zeros_like(arr)
    for i in range(arr.shape[0]):
        row = arr[i]
        longs = row > 0
        shorts = row < 0
        long_sum = row[longs].sum()
        short_sum = -row[shorts].sum()
        if long_sum > 0 and short_sum > 0:
            out[i, longs] = row[longs] / long_sum * (gross / 2.0)
            out[i, shorts] = row[shorts] / short_sum * (gross / 2.0)
    return pd.DataFrame(out, index=sized.index, columns=sized.columns)


def meta_weights(
    positions: pd.DataFrame,
    events: pd.DataFrame | None,
    mode: str,
    floor: float = 0.5,
    gross: float = 1.0,
) -> pd.DataFrame:
    """Full pipeline: per-event sizes → sized positions → dollar-neutral weights.
    `events` (name, t0, proba) is required for gate/size; ignored for off."""
    if mode == "off" or events is None:
        sized = apply_meta_sizes(positions, _all_entries(positions, size=1.0))
    else:
        ev = events.copy()
        ev["size"] = size_from_proba(ev["proba"].to_numpy(), mode, floor)
        sized = apply_meta_sizes(positions, ev)
    return dollar_neutral_from_sizes(sized, gross)


def _all_entries(positions: pd.DataFrame, size: float) -> pd.DataFrame:
    rows = []
    for name in positions.columns:
        for t0 in extract_entry_events(positions[name]).index:
            rows.append({"name": name, "t0": t0, "size": size})
    return pd.DataFrame(rows)


# ---------------------------------------------------------------------------
# Driver: emit weight files for a mode (reads the QR5.3 meta_dataset)
# ---------------------------------------------------------------------------

FEATURES = ["abs_sscore", "sscore", "kappa", "regime", "vol_21", "vol_5", "vol_ratio", "dow"]


def event_probabilities(dataset: pd.DataFrame, n_bars: int, n_groups: int, k: int, embargo: float):
    """Leak-free P(profitable) per event: pooled purged-CPCV OOS predictions."""
    X = dataset[FEATURES].to_numpy()
    y = dataset["label"].to_numpy()
    w = dataset["weight"].to_numpy()
    splits = purged_cpcv_splits(
        dataset["t0_idx"].to_numpy(), dataset["t1_idx"].to_numpy(), n_groups, k, embargo, n_bars
    )
    results = train_cpcv(X, y, splits, sample_weight=w, l2=1.0)
    return pooled_oos_proba(results, len(dataset))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--data-dir", type=Path, default=Path("data/universe"))
    parser.add_argument("--mode", choices=["off", "gate", "size"], default="size")
    parser.add_argument("--floor", type=float, default=0.5)
    parser.add_argument("--gross", type=float, default=1.0)
    parser.add_argument("--execution-lag", type=int, default=1)
    parser.add_argument("--n-groups", type=int, default=6)
    parser.add_argument("--k", type=int, default=2)
    parser.add_argument("--embargo-pct", type=float, default=0.01)
    args = parser.parse_args()

    positions = pd.read_parquet(args.data_dir / "signal_positions.parquet")
    events = None
    if args.mode != "off":
        ds = pd.read_parquet(args.data_dir / "meta_dataset.parquet")
        ds["proba"] = event_probabilities(
            ds, len(positions), args.n_groups, args.k, args.embargo_pct
        )
        events = ds[["name", "t0", "proba"]]

    weights = meta_weights(positions, events, args.mode, args.floor, args.gross)
    weights_dir = args.data_dir / f"weights_meta_{args.mode}"
    written = write_weight_files(weights, weights_dir, args.execution_lag)
    net = weights.sum(axis=1).abs().max()
    active = int((weights.abs().sum(axis=1) > 0).sum())
    print(
        f"meta '{args.mode}' (floor {args.floor}): {len(written)} weight files, "
        f"max|net| {net:.1e}, {active} active days -> {weights_dir}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
