"""QR5.3 — assemble the meta-labeling dataset and run the purged-CV meta-model.

For every QR4 entry event, gather causal (as-of entry) features that never encode
the direction of returns, attach the QR5.1 triple-barrier label and QR5.2
uniqueness weight, and evaluate a logistic meta-model through purged CPCV.

Features (all as-of the entry bar t0):
  abs_sscore  |s-score| — the primary model's conviction
  sscore      signed s-score
  kappa       OU mean-reversion speed at entry
  regime      committed HMM regime (QR3.3; forward-filled, −1 before it starts)
  vol_21/5    trailing realized vol of the name (slow / fast)
  vol_ratio   log(volume / 63d mean) — a daily order-flow proxy
  dow         day of week (a weak time feature)

Note (thesis limitations §1): the tick-level VPIN/OFI of QR1 are not available
per-name at daily frequency, so `vol_ratio` stands in as the daily order-flow
proxy. The regime and s-score features carry the QR3/QR4 information.

Output: data/universe/meta_dataset.parquet (features + label + weight + windows).
"""

import argparse
import sys
from pathlib import Path

import numpy as np
import pandas as pd

sys.path.insert(0, str(Path(__file__).resolve().parent))

from meta_model import purged_cpcv_splits, train_cpcv  # noqa: E402
from sample_uniqueness import weights_for_labels  # noqa: E402
from triple_barrier import apply_triple_barrier, extract_entry_events  # noqa: E402

FEATURES = ["abs_sscore", "sscore", "kappa", "regime", "vol_21", "vol_5", "vol_ratio", "dow"]


def build_dataset(data_dir: Path, pt: float, sl: float, max_holding: int) -> pd.DataFrame:
    pos = pd.read_parquet(data_dir / "signal_positions.parquet")
    rets = pd.read_parquet(data_dir / "universe_returns.parquet")
    sscore = pd.read_parquet(data_dir / "ou_sscore.parquet")
    kappa = pd.read_parquet(data_dir / "ou_kappa.parquet")
    prices = pd.read_csv(data_dir / "prices.csv", parse_dates=["date"])
    volume = prices.pivot(index="date", columns="symbol", values="volume")
    regime = pd.read_parquet(data_dir.parent / "regime" / "regime_committed.parquet")

    dates = pos.index
    pos_of = {d: i for i, d in enumerate(dates)}
    # causal per-name features on the universe date grid
    vol_21 = rets.rolling(21).std()
    vol_5 = rets.rolling(5).std()
    vol_ratio = np.log(volume / volume.rolling(63).mean()).reindex(dates)
    reg = regime["committed_state"].reindex(dates).ffill()  # regime known as-of t0

    rows = []
    for name in pos.columns:
        px = (1 + rets[name]).cumprod().reindex(dates).dropna()
        events = extract_entry_events(pos[name].reindex(px.index).fillna(0))
        if events.empty:
            continue
        labels = apply_triple_barrier(px, events, pt=pt, sl=sl, max_holding=max_holding)
        for t0, lab in labels.iterrows():
            rows.append(
                {
                    "name": name,
                    "t0": t0,
                    "abs_sscore": abs(sscore.at[t0, name]),
                    "sscore": sscore.at[t0, name],
                    "kappa": kappa.at[t0, name],
                    "regime": reg.get(t0, np.nan),
                    "vol_21": vol_21.at[t0, name],
                    "vol_5": vol_5.at[t0, name],
                    "vol_ratio": vol_ratio.at[t0, name],
                    "dow": t0.dayofweek,
                    "label": int(lab["label"]),
                    "ret": lab["ret"],
                    "t0_idx": pos_of[t0],
                    "t1_idx": pos_of[lab["t1"]],
                }
            )

    df = pd.DataFrame(rows)
    df["regime"] = df["regime"].fillna(-1.0)  # events before the regime series starts
    df = df.dropna(subset=FEATURES).reset_index(drop=True)  # drop warm-up NaNs
    df["weight"] = weights_for_labels(
        df.set_index("t0"), n_bars=len(dates), group_col="name"
    ).to_numpy()
    return df, len(dates)


def evaluate(df: pd.DataFrame, n_bars: int, n_groups: int, k: int, embargo_pct: float) -> dict:
    X = df[FEATURES].to_numpy()
    y = df["label"].to_numpy()
    w = df["weight"].to_numpy()
    splits = purged_cpcv_splits(
        df["t0_idx"].to_numpy(), df["t1_idx"].to_numpy(), n_groups, k, embargo_pct, n_bars
    )
    results = train_cpcv(X, y, splits, sample_weight=w, l2=1.0)
    # pool out-of-sample predictions across splits (every event tested phi times)
    all_p, all_y = [], []
    for r in results:
        all_p.append(r["proba"])
        all_y.append(y[r["test"]])
    p = np.concatenate(all_p)
    yy = np.concatenate(all_y)
    acc = ((p > 0.5) == yy).mean()
    base = max(yy.mean(), 1 - yy.mean())  # majority-class baseline
    return {"splits": len(splits), "oos_preds": len(p), "cv_accuracy": acc, "baseline": base}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--data-dir", type=Path, default=Path("data/universe"))
    parser.add_argument("--pt", type=float, default=0.03)
    parser.add_argument("--sl", type=float, default=0.03)
    parser.add_argument("--max-holding", type=int, default=10)
    parser.add_argument("--n-groups", type=int, default=6)
    parser.add_argument("--k", type=int, default=2)
    parser.add_argument("--embargo-pct", type=float, default=0.01)
    args = parser.parse_args()

    df, n_bars = build_dataset(args.data_dir, args.pt, args.sl, args.max_holding)
    out = args.data_dir / "meta_dataset.parquet"
    df.to_parquet(out)
    print(f"meta dataset: {len(df)} events x {len(FEATURES)} features -> {out}")
    print(f"label balance (win): {df['label'].mean():.3f}")

    m = evaluate(df, n_bars, args.n_groups, args.k, args.embargo_pct)
    print(
        f"purged CPCV ({args.n_groups} groups, k={args.k}): {m['splits']} splits, "
        f"{m['oos_preds']} OOS preds, CV accuracy {m['cv_accuracy']:.3f} "
        f"(majority baseline {m['baseline']:.3f})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
