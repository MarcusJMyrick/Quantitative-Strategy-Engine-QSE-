"""Unit tests for scripts/research/meta/meta_sizing.py (QR5.4).

Done-when: the meta-layer produces the same weight-file format the engine
consumes, and a mode flag toggles meta-sizing on/off (off == the raw QR4.5 book).
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "research" / "meta"))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "research" / "statarb"))

from meta_sizing import (  # noqa: E402
    apply_meta_sizes,
    dollar_neutral_from_sizes,
    meta_weights,
    size_from_proba,
)
from signals import weights_from_positions, write_weight_files  # noqa: E402

DAYS = pd.bdate_range("2024-01-01", periods=12)


# ---------------------------------------------------------------------------
# P → size mapping
# ---------------------------------------------------------------------------


def test_size_modes():
    p = np.array([0.3, 0.5, 0.6, 0.9, 1.0])
    assert np.array_equal(size_from_proba(p, "off"), np.ones(5))
    assert np.array_equal(size_from_proba(p, "gate", floor=0.6), [0, 0, 1, 1, 1])
    sz = size_from_proba(p, "size", floor=0.5)
    assert sz[0] == 0.0 and sz[1] == 0.0  # at/below floor → 0
    assert sz[2] == pytest.approx(0.2) and sz[4] == pytest.approx(1.0)  # ramp to 1
    assert np.all((sz >= 0) & (sz <= 1))


def test_unknown_mode_raises():
    with pytest.raises(ValueError):
        size_from_proba(np.array([0.5]), "bogus")


# ---------------------------------------------------------------------------
# Held-run propagation
# ---------------------------------------------------------------------------


def test_size_applied_over_the_whole_held_run():
    # a long held for 3 bars then flat; the entry's size scales the whole run
    pos = pd.DataFrame({"A": [0, 1, 1, 1, 0, 0]}, index=DAYS[:6])
    events = pd.DataFrame({"name": ["A"], "t0": [DAYS[1]], "size": [0.4]})
    sized = apply_meta_sizes(pos, events)
    assert list(sized["A"]) == [0.0, 0.4, 0.4, 0.4, 0.0, 0.0]


def test_uncovered_entries_keep_full_size():
    # two runs, only the first has a meta size; the second stays full (±1)
    pos = pd.DataFrame({"A": [1, 1, 0, -1, -1, 0]}, index=DAYS[:6])
    events = pd.DataFrame({"name": ["A"], "t0": [DAYS[0]], "size": [0.5]})
    sized = apply_meta_sizes(pos, events)
    assert list(sized["A"]) == [0.5, 0.5, 0.0, -1.0, -1.0, 0.0]


# ---------------------------------------------------------------------------
# Dollar-neutral from sizes + the meta-off == QR4.5 guarantee
# ---------------------------------------------------------------------------


def test_dollar_neutral_from_sizes_nets_zero():
    sized = pd.DataFrame(
        {"A": [0.8, 0.0], "B": [0.2, 1.0], "C": [-0.5, -1.0], "D": [0.0, 0.0]},
        index=DAYS[:2],
    )
    w = dollar_neutral_from_sizes(sized, gross=1.0)
    assert w.sum(axis=1).abs().max() < 1e-12  # net 0
    active = w.abs().sum(axis=1)
    assert active.iloc[0] == pytest.approx(1.0)  # gross cap when two-sided
    # sizes split the long side proportionally: A got 0.8/(0.8+0.2)=80% of +0.5
    assert w.loc[DAYS[0], "A"] == pytest.approx(0.4)
    assert w.loc[DAYS[0], "B"] == pytest.approx(0.1)


def test_meta_off_reproduces_raw_qr45_book():
    rng = np.random.default_rng(0)
    dates = pd.bdate_range("2024-01-01", periods=20)
    pos = pd.DataFrame(rng.choice([-1, 0, 1], size=(20, 5)), index=dates, columns=list("ABCDE"))
    off = meta_weights(pos, events=None, mode="off", gross=1.0)
    baseline = weights_from_positions(pos, gross=1.0)
    pd.testing.assert_frame_equal(off, baseline, check_dtype=False)


def test_gate_and_size_change_the_book():
    pos = pd.DataFrame({"A": [1, 1, 0], "B": [-1, -1, 0]}, index=DAYS[:3])
    events = pd.DataFrame({"name": ["A", "B"], "t0": [DAYS[0], DAYS[0]], "proba": [0.9, 0.3]})
    # gate at 0.5: B (P=0.3) is skipped → one-sided → flat (can't neutralize)
    gated = meta_weights(pos, events, mode="gate", floor=0.5)
    assert gated.abs().sum(axis=1).iloc[0] == pytest.approx(0.0)
    # size: both above the floor (A confident, B weaker) → two-sided, net 0, and
    # the confident long carries more weight than the weaker short
    ev2 = events.assign(proba=[0.9, 0.6])
    sized = meta_weights(pos, ev2, mode="size", floor=0.5)
    assert sized.sum(axis=1).abs().max() < 1e-12
    assert abs(sized.loc[DAYS[0], "A"]) == pytest.approx(0.5)  # long side = gross/2
    assert abs(sized.loc[DAYS[0], "B"]) == pytest.approx(0.5)  # short side = gross/2


# ---------------------------------------------------------------------------
# Weight-file format (the C++ handoff, same as QR4.5)
# ---------------------------------------------------------------------------


def test_emitted_files_match_the_loader_format(tmp_path):
    pos = pd.DataFrame({"A": [1, 1, 0], "B": [-1, -1, 0], "C": [0, 0, 1]}, index=DAYS[:3])
    w = meta_weights(pos, events=None, mode="off")
    written = write_weight_files(w, tmp_path, execution_lag=1)
    assert len(written) == 2
    for _, path in written:
        lines = path.read_text().splitlines()
        assert lines[0] == "symbol,weight"
        net = sum(float(line.split(",")[1]) for line in lines[1:])
        for line in lines[1:]:
            assert abs(float(line.split(",")[1])) <= 10.0  # loader cap
        assert abs(net) < 1e-9  # dollar-neutral
