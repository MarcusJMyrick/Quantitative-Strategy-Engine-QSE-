# Execution Intelligence (QR-P4) — OFI / VPIN toxicity filter

Reframed from standalone alpha to an **execution-timing filter** (QR-Data,
[thesis limitations §1](../../thesis/limitations.md)): our depth is
L1-reconstructed and the fill path is REST at hundreds of ms, so microstructure
features are used to *time entries* and avoid crossing the spread into toxic,
one-sided flow — measured by whether they improve fills in the A/B audit, not by
predicting price.

Build order: QR-Data (settled) → QR1.1 OFI engine → QR1.2 VPIN engine →
QR1.3 toxicity filter in `OrderManager`.

## QR1.1 — OFI engine ✅

[`include/qse/microstructure/OFICalculator.h`](../../../include/qse/microstructure/OFICalculator.h)
(header-only; verified by `tests/cpp/OFITest.cpp`, 11 gtests). Order Flow
Imbalance (Cont-Kukanov-Stoikov) on L1 quotes: per event (two consecutive
snapshots) `OFI = ΔV_bid − ΔV_ask`, where each side's contribution is
conditional on how its price moved:

| bid moves | contribution | ask moves | contribution |
|---|---|---|---|
| up | **+**new bid size (demand steps up) | up | **+**prior ask size (supply withdrawn → bullish) |
| down | **−**prior bid size (demand withdrawn) | down | **−**new ask size (new supply → bearish) |
| flat | new − prior size (Δ) | flat | −(new − prior) size |

Positive OFI = net buying pressure. Sizes are cast to `double` before
differencing so the unsigned `Volume` (uint64) never underflows on a shrinking
level. `OFICalculator` keeps a rolling-window sum for use as a live
`OrderManager` filter (QR1.3); `event_ofi(...)` is a pure static for reuse.

**Verified (the done-when):** a hand-built tick sequence with known level
changes reproduces the per-event OFI and the running sum. Each of the six
price-move cases (bid/ask × up/down/flat) is checked in isolation, plus a
shrinking-size case (no unsigned underflow), a combined bullish event, the
rolling-window eviction, first-snapshot-has-no-event, and reset.

**Scope caveat.** Computed on L1-reconstructed quotes, so this is a
toxicity/execution-timing signal, not an "OFI predicts price" alpha — see
[limitations §1](../../thesis/limitations.md).

### Reproduce

```bash
./build/run_tests --gtest_filter='OFITest.*'
```

## QR1.2 — VPIN engine — *next*

## QR1.3 — Toxicity filter in `OrderManager` — *pending*
