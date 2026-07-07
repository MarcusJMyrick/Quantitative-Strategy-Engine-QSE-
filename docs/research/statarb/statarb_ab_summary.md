# QR4.7 — Stat arb vs baselines under Engine B (A/B audit)

Net Sharpe under the full-depth book (**depth**) vs the naive infinite-liquidity fills (**naive**), per order-size regime. Phantom profit is the dollar PnL the naive fills hallucinate (naive − depth).

> **Provisional.** These Sharpes are candidates, not results, until QR2.5 deflates them for the configurations tried (QR-P2). Engine B's daily depth is synthesized from IEX-partial volume (a stated approximation); the A-vs-B gap growing with size is the robust finding.

## stat_arb

| Size | Naive PnL | Depth PnL | Phantom $ | Phantom % | Naive Sharpe | **Depth Sharpe** |
|---|---|---|---|---|---|---|
| 1× | 870,000 | 720,000 | 150,000 | 17.2% | 0.86 | **0.71** |
| 10× | 8,700,000 | 7,180,000 | 1,520,000 | 17.5% | 0.92 | **0.76** |
| 50× | 43,510,000 | 32,730,000 | 10,780,000 | 24.8% | 0.92 | **0.69** |

## momentum

| Size | Naive PnL | Depth PnL | Phantom $ | Phantom % | Naive Sharpe | **Depth Sharpe** |
|---|---|---|---|---|---|---|
| 1× | 700,000 | 650,000 | 50,000 | 7.1% | 0.85 | **0.81** |
| 10× | 6,950,000 | 6,530,000 | 420,000 | 6.0% | 0.90 | **0.85** |
| 50× | 34,760,000 | 32,340,000 | 2,420,000 | 7.0% | 0.90 | **0.84** |

## reversal

| Size | Naive PnL | Depth PnL | Phantom $ | Phantom % | Naive Sharpe | **Depth Sharpe** |
|---|---|---|---|---|---|---|
| 1× | -216,000 | -495,000 | 279,000 | 129.2% | -0.27 | **-0.63** |
| 10× | -2,164,000 | -4,973,000 | 2,809,000 | 129.8% | -0.28 | **-0.63** |
| 50× | -10,818,000 | -28,146,000 | 17,328,000 | 160.2% | -0.27 | **-0.71** |

## Headline (largest size regime)

Under Engine B at 50× size, net Sharpe: stat_arb 0.69, momentum 0.84, reversal -0.71. The elaborate stat arb does not clearly beat cheap 12-1 momentum once realistic fills are charged — and both clear the reversal floor. Whether *anything* survives is settled only after QR-P2 deflation.
