"""Unit tests for scripts/research/regime/regime_hmm.py (QR3.2).

Done-when centerpiece: the live state at t is unchanged when future data is
appended — i.e. the state series is genuinely FILTERED (not smoothed/Viterbi)
and CAUSALLY fit. Plus: known-regime recovery, vol-ordered labels, and the
forward filter not peeking at t+1.
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "research" / "regime"))

from regime_hmm import GaussianHMM, filtered_regime_states  # noqa: E402

DAYS = pd.bdate_range("2020-01-01", periods=780)


def _features_from_rv(rv, seed):
    rng = np.random.default_rng(seed + 1000)
    return pd.DataFrame(
        {
            "rv_21": rv,
            "vov_21": rv * 0.3 + rng.normal(0, 0.01, len(rv)),
            "vol_ratio_63": rng.normal(0, 1, len(rv)),
        },
        index=DAYS[: len(rv)],
    )


def two_regime_features(seed=0, n=700, switch=350):
    """rv_21-like feature: calm (low mean) then turbulent (high mean)."""
    rng = np.random.default_rng(seed)
    rv = np.abs(np.where(np.arange(n) < switch, 0.10, 0.30) + rng.normal(0, 0.015, n))
    return _features_from_rv(rv, seed)


def three_era_features(seed=0, n=750):
    """calm -> turbulent -> calm, so a model that has seen both regimes can be
    checked for correct recovery on BOTH a turbulent and a (later) calm era."""
    rng = np.random.default_rng(seed)
    era = np.select([np.arange(n) < 250, np.arange(n) < 500], [0.10, 0.30], default=0.10)
    rv = np.abs(era + rng.normal(0, 0.015, n))
    return _features_from_rv(rv, seed)


# ---------------------------------------------------------------------------
# Done-when: filtered + causal — appending future data can't change past states
# ---------------------------------------------------------------------------


def test_state_at_t_unchanged_when_future_appended():
    feats = two_regime_features(seed=1)
    kw = dict(n_states=2, warmup=120, refit_every=60, seed=7, max_iter=30)
    full = filtered_regime_states(feats, **kw)
    prefix = filtered_regime_states(feats.iloc[:450], **kw)

    common = prefix.index
    # states AND filtered probabilities match bit-for-bit on the overlap
    pd.testing.assert_frame_equal(full.loc[common], prefix, check_dtype=False)


def test_forward_filter_row_t_ignores_future_rows():
    feats = two_regime_features(seed=2)
    X = ((feats - feats.mean()) / feats.std()).to_numpy()
    model = GaussianHMM(2, seed=0, max_iter=50).fit(X)

    t = 300
    filt_full = model.forward_filter(X)
    filt_prefix = model.forward_filter(X[: t + 1])
    # filtered posterior at t is identical whether or not rows > t exist
    np.testing.assert_allclose(filt_full[t], filt_prefix[t], atol=1e-12)


def test_filtered_differs_from_a_smoothed_pass():
    """'Filtered' is a real constraint: near a regime transition the forward-only
    posterior lags, while the smoothed (forward-backward) posterior already knows
    the future — so they genuinely differ. Needs OVERLAPPING regimes; cleanly
    separated ones make both collapse to the same one-hot everywhere."""
    rng = np.random.default_rng(11)
    n = 400
    rv = np.where(np.arange(n) < 200, 0.15, 0.21) + rng.normal(0, 0.035, n)  # overlapping
    X = np.column_stack([rv, rv * 0.3 + rng.normal(0, 0.02, n), rng.normal(0, 1, n)])
    X = (X - X.mean(0)) / X.std(0)
    model = GaussianHMM(2, seed=0, max_iter=40).fit(X)

    log_alpha, log_beta, ll = model._forward_backward(_logb(model, X))
    smoothed = np.exp(log_alpha + log_beta - ll)
    filtered = model.forward_filter(X)
    # they must differ somewhere (the future is informative near the transition)
    assert np.abs(filtered - smoothed).max() > 1e-3


def _logb(model, X):
    from regime_hmm import _log_gauss_diag

    return _log_gauss_diag(X, model.means_, model.covars_)


# ---------------------------------------------------------------------------
# Recovery + labeling
# ---------------------------------------------------------------------------


def test_recovers_the_two_regimes():
    # calm(0:250) -> turbulent(250:500) -> calm(500:750). Check recovery only in
    # regions the trained model has already seen BOTH regimes for: the turbulent
    # middle and the FINAL calm era (a warmed-up, both-regimes-seen model). Causal
    # detection lags onset, so we skip the first calm era and the transitions.
    feats = three_era_features(seed=4)
    states = filtered_regime_states(
        feats, n_states=2, warmup=150, refit_every=50, seed=1, max_iter=30
    )
    turbulent = states.loc[(states.index >= DAYS[330]) & (states.index < DAYS[480]), "state"]
    late_calm = states.loc[states.index >= DAYS[560], "state"]
    assert (turbulent == 1).mean() > 0.8  # high-vol middle -> state 1
    assert (late_calm == 0).mean() > 0.8  # calm again -> back to state 0


def test_states_are_vol_ordered():
    feats = two_regime_features(seed=5)
    states = filtered_regime_states(
        feats, n_states=2, warmup=150, refit_every=50, seed=1, max_iter=30
    )
    joined = feats.join(states, how="inner")
    mean_rv_state0 = joined.loc[joined["state"] == 0, "rv_21"].mean()
    mean_rv_state1 = joined.loc[joined["state"] == 1, "rv_21"].mean()
    assert mean_rv_state0 < mean_rv_state1  # state 0 is the calmer regime by construction


def test_probabilities_are_normalized():
    feats = two_regime_features(seed=6)
    states = filtered_regime_states(
        feats, n_states=3, warmup=150, refit_every=60, seed=2, max_iter=30
    )
    p = states[[c for c in states.columns if c.startswith("p_state_")]]
    np.testing.assert_allclose(p.sum(axis=1).to_numpy(), 1.0, atol=1e-9)
    assert ((states["state"] >= 0) & (states["state"] < 3)).all()


# ---------------------------------------------------------------------------
# Determinism + guards
# ---------------------------------------------------------------------------


def test_same_seed_same_result():
    feats = two_regime_features(seed=8)
    a = filtered_regime_states(feats, n_states=2, warmup=150, refit_every=50, seed=3, max_iter=30)
    b = filtered_regime_states(feats, n_states=2, warmup=150, refit_every=50, seed=3, max_iter=30)
    pd.testing.assert_frame_equal(a, b)


def test_warmup_out_of_range_raises():
    feats = two_regime_features(seed=9, n=100)
    with pytest.raises(ValueError):
        filtered_regime_states(feats, n_states=3, warmup=3)  # too small
    with pytest.raises(ValueError):
        filtered_regime_states(feats, n_states=3, warmup=200)  # > n
