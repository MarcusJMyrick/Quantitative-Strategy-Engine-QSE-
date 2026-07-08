"""Unit tests for scripts/research/regime/regime_debounce.py (QR3.3).

Done-when centerpiece: a one-bar state blip does NOT trigger a regime (lambda)
change, but a sustained change does. Plus streaming causality and the switch
reduction on noisy input.
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "research" / "regime"))

from regime_debounce import apply_debounce, debounce_states, switch_count  # noqa: E402


# ---------------------------------------------------------------------------
# Done-when: blip suppressed, sustained change committed
# ---------------------------------------------------------------------------


def test_one_bar_blip_does_not_switch():
    raw = np.array([0, 0, 0, 1, 0, 0, 0])  # single-bar excursion to state 1
    committed = debounce_states(raw, min_dwell=3)
    assert np.array_equal(committed, np.zeros(7, dtype=int))  # lambda never moves


def test_sustained_change_switches_after_dwell():
    raw = np.array([0, 0, 0, 1, 1, 1, 1])
    committed = debounce_states(raw, min_dwell=3)
    # commits to 1 exactly when the 1-run reaches length 3 (index 5), lagging by min_dwell-1
    assert np.array_equal(committed, np.array([0, 0, 0, 0, 0, 1, 1]))


def test_alternating_blips_never_confirm():
    raw = np.array([0, 1, 0, 1, 0, 1, 0])  # rapid flip-flop, no 2-run
    committed = debounce_states(raw, min_dwell=2)
    assert np.array_equal(committed, np.zeros(7, dtype=int))


def test_two_bar_run_below_dwell_is_ignored():
    raw = np.array([0, 0, 1, 1, 0, 0])  # a 2-bar visit to state 1, dwell=3
    committed = debounce_states(raw, min_dwell=3)
    assert np.array_equal(committed, np.zeros(6, dtype=int))


# ---------------------------------------------------------------------------
# Dwell edge cases + multi-state
# ---------------------------------------------------------------------------


def test_min_dwell_one_is_identity():
    raw = np.array([0, 1, 0, 2, 2, 1])
    assert np.array_equal(debounce_states(raw, min_dwell=1), raw)


def test_direct_multistate_transition():
    # 0 -> sustained 2 (skipping 1); commits to 2 after the dwell
    raw = np.array([0, 0, 2, 2, 2, 2])
    committed = debounce_states(raw, min_dwell=3)
    assert np.array_equal(committed, np.array([0, 0, 0, 0, 2, 2]))


def test_min_dwell_below_one_raises():
    with pytest.raises(ValueError):
        debounce_states(np.array([0, 1]), min_dwell=0)


# ---------------------------------------------------------------------------
# Probability hysteresis (optional floor)
# ---------------------------------------------------------------------------


def test_low_confidence_bars_do_not_confirm():
    # raw wants to switch to state 1 for 3 bars, but the filtered prob is weak
    raw = np.array([0, 0, 1, 1, 1, 0])
    probs = np.array(
        [
            [0.9, 0.1],
            [0.9, 0.1],
            [0.55, 0.45],  # state 1 but low confidence
            [0.55, 0.45],
            [0.55, 0.45],
            [0.9, 0.1],
        ]
    )
    committed = debounce_states(raw, min_dwell=3, probs=probs, min_prob=0.6)
    assert np.array_equal(committed, np.zeros(6, dtype=int))  # never confident enough


def test_high_confidence_sustained_change_confirms_with_floor():
    raw = np.array([0, 0, 1, 1, 1, 1])
    probs = np.tile([0.2, 0.8], (6, 1))
    probs[:2] = [0.8, 0.2]  # state 0 confident early
    committed = debounce_states(raw, min_dwell=3, probs=probs, min_prob=0.6)
    assert committed[-1] == 1 and committed[0] == 0


# ---------------------------------------------------------------------------
# Causality + switch reduction + frame integration
# ---------------------------------------------------------------------------


def test_debounce_is_streaming_causal():
    rng = np.random.default_rng(0)
    raw = rng.integers(0, 3, size=500)
    full = debounce_states(raw, min_dwell=8)
    prefix = debounce_states(raw[:300], min_dwell=8)
    # appending future bars must not rewrite the committed past
    assert np.array_equal(full[:300], prefix)


def test_switch_reduction_on_noisy_input():
    rng = np.random.default_rng(1)
    # a persistent regime with frequent one-bar blips
    raw = np.zeros(600, dtype=int)
    raw[200:400] = 1
    blip = rng.random(600) < 0.15
    raw = np.where(blip, 1 - raw, raw)  # inject flip-flops
    committed = debounce_states(raw, min_dwell=10)
    assert switch_count(committed) < switch_count(raw) / 3  # far fewer switches
    # the two genuine regime edges survive (0->1 near 200, 1->0 near 400)
    assert switch_count(committed) >= 2


def test_apply_debounce_adds_committed_column():
    n = 100
    df = pd.DataFrame(
        {
            "state": np.r_[np.zeros(50), np.ones(50)].astype(int),
            "p_state_0": np.r_[np.full(50, 0.9), np.full(50, 0.1)],
            "p_state_1": np.r_[np.full(50, 0.1), np.full(50, 0.9)],
        },
        index=pd.bdate_range("2021-01-01", periods=n),
    )
    out = apply_debounce(df, min_dwell=5, min_prob=0.6)
    assert "committed_state" in out.columns
    assert out["committed_state"].iloc[0] == 0 and out["committed_state"].iloc[-1] == 1
    # the switch happens ~5 bars after the raw edge at index 50
    switch_idx = np.where(np.diff(out["committed_state"].to_numpy()) != 0)[0][0]
    assert 50 <= switch_idx <= 55
