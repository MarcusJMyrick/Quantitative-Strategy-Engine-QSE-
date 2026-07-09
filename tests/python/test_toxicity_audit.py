"""Unit tests for scripts/analysis/toxicity_audit.py (QR1.3 analysis).

The C++ toxicity_audit tool runs the blind-vs-filtered execution on real ticks;
this covers the analysis that turns its per-order slippage into the verdict.
"""

import sys
from pathlib import Path

import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "analysis"))

from toxicity_audit import analyze, write_summary  # noqa: E402


def make_orders(rows):
    """rows: list of (rested, filled_passive, filtered_slip). blind is always
    the half-spread 0.01; arrival_mid fixed at 200 for bps."""
    return pd.DataFrame(
        {
            "tick": range(len(rows)),
            "side": ["BUY"] * len(rows),
            "arrival_mid": [200.0] * len(rows),
            "vpin": [0.7] * len(rows),
            "ofi": [-1.0] * len(rows),
            "rested": [r[0] for r in rows],
            "filled_passive": [r[1] for r in rows],
            "blind_slip": [0.01] * len(rows),
            "filtered_slip": [r[2] for r in rows],
        }
    )


def test_reduction_positive_when_filter_helps():
    # all rested orders fill passively (−0.01), non-rested cross at 0.01
    df = make_orders([(0, 0, 0.01), (1, 1, -0.01), (1, 1, -0.01), (0, 0, 0.01)])
    m = analyze(df)
    assert m["blind_slip"] == pytest.approx(0.01)
    assert m["filtered_slip"] < m["blind_slip"]  # filter helped
    assert m["reduction"] > 0
    assert m["reduction_bps"] == pytest.approx(m["reduction"] / 200.0 * 1e4)


def test_reduction_negative_when_adverse_tail_dominates():
    # one passive capture (−0.01) vs one catastrophic fallback (+0.5)
    df = make_orders([(1, 1, -0.01), (1, 0, 0.5)])
    m = analyze(df)
    assert m["reduction"] < 0  # filter hurts
    assert m["passive_fills"] == 1
    assert m["rested"] == 2
    assert m["avg_fallback_slip"] == pytest.approx(0.5)


def test_decomposition_counts():
    df = make_orders([(0, 0, 0.01), (1, 1, -0.01), (1, 0, 0.3), (1, 1, -0.01)])
    m = analyze(df)
    assert m["orders"] == 4
    assert m["rested"] == 3
    assert m["passive_fills"] == 2
    assert m["passive_fill_rate"] == pytest.approx(2 / 3)
    assert m["avg_passive_slip"] == pytest.approx(-0.01)


def test_summary_states_the_verdict(tmp_path):
    losing = make_orders([(1, 1, -0.01), (1, 0, 0.5)])
    out = tmp_path / "s.md"
    write_summary(analyze(losing), out)
    text = out.read_text()
    assert "does not" in text  # honest negative verdict
    assert "adverse selection" in text.lower()
    assert "QR-P2" in text  # config-search deflation caveat

    winning = make_orders([(1, 1, -0.01), (1, 1, -0.01)])
    write_summary(analyze(winning), out)
    assert "earns its place" in out.read_text()
