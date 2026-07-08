"""QR2.4 — Probabilistic and Deflated Sharpe Ratio (Bailey & López de Prado).

The headline metric of the whole track. A Sharpe estimated from a finite,
non-normal return series is uncertain, and picking the *best* of many trials
inflates it — the more configurations you try, the higher the best in-sample
Sharpe climbs on luck alone. The DSR corrects for both.

  PSR(SR*) — Probabilistic Sharpe Ratio. The probability the *true* (per-period)
  Sharpe exceeds a benchmark SR*, given the estimate's standard error, which
  grows with non-normality (negative skew and fat tails make a high Sharpe less
  trustworthy):

      PSR(SR*) = Φ[ (SR_hat − SR*) · √(n−1)
                    / √(1 − γ3·SR_hat + ((γ4−1)/4)·SR_hat²) ]

  where γ3 = skewness, γ4 = kurtosis (non-excess; normal = 3), n = # returns.

  DSR — Deflated Sharpe Ratio = PSR(SR*₀) with the benchmark set to the
  *expected maximum* Sharpe under the null (zero true skill) across N trials:

      SR*₀ = √(V[SR]) · [ (1−γ)·Z⁻¹(1 − 1/N) + γ·Z⁻¹(1 − 1/(N·e)) ]

  V[SR] = variance of the trial Sharpes, γ ≈ 0.5772 (Euler-Mascheroni). SR*₀ is
  the Sharpe you would expect to see from the luckiest of N skill-less
  strategies; DSR asks whether the selected strategy clears *that* bar.

Everything is computed at the per-period (non-annualized) frequency so PSR's
SR_hat and V[SR] share units. Moments use the biased (÷n) estimators, matching
the López de Prado reference implementation.
"""

import sys
from pathlib import Path

import numpy as np
from scipy.stats import norm

sys.path.insert(0, str(Path(__file__).resolve().parent))

from trial_registry import TrialRegistry  # noqa: E402

EULER_MASCHERONI = 0.5772156649015329


def sharpe_ratio(returns) -> float:
    """Per-period Sharpe SR_hat = mean / std (ddof=1); 0 if degenerate."""
    r = np.asarray(returns, dtype=float)
    r = r[np.isfinite(r)]
    if len(r) < 2:
        return 0.0
    sd = r.std(ddof=1)
    return 0.0 if sd == 0 else float(r.mean() / sd)


def _skew_kurt(r: np.ndarray) -> tuple[float, float]:
    """Biased skewness and non-excess kurtosis (normal -> 3)."""
    m = r.mean()
    d = r - m
    m2 = np.mean(d**2)
    if m2 == 0:
        return 0.0, 3.0
    skew = np.mean(d**3) / m2**1.5
    kurt = np.mean(d**4) / m2**2
    return float(skew), float(kurt)


def probabilistic_sharpe_ratio(returns, sr_benchmark: float = 0.0) -> float:
    """PSR(SR*): probability the true per-period Sharpe exceeds `sr_benchmark`."""
    r = np.asarray(returns, dtype=float)
    r = r[np.isfinite(r)]
    n = len(r)
    if n < 2:
        return 0.5
    sr = sharpe_ratio(r)
    skew, kurt = _skew_kurt(r)
    denom = 1.0 - skew * sr + ((kurt - 1.0) / 4.0) * sr**2
    if denom <= 0:
        return float("nan")
    z = (sr - sr_benchmark) * np.sqrt(n - 1) / np.sqrt(denom)
    return float(norm.cdf(z))


def expected_max_sharpe(trial_sharpes) -> float:
    """SR*₀: expected maximum Sharpe under the null across N trials."""
    s = np.asarray(trial_sharpes, dtype=float)
    s = s[np.isfinite(s)]
    n = len(s)
    if n < 2:
        return 0.0
    var_sr = s.var(ddof=1)
    if var_sr <= 0:
        return 0.0
    g = EULER_MASCHERONI
    term = (1.0 - g) * norm.ppf(1.0 - 1.0 / n) + g * norm.ppf(1.0 - 1.0 / (n * np.e))
    return float(np.sqrt(var_sr) * term)


def deflated_sharpe_ratio(selected_returns, trial_sharpes) -> float:
    """DSR = PSR(SR*₀): does the selected strategy clear the expected-max-under-
    null benchmark set by the number of trials tried?"""
    sr_star = expected_max_sharpe(trial_sharpes)
    return probabilistic_sharpe_ratio(selected_returns, sr_star)


def deflate_registry(registry: TrialRegistry, selected_trial_id: str | None = None) -> dict:
    """Compute the DSR of the selected trial (default: the highest-Sharpe one)
    against the dispersion of every logged trial's per-period Sharpe."""
    trials = registry.load_all()
    if not trials:
        raise ValueError("registry is empty; log some trials first")
    sharpes = {t.trial_id: sharpe_ratio(t.returns) for t in trials}
    if selected_trial_id is None:
        selected_trial_id = max(sharpes, key=sharpes.get)
    selected = next(t for t in trials if t.trial_id == selected_trial_id)

    sr_star = expected_max_sharpe(list(sharpes.values()))
    return {
        "n_trials": len(trials),
        "selected_trial_id": selected_trial_id,
        "selected_sharpe": sharpes[selected_trial_id],
        "var_trial_sharpes": float(np.var(list(sharpes.values()), ddof=1)),
        "expected_max_sharpe_null": sr_star,
        "psr_vs_zero": probabilistic_sharpe_ratio(selected.returns, 0.0),
        "deflated_sharpe_ratio": probabilistic_sharpe_ratio(selected.returns, sr_star),
    }


# ---------------------------------------------------------------------------
# The multiple-testing demonstration (the done-when, as a reusable experiment)
# ---------------------------------------------------------------------------


def multiple_testing_experiment(n_trials: int = 100, n_obs: int = 250, seed: int = 0) -> dict:
    """`n_trials` pure-noise strategies (true Sharpe 0). The luckiest looks
    great undeflated but the DSR collapses to ~0.5 — the penalty for search."""
    rng = np.random.default_rng(seed)
    series = [rng.normal(0.0, 0.01, n_obs) for _ in range(n_trials)]
    sharpes = [sharpe_ratio(s) for s in series]
    best = int(np.argmax(sharpes))
    return {
        "n_trials": n_trials,
        "best_sharpe": sharpes[best],
        "psr_vs_zero_of_best": probabilistic_sharpe_ratio(series[best], 0.0),
        "expected_max_sharpe_null": expected_max_sharpe(sharpes),
        "dsr_of_best": deflated_sharpe_ratio(series[best], sharpes),
        "all_sharpes": sharpes,
        "best_series": series[best],
    }


def plot_deflation(exp: dict, path: Path):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(10, 6))
    ax.hist(exp["all_sharpes"], bins=25, color="tab:blue", alpha=0.7, label="per-trial Sharpe")
    ax.axvline(
        exp["best_sharpe"], color="tab:green", lw=1.6, label=f"best {exp['best_sharpe']:.3f}"
    )
    ax.axvline(
        exp["expected_max_sharpe_null"],
        color="red",
        ls="--",
        lw=1.6,
        label=f"E[max | null] SR*₀ = {exp['expected_max_sharpe_null']:.3f}",
    )
    ax.set_xlabel("per-period Sharpe")
    ax.set_ylabel("count")
    ax.set_title(
        f"{exp['n_trials']} noise strategies: best PSR(0) = "
        f"{exp['psr_vs_zero_of_best']:.2f} looks great, "
        f"but DSR = {exp['dsr_of_best']:.2f} (deflated to chance)"
    )
    ax.legend(fontsize=9)
    fig.tight_layout()
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=150)
    plt.close(fig)


def main() -> int:
    exp = multiple_testing_experiment(n_trials=100, n_obs=250, seed=7)
    print(
        f"{exp['n_trials']} noise strategies | best Sharpe {exp['best_sharpe']:.3f} | "
        f"PSR(0) {exp['psr_vs_zero_of_best']:.3f} | SR*0 "
        f"{exp['expected_max_sharpe_null']:.3f} | DSR {exp['dsr_of_best']:.3f}"
    )
    plot_deflation(exp, Path("docs/research/validation/dsr_deflation.png"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
