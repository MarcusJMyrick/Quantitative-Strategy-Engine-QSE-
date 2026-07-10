"""Unit tests for QR5.5 — judging the meta-layer (Engine B + DSR + MDA).

Done-when: meta-on vs meta-off compared under Engine B with a DSR for both, and
MDA feature importance under purged CV that ranks which features carried signal.
The pure functions are tested on synthetic curves/data; the real end-to-end
numbers live in docs/research/meta/qr5_judge_summary.md.
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(_ROOT / "scripts" / "research" / "meta"))
sys.path.insert(0, str(_ROOT / "scripts" / "research" / "validation"))
sys.path.insert(0, str(_ROOT / "scripts" / "research" / "statarb"))

from judge_meta import (  # noqa: E402
    deflated_verdict,
    engine_b_table,
    meta_search_family,
    net_returns,
)
from meta_model import mda_importance, purged_cpcv_splits  # noqa: E402


# ---------------------------------------------------------------------------
# Lens 1 — Engine B equity curves -> net returns / Sharpe table
# ---------------------------------------------------------------------------


def _write_curve(path: Path, equity: list[float]):
    path.parent.mkdir(parents=True, exist_ok=True)
    ts = pd.to_datetime(pd.bdate_range("2024-01-01", periods=len(equity))).astype("int64") // 10**6
    pd.DataFrame({"timestamp": ts, "equity": equity}).to_csv(path, index=False)


def test_net_returns_from_depth_curve(tmp_path):
    _write_curve(tmp_path / "meta_off" / "depth_1_equity.csv", [100.0, 110.0, 121.0])
    r = net_returns(tmp_path, "meta_off", 1)
    assert list(r.round(3)) == [0.1, 0.1]  # 10% each step


def test_engine_b_table_ranks_modes(tmp_path):
    # meta_off: steady up (high Sharpe); meta_gate: choppy (low Sharpe)
    _write_curve(tmp_path / "meta_off" / "depth_1_equity.csv", [100, 101, 102, 103, 104, 105])
    _write_curve(tmp_path / "meta_gate" / "depth_1_equity.csv", [100, 103, 99, 104, 98, 101])
    _write_curve(tmp_path / "meta_size" / "depth_1_equity.csv", [100, 102, 99, 103, 100, 102])
    tbl = engine_b_table(tmp_path, [1]).set_index("mode")
    assert tbl.loc["meta_off", "net_sharpe"] > tbl.loc["meta_gate", "net_sharpe"]
    assert tbl.loc["meta_off", "net_pnl"] == pytest.approx(5.0)
    # missing sizes are skipped, not errored
    assert set(engine_b_table(tmp_path, [1, 999])["size"]) == {1}


# ---------------------------------------------------------------------------
# Lens 2 — meta search family + deflated verdict
# ---------------------------------------------------------------------------


def _toy_book(n=60, seed=0):
    rng = np.random.default_rng(seed)
    dates = pd.bdate_range("2024-01-01", periods=n)
    names = list("ABCD")
    pos = pd.DataFrame(rng.choice([-1, 0, 1], size=(n, 4)), index=dates, columns=names)
    rets = pd.DataFrame(rng.normal(0, 0.01, size=(n, 4)), index=dates, columns=names)
    # one event per held run, moderate probabilities
    rows = []
    for name in names:
        run = pos[name]
        prev = 0
        for t, v in run.items():
            if v != 0 and v != prev:
                rows.append({"name": name, "t0": t, "proba": float(rng.uniform(0.4, 0.7))})
            prev = v
    return pos, pd.DataFrame(rows), rets


def test_meta_search_family_has_off_plus_grid():
    pos, events, rets = _toy_book()
    fam = meta_search_family(pos, events, rets, floors=[0.50, 0.55])
    assert "meta_off" in fam
    # gate/size × 2 floors + off = 5 configs
    assert len(fam) == 5
    assert {"gate@0.50", "gate@0.55", "size@0.50", "size@0.55"} <= set(fam)
    for series in fam.values():
        assert series.index.equals(rets.index)


def test_deflated_verdict_reports_dsr_for_both():
    pos, events, rets = _toy_book()
    fam = meta_search_family(pos, events, rets, floors=[0.50, 0.55, 0.60])
    v = deflated_verdict(fam)
    assert v["best_meta"] != "meta_off" and v["best_meta"] in fam
    for key in ("off_dsr", "on_dsr", "off_psr0", "on_psr0", "sr_star_null"):
        assert np.isfinite(v[key])
    assert 0.0 <= v["off_dsr"] <= 1.0 and 0.0 <= v["on_dsr"] <= 1.0
    # best_meta is the max-Sharpe meta config by construction
    meta_only = {k: s for k, s in v["per_period"].items() if k != "meta_off"}
    assert v["best_meta"] == max(meta_only, key=meta_only.get)


# ---------------------------------------------------------------------------
# Lens 3 — MDA feature importance under purged CV
# ---------------------------------------------------------------------------


def test_mda_ranks_informative_feature_above_noise():
    rng = np.random.default_rng(1)
    n = 400
    signal = rng.normal(size=n)
    y = (signal > 0).astype(float)  # feature 0 perfectly separates
    noise1 = rng.normal(size=n)
    noise2 = rng.normal(size=n)
    X = np.column_stack([signal, noise1, noise2])
    starts = np.arange(n)
    ends = starts  # zero-length windows (no purging needed for this synthetic)
    splits = purged_cpcv_splits(starts, ends, n_groups=5, k=1)
    imp = mda_importance(X, y, splits, ["signal", "noise1", "noise2"], n_repeats=5, l2=0.01)

    by = {r["feature"]: r["importance"] for r in imp}
    assert imp[0]["feature"] == "signal"  # sorted desc -> most important first
    assert by["signal"] > 0.1  # permuting the real feature hurts a lot
    assert by["signal"] > by["noise1"] and by["signal"] > by["noise2"]
    assert abs(by["noise1"]) < 0.05 and abs(by["noise2"]) < 0.05  # noise ~ 0


def test_mda_noise_only_is_near_zero():
    rng = np.random.default_rng(2)
    n = 300
    X = rng.normal(size=(n, 3))
    y = rng.integers(0, 2, size=n).astype(float)  # unrelated labels
    splits = purged_cpcv_splits(np.arange(n), np.arange(n), n_groups=4, k=1)
    imp = mda_importance(X, y, splits, ["a", "b", "c"], n_repeats=5)
    assert all(abs(r["importance"]) < 0.06 for r in imp)  # nothing carries signal
