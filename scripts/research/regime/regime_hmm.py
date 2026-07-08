"""QR3.2 — Gaussian HMM regime classifier, fit CAUSALLY (the sophistication point).

A diagonal-covariance Gaussian HMM clusters the QR3.1 features into K market
states. The states are *discovered* by EM, then labelled by inspection — sorted
by the emission mean of realized vol (rv_21), so state 0 is the calmest and
state K-1 the most turbulent ("low / high / crash"). Whether a distinct "crash"
state emerges is an empirical question, reported honestly.

TWO look-ahead traps, both avoided here — this is what separates an honest
regime backtest from the many published ones that leak:

  1. Smoothed vs filtered inference. hmmlearn's predict / predict_proba return
     the Viterbi path / smoothed posterior gamma_t = P(s_t | y_1..y_T), which
     use the WHOLE series — a look-ahead bug for any live/backtest use. The
     live quantity is the FILTERED posterior alpha_t = P(s_t | y_1..y_t), from
     the forward pass alone. We implement the forward filter ourselves
     (`forward_filter`) so the distinction is explicit and testable, rather
     than hidden behind a library call.

  2. The model parameters themselves. Fitting one HMM over all history and then
     filtering still leaks: the means/covariances/transitions were learned from
     future data. So the state series is produced on an EXPANDING window —
     refit only on data <= t (periodically, for cost), and the filtered state
     at t uses only observations <= t. Feature standardization is frozen at
     each refit (training-window stats) so model and observations share a scale.

The consequence, and the done-when: the live state at date t is unchanged when
future data is appended. A prefix run and a full run agree bit-for-bit on their
overlap (tested) — the operational definition of "genuinely filtered, not
smoothed, and causally fit".

Implemented in pure numpy (no hmmlearn/sklearn dependency; own log-sum-exp):
Baum-Welch EM (smoothed forward-backward, over the training window only) for
fitting; a separate log-space forward filter for inference. Cost is managed by
refitting only per segment and filtering each segment's prefix once.

Output: data/regime/regime_states.parquet (date, state, p_state_0..p_state_K-1)
        data/regime/regime_hmm_manifest.json
"""

import argparse
import json
import logging
import sys
from dataclasses import dataclass
from datetime import date
from pathlib import Path

import numpy as np
import pandas as pd

logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")
logger = logging.getLogger(__name__)

MIN_COVAR = 1e-6
MIN_PROB = 1e-300  # floor before log() to avoid -inf on unused transitions


def logsumexp(a: np.ndarray, axis: int) -> np.ndarray:
    """Fast, stable log-sum-exp (no scipy dispatch overhead in the HMM hot loops)."""
    m = np.max(a, axis=axis, keepdims=True)
    m = np.where(np.isfinite(m), m, 0.0)
    return np.log(np.sum(np.exp(a - m), axis=axis)) + np.squeeze(m, axis=axis)


def _safe_log(p: np.ndarray) -> np.ndarray:
    return np.log(np.maximum(p, MIN_PROB))


def _log_gauss_diag(X: np.ndarray, means: np.ndarray, covars: np.ndarray) -> np.ndarray:
    """log N(x; mu_j, diag(var_j)) for each observation x and state j -> [n, k]."""
    n, d = X.shape
    k = means.shape[0]
    out = np.empty((n, k))
    for j in range(k):
        diff = X - means[j]
        out[:, j] = -0.5 * (
            d * np.log(2 * np.pi)
            + np.sum(np.log(covars[j]))
            + np.sum(diff * diff / covars[j], axis=1)
        )
    return out


def _kmeans(X: np.ndarray, k: int, rng: np.random.Generator, iters: int = 25):
    """Small seeded k-means for EM initialisation (deterministic given seed)."""
    centers = X[rng.choice(len(X), k, replace=False)].copy()
    labels = np.zeros(len(X), dtype=int)
    for _ in range(iters):
        dist = ((X[:, None, :] - centers[None, :, :]) ** 2).sum(axis=2)
        new = dist.argmin(axis=1)
        if np.array_equal(new, labels):
            break
        labels = new
        for j in range(k):
            if (labels == j).any():
                centers[j] = X[labels == j].mean(axis=0)
    return labels, centers


@dataclass
class GaussianHMM:
    """Diagonal-covariance Gaussian HMM (fit by Baum-Welch, seeded)."""

    n_states: int
    seed: int = 0
    max_iter: int = 50
    tol: float = 1e-4

    startprob_: np.ndarray = None
    transmat_: np.ndarray = None
    means_: np.ndarray = None
    covars_: np.ndarray = None

    def fit(self, X: np.ndarray) -> "GaussianHMM":
        rng = np.random.default_rng(self.seed)
        n, d = X.shape
        k = self.n_states
        labels, centers = _kmeans(X, k, rng)

        self.means_ = centers
        self.covars_ = np.stack(
            [
                np.maximum(
                    X[labels == j].var(axis=0) if (labels == j).any() else np.ones(d), MIN_COVAR
                )
                for j in range(k)
            ]
        )
        self.startprob_ = np.full(k, 1.0 / k)
        self.transmat_ = np.full((k, k), 1.0 / k)

        prev_ll = -np.inf
        for _ in range(self.max_iter):
            log_b = _log_gauss_diag(X, self.means_, self.covars_)
            log_alpha, log_beta, ll = self._forward_backward(log_b)
            gamma = np.exp(log_alpha + log_beta - ll)  # smoothed P(s_t | y_1..y_T)

            # xi summed over t: expected transitions
            log_xi = (
                log_alpha[:-1, :, None]
                + _safe_log(self.transmat_)[None, :, :]
                + (log_b[1:] + log_beta[1:])[:, None, :]
                - ll
            )
            xi_sum = np.exp(logsumexp(log_xi, axis=0))

            self.startprob_ = gamma[0] / gamma[0].sum()
            self.transmat_ = xi_sum / xi_sum.sum(axis=1, keepdims=True)
            w = gamma.sum(axis=0)
            self.means_ = (gamma.T @ X) / w[:, None]
            for j in range(k):
                diff = X - self.means_[j]
                self.covars_[j] = np.maximum(
                    (gamma[:, j][:, None] * diff * diff).sum(0) / w[j], MIN_COVAR
                )

            if ll - prev_ll < self.tol:
                break
            prev_ll = ll
        return self

    def _forward_backward(self, log_b: np.ndarray):
        n, k = log_b.shape
        log_t = _safe_log(self.transmat_)
        log_alpha = np.empty((n, k))
        log_alpha[0] = _safe_log(self.startprob_) + log_b[0]
        for t in range(1, n):
            log_alpha[t] = logsumexp(log_alpha[t - 1][:, None] + log_t, axis=0) + log_b[t]
        log_beta = np.zeros((n, k))
        for t in range(n - 2, -1, -1):
            log_beta[t] = logsumexp(log_t + (log_b[t + 1] + log_beta[t + 1])[None, :], axis=1)
        ll = float(logsumexp(log_alpha[-1], axis=0))
        return log_alpha, log_beta, ll

    def forward_filter(self, X: np.ndarray) -> np.ndarray:
        """FILTERED posteriors P(s_t | y_1..y_t) -> [n, k]. Forward pass only:
        row t depends on observations 0..t, never t+1.."""
        log_b = _log_gauss_diag(X, self.means_, self.covars_)
        n, k = log_b.shape
        log_t = _safe_log(self.transmat_)
        filtered = np.empty((n, k))
        log_alpha = _safe_log(self.startprob_) + log_b[0]
        filtered[0] = np.exp(log_alpha - logsumexp(log_alpha, axis=0))
        for t in range(1, n):
            prev = _safe_log(filtered[t - 1])
            log_alpha = logsumexp(prev[:, None] + log_t, axis=0) + log_b[t]
            filtered[t] = np.exp(log_alpha - logsumexp(log_alpha, axis=0))
        return filtered

    def reorder(self, order: np.ndarray) -> "GaussianHMM":
        """Permute state indices (used to sort states by vol)."""
        self.startprob_ = self.startprob_[order]
        self.transmat_ = self.transmat_[np.ix_(order, order)]
        self.means_ = self.means_[order]
        self.covars_ = self.covars_[order]
        return self


def _vol_order(means_standardized: np.ndarray, vol_col: int = 0) -> np.ndarray:
    """State ordering by emission mean of the vol feature: 0 = calm, K-1 = crash."""
    return np.argsort(means_standardized[:, vol_col])


def filtered_regime_states(
    features: pd.DataFrame,
    n_states: int = 3,
    warmup: int = 252,
    refit_every: int = 63,
    seed: int = 0,
    max_iter: int = 100,
) -> pd.DataFrame:
    """Expanding-window, filtered regime states — the causal live series.

    For each date t >= warmup-1: use the HMM last refit on data <= t (refit every
    `refit_every` days on the expanding window), standardize with that refit's
    frozen training stats, and take argmax of the filtered posterior at t. States
    are vol-ordered (0 calm .. K-1 crash) consistently at every refit."""
    X = features.to_numpy(dtype=float)
    n = len(X)
    if warmup < n_states + 2 or warmup > n:
        raise ValueError(f"warmup {warmup} out of range for n={n}, n_states={n_states}")

    states = np.full(n, -1, dtype=int)
    probs = np.full((n, n_states), np.nan)

    # Refit on the expanding window at each segment start; the model is frozen
    # for the segment. Filter ONCE over the prefix [0:seg_end] per segment and
    # slice out rows [start:seg_end] — the filtered posterior at t depends only
    # on rows <= t (forward pass), so filtering the longer prefix is identical to
    # filtering [0:t+1] for each t, but far cheaper than redoing it per day.
    refit_starts = list(range(warmup - 1, n, refit_every))
    for i, start in enumerate(refit_starts):
        seg_end = refit_starts[i + 1] if i + 1 < len(refit_starts) else n
        train = X[: start + 1]
        mu = train.mean(axis=0)
        sd = train.std(axis=0, ddof=1)
        sd[sd == 0] = 1.0
        model = GaussianHMM(n_states, seed=seed, max_iter=max_iter).fit((train - mu) / sd)
        model.reorder(_vol_order(model.means_))

        filt = model.forward_filter((X[:seg_end] - mu) / sd)
        states[start:seg_end] = np.argmax(filt[start:seg_end], axis=1)
        probs[start:seg_end] = filt[start:seg_end]

    out = pd.DataFrame(
        {"state": states, **{f"p_state_{j}": probs[:, j] for j in range(n_states)}},
        index=features.index,
    )
    return out.iloc[warmup - 1 :]


def build(
    features_path: Path,
    out_dir: Path,
    n_states: int = 3,
    warmup: int = 252,
    refit_every: int = 63,
    seed: int = 0,
) -> dict:
    features = pd.read_parquet(features_path)
    states = filtered_regime_states(
        features, n_states=n_states, warmup=warmup, refit_every=refit_every, seed=seed
    )
    out_dir.mkdir(parents=True, exist_ok=True)
    states_path = out_dir / "regime_states.parquet"
    states.to_parquet(states_path)

    # regime occupancy + which feature-means characterise each labelled state,
    # by joining the filtered state back onto rv_21
    occ = states["state"].value_counts(normalize=True).sort_index()
    rv_by_state = (
        features["rv_21"].reindex(states.index).groupby(states["state"]).mean().round(4).to_dict()
    )
    manifest = {
        "generated": date.today().isoformat(),
        "n_states": n_states,
        "warmup": warmup,
        "refit_every": refit_every,
        "seed": seed,
        "rows": len(states),
        "span": [states.index[0].date().isoformat(), states.index[-1].date().isoformat()],
        "state_labeling": "sorted ascending by rv_21 emission mean: 0 = calmest, K-1 = most turbulent",
        "occupancy": {int(s): round(float(f), 4) for s, f in occ.items()},
        "mean_rv_21_by_state": {int(s): v for s, v in rv_by_state.items()},
        "as_of_alignment": (
            "state at t uses an HMM refit on data <= t and the FILTERED posterior "
            "P(s_t | y_1..y_t); no future data. Consumers act at >= t+1."
        ),
        "note": (
            "States are discovered then labelled by vol; a distinct 'crash' state "
            "emerges only if the data supports it — reported, not assumed."
        ),
    }
    (out_dir / "regime_hmm_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    logger.info(
        f"Regime states: {len(states)} rows, {n_states} states, occupancy "
        f"{manifest['occupancy']}, mean rv_21 by state {manifest['mean_rv_21_by_state']}"
    )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--features", type=Path, default=Path("data/regime/regime_features.parquet")
    )
    parser.add_argument("--out-dir", type=Path, default=Path("data/regime"))
    parser.add_argument("--states", type=int, default=3)
    parser.add_argument("--warmup", type=int, default=252)
    parser.add_argument("--refit-every", type=int, default=63)
    parser.add_argument("--seed", type=int, default=0)
    args = parser.parse_args()
    build(args.features, args.out_dir, args.states, args.warmup, args.refit_every, args.seed)
    return 0


if __name__ == "__main__":
    sys.exit(main())
