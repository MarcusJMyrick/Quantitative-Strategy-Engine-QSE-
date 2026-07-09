# Learned Meta-Layer (QR-P5) — meta-labeling

The one defensible ML addition, and the capstone. QR4's s-score still decides
the **side** of each bet; a classifier decides **whether to act and how big**,
trained on features from QR1 (VPIN/OFI), QR3 (regime), and s-score magnitude —
López de Prado meta-labeling, which improves precision and sizes bets **without
ever predicting the direction of returns**. That is why it survives where
return-prediction ML doesn't, and why it is gated behind QR-P2: it is only
trustworthy validated under purging/embargo and judged by the same DSR.

Build order: QR5.1 triple-barrier labels → QR5.2 sample uniqueness →
QR5.3 meta-model under purged CV → QR5.4 probability → size/gate →
QR5.5 judge it (Engine B + DSR + MDA feature importance).

## QR5.1 — Triple-barrier labels ✅

[`scripts/research/meta/triple_barrier.py`](../../../scripts/research/meta/triple_barrier.py)
(verified by `tests/python/test_triple_barrier.py`, 12 cases). For each QR4
entry event (t0, entry price p0, side s) three barriers are placed on the return
*in the bet's direction*, `signed = s·(pₜ/p0 − 1)`:

- **upper (profit-take):** first bar with `signed ≥ pt` → **label 1** (win)
- **lower (stop-loss):** first bar with `signed ≤ −sl` → **label 0** (loss)
- **vertical (time limit):** neither within `max_holding` bars → label by the
  sign of `signed` at the horizon

Whichever is touched first wins. The touch time `t1` is not just for the label:
`[t0, t1]` is the observation's **information window**, so QR2.1 purge/embargo
and QR5.2 sample uniqueness consume the emitted `t0_idx`/`t1_idx` directly
(`label_windows`). `apply_triple_barrier` is generic over the input series —
prices to label the realized bet, or a residual/cumulative series to label the
idiosyncratic reversion the s-score targets (QR5.3's choice).

### On the real QR4 signals

![Triple-barrier outcomes and holding time](triple_barrier_labels.png)

Labeling all 748 QR4 entry events (pt = sl = 3%, 10-bar horizon) on each name's
price path: **332 profit-take / 331 stop-loss / 85 timeout**, a meta-label
balance of **0.497** (≈50% wins) and a **median 3-bar** holding — consistent
with the ~5-day OU half-life. The near-even win rate is itself the motivation
for meta-labeling: the s-score picks the side but is barely better than a
coin-flip on *which* bets pay, so a classifier that can rank the winners has
room to add precision. Balanced labels are also clean training targets (no class
imbalance to fight).

**Verified (the done-when):** barrier-touch labeling on hand-built price paths —
up-first (profit-take → win), down-first (stop → loss), and timeout (labeled by
sign) — plus the short side, first-touch precedence when both barriers would
eventually hit, exact-threshold touches, horizon clamping at the series end, and
the entry-event extraction (opens and long↔short flips, holds skipped) that
feeds it.

### Reproduce

```bash
venv/bin/python -m pytest tests/python/test_triple_barrier.py -q
```

## QR5.2 — Sample uniqueness — *next*

## QR5.3 — Meta-model under purged CV — *pending*

## QR5.4 — Probability → size / gate — *pending*

## QR5.5 — Judge it (Engine B + DSR + MDA) — *pending*
