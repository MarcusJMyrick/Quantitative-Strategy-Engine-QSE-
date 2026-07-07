"""QR4.5 — Signals + dollar-neutral weights for eigenportfolio stat arb.

Turns the QR4.4 s-scores into a dollar-neutral daily target book and writes it
in the exact `weights_YYYYMMDD.csv` / `symbol,weight` format the C++ engine's
WeightsLoader + FactorStrategy already consume — the handoff point from the
Python research pipeline to the execution engine.

Trading rules (Avellaneda-Lee defaults; STARTING POINTS ONLY — these bands are
exactly the overfitting-prone knobs QR-P2's deflated Sharpe must protect). A
per-name state machine with hysteresis: open a position on a strong deviation,
hold it through a dead band, close near equilibrium.

    flat  -> long   if s < -1.25         (name cheap: residual expected to
                                           revert up)
    flat  -> short  if s > +1.25         (name rich)
    long  -> flat   if s > -0.50         (asymmetric close = drift-aware)
    short -> flat   if s < +0.75
    any   -> flat   if s is NaN          (speed filter/fit rejected the name;
                                           cannot model -> close)

A NaN s-score forces the position flat: if the OU fit no longer validates a
name we stop trusting its signal rather than holding a stale bet.

Dollar-neutral construction. Active longs and shorts are each equal-weighted so
the two sides cancel: each long gets +gross/(2*n_long), each short
-gross/(2*n_short). Net exposure is then 0 regardless of how the counts split,
and gross exposure is `gross` (default 1.0 -> 50% long / 50% short of NAV,
since the engine sizes target_notional = weight * NAV). A day with positions on
only one side cannot be neutralized, so the whole book goes flat that day.

Every universe name is written every day (inactive names at weight 0.0) so the
engine closes exited positions -- a name omitted from the file keeps its prior
target. All weights are finite and |w| well under the loader's |w| <= 10 cap.

Execution lag (no look-ahead). FactorStrategy loads `weights_<date>.csv` at that
date's close and rebalances then, so a signal computed from the window ending at
date t (which uses the close of t) must be executed no earlier than t+1. Files
are therefore dated one trading day AFTER the signal: `weights_<t+1>.csv` holds
the book chosen from data through t. The last `execution_lag` signal dates have
no in-sample execution day and are held out (a live run writes them to the real
next session).

Outputs:
  data/universe/weights/weights_YYYYMMDD.csv   per execution date (gitignored)
  data/universe/signal_weights.parquet         date x name target weights
  data/universe/signal_positions.parquet       date x name in {-1, 0, +1}
  data/universe/signal_diagnostics.parquet     date x [net, gross, n_long,
                                               n_short, turnover]
  data/universe/signal_manifest.json           bands, gross, lag, summary
Plot (committed): docs/research/statarb/signal_exposure.png
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

sys.path.insert(0, str(Path(__file__).resolve().parent))

from ou_sscore import rolling_ou_scores  # noqa: E402

logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")
logger = logging.getLogger(__name__)

LONG, FLAT, SHORT = 1, 0, -1


@dataclass(frozen=True)
class Bands:
    """Avellaneda-Lee entry/exit thresholds (the QR-P2-protected knobs)."""

    open_long: float = -1.25
    open_short: float = 1.25
    close_long: float = -0.50
    close_short: float = 0.75

    def __post_init__(self):
        # long hysteresis band [open_long, close_long], short [close_short, open_short]
        if not (self.open_long < self.close_long < self.close_short < self.open_short):
            raise ValueError("bands must satisfy open_long < close_long < close_short < open_short")


# ---------------------------------------------------------------------------
# Trading rules (pure functions, unit-tested)
# ---------------------------------------------------------------------------


def next_position(prev: int, s: float, bands: Bands) -> int:
    """One state transition given the previous position and today's s-score."""
    if not np.isfinite(s):
        return FLAT  # cannot model the name -> close
    state = prev
    if state == LONG and s > bands.close_long:
        state = FLAT
    elif state == SHORT and s < bands.close_short:
        state = FLAT
    if state == FLAT:  # entry from flat (incl. a same-bar exit)
        if s < bands.open_long:
            state = LONG
        elif s > bands.open_short:
            state = SHORT
    return state


def scan_positions(sscores: np.ndarray, bands: Bands) -> np.ndarray:
    """Run the state machine down one name's s-score series (start flat)."""
    out = np.empty(sscores.shape[0], dtype=int)
    prev = FLAT
    for i, s in enumerate(sscores):
        prev = next_position(prev, s, bands)
        out[i] = prev
    return out


def positions_from_sscores(sscore: pd.DataFrame, bands: Bands) -> pd.DataFrame:
    """Apply the state machine to every name's s-score column."""
    data = {col: scan_positions(sscore[col].to_numpy(), bands) for col in sscore.columns}
    return pd.DataFrame(data, index=sscore.index, columns=sscore.columns)


# ---------------------------------------------------------------------------
# Dollar-neutral weights (pure functions, unit-tested)
# ---------------------------------------------------------------------------


def dollar_neutral_row(positions: np.ndarray, gross: float = 1.0) -> np.ndarray:
    """Map a {-1,0,+1} position row to dollar-neutral weights: each side
    equal-weighted to +/- gross/2, so net = 0 and gross exposure = `gross`.
    If either side is empty the book cannot be neutralized -> all zeros."""
    w = np.zeros(positions.shape[0], dtype=float)
    longs = positions == LONG
    shorts = positions == SHORT
    n_long, n_short = int(longs.sum()), int(shorts.sum())
    if n_long == 0 or n_short == 0:
        return w
    w[longs] = gross / (2.0 * n_long)
    w[shorts] = -gross / (2.0 * n_short)
    return w


def weights_from_positions(positions: pd.DataFrame, gross: float = 1.0) -> pd.DataFrame:
    """Dollar-neutral target weights for every day."""
    rows = [dollar_neutral_row(positions.iloc[i].to_numpy(), gross) for i in range(len(positions))]
    return pd.DataFrame(rows, index=positions.index, columns=positions.columns)


def diagnostics(weights: pd.DataFrame, positions: pd.DataFrame) -> pd.DataFrame:
    """Per-day net/gross exposure, long/short counts, and turnover (half the
    L1 change in the weight vector day over day)."""
    net = weights.sum(axis=1)
    gross = weights.abs().sum(axis=1)
    n_long = (positions == LONG).sum(axis=1)
    n_short = (positions == SHORT).sum(axis=1)
    turnover = weights.diff().abs().sum(axis=1) / 2.0
    turnover.iloc[0] = weights.iloc[0].abs().sum() / 2.0  # from flat on day one
    return pd.DataFrame(
        {"net": net, "gross": gross, "n_long": n_long, "n_short": n_short, "turnover": turnover}
    )


# ---------------------------------------------------------------------------
# Weight-file emission (the C++ handoff)
# ---------------------------------------------------------------------------


def write_weight_files(weights: pd.DataFrame, out_dir: Path, execution_lag: int = 1) -> list:
    """Write one `weights_YYYYMMDD.csv` per execution date. The row from signal
    date index[i] is written to the file dated index[i + execution_lag] (the
    look-ahead-safe shift). Every universe name is written, inactive at 0.0.
    Returns [(exec_date, path)]."""
    out_dir.mkdir(parents=True, exist_ok=True)
    dates = weights.index
    written = []
    for i in range(len(dates) - execution_lag):
        exec_date = dates[i + execution_lag]
        row = weights.iloc[i]
        path = out_dir / f"weights_{exec_date.strftime('%Y%m%d')}.csv"
        lines = ["symbol,weight"]
        lines += [f"{sym},{row[sym]:.10g}" for sym in weights.columns]
        path.write_text("\n".join(lines) + "\n")
        written.append((exec_date, path))
    return written


# ---------------------------------------------------------------------------
# Driver + artifacts
# ---------------------------------------------------------------------------


def generate(
    returns: pd.DataFrame,
    window: int = 60,
    bands: Bands = Bands(),
    gross: float = 1.0,
    execution_lag: int = 1,
    mode: str = "mp",
    fixed_k: int | None = None,
    variance_target: float = 0.55,
    max_tau_fraction: float = 0.5,
) -> dict:
    """Full QR4.5 pipeline: s-scores -> positions -> dollar-neutral weights."""
    ou = rolling_ou_scores(
        returns,
        window=window,
        mode=mode,
        fixed_k=fixed_k,
        variance_target=variance_target,
        max_tau_fraction=max_tau_fraction,
    )
    positions = positions_from_sscores(ou["sscore"], bands)
    weights = weights_from_positions(positions, gross)
    diag = diagnostics(weights, positions)
    return {"sscore": ou["sscore"], "positions": positions, "weights": weights, "diagnostics": diag}


def plot_exposure(diag: pd.DataFrame, path: Path):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, (top, bottom) = plt.subplots(2, 1, figsize=(10, 7), sharex=True)

    top.plot(diag.index, diag["n_long"], lw=0.9, color="tab:green", label="# long")
    top.plot(diag.index, diag["n_short"], lw=0.9, color="tab:red", label="# short")
    top.set_ylabel("active names")
    top.set_title("Dollar-neutral book: exposure, and net ~ 0 by construction")
    top.legend(loc="upper left", fontsize=8)

    bottom.plot(diag.index, diag["gross"], lw=0.9, color="tab:blue", label="gross exposure")
    bottom.plot(diag.index, diag["net"], lw=1.0, color="black", label="net exposure")
    bottom.axhline(0.0, color="gray", ls=":", lw=0.8)
    bottom.set_ylabel("exposure (fraction of NAV)")
    bottom.set_xlabel("signal date")
    bottom.legend(loc="upper left", fontsize=8)

    fig.tight_layout()
    path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(path, dpi=150)
    plt.close(fig)
    logger.info(f"Exposure plot written to {path}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--returns", type=Path, default=Path("data/universe/universe_returns.parquet")
    )
    parser.add_argument("--window", type=int, default=60)
    parser.add_argument("--gross", type=float, default=1.0)
    parser.add_argument("--execution-lag", type=int, default=1)
    parser.add_argument("--open-long", type=float, default=-1.25)
    parser.add_argument("--open-short", type=float, default=1.25)
    parser.add_argument("--close-long", type=float, default=-0.50)
    parser.add_argument("--close-short", type=float, default=0.75)
    parser.add_argument("--mode", choices=["mp", "fixed", "variance"], default="mp")
    parser.add_argument("--fixed-k", type=int, default=None)
    parser.add_argument("--variance-target", type=float, default=0.55)
    parser.add_argument("--max-tau-fraction", type=float, default=0.5)
    parser.add_argument("--out-dir", type=Path, default=Path("data/universe"))
    parser.add_argument("--weights-dir", type=Path, default=Path("data/universe/weights"))
    parser.add_argument(
        "--plot", type=Path, default=Path("docs/research/statarb/signal_exposure.png")
    )
    args = parser.parse_args()

    bands = Bands(args.open_long, args.open_short, args.close_long, args.close_short)
    returns = pd.read_parquet(args.returns)
    out = generate(
        returns,
        window=args.window,
        bands=bands,
        gross=args.gross,
        execution_lag=args.execution_lag,
        mode=args.mode,
        fixed_k=args.fixed_k,
        variance_target=args.variance_target,
        max_tau_fraction=args.max_tau_fraction,
    )
    positions, weights, diag = out["positions"], out["weights"], out["diagnostics"]

    args.out_dir.mkdir(parents=True, exist_ok=True)
    positions.to_parquet(args.out_dir / "signal_positions.parquet")
    weights.to_parquet(args.out_dir / "signal_weights.parquet")
    diag.to_parquet(args.out_dir / "signal_diagnostics.parquet")
    written = write_weight_files(weights, args.weights_dir, args.execution_lag)

    active = diag[diag["gross"] > 0]
    manifest = {
        "generated": date.today().isoformat(),
        "window": args.window,
        "gross_cap": args.gross,
        "execution_lag_days": args.execution_lag,
        "bands": {
            "open_long": bands.open_long,
            "open_short": bands.open_short,
            "close_long": bands.close_long,
            "close_short": bands.close_short,
        },
        "signal_dates": len(weights),
        "weight_files_written": len(written),
        "span": [weights.index[0].date().isoformat(), weights.index[-1].date().isoformat()],
        "max_abs_net_exposure": float(diag["net"].abs().max()),
        "days_with_a_book": int((diag["gross"] > 0).sum()),
        "mean_gross_on_active_days": float(active["gross"].mean()) if len(active) else 0.0,
        "mean_n_long": float(diag["n_long"].mean()),
        "mean_n_short": float(diag["n_short"].mean()),
        "mean_daily_turnover": float(diag["turnover"].mean()),
        "as_of_alignment": (
            "Signal from the window ending at date t is written to weights_<t+"
            f"{args.execution_lag}>.csv and executed at that later date's close; no order "
            "uses data beyond its own signal date."
        ),
    }
    (args.out_dir / "signal_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

    plot_exposure(diag, args.plot)
    logger.info(
        f"Signals done: {len(written)} weight files, max|net| {manifest['max_abs_net_exposure']:.1e}, "
        f"mean gross {manifest['mean_gross_on_active_days']:.2f} on "
        f"{manifest['days_with_a_book']} active days, mean turnover "
        f"{manifest['mean_daily_turnover']:.3f}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
