"""Unit tests for scripts/research/statarb/deflate_qr4.py (QR2.5).

Done-when coverage: the QR4 parameter sweep runs through the registry and the
result carries a DSR line and the number of trials it was deflated against.
Kept small/fast (tiny synthetic universe, the grid is the real one).
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd

_STATARB = Path(__file__).resolve().parents[2] / "scripts" / "research" / "statarb"
_VALID = Path(__file__).resolve().parents[2] / "scripts" / "research" / "validation"
sys.path.insert(0, str(_STATARB))
sys.path.insert(0, str(_VALID))

from deflate_qr4 import (  # noqa: E402
    build_grid,
    block_sharpes,
    deflate,
    run_config,
)
from trial_registry import TrialRegistry  # noqa: E402

BDAYS = pd.bdate_range("2020-01-01", periods=300)


def synthetic_universe(seed=0, t=300, n=8) -> pd.DataFrame:
    """Common factor + mean-reverting idiosyncratic parts, enough for the
    PCA -> residual -> OU -> signal chain to produce trades."""
    rng = np.random.default_rng(seed)
    factor = rng.normal(0, 0.02, t)
    cols = [f"S{i}" for i in range(n)]
    return pd.DataFrame(
        {c: 0.7 * factor + rng.normal(0, 0.012, t) for c in cols}, index=BDAYS[:t], columns=cols
    )


def test_build_grid_covers_bands_and_windows():
    grid = build_grid()
    assert len(grid) == 12  # 4 band sets x 3 windows
    assert all(set(p) == {"bands", "window"} for p in grid)
    assert {p["window"] for p in grid} == {40, 60, 80}
    assert all(len(p["bands"]) == 4 for p in grid)


def test_run_config_returns_a_daily_series():
    returns = synthetic_universe()
    pnl = run_config(returns, {"bands": [-1.25, 1.25, -0.5, 0.75], "window": 60})
    assert isinstance(pnl, pd.Series)
    assert len(pnl) == len(returns)
    assert np.isfinite(pnl.to_numpy()).all()


def test_block_sharpes_returns_phi_paths_worth():
    s = pd.Series(np.random.default_rng(1).normal(0, 0.01, 240))
    blk = block_sharpes(s, n_groups=6, k=2)
    assert len(blk) == 6  # one per CPCV group
    assert all(np.isfinite(blk))


def test_deflate_end_to_end_carries_dsr_and_trial_count(tmp_path):
    returns = synthetic_universe(seed=2)
    reg = TrialRegistry(tmp_path)
    result = deflate(returns, reg)

    # the done-when: a DSR line and the number of trials it was deflated against
    assert result["n_trials"] == 12
    assert len(reg) == 12  # every config logged with its return series
    assert 0.0 <= result["deflated_sharpe_ratio"] <= 1.0
    assert result["deflated_sharpe_ratio"] <= result["psr_vs_zero"] + 1e-9
    assert "expected_max_sharpe_null" in result
    assert "annualized_selected_sharpe" in result
    assert len(result["block_sharpes"]) == 6  # CPCV temporal-stability read
    assert set(result["best_params"]) == {"bands", "window"}
