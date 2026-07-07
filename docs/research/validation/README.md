# The Truth Serum (QR-P2) — CPCV + Deflated Sharpe

The credibility layer that judges QR-P1 and everything after. QR-P1 produced a
provisional finding — cheap momentum beats the eigen stat arb net of Engine B
costs. QR-P2 is what turns "provisional" into "judged": it prevents the two
ways financial cross-validation leaks (purging + embargo), builds a
*distribution* of out-of-sample Sharpes (CPCV), and deflates the reported
Sharpe for the number of configurations tried (DSR).

Build order: QR2.1 purge/embargo → QR2.2 CPCV → QR2.3 trial registry →
QR2.4 PSR→DSR → QR2.5 wire QR4 through it.

## QR2.1 — Purge + embargo primitives ✅

[`scripts/research/validation/purge_embargo.py`](../../../scripts/research/validation/purge_embargo.py)
(verified by `tests/python/test_purge_embargo.py`, 12 cases). The López de Prado
leak fixes (AFML ch. 7), as pure functions on integer bar positions so QR2.2's
CPCV and QR5's triple-barrier labels both reuse them.

Each observation carries an **information window** `[start, end]` — the bars
whose data determine its label (one bar for a daily strategy, `[i, i+h]` for a
triple-barrier hold).

- **Purge.** A training observation is dropped if its window overlaps *any*
  test window (`a_start ≤ b_end AND b_start ≤ a_end`). The check is symmetric,
  so it catches both a train label reaching forward into the test region and a
  test label reaching back — otherwise the "out-of-sample" test shares bars
  with training and isn't out of sample at all.
- **Embargo.** Serial correlation leaks forward even across non-overlapping
  windows, so an extra block of `embargo_size = int(n · embargo_pct)` training
  bars immediately *after* each test region is dropped (AFML's convention).

**Verified (the done-when):** after purging, no surviving training window
overlaps any test window (brute-force checked on multi-bar labels); and the
embargo removes *exactly* `int(n · embargo_pct)` bars after a test block (5 / 10
/ 20 at 5% / 10% / 20% on n=100; 28 at 2% on n=1432) — clipped correctly at the
series end, composing cleanly with purge and across the multi-block test sets
CPCV produces.

## QR2.2 — Combinatorial Purged CV ✅

[`scripts/research/validation/cpcv.py`](../../../scripts/research/validation/cpcv.py)
(verified by `tests/python/test_cpcv.py`, 12 cases). Standard k-fold gives one
out-of-sample estimate; CPCV (AFML ch. 12) gives a *distribution*. Partition the
series into N contiguous groups, use every k-subset as the test set —
**C(N, k)** splits — and purge+embargo the training complement of each (QR2.1).
Because each group is tested in **C(N−1, k−1) = φ** splits, the per-split test
predictions recombine into **φ full-length backtest paths**, each a complete
out-of-sample equity curve. The spread of Sharpes across those paths is exactly
the variance QR2.4's Deflated Sharpe consumes.

`path_assignments` tiles the C(N, k) splits into φ paths × N groups (each
(split, test-group) pair used exactly once); `assemble_paths` stitches per-split
test predictions into the φ curves.

**Verified (the done-when):** split and path counts for small (N, k) —
N=6,k=2 → **15 splits, 5 paths** (AFML's worked example) — and every observation
appears in the test set of exactly φ splits. Train/test are disjoint and purge
bites at block boundaries under multi-bar labels. On the real QR4 series
(n=1,432, N=6, k=2, 1% embargo): 15 splits, 5 paths, every bar tested exactly
5×, mean training set 933/1,432 bars after purge+embargo.

## QR2.3 — Trial registry — *next*

## QR2.3 — Trial registry — *pending*

## QR2.4 — PSR → Deflated Sharpe — *pending*

## QR2.5 — Wire QR4 through it (DSR on the tearsheet) — *pending*
