"""Unit tests for scripts/research/meta/triple_barrier.py (QR5.1).

Done-when: barrier-touch labeling on hand-built price paths — up-first (profit),
down-first (stop), and timeout.
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "research" / "meta"))

from triple_barrier import (  # noqa: E402
    apply_triple_barrier,
    extract_entry_events,
    label_windows,
)

DAYS = pd.bdate_range("2024-01-01", periods=40)


def path(values):
    return pd.Series(values, index=DAYS[: len(values)])


def one_event(t0, side):
    return pd.DataFrame({"side": [side]}, index=[t0])


# ---------------------------------------------------------------------------
# Done-when: up-first / down-first / timeout
# ---------------------------------------------------------------------------


def test_up_first_hits_profit_take():
    # long, price rises to +5% before any −5% → profit-take, win
    prices = path([100, 102, 105, 103, 100])
    out = apply_triple_barrier(prices, one_event(DAYS[0], +1), pt=0.05, sl=0.05, max_holding=4)
    r = out.iloc[0]
    assert r["barrier"] == "pt"
    assert r["label"] == 1
    assert r["t1_idx"] == 2  # touched at bar 2 (105)
    assert r["ret"] == pytest.approx(0.05)


def test_down_first_hits_stop():
    # long, price falls to −5% first → stop, loss
    prices = path([100, 99, 95, 101, 106])
    out = apply_triple_barrier(prices, one_event(DAYS[0], +1), pt=0.05, sl=0.05, max_holding=4)
    r = out.iloc[0]
    assert r["barrier"] == "sl"
    assert r["label"] == 0
    assert r["t1_idx"] == 2  # touched at bar 2 (95), before the later rally


def test_timeout_profitable_and_loss():
    # neither barrier within the horizon; label by the sign at the vertical barrier
    up = apply_triple_barrier(
        path([100, 101, 102, 103]), one_event(DAYS[0], +1), pt=0.10, sl=0.10, max_holding=3
    ).iloc[0]
    assert up["barrier"] == "time" and up["label"] == 1 and up["t1_idx"] == 3

    down = apply_triple_barrier(
        path([100, 99, 98, 97]), one_event(DAYS[0], +1), pt=0.10, sl=0.10, max_holding=3
    ).iloc[0]
    assert down["barrier"] == "time" and down["label"] == 0


# ---------------------------------------------------------------------------
# Short side + first-touch precedence
# ---------------------------------------------------------------------------


def test_short_side_profits_when_price_falls():
    # short: a falling price is profitable → profit-take, win
    prices = path([100, 98, 95, 97])
    r = apply_triple_barrier(prices, one_event(DAYS[0], -1), pt=0.05, sl=0.05, max_holding=3).iloc[
        0
    ]
    assert r["barrier"] == "pt" and r["label"] == 1 and r["ret"] == pytest.approx(0.05)


def test_short_side_stops_when_price_rises():
    prices = path([100, 103, 106, 100])
    r = apply_triple_barrier(prices, one_event(DAYS[0], -1), pt=0.05, sl=0.05, max_holding=3).iloc[
        0
    ]
    assert r["barrier"] == "sl" and r["label"] == 0


def test_first_touch_wins_when_both_would_hit():
    # dips to the stop first, then would have hit the profit-take later
    prices = path([100, 94, 108])
    r = apply_triple_barrier(prices, one_event(DAYS[0], +1), pt=0.05, sl=0.05, max_holding=2).iloc[
        0
    ]
    assert r["barrier"] == "sl" and r["t1_idx"] == 1


def test_exact_threshold_touches():
    prices = path([100, 105])  # exactly +5%
    r = apply_triple_barrier(prices, one_event(DAYS[0], +1), pt=0.05, sl=0.05, max_holding=1).iloc[
        0
    ]
    assert r["barrier"] == "pt" and r["label"] == 1


# ---------------------------------------------------------------------------
# Horizon clamping + validation
# ---------------------------------------------------------------------------


def test_horizon_clamps_at_series_end():
    # max_holding reaches past the series end → vertical barrier at the last bar
    prices = path([100, 101, 102])
    r = apply_triple_barrier(prices, one_event(DAYS[0], +1), pt=0.5, sl=0.5, max_holding=10).iloc[0]
    assert r["barrier"] == "time" and r["t1_idx"] == 2  # clamped to the last available bar


def test_invalid_params_raise():
    prices = path([100, 101])
    with pytest.raises(ValueError):
        apply_triple_barrier(prices, one_event(DAYS[0], +1), pt=0.0, sl=0.05, max_holding=1)
    with pytest.raises(ValueError):
        apply_triple_barrier(prices, one_event(DAYS[0], +1), pt=0.05, sl=0.05, max_holding=0)


# ---------------------------------------------------------------------------
# Entry-event extraction + info windows (the QR2.1 / QR5.2 handoff)
# ---------------------------------------------------------------------------


def test_extract_entry_events_opens_flips_not_holds():
    pos = pd.Series([0, 1, 1, 0, -1, -1, 1], index=DAYS[:7])
    ev = extract_entry_events(pos)
    # opens at bar 1 (long), 4 (short), and a flip at bar 6 (short→long); holds skipped
    assert list(ev.index) == [DAYS[1], DAYS[4], DAYS[6]]
    assert list(ev["side"]) == [1, -1, 1]


def test_label_windows_are_the_information_intervals():
    prices = path([100, 102, 105, 95, 100, 100, 100])
    events = pd.DataFrame({"side": [1, 1]}, index=[DAYS[0], DAYS[3]])
    out = apply_triple_barrier(prices, events, pt=0.05, sl=0.05, max_holding=3)
    starts, ends = label_windows(out, n=len(prices))
    assert np.array_equal(starts, out["t0_idx"].to_numpy())
    assert np.array_equal(ends, out["t1_idx"].to_numpy())
    assert (ends >= starts).all()  # a window never ends before it starts


def test_empty_events_returns_empty_frame():
    out = apply_triple_barrier(path([100, 101]), pd.DataFrame({"side": []}), 0.05, 0.05, 1)
    assert out.empty and "label" in out.columns
