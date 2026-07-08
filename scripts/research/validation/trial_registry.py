"""QR2.3 — Trial registry: params + return series for every backtest variation.

The Deflated Sharpe (QR2.4) is only honest if the number of trials and the
dispersion of their Sharpes are *real*, not asserted. This registry is that
bookkeeping: every configuration tried (s-score bands, window, factor count,
depth assumptions, ...) logs its params AND its realized return series to a
directory, so V[SR] across trials is computable after the fact.

Each trial is stored self-describingly under `root/`:
  <trial_id>.params.json   the params, plus n and a convenience Sharpe
  <trial_id>.returns.parquet   the return series (index preserved explicitly)

`trial_id` is a content hash of the canonical params, so re-logging the same
configuration is idempotent (it overwrites in place, never inflating the count)
— which matters, because an inflated trial count would *over*-deflate the DSR.
"""

import hashlib
import json
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import pandas as pd

TRADING_DAYS = 252.0


def annualized_sharpe(returns: pd.Series, periods: float = TRADING_DAYS) -> float:
    """Plain annualized Sharpe (sample std, ddof=1); 0 if degenerate."""
    r = pd.Series(returns).dropna()
    if len(r) < 2 or r.std(ddof=1) == 0:
        return 0.0
    return float(r.mean() / r.std(ddof=1) * np.sqrt(periods))


def canonical_params(params: dict) -> str:
    """Deterministic JSON for hashing (sorted keys, str fallback)."""
    return json.dumps(params, sort_keys=True, default=str)


def trial_id(params: dict) -> str:
    return hashlib.sha1(canonical_params(params).encode()).hexdigest()[:12]


@dataclass
class Trial:
    trial_id: str
    params: dict
    returns: pd.Series

    @property
    def sharpe(self) -> float:
        return annualized_sharpe(self.returns)


class TrialRegistry:
    """Persist and reload {params -> return series} for a parameter sweep."""

    def __init__(self, root):
        self.root = Path(root)
        self.root.mkdir(parents=True, exist_ok=True)

    def _paths(self, tid: str) -> tuple[Path, Path]:
        return self.root / f"{tid}.params.json", self.root / f"{tid}.returns.parquet"

    def log(self, params: dict, returns: pd.Series) -> str:
        """Store one trial; returns its id. Idempotent on identical params."""
        tid = trial_id(params)
        params_path, returns_path = self._paths(tid)
        returns = pd.Series(returns)
        # store the index explicitly so any index type round-trips
        pd.DataFrame({"index": returns.index, "ret": returns.to_numpy()}).to_parquet(returns_path)
        params_path.write_text(
            json.dumps(
                {
                    "trial_id": tid,
                    "params": params,
                    "n": int(returns.notna().sum()),
                    "sharpe": annualized_sharpe(returns),
                },
                indent=2,
                default=str,
            )
            + "\n"
        )
        return tid

    def load(self, tid: str) -> Trial:
        params_path, returns_path = self._paths(tid)
        meta = json.loads(params_path.read_text())
        df = pd.read_parquet(returns_path)
        returns = pd.Series(df["ret"].to_numpy(), index=df["index"].to_numpy(), name="ret")
        return Trial(tid, meta["params"], returns)

    def trial_ids(self) -> list[str]:
        return sorted(p.stem.replace(".params", "") for p in self.root.glob("*.params.json"))

    def load_all(self) -> list[Trial]:
        return [self.load(tid) for tid in self.trial_ids()]

    def sharpes(self) -> dict[str, float]:
        """{trial_id -> Sharpe} — the dispersion input the DSR (QR2.4) needs."""
        out = {}
        for tid in self.trial_ids():
            params_path, _ = self._paths(tid)
            out[tid] = float(json.loads(params_path.read_text())["sharpe"])
        return out

    def __len__(self) -> int:
        return len(self.trial_ids())


def run_sweep(registry: TrialRegistry, grid: list[dict], run_fn) -> list[str]:
    """Run `run_fn(params) -> return series` over a param grid, logging each
    trial. Returns the list of trial ids (deduplicated by params)."""
    ids = []
    for params in grid:
        returns = run_fn(params)
        ids.append(registry.log(params, returns))
    return ids
