# QR5.5 — Judging the meta-layer (Engine B + DSR + MDA)

The capstone of Track QR: the same truth serum — realistic fills, deflation, and purged-CV importance — turned on the learned meta-layer itself. All three lenses are leak-free.

## Lens 1 — meta-on vs meta-off under Engine B (full-depth fills)

Net-of-cost Sharpe of the meta-sized books (QR5.4) run through the same depth fill model as QR4.7. `meta_off` reproduces the raw QR4.5 book, so it lands on QR4.7's stat_arb numbers (a cross-check).

| Mode | Size | Net Sharpe (Engine B) | Net PnL |
|---|---|---|---|
| meta_off | 1× | **0.71** | 720,000 |
| meta_off | 10× | **0.76** | 7,180,000 |
| meta_off | 50× | **0.69** | 32,730,000 |
| meta_gate | 1× | **0.20** | 130,000 |
| meta_gate | 10× | **0.19** | 1,250,000 |
| meta_gate | 50× | **0.17** | 5,620,000 |
| meta_size | 1× | **0.19** | 130,000 |
| meta_size | 10× | **0.19** | 1,230,000 |
| meta_size | 50× | **0.16** | 5,360,000 |

**At 50× size, net Sharpe: meta_off 0.69, meta_gate 0.17, meta_size 0.16.** Gating/sizing on the meta model *destroys* risk-adjusted return — it keeps ~18% of the trades (1,419 vs 7,747 rebalances) but the survivors are not better-selected (QR5.3: 0.500 CV), so the 15-name book's diversification collapses without any compensating skill and the Sharpe craters.

## Lens 2 — Deflated Sharpe for both (meta search deflation)

Swept **13 meta configs** (mode × floor) on the cost-free paper PnL and deflated against the dispersion of that search (as in QR2.5; the Engine B haircut above applies on top).

- Best meta-on config: `gate@0.52`
- Expected max Sharpe under the null: SR*₀ = 0.017 (per-period)
- meta_off: annualized Sharpe 0.92, PSR(0) 0.987, **DSR 0.943**
- meta_on:  annualized Sharpe 0.47, PSR(0) 0.959, **DSR 0.771**

The meta search cannot produce a config whose deflated Sharpe beats doing nothing (meta_off) — meta_on's DSR is below meta_off's, so the layer adds no search-corrected edge.

## Lens 3 — MDA feature importance under purged CV

Permutation importance scored on purged test folds — the mean accuracy drop when each feature is shuffled. Positive ⇒ the model leans on the feature; ≤ 0 ⇒ it carries no leak-free signal (permuting it doesn't hurt, or even helps — a fingerprint of mild overfit).

First, the model's own scoreline: pooled purged-CV accuracy **0.500** vs a **0.502** majority baseline — it sits on the coin-flip (QR5.3). MDA sharpens *why*:

| Feature | MDA importance | t-stat |
|---|---|---|
| sscore | +0.0188 | +8.86 |
| dow | +0.0027 | +1.78 |
| vol_ratio | -0.0022 | -2.57 |
| regime | -0.0032 | -3.72 |
| vol_5 | -0.0039 | -4.37 |
| kappa | -0.0047 | -3.90 |
| vol_21 | -0.0058 | -6.23 |
| abs_sscore | -0.0074 | -5.36 |

The *only* feature with positive importance is `sscore` (+0.0188) — the signed s-score, i.e. the primary signal's own sign, which the meta-model merely rediscovers rather than adding to. Every **engineered** meta feature — the `abs_sscore` conviction proxy, `regime`, the vol pair, `kappa`, the order-flow `vol_ratio` — has ≤ 0 importance: none tells winning QR4 bets from losers out-of-sample. So the classifier, at best, re-derives the signal it was handed and still cannot clear the baseline. (The per-feature t-stats treat the 10 permutation repeats within each fold as independent, so they overstate significance; the robust reading is the sign and ranking, not the magnitude.)

## Verdict

Meta-labeling, honestly validated, **adds nothing here**, and applied naively it *subtracts* (Engine-B Sharpe 0.69 → 0.17). This is the intended payoff of the whole track: the guardrails (purged CPCV, DSR, MDA) let the project state a clean negative with conviction instead of shipping an overfit story. The one defensible ML addition was worth building precisely so it could be *rejected* on the evidence.
