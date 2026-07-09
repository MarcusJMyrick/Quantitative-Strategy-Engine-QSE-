# QR1.3 — Toxicity filter A/B execution audit

On 1,893 scheduled AAPL orders, the VPIN+OFI toxicity filter **does not** reduce slippage — the filter does not earn its place on this data.

| Policy | Avg slippage/order ($) | vs arrival mid (bps) |
|---|---|---|
| Blind market order | 0.01000 | 0.499 |
| VPIN+OFI filtered | 0.01168 | 0.583 |
| **Reduction** | **-0.00168** | **-0.084** |

## Why (the decomposition)

The filter rested passive on **34** orders (toxic *and* directionally favorable); the rest crossed like blind. Of those, **27 (79%)** filled passively, capturing the spread (avg slip -0.0100). But the **7** that did *not* fill fell back to crossing after the toxic flow had run away — average slip +0.5413, the adverse-selection tail.

**The honest finding.** The adverse selection on the fallback orders (the toxic flow continuing away from the resting order) outweighs the spread captured on passive fills. On 1-minute AAPL, high VPIN predicts *continued* adverse movement, so resting passive into it is the wrong move — you only get filled when the flow reverses. A robust negative across every threshold/horizon tested.

> Provisional either way: any apparent win from sweeping the threshold / wait horizon would itself need deflating for the config search (QR-P2). Depth is L1-reconstructed (thesis limitations §1), so this is an execution-fill result, not a price-prediction claim.
