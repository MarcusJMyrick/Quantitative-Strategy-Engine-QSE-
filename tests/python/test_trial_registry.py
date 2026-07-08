"""Unit tests for scripts/research/validation/trial_registry.py (QR2.3).

Done-when coverage: running a parameter sweep populates a registry directory,
and a loader reconstructs {params -> return series} for all trials.
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "research" / "validation"))

from trial_registry import (  # noqa: E402
    TrialRegistry,
    annualized_sharpe,
    run_sweep,
    trial_id,
)

DATES = pd.bdate_range("2020-01-01", periods=250)


def rand_returns(seed: int) -> pd.Series:
    return pd.Series(np.random.default_rng(seed).normal(0.0004, 0.01, 250), index=DATES)


# ---------------------------------------------------------------------------
# Done-when: a sweep populates the registry; the loader reconstructs everything
# ---------------------------------------------------------------------------


def test_sweep_populates_and_reloads(tmp_path):
    reg = TrialRegistry(tmp_path)
    grid = [{"band": b, "window": w} for b in (1.0, 1.25, 1.5) for w in (40, 60)]  # 6 trials

    def run_fn(params):
        # deterministic per-params return series
        return rand_returns(seed=int(params["band"] * 100) + params["window"])

    ids = run_sweep(reg, grid, run_fn)
    assert len(ids) == 6
    assert len(reg) == 6
    assert list(tmp_path.glob("*.params.json"))  # files on disk
    assert list(tmp_path.glob("*.returns.parquet"))

    # loader reconstructs {params -> return series} exactly
    trials = {tuple(sorted(t.params.items())): t for t in reg.load_all()}
    assert len(trials) == 6
    for params in grid:
        key = tuple(sorted(params.items()))
        assert key in trials
        expected = run_fn(params)
        pd.testing.assert_series_equal(
            trials[key].returns.reset_index(drop=True),
            expected.reset_index(drop=True),
            check_names=False,
        )


def test_return_series_round_trips_with_index(tmp_path):
    reg = TrialRegistry(tmp_path)
    r = rand_returns(1)
    tid = reg.log({"k": 3}, r)
    loaded = reg.load(tid).returns
    np.testing.assert_allclose(loaded.to_numpy(), r.to_numpy())
    # the datetime index survives the parquet round-trip
    assert list(pd.to_datetime(loaded.index)) == list(r.index)


# ---------------------------------------------------------------------------
# Idempotency: identical params must not inflate the trial count
# ---------------------------------------------------------------------------


def test_logging_same_params_is_idempotent(tmp_path):
    reg = TrialRegistry(tmp_path)
    r = rand_returns(2)
    id1 = reg.log({"band": 1.25, "window": 60}, r)
    id2 = reg.log({"band": 1.25, "window": 60}, r)  # same params -> same id, overwrite
    assert id1 == id2
    assert len(reg) == 1  # not inflated


def test_trial_id_depends_only_on_params():
    # order-independent, value-sensitive
    assert trial_id({"a": 1, "b": 2}) == trial_id({"b": 2, "a": 1})
    assert trial_id({"a": 1}) != trial_id({"a": 2})


def test_different_params_are_distinct_trials(tmp_path):
    reg = TrialRegistry(tmp_path)
    reg.log({"band": 1.25}, rand_returns(3))
    reg.log({"band": 1.50}, rand_returns(4))
    assert len(reg) == 2


# ---------------------------------------------------------------------------
# Sharpe convenience (the dispersion input for QR2.4's DSR)
# ---------------------------------------------------------------------------


def test_sharpes_map_matches_recomputation(tmp_path):
    reg = TrialRegistry(tmp_path)
    ids = [reg.log({"i": i}, rand_returns(10 + i)) for i in range(5)]
    stored = reg.sharpes()
    assert set(stored.keys()) == set(ids)
    for t in reg.load_all():
        assert stored[t.trial_id] == pytest.approx(annualized_sharpe(t.returns))


def test_annualized_sharpe_degenerate_is_zero():
    assert annualized_sharpe(pd.Series([0.01])) == 0.0  # < 2 points
    assert annualized_sharpe(pd.Series([0.01, 0.01, 0.01])) == 0.0  # zero vol
