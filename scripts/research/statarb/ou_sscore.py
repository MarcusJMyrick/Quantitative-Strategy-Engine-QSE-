"""QR4.4 — OU fit + s-score for eigenportfolio stat arb.

The QR4.3 cumulative residual X_i is modeled as a mean-reverting
Ornstein-Uhlenbeck process, fit per window as its AR(1) discretization:

    X_{n+1} = a + b * X_n + zeta                          (OLS on the lagged X)

With step dt (default 1/252) the OU parameters follow:

    b      = e^{-kappa*dt}
    kappa  = -ln(b) / dt                                  (mean-reversion speed)
    m      = a / (1 - b)                                  (equilibrium level)
    sigma_eq = sqrt( Var(zeta) / (1 - b^2) )              (equilibrium std)
    s      = (X_last - m) / sigma_eq                      (the s-score)

Speed filter (critical, per Avellaneda-Lee). A slow-reverting fit is a
spurious signal: over a short window a near-random-walk (b -> 1) produces a
huge, meaningless s-score. Reject a name unless its mean-reversion time
tau = 1/kappa is short relative to the window — specifically
tau < max_tau_fraction * window (default half the window). In step units
tau = -1/ln(b), so the filter is dt-independent: keep only
-1/ln(b) < 0.5 * window, i.e. b below e^{-2/window} (~0.967 at window=60).
b outside (0, 1) is not mean-reverting at all and is rejected outright.

The sum-to-zero handoff from QR4.3. OLS-with-intercept residuals sum to zero
in-window, so on the real pipeline X_last ~ 0 and the s-score reduces to
s ~ -m/sigma_eq: the signal is carried by where the OU equilibrium sits
relative to the pinned endpoint, not by a raw "current level". A large
positive m (equilibrium above the current 0) means the residual is expected
to revert up -> the name is cheap -> a negative s-score -> a buy under the
Avellaneda-Lee bands (QR4.5). The estimators below are written for a general
X (the unit tests recover kappa/m/sigma_eq from genuine OU paths that do not
end at zero); the pinned-endpoint case is just where the real data lands.

As-of alignment (inherited from QR4.1-4.3): the window ending at date t uses
returns r_{t-w+1}..r_t only, so every s-score at t is observable at the close
of t; consumers trade no earlier than t+1. Appending future data never
changes an emitted row (tested).

Outputs (data/universe/, Parquets regenerated not committed):
  ou_sscore.parquet     date x name — s-score, NaN where the speed filter or
                        the fit rejects the name
  ou_kappa.parquet      date x name — annualized mean-reversion speed (NaN if
                        the AR(1) fit is not mean-reverting)
  ou_speed_ok.parquet   date x name — bool, passed the speed filter
  ou_manifest.json      parameters, filter pass-rate, s-score distribution
Plot (committed): docs/research/statarb/ou_sscore.png
"""

import argparse
import json
import logging
import sys
from datetime import date
from pathlib import Path

import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parent))

from residuals import residuals_for_window  # noqa: E402

logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")
logger = logging.getLogger(__name__)

TRADING_DAYS = 252.0
DEFAULT_DT = 1.0 / TRADING_DAYS


# ---------------------------------------------------------------------------
# OU estimation (pure functions, unit-tested)
# ---------------------------------------------------------------------------


def _invalid_fit(a: float, b: float, var_zeta: float) -> dict:
    return {
        "a": a,
        "b": b,
        "valid": False,
        "kappa": np.nan,
        "m": np.nan,
        "sigma_eq": np.nan,
        "var_zeta": var_zeta,
        "tau_steps": np.nan,
    }


def fit_ou(x: np.ndarray, dt: float = DEFAULT_DT) -> dict:
    """Fit AR(1) X_{n+1} = a + b*X_n + zeta by OLS and back out the OU
    parameters. `valid` is False when the series is degenerate or the fit is
    not mean-reverting (b outside (0, 1)), in which case kappa/m/sigma_eq are
    NaN. `tau_steps` = -1/ln(b) is the mean-reversion time in steps."""
    x = np.asarray(x, dtype=float)
    if x.size < 3:
        return _invalid_fit(np.nan, np.nan, np.nan)

    x0, x1 = x[:-1], x[1:]
    x0m, x1m = x0.mean(), x1.mean()
    sxx = float(((x0 - x0m) ** 2).sum())
    if sxx < 1e-300:
        return _invalid_fit(np.nan, np.nan, np.nan)  # constant series, no dynamics

    b = float(((x0 - x0m) * (x1 - x1m)).sum() / sxx)
    a = float(x1m - b * x0m)
    resid = x1 - (a + b * x0)
    dof = max(x1.size - 2, 1)
    var_zeta = float(resid @ resid) / dof

    if not (0.0 < b < 1.0) or var_zeta <= 0.0:
        return _invalid_fit(a, b, var_zeta)

    return {
        "a": a,
        "b": b,
        "valid": True,
        "kappa": -np.log(b) / dt,
        "m": a / (1.0 - b),
        "sigma_eq": np.sqrt(var_zeta / (1.0 - b * b)),
        "var_zeta": var_zeta,
        "tau_steps": -1.0 / np.log(b),
    }


def ou_s_score(
    x: np.ndarray,
    dt: float = DEFAULT_DT,
    window: int | None = None,
    max_tau_fraction: float = 0.5,
) -> tuple[float, dict]:
    """s-score for one cumulative-residual series, with the speed filter.

    Returns (s, fit) where s is NaN when the AR(1) fit is not mean-reverting
    or the mean-reversion time is too slow relative to the window. `fit`
    always carries `speed_ok`."""
    fit = fit_ou(x, dt)
    w = window if window is not None else len(x)
    fit["speed_ok"] = bool(fit["valid"] and fit["tau_steps"] < max_tau_fraction * w)
    if not fit["speed_ok"]:
        return np.nan, fit
    s = (float(x[-1]) - fit["m"]) / fit["sigma_eq"]
    return s, fit


def half_life_days(kappa: float) -> float:
    """Time for a deviation to decay by half, in trading days."""
    return np.log(2.0) / kappa * TRADING_DAYS


# ---------------------------------------------------------------------------
# Rolling driver
# ---------------------------------------------------------------------------


def rolling_ou_scores(
    returns: pd.DataFrame,
    window: int = 60,
    dt: float = DEFAULT_DT,
    mode: str = "mp",
    fixed_k: int | None = None,
    variance_target: float = 0.55,
    max_tau_fraction: float = 0.5,
) -> dict:
    """Slide the trailing window; per date t fit the OU to each name's
    cumulative residual and emit the s-score, kappa, and speed-filter flag."""
    cols = list(returns.columns)
    n = len(cols)
    dates, s_rows, k_rows, ok_rows = [], [], [], []

    for end in range(window, len(returns) + 1):
        win = returns.iloc[end - window : end]
        res = residuals_for_window(win, mode, fixed_k, variance_target)
        cum = res["cumulative_residuals"]  # w x N

        s_vals = np.full(n, np.nan)
        k_vals = np.full(n, np.nan)
        ok_vals = np.zeros(n, dtype=bool)
        for i in range(n):
            s, fit = ou_s_score(cum[:, i], dt=dt, window=window, max_tau_fraction=max_tau_fraction)
            if fit["valid"]:
                k_vals[i] = fit["kappa"]
            s_vals[i] = s
            ok_vals[i] = fit["speed_ok"]

        dates.append(returns.index[end - 1])
        s_rows.append(s_vals)
        k_rows.append(k_vals)
        ok_rows.append(ok_vals)

    index = pd.DatetimeIndex(dates, name="date")
    return {
        "sscore": pd.DataFrame(s_rows, index=index, columns=cols),
        "kappa": pd.DataFrame(k_rows, index=index, columns=cols),
        "speed_ok": pd.DataFrame(ok_rows, index=index, columns=cols),
    }


# ---------------------------------------------------------------------------
# Artifacts
# ---------------------------------------------------------------------------

# Avellaneda-Lee default entry/exit bands — starting points only; these are
# exactly the overfitting-prone knobs QR2's deflated Sharpe must protect.
OPEN_LONG, OPEN_SHORT = -1.25, 1.25


def plot_sscores(sscore: pd.DataFrame, speed_ok: pd.DataFrame, kappa: pd.DataFrame, path: Path):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, (top, bottom) = plt.subplots(2, 1, figsize=(10, 7))

    pooled = sscore.to_numpy().ravel()
    pooled = pooled[np.isfinite(pooled)]
    top.hist(pooled, bins=80, color="tab:blue", alpha=0.8)
    for band, label in ((OPEN_LONG, "open long -1.25"), (OPEN_SHORT, "open short +1.25")):
        top.axvline(band, color="red", ls="--", lw=1.1, label=label)
    top.set_xlabel("s-score")
    top.set_ylabel("count (name-days)")
    top.set_title(f"Pooled s-score distribution (mean {pooled.mean():.2f}, std {pooled.std():.2f})")
    top.legend(fontsize=8)
    top.set_xlim(-6, 6)

    frac = speed_ok.mean(axis=1)
    med_hl = half_life_days(kappa.median(axis=1))
    bottom.plot(frac.index, frac, lw=1.0, color="tab:green", label="fraction passing speed filter")
    bottom.set_ylabel("fraction of universe")
    bottom.set_ylim(0, 1)
    bottom.set_xlabel("window end date")
    twin = bottom.twinx()
    twin.plot(med_hl.index, med_hl, lw=0.8, color="tab:orange", label="median half-life (days)")
    twin.set_ylabel("median half-life (days)")
    lines = bottom.get_lines() + twin.get_lines()
    bottom.legend(lines, [ln.get_label() for ln in lines], loc="upper left", fontsize=8)

    fig.tight_layout()
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=150)
    plt.close(fig)
    logger.info(f"s-score plot written to {path}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--returns", type=Path, default=Path("data/universe/universe_returns.parquet")
    )
    parser.add_argument("--window", type=int, default=60)
    parser.add_argument("--mode", choices=["mp", "fixed", "variance"], default="mp")
    parser.add_argument("--fixed-k", type=int, default=None)
    parser.add_argument("--variance-target", type=float, default=0.55)
    parser.add_argument(
        "--max-tau-fraction",
        type=float,
        default=0.5,
        help="reject names whose mean-reversion time exceeds this fraction of the window",
    )
    parser.add_argument("--out-dir", type=Path, default=Path("data/universe"))
    parser.add_argument("--plot", type=Path, default=Path("docs/research/statarb/ou_sscore.png"))
    args = parser.parse_args()

    returns = pd.read_parquet(args.returns)
    out = rolling_ou_scores(
        returns,
        window=args.window,
        mode=args.mode,
        fixed_k=args.fixed_k,
        variance_target=args.variance_target,
        max_tau_fraction=args.max_tau_fraction,
    )
    sscore, kappa, speed_ok = out["sscore"], out["kappa"], out["speed_ok"]

    args.out_dir.mkdir(parents=True, exist_ok=True)
    sscore.to_parquet(args.out_dir / "ou_sscore.parquet")
    kappa.to_parquet(args.out_dir / "ou_kappa.parquet")
    speed_ok.to_parquet(args.out_dir / "ou_speed_ok.parquet")

    pooled = sscore.to_numpy().ravel()
    pooled = pooled[np.isfinite(pooled)]
    pass_rate = float(speed_ok.to_numpy().mean())
    manifest = {
        "generated": date.today().isoformat(),
        "window": args.window,
        "dt": DEFAULT_DT,
        "mode": args.mode,
        "max_tau_fraction": args.max_tau_fraction,
        "windows_evaluated": len(sscore),
        "span": [sscore.index[0].date().isoformat(), sscore.index[-1].date().isoformat()],
        "speed_filter_pass_rate": pass_rate,
        "median_half_life_days": float(half_life_days(np.nanmedian(kappa.to_numpy()))),
        "sscore_distribution": {
            "count": int(pooled.size),
            "mean": float(pooled.mean()),
            "std": float(pooled.std()),
            "min": float(pooled.min()),
            "max": float(pooled.max()),
        },
        "open_bands": {"open_long": OPEN_LONG, "open_short": OPEN_SHORT},
        "as_of_alignment": (
            f"The window ending at date t fits the OU to each name's cumulative residual over "
            f"r_{{t-{args.window}+1}}..r_t; s-scores at t use data <= t only. Consumers must "
            "not execute before t+1."
        ),
        "note": (
            "On the real pipeline X_last ~ 0 (QR4.3 residuals sum to zero), so the s-score "
            "reduces to ~ -m/sigma_eq; the OU equilibrium m carries the signal."
        ),
    }
    (args.out_dir / "ou_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

    plot_sscores(sscore, speed_ok, kappa, args.plot)
    logger.info(
        f"OU s-scores done: {len(sscore)} windows, speed-filter pass rate {pass_rate:.1%}, "
        f"median half-life {manifest['median_half_life_days']:.1f} days, "
        f"s-score mean/std {pooled.mean():.2f}/{pooled.std():.2f}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
