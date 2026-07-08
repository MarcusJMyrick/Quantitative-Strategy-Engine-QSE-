# Risk Architecture (QR-P3) — HMM regime overlay

Regime models detect the regime *after* it is underway and overfit easily, so
the honest expectation is **not** added return — it is **cut drawdown**: flip
the A5 risk-aversion λ toward minimum-variance / lower gross when the market
turns, which lifts Sharpe by shrinking the denominator. This track builds that
overlay while avoiding the look-ahead trap that silently inflates most published
regime backtests.

Build order: QR3.1 causal features → QR3.2 Gaussian HMM (filtered) → QR3.3
anti-whipsaw → QR3.4 integrate with the A5 λ.

## QR3.1 — Regime features ✅

[`scripts/research/regime/regime_features.py`](../../../scripts/research/regime/regime_features.py)
(verified by `tests/python/test_regime_features.py`, 7 cases). Causal
market-regime features on the SPY proxy, which the HMM will cluster into states
— it never sees returns directly. Every feature is a **trailing-window**
statistic, so the value at date t uses only data at dates ≤ t.

| Feature | Meaning |
|---|---|
| `rv_21` | 21-day annualized realized volatility — the primary regime axis |
| `rv_5` | 5-day annualized realized vol — fast moves that lead `rv_21` |
| `vov_21` | 21-day std of `rv_21` — vol-of-vol (how unstable vol itself is) |
| `range_5` | 5-day mean of (high−low)/close — intraday-range / spread-expansion proxy |
| `vol_ratio_63` | log(volume / 63-day mean volume) — volume profile |

![Causal regime features on SPY](regime_features.png)

**Data.** SPY daily bars fetched through the same Alpaca IEX path as the QR4
universe, so the features span **2020-10-22 → 2026-07-07** and align almost
exactly with the stat-arb signal dates (the overlay can map regime → λ
day-by-day). Computed on *unadjusted* SPY — dividends (~0.3–0.5%/quarter) are
negligible for volatility/range features, and the volume *ratio* is robust to
the IEX-partial feed (numerator and denominator scale together). The window
begins mid-2020, so it captures the 2022 bear (rv_21 ≈ 0.24) and the April 2025
selloff (peak rv_21 ≈ 0.49) but **not** the March 2020 COVID crash — a stated
coverage limitation.

**As-of contract (the no-look-ahead guarantee QR3.2 rests on):** row t is a
trailing-window statistic of rows ≤ t; warm-up rows are dropped (the frame
begins after the 63-day volume window fills). The HMM must consume the
*filtered* feature at t and act no earlier than t+1.

**Verified (the done-when):** a clean 1,431 × 5 feature frame with zero NaNs;
the features separate a synthetic calm→turbulent regime shift (rv_21 more than
doubles); and **strict causality** — appending or perturbing any future data
leaves every already-emitted row bit-identical, and `rv_21` at t matches the
std of exactly the trailing 21 returns ending at t.

### Reproduce

```bash
# refetch SPY (needs APCA_* env) or use the committed data/regime/SPY.csv
venv/bin/python scripts/research/regime/regime_features.py
venv/bin/python -m pytest tests/python/test_regime_features.py -q
```

Outputs: `data/regime/regime_features.parquet`, `data/regime/regime_manifest.json`,
and the committed plot above.

## QR3.2 — Gaussian HMM (filtered, no look-ahead) — *next*

## QR3.3 — Anti-whipsaw — *pending*

## QR3.4 — Integrate with the A5 λ — *pending*
