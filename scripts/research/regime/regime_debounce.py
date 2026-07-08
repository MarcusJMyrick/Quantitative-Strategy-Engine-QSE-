"""QR3.3 — Anti-whipsaw: debounce the filtered HMM regime into a committed state.

The QR3.2 filtered states flip-flop between adjacent regimes; feeding that
straight into the A5 risk-aversion lambda (QR3.4) would churn the book on noise.
This module debounces the raw state into a *committed* regime that only changes
on a PERSISTENT shift, so lambda moves rarely and deliberately.

The rule is an N-bar confirmation (minimum dwell) with an optional filtered-
probability floor (hysteresis): a new raw state becomes the committed regime
only after it has been the argmax for `min_dwell` consecutive bars, each with
filtered probability >= `min_prob`. A one-bar blip never reaches the
confirmation count, so it cannot move lambda; a sustained change does (after a
`min_dwell`-bar lag — the deliberate cost of not whipsawing).

Strictly causal / streaming: committed[t] depends only on raw states and
probabilities at bars <= t, so appending future data never rewrites the past
(tested) — the same no-look-ahead contract as QR3.1/QR3.2. Larger `min_dwell`
trades responsiveness for fewer switches; the reduction is reported.

Output: data/regime/regime_committed.parquet (adds committed_state to the
QR3.2 columns) + data/regime/regime_debounce_manifest.json.
"""

import argparse
import json
import logging
import sys
from datetime import date
from pathlib import Path

import numpy as np
import pandas as pd

logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")
logger = logging.getLogger(__name__)


def debounce_states(
    states: np.ndarray,
    min_dwell: int = 10,
    probs: np.ndarray | None = None,
    min_prob: float = 0.0,
) -> np.ndarray:
    """Committed regime from a raw state series via N-bar confirmation.

    A candidate state is committed only once it has persisted for `min_dwell`
    consecutive bars, each meeting the `min_prob` filtered-probability floor
    (probs is [n, K]; ignored if None). min_dwell=1 with min_prob=0 returns the
    raw states unchanged. Streaming: committed[t] uses only bars <= t."""
    states = np.asarray(states, dtype=int)
    n = len(states)
    if n == 0:
        return states.copy()
    if min_dwell < 1:
        raise ValueError("min_dwell must be >= 1")

    committed = np.empty(n, dtype=int)
    committed[0] = states[0]

    def qualifies(t: int) -> bool:
        return probs is None or probs[t, states[t]] >= min_prob

    qual_run = 1 if qualifies(0) else 0
    for t in range(1, n):
        same = states[t] == states[t - 1]
        if qualifies(t):
            qual_run = qual_run + 1 if same else 1
        else:
            qual_run = 0  # low-confidence bar breaks the confirmation streak

        committed[t] = committed[t - 1]
        if states[t] != committed[t - 1] and qual_run >= min_dwell:
            committed[t] = states[t]
    return committed


def switch_count(series: np.ndarray) -> int:
    """Number of regime changes in a state series."""
    series = np.asarray(series)
    return int((np.diff(series) != 0).sum()) if len(series) > 1 else 0


def apply_debounce(
    regime_states: pd.DataFrame,
    min_dwell: int = 10,
    min_prob: float = 0.0,
) -> pd.DataFrame:
    """Add a `committed_state` column to a QR3.2 regime_states frame (which has
    `state` and `p_state_0..K-1`)."""
    prob_cols = sorted(c for c in regime_states.columns if c.startswith("p_state_"))
    probs = regime_states[prob_cols].to_numpy() if prob_cols else None
    committed = debounce_states(
        regime_states["state"].to_numpy(), min_dwell=min_dwell, probs=probs, min_prob=min_prob
    )
    out = regime_states.copy()
    out["committed_state"] = committed
    return out


def build(
    states_path: Path,
    out_dir: Path,
    min_dwell: int = 10,
    min_prob: float = 0.0,
) -> dict:
    states = pd.read_parquet(states_path)
    out = apply_debounce(states, min_dwell=min_dwell, min_prob=min_prob)
    out_dir.mkdir(parents=True, exist_ok=True)
    out.to_parquet(out_dir / "regime_committed.parquet")

    raw_sw = switch_count(out["state"].to_numpy())
    com_sw = switch_count(out["committed_state"].to_numpy())
    occ = out["committed_state"].value_counts(normalize=True).sort_index()
    manifest = {
        "generated": date.today().isoformat(),
        "min_dwell": min_dwell,
        "min_prob": min_prob,
        "rows": len(out),
        "raw_switches": raw_sw,
        "committed_switches": com_sw,
        "switch_reduction_pct": round(100.0 * (1 - com_sw / raw_sw), 1) if raw_sw else 0.0,
        "committed_occupancy": {int(s): round(float(f), 4) for s, f in occ.items()},
        "as_of_alignment": (
            "committed_state[t] uses only raw states/probs at bars <= t (streaming "
            "confirmation); no future data. Consumers act at >= t+1."
        ),
    }
    (out_dir / "regime_debounce_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    logger.info(
        f"Debounced regime: raw switches {raw_sw} -> committed {com_sw} "
        f"({manifest['switch_reduction_pct']}% fewer), occupancy {manifest['committed_occupancy']}"
    )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--states", type=Path, default=Path("data/regime/regime_states.parquet"))
    parser.add_argument("--out-dir", type=Path, default=Path("data/regime"))
    parser.add_argument("--min-dwell", type=int, default=10)
    parser.add_argument("--min-prob", type=float, default=0.0)
    args = parser.parse_args()
    build(args.states, args.out_dir, args.min_dwell, args.min_prob)
    return 0


if __name__ == "__main__":
    sys.exit(main())
