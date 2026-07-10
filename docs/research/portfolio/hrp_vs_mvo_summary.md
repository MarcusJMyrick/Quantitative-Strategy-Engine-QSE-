# QR-X — MVO vs HRP out-of-sample (QR4 universe, judged under CPCV)

Walk-forward over the 15-name QR4 universe: 252-day trailing covariance, rebalanced every 21 days, weights applied to the next 21 days (59 rebalances). MVO is the minimum-variance optimum that inverts Σ; HRP clusters and bisects without ever inverting it. IVP and equal-weight are naive baselines.

| Allocator | OOS Sharpe (ann.) | Turnover / rebal | Ann. vol | CPCV block Sharpe (mean ± sd, min) |
|---|---|---|---|---|
| equal | **0.90** | 0.000 | 0.283 | 0.067 ± 0.071 (min -0.035) |
| ivp | **0.68** | 0.052 | 0.250 | 0.053 ± 0.064 (min -0.044) |
| mvo | **-0.35** | 0.370 | 0.222 | -0.020 ± 0.050 (min -0.078) |
| hrp | **0.65** | 0.098 | 0.248 | 0.051 ± 0.066 (min -0.052) |

## Reading it

- **MVO collapses out-of-sample:** Sharpe **-0.35** — on a universe of 15 co-moving, near-all-appreciating tech names the inverted Σ is so unstable it shorts legs and *loses money*, while doing it churns **0.370/rebalance (3.8× HRP)**. This is the textbook failure of matrix inversion on near-singular covariance, live on real data.
- **HRP repairs it:** Sharpe **0.65** vs MVO's -0.35 — a 1.01 swing — at 3.8× *less* turnover. Never inverting Σ, HRP stays long-only, sane, and stable: the López de Prado result, confirmed.
- **But 1/N still wins:** equal-weight posts **0.90** at **zero** turnover, beating every covariance-based allocator (HRP 0.65, IVP 0.68). On this highly-correlated, trending universe the assumption-free portfolio is the one to beat — the classic DeMiguel–Garlappi–Uppal finding.
- **Best by OOS Sharpe:** `equal` (DSR 0.756 deflating the 4-allocator choice).

## Verdict

The honest three-part result: **MVO is a disaster** (the inversion A5's mean-variance objective needs blows up on near-singular Σ), **HRP fixes the disaster** (−0.35 → 0.65 Sharpe at ~4× lower turnover — its ML-adjacent clustering buys robustness while predicting nothing), **and even HRP does not beat 1/N** here. The takeaway isn't a new alpha — it's that hierarchical clustering earns its keep as *risk control*: it makes a covariance-based book usable where the textbook optimizer is unusable, though on a small, co-moving universe naive diversification is still the bar none of them clears. Judged, as everything in this track is, under CPCV.
