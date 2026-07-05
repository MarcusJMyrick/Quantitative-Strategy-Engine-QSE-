# A/B Slippage Audit

*Generated 2026-07-05 by `scripts/analysis/slippage_audit.py`*

## Headline

At **25,000 shares per signal**, the naive backtester reports
**$152,900** PnL while the full-depth engine reports
**$-660,800** — **$813,700 of phantom
profit** that exists only under the infinite-liquidity assumption
(naive Sharpe 1.93 vs real -5.26).

## Per-regime results

| Size/signal | Naive PnL | Real PnL | Phantom $ | Phantom $/share | Naive Sharpe | Real Sharpe |
|---|---|---|---|---|---|---|
| 1,000 | $-10,000 | $-18,000 | $8,000 | 0.0176 | -2.12 | -3.71 |
| 5,000 | $-7,900 | $-112,900 | $105,000 | 0.0462 | -0.50 | -4.59 |
| 25,000 | $152,900 | $-660,800 | $813,700 | 0.0715 | 1.93 | -5.26 |

## Method

- Identical SMA 20/50 crossover signals (455 per run) on the AAPL
  minute-tick file, long/flat, executed as market orders
- **Engine A (naive):** fills at the tick mid; no spread, no impact,
  no queue — the standard tutorial-backtester assumption
- **Engine B (institutional):** full-depth book with a one-tick
  half-spread and a 12-level uniform depth profile behind the touch
  (each level as thick as displayed volume, the profile validated in
  the [impact study](results_summary.md)); market orders walk levels
  and pay the consumed-liquidity VWAP
- Both engines share signals, data, cash, and mark-to-market — the
  only difference is the fill model, so the entire PnL gap is
  execution cost
- Fully deterministic: no randomness anywhere, reproducible
  run-to-run

## Interpretation

Per-share phantom cost grows with order size (spread cost plus
book-walk impact), so the distortion is superlinear in size: the
regime where the naive backtester is most confidently wrong is
exactly the size a profitable-looking strategy would scale into.

![Slippage audit](slippage_audit.png)
