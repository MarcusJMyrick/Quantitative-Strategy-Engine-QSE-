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

## QR2.3 — Trial registry ✅

[`scripts/research/validation/trial_registry.py`](../../../scripts/research/validation/trial_registry.py)
(verified by `tests/python/test_trial_registry.py`, 7 cases). The DSR is only
honest if the trial count and Sharpe dispersion are *real*, so every
configuration tried logs its params **and** its realized return series. Each
trial is stored self-describingly under `root/`: `<id>.params.json` (params +
n + a convenience Sharpe) and `<id>.returns.parquet` (the series, index
preserved). `trial_id` is a content hash of the canonical params, so re-logging
the same configuration is **idempotent** — it overwrites in place rather than
inflating the count, which matters because a phantom trial would *over*-deflate
the DSR. `run_sweep` iterates a param grid and logs each; `load_all` and
`sharpes()` reconstruct `{params → returns}` and `{id → Sharpe}` for QR2.4.

**Verified (the done-when):** a 6-point parameter sweep populates the registry
directory, and the loader reconstructs every trial's params and return series
exactly (datetime index round-trips through parquet). Re-logging identical
params keeps the count at 1; distinct params are distinct trials.

## QR2.4 — PSR → Deflated Sharpe ✅

[`scripts/research/validation/deflated_sharpe.py`](../../../scripts/research/validation/deflated_sharpe.py)
(verified by `tests/python/test_deflated_sharpe.py`, 10 cases). The headline
metric (Bailey & López de Prado).

- **PSR(SR\*)** — probability the *true* per-period Sharpe exceeds a benchmark,
  given the estimate's standard error, which grows with non-normality:
  `Φ[(SR − SR*)·√(n−1) / √(1 − skew·SR + ((kurt−1)/4)·SR²)]`. Negative skew and
  fat tails widen the error and *lower* the PSR for the same Sharpe.
- **DSR** = `PSR(SR*₀)` with the benchmark set to the **expected maximum Sharpe
  under the null** across N trials,
  `SR*₀ = √(V[SR])·[(1−γ)·Z⁻¹(1−1/N) + γ·Z⁻¹(1−1/(N·e))]` (γ = Euler-
  Mascheroni). DSR asks whether the selected strategy clears the bar the
  luckiest of N skill-less strategies would have set.

Everything is per-period (non-annualized) so PSR's SR and V[SR] share units.
`deflate_registry` reads the QR2.3 registry, picks the best trial, and deflates
it against the dispersion of all logged trials.

### The multiple-testing penalty, made visible (the done-when)

![100 noise strategies: PSR looks great, DSR deflates to chance](dsr_deflation.png)

100 pure-noise strategies (true Sharpe 0). The luckiest has per-period Sharpe
**0.162**, and its undeflated **PSR(0) = 0.994** — it looks like near-certain
skill. But the expected max under the null is **SR*₀ = 0.166**, so the
**DSR = 0.475** — deflated below chance. The penalty for searching is exactly
the gap between 0.99 and 0.47. Tested: the best-of-100 sits on SR*₀ to within
0.05, PSR(0) > 0.85 while DSR < 0.60, and a genuinely skilled single hypothesis
keeps PSR > 0.9 (the deflation bites *search*, not skill).

## QR2.5 — Wire QR4 through it (DSR on the tearsheet) — *next*

## QR2.5 — Wire QR4 through it (DSR on the tearsheet) — *pending*
