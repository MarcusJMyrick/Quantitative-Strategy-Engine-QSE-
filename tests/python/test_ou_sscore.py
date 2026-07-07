"""Unit tests for scripts/research/statarb/ou_sscore.py (QR4.4).

Done-when coverage: on a simulated OU path with known (kappa, m, sigma) the
estimator recovers them within tolerance, and names with b -> 1 (slow / no
mean reversion) are filtered out. Plus s-score sign/formula, the sum-to-zero
handoff from QR4.3, and causality.
"""

import sys
from pathlib import Path

import numpy as np
import pandas as pd
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts" / "research" / "statarb"))

from ou_sscore import (  # noqa: E402
    DEFAULT_DT,
    TRADING_DAYS,
    fit_ou,
    ou_s_score,
    rolling_ou_scores,
)

BDAYS = pd.bdate_range("2020-01-01", periods=800)


def simulate_ou(kappa, m, sigma, n, dt=DEFAULT_DT, x0=None, seed=0):
    """Euler-Maruyama OU path dX = kappa(m - X)dt + sigma dW."""
    rng = np.random.default_rng(seed)
    x = np.empty(n)
    x[0] = m if x0 is None else x0
    sqrt_dt = np.sqrt(dt)
    for i in range(1, n):
        x[i] = x[i - 1] + kappa * (m - x[i - 1]) * dt + sigma * sqrt_dt * rng.standard_normal()
    return x


def ar1_series(b, n=10000, noise=0.01, seed=0):
    """AR(1) X_{n+1} = b*X_n + noise with an exactly known b, so the fitted
    mean-reversion time tau = -1/ln(b) is accurate on a long sample and the
    speed filter behaves deterministically (a 60-point OU fit is too noisy)."""
    rng = np.random.default_rng(seed)
    x = np.empty(n)
    x[0] = 0.0
    for i in range(1, n):
        x[i] = b * x[i - 1] + rng.normal(0, noise)
    return x


# ---------------------------------------------------------------------------
# Done-when #1: recover known (kappa, m, sigma_eq) from a simulated OU path
# ---------------------------------------------------------------------------


def test_recovers_known_ou_parameters():
    kappa, m, sigma = 12.0, 0.05, 0.20
    x = simulate_ou(kappa, m, sigma, n=20000, seed=1)
    fit = fit_ou(x, dt=DEFAULT_DT)

    assert fit["valid"]
    assert fit["kappa"] == pytest.approx(kappa, rel=0.12)
    assert fit["m"] == pytest.approx(m, abs=0.01)
    sigma_eq_true = sigma / np.sqrt(2 * kappa)  # OU stationary std
    assert fit["sigma_eq"] == pytest.approx(sigma_eq_true, rel=0.12)


def test_half_life_matches_kappa():
    x = simulate_ou(kappa=10.0, m=0.0, sigma=0.15, n=20000, seed=2)
    fit = fit_ou(x)
    # half-life = ln2 / kappa (years); recovered kappa near 10
    assert fit["kappa"] == pytest.approx(10.0, rel=0.15)


# ---------------------------------------------------------------------------
# Done-when #2: the speed filter rejects slow / non-mean-reverting names
# ---------------------------------------------------------------------------


def test_speed_filter_keeps_fast_reversion():
    x = ar1_series(b=np.exp(-1.0 / 10), seed=3)  # tau ~ 10 steps < 30
    _, fit = ou_s_score(x, window=60, max_tau_fraction=0.5)
    assert fit["valid"] and fit["speed_ok"]
    assert fit["tau_steps"] == pytest.approx(10.0, rel=0.1)


def test_speed_filter_rejects_slow_reversion():
    x = ar1_series(b=np.exp(-1.0 / 100), seed=4)  # tau ~ 100 steps, far above 30
    s, fit = ou_s_score(x, window=60, max_tau_fraction=0.5)
    assert fit["valid"] and not fit["speed_ok"]
    assert np.isnan(s)  # filtered names emit no s-score


def test_near_random_walk_fails_speed_filter():
    x = ar1_series(b=0.9995, seed=5)  # tau ~ 2000 steps, essentially a random walk
    s, fit = ou_s_score(x, window=60)
    assert not fit["speed_ok"]
    assert np.isnan(s)


def test_speed_filter_threshold_is_half_window():
    """A name whose tau is clearly below / above window/2 sets the flag."""
    for tau_target, expect_ok in ((15.0, True), (60.0, False)):
        x = ar1_series(b=np.exp(-1.0 / tau_target), seed=int(tau_target))
        _, fit = ou_s_score(x, window=60, max_tau_fraction=0.5)  # threshold tau < 30
        assert fit["valid"]
        assert fit["speed_ok"] is expect_ok


# ---------------------------------------------------------------------------
# s-score formula, sign, and the QR4.3 sum-to-zero handoff
# ---------------------------------------------------------------------------


def test_s_score_matches_formula():
    x = simulate_ou(kappa=20.0, m=0.03, sigma=0.2, n=60, seed=6)
    s, fit = ou_s_score(x, window=60)
    assert fit["speed_ok"]
    assert s == pytest.approx((x[-1] - fit["m"]) / fit["sigma_eq"])


def test_s_score_sign_tracks_position_vs_equilibrium():
    """sigma_eq > 0, so sign(s) = sign(X_last - m): below equilibrium is a
    negative s-score (a buy under the Avellaneda-Lee bands), above is positive.
    Checked on a long, cleanly-fitting path (both signs, by shifting m)."""
    x = simulate_ou(kappa=18.0, m=0.06, sigma=0.2, n=400, seed=7)
    s, fit = ou_s_score(x, window=400)
    assert fit["speed_ok"]
    assert np.sign(s) == np.sign(x[-1] - fit["m"])

    # a path whose endpoint sits below its equilibrium yields s < 0
    low = simulate_ou(kappa=18.0, m=0.0, sigma=0.2, n=400, seed=7, x0=0.0)
    low = low - low[-1] - 0.05  # shift so the last point is below the (shifted) mean
    s_low, fit_low = ou_s_score(low, window=400)
    assert low[-1] < fit_low["m"]
    assert s_low < 0


def test_sum_to_zero_series_gives_minus_m_over_sigma():
    """QR4.3 handoff: when X ends at 0 (residuals sum to zero), the s-score is
    exactly -m/sigma_eq."""
    x = simulate_ou(kappa=15.0, m=0.04, sigma=0.2, n=120, seed=8)
    x = x - x.mean()  # a de-meaned proxy; endpoint not 0
    x_pinned = x - x[-1]  # force X_last = 0 exactly
    s, fit = ou_s_score(x_pinned, window=120)
    assert x_pinned[-1] == pytest.approx(0.0, abs=1e-15)
    assert s == pytest.approx(-fit["m"] / fit["sigma_eq"])


def test_constant_series_is_invalid():
    s, fit = ou_s_score(np.zeros(60), window=60)
    assert not fit["valid"]
    assert np.isnan(s)


# ---------------------------------------------------------------------------
# Rolling driver: shapes, filtering, causality
# ---------------------------------------------------------------------------


def make_universe(seed=11, t=200, n=6):
    """A returns matrix with a common factor + idiosyncratic mean reversion,
    enough structure for residuals_for_window to retain a factor."""
    rng = np.random.default_rng(seed)
    factor = rng.normal(0, 0.02, t)
    cols = [f"S{i}" for i in range(n)]
    data = {c: 0.8 * factor + rng.normal(0, 0.012, t) for c in cols}
    return pd.DataFrame(data, index=BDAYS[:t], columns=cols)


def test_rolling_shapes_and_filter():
    returns = make_universe()
    window = 60
    out = rolling_ou_scores(returns, window=window)
    n_windows = len(returns) - window + 1

    assert out["sscore"].shape == (n_windows, returns.shape[1])
    assert out["kappa"].shape == (n_windows, returns.shape[1])
    assert out["speed_ok"].shape == (n_windows, returns.shape[1])
    assert out["sscore"].index[0] == returns.index[window - 1]
    # s-scores exist exactly where the speed filter passed
    passed = out["speed_ok"].to_numpy()
    finite = np.isfinite(out["sscore"].to_numpy())
    assert (finite == passed).all()


def test_rolling_ou_is_causal():
    returns = make_universe(seed=12, t=200)
    full = rolling_ou_scores(returns, window=50)
    truncated = rolling_ou_scores(returns.iloc[:120], window=50)
    for key in ("sscore", "kappa", "speed_ok"):
        pd.testing.assert_frame_equal(full[key].loc[truncated[key].index], truncated[key])


def test_trading_days_constant():
    assert TRADING_DAYS == 252.0
    assert DEFAULT_DT == pytest.approx(1.0 / 252.0)
