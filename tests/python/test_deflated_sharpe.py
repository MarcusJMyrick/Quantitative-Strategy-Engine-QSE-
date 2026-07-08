"""Unit tests for scripts/research/validation/deflated_sharpe.py (QR2.4).

Done-when coverage: 100 random-noise strategy variations severely deflate the
final DSR versus a single-hypothesis run — the multiple-testing penalty is
visible and correct.
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "research" / "validation"))

from deflated_sharpe import (  # noqa: E402
    _skew_kurt,
    deflate_registry,
    deflated_sharpe_ratio,
    expected_max_sharpe,
    multiple_testing_experiment,
    probabilistic_sharpe_ratio,
    sharpe_ratio,
)
from trial_registry import TrialRegistry  # noqa: E402


# ---------------------------------------------------------------------------
# Done-when: the multiple-testing penalty is visible and correct
# ---------------------------------------------------------------------------


def test_100_noise_trials_collapse_the_dsr():
    exp = multiple_testing_experiment(n_trials=100, n_obs=250, seed=7)
    # undeflated, the luckiest of 100 noise strategies looks excellent...
    assert exp["psr_vs_zero_of_best"] > 0.85
    # ...but deflating for having tried 100 collapses it to ~chance
    assert exp["dsr_of_best"] < 0.60
    # the deflation is severe: a large drop, not a rounding effect
    assert exp["psr_vs_zero_of_best"] - exp["dsr_of_best"] > 0.30
    # and the best sits right at the null's expected max (that's why DSR ~ 0.5)
    assert exp["best_sharpe"] == pytest.approx(exp["expected_max_sharpe_null"], abs=0.05)


def test_more_trials_deflate_harder():
    # SR*0 (the bar) rises with the number of trials, so a fixed strategy's DSR
    # falls as the search widens
    rng = np.random.default_rng(1)
    few = [sharpe_ratio(rng.normal(0, 0.01, 250)) for _ in range(10)]
    many = [sharpe_ratio(rng.normal(0, 0.01, 250)) for _ in range(500)]
    assert expected_max_sharpe(many) > expected_max_sharpe(few)


def test_single_hypothesis_is_not_deflated():
    # a genuinely skilled strategy, judged as a single hypothesis, keeps a high
    # PSR — the deflation only bites when many trials widen the null's max
    rng = np.random.default_rng(2)
    skilled = rng.normal(0.06, 0.5, 500)  # per-period Sharpe ~ 0.12
    assert probabilistic_sharpe_ratio(skilled, 0.0) > 0.9


# ---------------------------------------------------------------------------
# PSR formula properties
# ---------------------------------------------------------------------------


def test_psr_half_at_own_sharpe():
    rng = np.random.default_rng(3)
    r = rng.normal(0.05, 1.0, 400)
    sr = sharpe_ratio(r)
    # benchmarking against its own point estimate gives exactly 0.5
    assert probabilistic_sharpe_ratio(r, sr) == pytest.approx(0.5, abs=1e-9)


def test_psr_rises_with_sample_size():
    rng = np.random.default_rng(4)
    short = rng.normal(0.05, 1.0, 60)
    long = np.concatenate([short] + [rng.normal(0.05, 1.0, 60) for _ in range(9)])
    # same signal, more evidence -> more confident the true Sharpe > 0
    assert probabilistic_sharpe_ratio(long, 0.0) > probabilistic_sharpe_ratio(short, 0.0)


def test_negative_skew_and_fat_tails_lower_psr():
    # two series with the SAME Sharpe; the one with negative skew / fat tails is
    # less trustworthy -> lower PSR. (Sharpe is scale-invariant, so we equalize
    # it by SHIFTING the mean, which leaves skew/kurtosis untouched.)
    rng = np.random.default_rng(5)
    n = 2000
    symmetric = rng.normal(0.05, 1.0, n)
    skewed = rng.normal(0.05, 1.0, n)
    skewed[::50] -= 6.0  # rare large losses -> negative skew, fat left tail

    target = sharpe_ratio(symmetric)
    skewed = skewed - skewed.mean() + target * skewed.std(ddof=1)  # shift Sharpe -> target
    assert sharpe_ratio(symmetric) == pytest.approx(sharpe_ratio(skewed), abs=1e-9)

    _, kurt = _skew_kurt(skewed)
    assert kurt > 3.0  # genuinely fat-tailed
    assert probabilistic_sharpe_ratio(skewed, 0.0) < probabilistic_sharpe_ratio(symmetric, 0.0)


def test_expected_max_sharpe_scales_with_dispersion():
    base = np.linspace(-1, 1, 50)
    assert expected_max_sharpe(2 * base) == pytest.approx(2 * expected_max_sharpe(base), rel=1e-6)


def test_sharpe_ratio_degenerate():
    assert sharpe_ratio([0.01]) == 0.0
    assert sharpe_ratio([0.01, 0.01, 0.01]) == 0.0


# ---------------------------------------------------------------------------
# Registry integration
# ---------------------------------------------------------------------------


def test_deflate_registry_selects_best_and_deflates(tmp_path):
    reg = TrialRegistry(tmp_path)
    rng = np.random.default_rng(6)
    idx = pd.RangeIndex(250)
    # 40 noise trials + one clearly-better trial
    for i in range(40):
        reg.log({"trial": i}, pd.Series(rng.normal(0, 0.01, 250), index=idx))
    reg.log({"trial": "skilled"}, pd.Series(rng.normal(0.004, 0.01, 250), index=idx))

    out = deflate_registry(reg)
    assert out["n_trials"] == 41
    assert out["selected_sharpe"] == pytest.approx(
        max(reg.sharpes().values()) / np.sqrt(252), rel=0.05
    )  # per-period vs the registry's annualized convenience Sharpe
    # deflated <= undeflated, and both are probabilities
    assert 0.0 <= out["deflated_sharpe_ratio"] <= 1.0
    assert out["deflated_sharpe_ratio"] <= out["psr_vs_zero"] + 1e-9


def test_deflated_ratio_matches_manual_composition():
    rng = np.random.default_rng(8)
    selected = rng.normal(0.05, 1.0, 300)
    trials = [sharpe_ratio(rng.normal(0, 1.0, 300)) for _ in range(50)]
    manual = probabilistic_sharpe_ratio(selected, expected_max_sharpe(trials))
    assert deflated_sharpe_ratio(selected, trials) == pytest.approx(manual)
