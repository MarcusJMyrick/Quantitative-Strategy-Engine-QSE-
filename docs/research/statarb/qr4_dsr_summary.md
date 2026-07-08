# QR4 under CPCV + Deflated Sharpe (QR2.5)

Swept **12 configurations** of QR4's entry/exit bands × estimation window — the overfitting-prone knobs — logging each to the trial registry with its cost-free paper-PnL return series.

## The deflated result

- Best config: `{'bands': [-1.25, 1.25, -0.5, 0.75], 'window': 60}`
- Best annualized Sharpe (cost-free): **0.92**
- Trials deflated against: **N = 12**
- Expected max Sharpe under the null (per-period): SR*₀ = 0.051
- Undeflated PSR(0): 0.987
- **Deflated Sharpe Ratio: DSR = 0.610**

## Temporal stability (CPCV blocks)

Per-block out-of-sample per-period Sharpe of the chosen config across 6 time blocks: mean 0.054, std 0.053 (min -0.009, max 0.129).

## Reading it

DSR = 0.61 is the probability the chosen config's *true* Sharpe beats what the luckiest of 12 skill-less configurations would have produced. It clears 0.5, so the paper edge is not purely an artifact of the band/window search — but this is the *cost-free* series; the Engine B haircut (QR4.7) applies on top, so the net-of-cost, deflated verdict is lower still.
