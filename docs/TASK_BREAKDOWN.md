# QSE Task Breakdown — Roadmap to a Thesis-Grade System

Each chunk is independently completable and has an explicit **Done when** test. Work top to
bottom within a track; tracks are mostly independent of each other. The narrative companion
to this checklist — full phase descriptions including completed work — is
[PROJECT_PHASES.md](PROJECT_PHASES.md).

**Remaining work, recommended order:** F4 thesis write-up (**Track QR complete** — QR-P1 → QR-P5 all done 2026-07-09; **F1 README, F2 notebook, F3 one-pager all done** 2026-07-09)
(QR leads: it is the large majority of the remaining effort, and F2/F3 are results showcases — built
after QR they tell the sharpened survives-or-doesn't story instead of presenting the pre-QR system
while the thesis tells the QR story. F2/F3 have no upstream dependency and are cheap, so they *may*
be pulled forward at any point — but only if built strategy-agnostic (notebook loops over whatever
strategies exist; one-pager templated on the results ledger), never hardcoded to the current SMA
results, or they get rebuilt after QR anyway. F4 stays last: it consumes the QR results directly.)
**Completed so far:** A1 → C1 → C4 → A2 → A3 → A4 → B3 → H1 → B1 → B2 → D1 → C2 → C3 → G1 → G2 → F1 → E1 → E2 → E3 → A5 → QR4.1 → QR4.2 → QR4.3 → QR4.4 → QR4.5 → QR4.6 → QR4.7 (**QR-P1 complete**) → QR2.1 → QR2.2 → QR2.3 → QR2.4 → QR2.5 (**QR-P2 complete**) → QR3.1 → QR3.2 → QR3.3 → QR3.4 (**QR-P3 complete**) → QR-Data → QR1.1 → QR1.2 → QR1.3 (**QR-P4 complete**) → QR5.1 → QR5.2 → QR5.3 → QR5.4 → QR5.5 (**QR-P5 complete → Track QR complete**) → QR-X (optional HRP extension)

---

## Current state (audited 2026-07-04, updated 2026-07-06)

| Roadmap phase | Actual status |
|---|---|
| 1–3 (costs, perf, threading, ticks, ZeroMQ) | ✅ Done, tested |
| 4.1 Pairs trading | ✅ Done, tested |
| 4.2 Factor model | ✅ Done **far beyond spec**: MultiFactorCalculator, UniverseFilter, CrossSectionalRegression, ICMonitor, AlphaBlender, RiskModel, PortfolioBuilder (QP), FactorExecutionEngine, rebalance guard, YAML config |
| 4.3 Portfolio optimizer | ✅ Complete incl. A5 mean-variance extension 2026-07-06 (efficient frontier in docs/research/factor) |
| **OrderBookFullDepth** | ✅ Committed 2026-07-04: all 38 tests pass (PriceLevel, QueuePosition, Impact) |
| 5 Data & tearsheet | ✅ Track B complete 2026-07-05 (B1 ffill, B2 corporate actions, B3 tearsheet) |
| 6 CI / format / lint | ✅ Track C complete 2026-07-05 (CI, hygiene, format, clang-tidy gates) |
| 7 Live trading | ✅ Track E complete 2026-07-06 (IExecutionHandler, Alpaca REST handler, live_engine with reconciliation) |
| 8 Presentation | 🔄 In progress — F1 whitepaper README done 2026-07-06; F2/F3/F4 remain |
| Docker | ✅ D1 done 2026-07-05 — multi-stage image, container run bit-identical to native |
| G Low-latency engineering (arena, SPSC) | ✅ Track G complete 2026-07-06 — arena 16–20× alloc speedup; ring p99 42ns vs 16µs locked |
| H A/B slippage audit | ✅ Done 2026-07-05 — phantom profit $8k/$105k/$814k at 1k/5k/25k shares |
| QR Quantitative research (stat arb, CPCV/DSR, regime, OFI/VPIN, meta-labeling) | ✅ **Complete** (QR-P1–P5, 2026-07-09). Headline findings, all honest: QR-P1 stat arb 0.69 net Sharpe under Engine B ≈ cheap 12-1 momentum (0.84). QR-P4 VPIN+OFI toxicity filter *raises* slippage (adverse selection swamps spread capture). QR-P5 meta-labeling **rejected on the evidence** — under Engine B gating/sizing craters net Sharpe (0.69→0.17), no deflated edge (meta_on DSR 0.771 < meta_off 0.943), and MDA shows no engineered feature carries leak-free signal (0.500 vs 0.502 CV). The lasting deliverables are the reusable guardrails (CPCV, DSR, purged MDA) that make the negatives *findings*. Optional QR-X (HRP): MVO collapses OOS (−0.35 Sharpe, 3.8× churn), HRP repairs it (0.65) but 1/N still wins (0.90) — clustering buys risk control, not alpha. |

---

## Track A — Market Microstructure (finish the in-flight work) 🎓

*This is the thesis core. Nobody's portfolio project has queue-position-aware fills.*

### A1. ✅ Implement `fill_market` / `consume_liquidity` in OrderBookFullDepth (done 2026-07-04)
- Walk the book best-price-first, consume FIFO within each level, return
  `(filled_qty, VWAP)`, remove emptied levels, handle partial fills and
  zero/negative quantity.
- Files: `src/data/OrderBookFullDepth.cpp`
- **Done when:** `./build/run_tests --gtest_filter='Impact*'` → 8/8 pass
  (currently 0/8), and the other 30 order-book tests still pass. Then commit
  the whole OrderBookFullDepth feature.

### A2. ✅ Route market orders through the full-depth book (done 2026-07-04)
- Add an `OrderManager` mode where market-order fills come from
  `OrderBookFullDepth::fill_market` (VWAP + walked depth) instead of the fixed
  slippage constant. Keep the old model behind a config flag for comparison.
- Files: `src/order/OrderManager.cpp`, `include/qse/order/OrderManager.h`, `src/core/Config.cpp`
- **Done when:** new gtest proves a large order pays more per share than a
  small one against the same book, and the config flag toggles between models.

### A3. ✅ Queue-position-aware limit order fills (done 2026-07-04)
- Limit orders join the back of the queue at their level; they fill only after
  the queue ahead is consumed by trades at that price. Uses the existing
  `queue_position` machinery.
- Files: `OrderManager.cpp`, new `tests/cpp/LimitQueueFillTest.cpp`
- **Done when:** gtest cases cover (a) order behind queue doesn't fill when a
  small trade prints, (b) fills after queue ahead is exhausted, (c) cancel
  removes it from the queue.

### A4. ✅ Empirical impact study (done 2026-07-05; fitted b: uniform 1.017 / linear 0.569, artifacts in docs/research/microstructure/)
- Script that sweeps order sizes through a replayed book and plots realized
  slippage vs size; compare against the square-root impact law.
- Files: new `scripts/analysis/impact_study.py`, output to `docs/research/microstructure/`
- **Done when:** `python scripts/analysis/impact_study.py` produces
  `impact_curve.png` + `results_summary.md` with the fitted exponent.

### A5. ✅ Mean-variance extension of PortfolioBuilder (done 2026-07-06)
- Objective gains `−λ/2·wᵀΣw` with the single-factor covariance
  `Σ = σ_m²ββᵀ + diag(σ_resid²)` from RiskModel outputs, applied as an O(n)
  operator (never materialized); step size obeys the Lipschitz bound so any
  λ converges while λ=0 keeps the legacy step bit-for-bit. New 4-arg
  `optimize(α, β, σ_resid, symbols)` overload; `risk_aversion` +
  `market_variance` are optional YAML keys (pre-A5 configs load unchanged).
- Done-when verified: 4 gtest cases — λ=0 identical to legacy weights to
  1e-12; rising λ tilts monotonically toward low-vol assets with the ratio
  hitting the closed-form σ_h²/σ_l² limit (15.85 at λ=200); portfolio
  variance strictly decreasing; the σ_m²ββᵀ channel shrinks a factor-exposed
  book; YAML round-trip both ways. `frontier_sweep` + `efficient_frontier.py`
  trace the frontier (alpha 0.32→0.003, stdev 0.35→0.002 over 20 λ points)
  into `docs/research/factor/`. 253/253 ctest; all gates clean.

---

## Track B — Data Quality & Analysis (Phase 5)

### B1. ✅ Missing-data handling (done 2026-07-05)
- Python: `forward_fill_ticks` in `process_data.py` — ffill prices, zero
  missing volumes, drop unfillable leading rows, report every repair
  (7 pytest cases). C++: `CSVDataReader` counts unparseable rows instead of
  crashing and surfaces time-grid holes via `gap_count()` (median-spacing
  inference; 4 gtest cases); a data-quality warning prints on load.
- ParquetDataReader left as-is (Arrow handles nulls upstream) — revisit if it
  becomes a primary ingest path.

### B2. ✅ Corporate actions handler (done 2026-07-05)
- Landed in the Python layer: `scripts/data/corporate_actions.py` back-adjusts
  splits (price ÷ ratio, volume × ratio) and dividends (proportional,
  CRSP-style) from `config/corporate_actions.csv` (real split history for
  AAPL/TSLA/NVDA/GOOG/AMZN); factors computed on the raw series and applied
  as a cumulative product so multiple events compound correctly; wired into
  `process_data.py` behind the ffill step.
- Done-when verified: 10 pytest cases including the flagship AAPL 4:1
  2020-08-31 test — pre-split prices ÷4, volumes ×4, and a buy-and-hold
  equity curve flat across the split date (raw data shows the fake −75%
  crash).

### B3. ✅ Institutional tearsheet (done 2026-07-05)
- Landed as `scripts/analysis/tearsheet.py` (new module instead of extending
  analyze.py): Calmar, annualized turnover, rolling Sharpe, alpha/beta
  regression vs benchmark, 3-page PDF via PdfPages; 16 pytest cases with
  hand-computed metrics in `tests/python/test_tearsheet.py`.
- Fixed along the way: Backtester never called `record_equity` (all equity
  CSVs were header-only), and three failbit hacks suppressed stdout in every
  qse binary — debug logging is now opt-in via `QSE_DEBUG=1` (Debug.h).

---

## Track C — DevOps & Hygiene (Phase 6) — do C1 early

### C1. ✅ CI with GitHub Actions (done 2026-07-04)
- `.github/workflows/ci.yml`: ubuntu-latest, install deps (arrow, protobuf,
  zeromq, yaml-cpp via apt), configure, build, `ctest --output-on-failure`.
- **Done when:** green check on a pushed commit; a deliberately broken test on
  a branch turns red.

### C2. ✅ Formatting (done 2026-07-05)
- `.clang-format` (LLVM base, 100 col, 4-space indent, left pointers) with a
  one-time reformat of 124 C++ files; `black` (pyproject.toml, 100 col) over
  30 Python files; `flake8` gate limited to error classes (syntax, undefined
  names, unused imports/variables) with legacy violations fixed via autoflake.
  New CI `format` job runs all three with pip-pinned versions
  (clang-format 18.1.8 / black 25.1.0 / flake8 7.2.0) so local == CI.
- Done-when verified locally both ways: gates exit 0 on the tree, exit 1 on a
  deliberately unformatted file. Full suites re-run after reformat: 211/211
  ctest, 33/33 pytest.

### C3. ✅ Static analysis (done 2026-07-05)
- `.clang-tidy` gate (bugprone-*, performance-*, modernize-use-override;
  WarningsAsErrors) driven by `scripts/run_clang_tidy.sh` over every TU in
  compile_commands.json — dead files and generated protobuf code are
  automatically out of scope. Pinned clang-tidy 18.1.8 via pip; new CI `tidy`
  job. Suppressed with rationale: swappable-params, narrowing-conversions
  (money-math burn-down is future work), avoid-endl, enum-size.
- 49 findings fixed, including a **real bug**: `WeightsLoader::load_weights`
  fell off the end without a return on the Arrow-success path (UB). Plus 17
  ignored Arrow Status (now `throw_if_not_ok`), hot-path `OrderId` copies →
  const&, shared_ptr moves, missing overrides, deprecated zmq poll, dead
  stores, branch clones, reserve-before-push_back.
- Done-when verified: gate exits 0 over 38 TUs locally (and in the CI job),
  exits 1 with a planted finding. 211/211 ctest after fixes.
- Found but out of scope: `src/engine/CLI.cpp` and `src/main.cpp` are dead
  files not built by any target and do not compile — deleted 2026-07-05 after
  verifying no references in CMakeLists.txt, scripts/, docs/, or CI.

### C4. ✅ Repo hygiene (done 2026-07-04; root analyze_pairs_trading.py left in place — differs from scripts/analysis copy, needs manual merge)
- `.gitignore` for `build/`, `venv/`, `Testing/`, `*.log`, `organized_runs/`,
  `test_output/`; `git rm --cached` the tracked `Testing/Temporary/LastTest.log`;
  move stray root files (`bar_debug.log`, `analyze_pairs_trading.py` duplicate,
  `direct_test`) into place or delete.
- **Done when:** `git status` is clean after a full build + test run.

---

## Track D — Docker

### D1. ✅ Multi-stage Dockerfile (done 2026-07-05)
- Stage 1 compiles on the same ubuntu:24.04 + Arrow-apt toolchain as CI;
  stage 2 ships binaries (`strategy_engine`, `impact_sweep`, `ab_audit`),
  runtime libs, sample data, and the Python analysis stack (apt pandas/
  numpy/matplotlib). `docker/entrypoint.sh` runs the SMA demo and emits the
  tearsheet; README gained a "Running with Docker" section.
- Done-when verified: `docker build -t qse . && docker run -v "$PWD/out:/results" qse`
  wrote equity_curve.csv, tradelog.csv, and tearsheet.pdf to the host, with
  metrics **identical to the native macOS run** (Sharpe −2.404, 456 trades) —
  a cross-platform determinism check for free. Image 1.74 GB (runtime keeps
  dev packages for version-proof Arrow libs; slimming is possible later).

---

## Track E — Live Paper Trading (Phase 7)

### E1. ✅ `IExecutionHandler` interface (done 2026-07-06)
- `include/qse/exe/IExecutionHandler.h`: venue-shaped contract —
  submit market/limit, cancel, replace (cancel-and-resubmit semantics),
  `get_order`, and an async fill callback — designed to map 1:1 onto the
  Alpaca REST API in E2. `SimulatedExecutionHandler` adapts the proven
  OrderManager fill logic (whichever fill model it is configured for) behind
  the interface; it owns the fill stream. `MockExecutionHandler` (gmock) for
  contract tests.
- Done-when verified: 8 new tests — 3 contract tests where callers depend
  only on the interface (StrictMock), 5 integration tests through the real
  full-depth engine (market fill pays the ask, resting limit cancels cleanly,
  replace preserves symbol/side and rejects market/unknown ids, and a
  handler-vs-direct equivalence check on cash+position). 235/235 ctest;
  tidy/format gates clean.

### E2. ✅ `AlpacaExecutionHandler` (done 2026-07-06; dashboard smoke verified: order ed05ef12 placed PENDING and cancelled CANCELLED against the live paper account)
- Implements the E1 interface over the Alpaca paper REST API: submit
  market/limit (POST /v2/orders), cancel (DELETE), replace (PATCH, new-id
  fill-count carryover), get_order with full status/field mapping (Alpaca's
  stringified numbers and nulls handled), and a polling fill stream
  (`poll_fills` emits deltas once, untracks terminal orders). HTTP behind an
  `IHttpClient` seam: `CurlHttpClient` (libcurl) in production, gmock in
  tests — zero network in CI. Credentials only from APCA_API_KEY_ID /
  APCA_API_SECRET_KEY env (`from_env` throws if missing; .env.example
  documents them). Deps wired everywhere: CMake (curl lib + FetchContent
  nlohmann 3.11.3), both CI jobs, both Docker stages — Linux builder verified.
- Done-when: 8 mock-HTTP unit tests green (endpoints, auth headers, payloads,
  rejection, status mapping, no-duplicate fill polling); 243/243 ctest.
  Manual half: run `APCA_API_KEY_ID=... APCA_API_SECRET_KEY=...
  ./build/alpaca_smoke --paper` — places and cancels a 1-share $1.00 AAPL
  limit, verifiable in the dashboard (guards confirmed: refuses without
  --paper or keys).

### E3. ✅ Live mode (done 2026-07-06; VERIFIED LIVE: 5 signals, 5 fills, 5/5 orders reconciled local==venue in a 15-min market-hours session)
- Built as the `live_engine` tool: `AlpacaMarketDataFeed` (REST latest-quote
  polling on a producer thread — chosen over a websocket dependency; same
  ring architecture, transport swappable later) → **G2 SPSC ring** →
  `LiveEngine` (BarBuilder → SMA crossover → any `IExecutionHandler`,
  venue-agnostic via a drain function) → `AlpacaExecutionHandler` with
  `poll_fills` (now a default-no-op on the interface). Local orders/fills
  CSVs + `reconcile()` compares per-order venue fill quantity against the
  local log; SIGINT-clean; PAPER-only guard.
- Verified: 6 new tests (golden cross submits exactly one buy through the
  mock venue, fills recorded, reconciliation match AND mismatch paths, symbol
  filtering, feed parsing/dedup vs a fake HTTP client) — 249/249 ctest, tidy
  clean. A 1-minute live session against production endpoints ran the full
  loop cleanly (market closed: quotes correctly deduped, 0 bars, vacuous
  reconcile, exit 0).
- Remaining half of done-when (needs market hours):
  `set -a; source .env; set +a && ./build/live_engine --paper --minutes 10`
  during US trading hours — expect crossover orders, fills, and
  `RECONCILED: local log matches the venue`.

---

## Track G — Low-Latency Engineering 🎓

*Hardware-level engineering: heap jitter, cache locality, false sharing, atomic
memory ordering. Each chunk produces a committed benchmark artifact, not just code.*

### G1. ✅ Arena (bump) allocator for the hot path (done 2026-07-05)
- `qse::Arena` (include/qse/core/Arena.h): custom fixed-capacity bump
  allocator exposed as a `std::pmr::memory_resource` — address-aligned bump
  cursor, no-op deallocate, O(1) reset, bad_alloc on exhaustion, full
  instrumentation (bytes used, high-water mark, alloc/dealloc/reset counts).
- `OrderBookFullDepth` takes an optional resource; its maps, per-level FIFO
  deques, and lookup tables are now `std::pmr` and allocate from the arena
  (levels constructed explicitly with the book's resource — `operator[]`
  would have silently defaulted their inner containers to the heap).
- **Measured** (docs/benchmarks/04_arena_allocator.md, arm64 -O3):
  raw allocation path 57–70 ns/op → **3.5 ns/op (16–20×)**; end-to-end
  order-book workload (2,000 books × 200 levels, build+walk+destroy)
  139 µs → **58 µs/book (2.4×)**; high-water 884 KiB/book.
- Done-when verified: 7 ArenaTest cases (alignment on a misaligned cursor,
  bump contiguity, exhaustion + state survival, no-op dealloc, reset reuse,
  pmr-container integration, arena-vs-heap book equivalence); 218/218 ctest;
  format + tidy gates clean.

### G2. ✅ SPSC lock-free ring buffer (done 2026-07-06)
- `qse::SPSCRingBuffer<T>`: power-of-two capacity, alignas(64) on both
  indices AND each side's cached copy of the opposite index,
  acquire/release hand-off, `consume_all` batch drain (one acquire+release
  per batch). Integrated as `qse::LiveTickPipeline`: ZeroMQ subscriber
  thread → ring → strategy thread, with dropped-tick counting on overflow;
  end-to-end pipeline gtest over real ZeroMQ.
- Done-when verified: 9 gtest cases incl. two 10M-item stress tests
  (ordering + checksum); **ThreadSanitizer clean** over both consumer paths;
  benchmark in `docs/benchmarks/05_spsc_ring_buffer.md`.
- Honest headline: throughput parity with mutex+queue in chase mode (both
  coherence-bound, ~23M items/s) — the win is **jitter**: with a working
  consumer, producer push p99 is 42ns (ring) vs 16,334ns (locked, 389×) and
  max 71µs vs 1.15ms, plus 1.75× wall time from pipeline overlap.

## Track H — The Business Proof: A/B Slippage Audit 🎓

*The thesis experiment operationalized: identical strategy and data through two
fill models; the gap between the equity curves is the "phantom profit" a naive
backtester hallucinates.*

### H1. ✅ A/B slippage audit (done 2026-07-05; headline: at 25,000 sh/signal naive reports +$153k / Sharpe 1.93, real engine −$661k / Sharpe −5.26 — $814k phantom profit)
- **Engine A (naive):** instant fills at close/mid with fixed slippage —
  infinite-liquidity assumption. **Engine B (institutional):** the full-depth
  book — market orders walk levels and pay VWAP (A2), limit orders join the
  back of the FIFO queue and wait (A3). Both already exist behind the
  `fill_model` config flag; this chunk builds the paired harness and audit.
- Harness runs the same strategy + data through both configurations at several
  order-size regimes (e.g. 1×, 10×, 50× base size), writing paired equity
  curves and tradelogs. `scripts/analysis/slippage_audit.py` overlays the
  curves, computes **phantom profit** ($ and % of naive PnL), naive-vs-real
  Sharpe inflation, and a per-trade slippage distribution, and emits a PDF
  through the tearsheet machinery (B3).
- Files: new `src/tools/ab_audit.cpp`, new `scripts/analysis/slippage_audit.py`,
  artifacts to `docs/research/microstructure/`
- **Done when:** `python scripts/analysis/slippage_audit.py` produces
  `slippage_audit.pdf` + `ab_audit_summary.md` reporting phantom profit and
  Sharpe inflation per order-size regime, reproducible run-to-run; the
  headline number ("naive backtest overstates Sharpe by X% at size Y") is
  stated in the summary.

## Track QR — Quantitative Research & Signal Intelligence 🎓

*The research half of the artifact: from a systems-and-execution engine to a
strategy-discovery stack with the statistical guardrails to trust its own
results. Narrative companion: [PROJECT_PHASES.md](PROJECT_PHASES.md)
Phases 12–16.*

**The rule that governs this whole track:** "Profitable in Alpaca paper" is
not a result — paper fills are near-mid, instant, effectively infinite
liquidity, and H1 already proved what that hides: identical SMA signals went
from +$153k / Sharpe 1.93 (naive) to −$661k / Sharpe −5.26 once orders walk
the book at 25k shares/signal. A strategy is *viable* only if it survives
**Engine B** (VWAP-walk market orders + FIFO queue-position limit fills)
**and** clears a Sharpe **deflated for the number of configurations tried**
(QR-P2). Everything else is a candidate, not a result. Work the phases in
order — they are sequenced by expected value: the profit candidate first
(QR-P1), the honesty layer before tuning anything (QR-P2), then the risk
overlay (QR-P3), execution timing (QR-P4), and the ML capstone last, gated
behind QR-P2 by design (QR-P5).

### QR-P1 — Alpha Discovery: eigen stat arb + baselines (QR4) 🎓

*Avellaneda-Lee residual reversion: rolling PCA on a liquid universe, model
each name's idiosyncratic residual as a mean-reverting Ornstein-Uhlenbeck
process, trade the standardized deviation (s-score), stay dollar-neutral.
Market-neutral, daily frequency (inside what Alpaca can execute), documented
edge — the one item in the track with a real shot at net-positive PnL.
**Prove it on 10–15 names before scaling to 100.***

#### QR4.1 ✅ Universe + returns matrix (done 2026-07-06)
- Landed as `scripts/research/statarb/build_universe.py`: 15-name large-cap
  tech universe (AAPL MSFT GOOG AMZN META NVDA AVGO AMD INTC QCOM TXN MU
  ADBE CRM ORCL — one sector so PCA has structure; measured mean pairwise
  correlation 0.44). Bars are fetched **raw** from Alpaca (SIP→IEX fallback;
  committed as CSVs in `data/universe/` for offline rebuild) and adjusted by
  the audited B2 handler — AVGO 10:1 2024-07-15 added to
  `config/corporate_actions.csv`, six splits apply in-window. Cleaning is
  B1-style: interior gaps ffilled + counted (CRM 3, ORCL 3), leading rows
  dropped, and any |return| > 45% flagged loudly as a suspected missing
  corporate action (none flagged). Emits `universe_returns.parquet`,
  `universe_standardized.parquet`, and a manifest recording every repair,
  adjustment, and the as-of contract.
- Done-when verified: 1,432 standardized rows × 15 names (2020-10-20 →
  2026-07-06; IEX daily history starts 2020-07-27), **zero NaNs** in both
  matrices; as-of alignment documented in `docs/research/statarb/README.md`
  (trailing inclusive window, warm-up rows dropped, consumers trade ≥ t+1)
  and enforced by 13 pytest cases — including a causality test (appending
  future data leaves emitted rows bit-identical) and the split flagship
  (AAPL split day +3.4% adjusted vs −75% raw). 46/46 pytest;
  black/flake8 gates clean.

#### QR4.2 ✅ Rolling PCA + eigenvalue count (done 2026-07-06)
- Landed as `scripts/research/statarb/rolling_pca.py`: per trailing 60-day
  window, eigendecomposition of the window correlation matrix (within-window
  standardization is implicit — the corr matrix of raw returns IS the
  covariance of standardized returns), retained-factor count via the
  **Marchenko-Pastur cutoff** `λ+ = (1+√(N/T))² = 2.25`, with fixed-count and
  explain-X%-variance comparison modes. Eigenportfolio weights `Q = v/σ`
  (deterministic eigenvector sign convention; residuals are sign-invariant),
  factor returns emitted long-format with no NaN padding. Same as-of contract
  as QR4.1 (window trailing-inclusive, consumers trade ≥ t+1), plus
  `pca_for_window` exposing everything QR4.3's residual regression needs.
- Measured on the real universe (1,432 windows): the market eigenvalue
  clears the noise edge in **every** window (median λ₁ = 7.11 = 47.4% of
  variance); **MP retains 1 factor in 86% of windows, 2 in 14%**, while the
  explain-55% rule wobbles 1–4 — the arbitrariness MP removes, demonstrated.
  Committed plot: `docs/research/statarb/eigen_spectrum.png`.
- Done-when verified: 11 pytest cases — the top eigenvector of a synthetic
  block-correlated matrix recovered analytically (equicorrelated block ⇒
  uniform eigenvector, λ₁ = 1+(N−1)ρ) and structurally (loads on the
  correlated block, ~0 on the noise block); the MP cutoff retains **0**
  factors on pure noise and exactly 1 on a planted factor; inverse-vol
  weights + factor returns in closed form on a perfect-correlation pair;
  causality (future data leaves emitted windows bit-identical). 57/57
  pytest; black/flake8 clean.

#### QR4.3 ✅ Idiosyncratic residuals (done 2026-07-06)
- Landed as `scripts/research/statarb/residuals.py`: per trailing 60-day
  window, OLS-regress every name's returns on the retained eigenportfolio
  factor returns (one shared design `[1 | F]`, one least-squares solve for
  the whole cross-section) → betas, alphas, idiosyncratic residuals `ε_i`,
  and the cumulative residual `X_i = cumsum(ε_i)` — the OU process QR4.4
  consumes via `residuals_for_window`. Same as-of contract as QR4.1/QR4.2.
  Emits per-name R² + market-beta panels, window diagnostics, and a plot.
- Two structural facts flagged for QR4.4, both tested: OLS-with-intercept
  residuals are **orthogonal** to the factors (the done-when) *and*
  **sum to zero** in-window, so `X` returns to ~0 at the window's right edge
  — the s-score must come from the OU fit of the `X` path (equilibrium `m`,
  speed `κ`), not the endpoint level.
- Measured on the real universe (1,432 windows): median factor-explained
  variance **50.9%** (range 24–79%, tracking the market regime), so ~half of
  each name's daily variance is idiosyncratic (the tradeable residual);
  per-name median R² spans 0.35 (ORCL) to 0.65 (MSFT/AVGO); worst
  residual-factor \|corr\| across all windows **7.1×10⁻¹⁵** (machine
  precision). Committed plot: `docs/research/statarb/residual_diagnostics.png`.
- Done-when verified: 10 pytest cases — orthogonality both with handed-in
  factors and through the real PCA path (max \|corr\| < 1e-9); known-beta
  recovery; cumulative-residual identity + sum-to-zero; R² high for
  factor-driven / ~0 for noise; intercept-only edge case; causality
  bit-identical. 67/67 pytest; black/flake8 clean.

#### QR4.4 ✅ OU fit + s-score (done 2026-07-06)
- Landed as `scripts/research/statarb/ou_sscore.py`: per window, fit AR(1)
  `X_{n+1} = a + b·X_n + ζ` (closed-form OLS) to each name's cumulative
  residual and back out `κ = −ln(b)/Δt`, `m = a/(1−b)`,
  `σ_eq = √(Var(ζ)/(1−b²))`, and the **s-score** `s = (X_last − m)/σ_eq`.
  **Speed filter** keeps a name only if `τ = 1/κ < ½·window` — in step units
  `τ = −1/ln(b)`, so it's Δt-independent (`b < e^{−2/window} ≈ 0.967`); `b`
  outside `(0,1)` rejected outright. Filtered names emit NaN. Same as-of
  contract as QR4.1–4.3.
- **Sum-to-zero handoff resolved:** with QR4.3's `X_last ≈ 0`, the s-score is
  `≈ −m/σ_eq` (signal carried by the equilibrium vs the pinned endpoint);
  large `+m` → residual reverts up → cheap name → negative s → a buy under
  the Avellaneda-Lee bands. Estimators are general (recover κ/m/σ_eq from
  genuine non-pinned OU paths in the tests).
- Measured on the real universe (1,432 windows, 21,297 valid name-days): the
  s-score is genuinely standardized — **mean 0.01, std 0.95** (a strong
  end-to-end calibration check on the whole PCA→residual→OU chain); median
  half-life **5.8 days** so **99.1%** of name-days pass the speed filter;
  ~19% sit beyond the ±1.25 open bands. Committed plot:
  `docs/research/statarb/ou_sscore.png`.
- Done-when verified: 13 pytest cases — known `(κ, m, σ)` recovered from a
  simulated OU path (20k steps); speed filter keeps fast (τ≈10), rejects slow
  (τ≈100) and near-random-walk (b=0.9995) with the threshold flipping at
  ½·window; s-score formula + sign + the exact `−m/σ_eq` pinned case;
  constant-series rejection; causality bit-identical. 80/80 pytest;
  black/flake8 clean.

#### QR4.5 ✅ Signals + dollar-neutral weights (done 2026-07-07)
- Landed as `scripts/research/statarb/signals.py`: a per-name hysteresis state
  machine on the QR4.4 s-scores (open ±1.25, asymmetric close −0.50/+0.75,
  NaN→flat) → dollar-neutral daily target weights (each side equal-weighted to
  ±gross/2, net 0, gross cap; one-sided days go flat) → `weights_YYYYMMDD.csv`
  files in the exact `symbol,weight` format `WeightsLoader`/`FactorStrategy`
  consume. Bands are a validated frozen dataclass — the overfitting-prone
  knobs QR-P2 protects. Every universe name is written each day (inactive at
  0.0) so the engine closes exits. **Execution lag:** signal from date t is
  written to `weights_<t+1>.csv` and executed at that later close — no order
  uses data beyond its own signal date.
- Measured on the real universe: **1,431 weight files**, dollar-neutral to
  **max |net| = 1.4×10⁻¹⁶**; gross exactly 1.0 on the 1,338 two-sided days
  (94%); ~2.7 long / 2.3 short names; 16% mean daily turnover. Committed
  exposure plot: `docs/research/statarb/signal_exposure.png`.
- **Done when — verified both sides of the handoff:** 15 pytest cases (emitted
  files satisfy the loader's exact constraints — header, finite doubles,
  |w|≤10 — dated t+1, net ~0 each day; the state machine incl. NaN→flat and
  same-bar long↔short flip; dollar-neutral algebra; causality) **and** a new
  C++ gtest `WeightsLoaderTest.LoadsDollarNeutralStatArbBook` that loads a book
  in this exact format through the real `WeightsLoader` asserting net ≈ 0,
  gross = 1, inactive names load flat. 95/95 pytest; 254/254 ctest;
  black/flake8 clean.

#### QR4.6 ✅ Cheap baselines (the floor) (done 2026-07-07)
- Landed as `scripts/research/statarb/baselines.py`: cross-sectional
  short-term reversal (buy raw-return losers) and 12-1 momentum (Jegadeesh-
  Titman, skip the recent month). Each ranks the universe daily → long top
  third / short bottom third → **reuses QR4.5's `weights_from_positions` +
  `write_weight_files` unchanged**, so all three strategies emit the identical
  dollar-neutral `weights_YYYYMMDD.csv` on the same universe/lag — the
  apples-to-apples harness QR4.7 runs through Engine B. (Deliberate deviation:
  the proposal said add to the C++ `MultiFactorCalculator`; putting them in the
  Python harness is what makes "the same harness" literally true instead of a
  parallel path.)
- Cost-free floor (provisional — candidates, not results): **stat arb Sharpe
  0.97 ≈ 12-1 momentum 0.99, both clear reversal −0.28** (which loses outright).
  Turnover differs sharply (momentum 0.04, stat arb 0.16, reversal 0.29), which
  is exactly what Engine B will charge for in QR4.7. Committed comparison plot:
  `docs/research/statarb/baseline_comparison.png`.
- **Done when — verified:** all three run through the same harness and produce
  comparable (paper) tearsheets; 13 pytest cases — signal correctness (momentum
  skips the recent month, reversal ranks oppositely), cross-sectional selection,
  paper-PnL lag to the day, dollar-neutral + loader-compatible files identical
  to QR4.5, momentum warm-up, causality. 108/108 pytest; black/flake8 clean.

#### QR4.7 ✅ Survive Engine B (done 2026-07-07)
- Landed as the `statarb_audit` C++ tool (`src/tools/statarb_audit.cpp`) +
  `scripts/analysis/statarb_audit.py`. A multi-symbol daily analogue of H1:
  all three strategies' dollar-neutral weight files run through the **real**
  OrderManager fill models at 1×/10×/50× gross-notional regimes — Engine A
  (naive, fills at mid) vs Engine B (depth, market orders walk the seeded book
  and pay VWAP). Daily depth is synthesized from IEX-partial volume (a stated
  approximation; no intraday L2 for the 15 names). `build_universe.py` gained
  a `prices.csv` (adjusted close+volume) emit to feed it.
- **The finding (a credible negative):** under Engine B at 50×, net Sharpe is
  **momentum 0.84 > stat arb 0.69 > reversal −0.71** — cheap 12-1 momentum
  *beats* the elaborate eigen stat arb once fills are charged, because the stat
  arb's 16% turnover pays 17–25% phantom cost vs momentum's 4%/~6%. Phantom
  profit grows super-linearly with size for the stat arb. The fancy machinery
  does not earn its complexity over a one-line rule.
- **Done when — verified:** the tool + analysis run clean; the summary states
  the net Sharpe under Engine B at each size (`docs/research/statarb/statarb_ab_summary.md`)
  with the phantom-cost decomposition, plus a committed plot. 5 pytest cases
  cover the analysis (phantom + net-Sharpe, losing-strategy sign, missing-run
  handling, summary generation); fill mechanics are the A1–A3 gtest paths.
  254/254 ctest, 113/113 pytest; black/flake8/clang-format clean.
- **Provisional:** every Sharpe here is a candidate, not a result, until QR2.5
  deflates it for the parameter search and carries the DSR + trial count.

**QR-P1 (Phase 12) is complete** — the industry-standard market-neutral
stat-arb pipeline end to end (universe → RMT factor selection → OU residual
modeling → dollar-neutral construction → Engine B + baselines), with an honest
provisional result. Next: **QR-P2** builds the CPCV + Deflated Sharpe machinery
that turns these provisional Sharpes into judged ones.

### QR-P2 — The Truth Serum: CPCV + Deflated Sharpe (QR2) 🎓

*Financial time series have heavy serial correlation, so vanilla k-fold CV
leaks the future into the past — and if you test 1,000 threshold
combinations, the best is almost always a fluke. CPCV produces a distribution
of out-of-sample Sharpes; the DSR consumes that distribution's variance plus
the trial count. Build before tuning anything: it judges QR4 and everything
after, including the ML layer. This is the section a skeptical PM checks
first.*

#### QR2.1 ✅ Purge + embargo primitives (done 2026-07-07)
- Landed as `scripts/research/validation/purge_embargo.py`: the López de Prado
  leak fixes (AFML ch. 7) as pure functions on integer bar positions, so QR2.2's
  CPCV and QR5's triple-barrier labels reuse them. Each observation carries an
  information window `[start, end]`; **purge** drops a train obs whose window
  overlaps any test window (symmetric check — catches leakage both directions),
  **embargo** drops `int(n·embargo_pct)` train bars right after each test region
  (serial-correlation guard). Defaults to one-bar (daily) labels; multi-bar
  holds supported via explicit windows.
- **Done when — verified:** 12 pytest cases — after purge, no surviving train
  window overlaps any test window (brute-forced on multi-bar labels); embargo
  removes *exactly* the configured fraction (5/10/20 at 5/10/20% on n=100; 28 at
  2% on n=1432), clipped at the series end, composing with purge across
  multi-block test sets. black/flake8 clean.

#### QR2.2 ✅ Combinatorial path generation (CPCV) (done 2026-07-07)
- Landed as `scripts/research/validation/cpcv.py` (composes on QR2.1): partition
  into N contiguous groups, every k-subset as test → C(N,k) splits, each with a
  purged+embargoed training complement. `path_assignments` tiles the splits into
  **φ = C(N−1,k−1)** full-length backtest paths (each (split, test-group) pair
  used once); `assemble_paths` stitches per-split test predictions into the φ
  out-of-sample equity curves — the distribution QR2.4's DSR consumes.
- **Done when — verified:** 12 pytest cases — split/path counts for small (N,k)
  (N=6,k=2 → 15 splits, 5 paths, the AFML example), every observation tested in
  exactly φ splits, train/test disjoint + purged at block boundaries, and path
  reconstruction covers every bar exactly once from a split that tested it. On
  the real QR4 series (n=1,432, N=6, k=2, 1% embargo): 15 splits, 5 paths, every
  bar tested 5×, mean train 933/1,432. black/flake8 clean.

#### QR2.3 ✅ Trial registry (done 2026-07-07)
- Landed as `scripts/research/validation/trial_registry.py`: every configuration
  tried logs its params **and** its return series under `root/` as
  `<id>.params.json` + `<id>.returns.parquet`. `trial_id` is a content hash of
  the canonical params, so re-logging identical params is **idempotent** (never
  inflates the count — a phantom trial would over-deflate the DSR). `run_sweep`
  drives a param grid; `load_all` / `sharpes()` reconstruct `{params → returns}`
  and `{id → Sharpe}` — the dispersion input QR2.4 needs.
- **Done when — verified:** 7 pytest cases — a 6-point sweep populates the
  registry and the loader reconstructs every trial's params + return series
  exactly (datetime index round-trips through parquet); identical params keep
  the count at 1; distinct params are distinct. black/flake8 clean.

#### QR2.4 ✅ PSR → DSR (done 2026-07-07)
- Landed as `scripts/research/validation/deflated_sharpe.py` (Bailey & López de
  Prado). **PSR(SR\*)** = `Φ[(SR − SR*)·√(n−1) / √(1 − skew·SR + ((kurt−1)/4)·SR²)]`
  — probability the true per-period Sharpe beats SR\*, penalizing negative skew
  and fat tails. **DSR** = `PSR(SR*₀)` with `SR*₀ = √(V[SR])·[(1−γ)·Z⁻¹(1−1/N) +
  γ·Z⁻¹(1−1/(N·e))]`, the expected max Sharpe under the null across N trials.
  All per-period for unit consistency; biased moments per the reference impl;
  `deflate_registry` deflates the best trial against the QR2.3 registry.
- **Done when — verified:** 10 pytest cases — the flagship: **100 noise
  strategies, best PSR(0) = 0.994 but DSR = 0.475** (SR*₀ = 0.166 ≈ the best
  Sharpe), a >0.30 collapse — the multiple-testing penalty visible and correct,
  committed as `docs/research/validation/dsr_deflation.png`. Plus PSR = 0.5 at
  its own Sharpe, PSR rising with n, skew/kurtosis lowering PSR, and a skilled
  single hypothesis surviving (deflation bites search, not skill). black/flake8
  clean.

#### QR2.5 ✅ Wire QR4 through it (done 2026-07-07)
- Landed as `scripts/research/statarb/deflate_qr4.py`: sweeps QR4's entry/exit
  bands × estimation window (the overfitting-prone knobs), logs each config +
  return series to the registry, and deflates the best against the trial count.
- **The tearsheet DSR line** (`docs/research/statarb/qr4_dsr_summary.md`): swept
  **N = 12** configs; the **Avellaneda-Lee default** won (cost-free annualized
  Sharpe **0.92**, undeflated PSR(0) 0.987), but **DSR = 0.610** — clears chance
  but only just (best per-period Sharpe 0.058 vs SR*₀ = 0.051). Honest: this is
  the cost-free series; the Engine B haircut (QR4.7) applies on top, so the
  net-of-cost + search-deflated verdict is lower still. The AL default winning
  (not a contorted corner) is itself reassuring. CPCV gives the temporal-
  stability read (per-block OOS Sharpe mean 0.054 / std 0.053).
- **Done when — verified:** QR4's tearsheet carries the DSR line + N=12 trials;
  4 pytest cases (grid coverage, run_config produces a daily series, block
  Sharpes, end-to-end deflate carries DSR + trial count). black/flake8 clean.

**QR-P2 (Phase 13) is complete** — the truth serum end to end (purge/embargo →
CPCV → registry → PSR/DSR → wired through QR4). Every Sharpe the track reports
can now be deflated for the search that produced it.

### QR-P3 — Risk Architecture: HMM regime overlay (QR3)

*Honest expectation: regime models detect the regime after it's underway and
overfit easily — **don't expect added return.** What they reliably do is cut
drawdown (flip to min-variance / reduce gross in high-vol states), lifting
Sharpe by shrinking the denominator. Risk management, correctly attributed.*

#### QR3.1 ✅ Regime features (done 2026-07-07)
- Landed as `scripts/research/regime/regime_features.py`: five causal
  trailing-window features on the SPY proxy — `rv_21`, `rv_5` (realized vol),
  `vov_21` (vol-of-vol), `range_5` ((high−low)/close, a spread-expansion proxy),
  `vol_ratio_63` (log volume / 63d mean, the volume profile). SPY is fetched via
  the same Alpaca IEX path as the QR4 universe, so the frame (2020-10-22 →
  2026-07-07, 1,431 rows, zero NaNs) aligns with the stat-arb signal dates. It
  captures the 2022 bear (rv≈0.24) and the April 2025 selloff (peak rv≈0.49);
  misses March 2020 COVID (stated coverage limit). Committed plot +
  data/regime/SPY.csv for offline rebuild.
- **Done when — verified:** a clean feature parquet + manifest documenting the
  as-of contract (row t is a trailing-window stat of rows ≤ t; consumers act at
  ≥ t+1); 7 pytest cases — expected columns, no NaNs, features separate a
  synthetic calm→turbulent shift, and **strict causality** (appending *or*
  perturbing future data leaves emitted rows bit-identical; rv_21 at t matches
  the trailing-21 std). black/flake8 clean.

#### QR3.2 ✅ Gaussian HMM (fit causally) (done 2026-07-08)
- Landed as `scripts/research/regime/regime_hmm.py`: a diagonal-covariance
  Gaussian HMM in **pure numpy** (Baum-Welch EM + a separate log-space forward
  filter), no hmmlearn/sklearn — specifically so the filtered-vs-smoothed
  distinction is explicit. States discovered by EM, labelled by rv_21 emission
  mean (0 calm … K−1 turbulent). **Both look-ahead traps avoided:** filtered
  posterior `P(sₜ|y₁..yₜ)` (not the smoothed/Viterbi whole-series inference),
  AND an expanding-window refit (model params only ever see data ≤ t), with
  per-refit frozen standardization. Segment-based filtering keeps the real
  build ~9s.
- Result on SPY (3 states, 1,180 days): cleanly vol-ordered **calm (rv 10.9%,
  43%) / elevated (17.2%, 35%) / turbulent (22.8%, 22%)**. Honest finding — a
  distinct "crash" state did **not** separate (state 2 is turbulent, not a 40%+
  crash cluster). April 2025 selloff = 21/21 turbulent. States flip-flop between
  adjacent regimes → motivates QR3.3. Committed plot + manifest.
- **Done when — verified:** 8 pytest cases, centered on the operational test —
  **the live state at t is unchanged when future data is appended** (states +
  filtered probs bit-identical on the overlap of a prefix vs full run). Plus:
  the forward filter ignoring t+1, filtered ≠ smoothed on overlapping regimes,
  regime recovery (checked only where the model has seen both regimes — causal
  detection lags onset), vol-ordered labels, normalized probs, seed determinism.
  black/flake8 clean.

#### QR3.3 ✅ Anti-whipsaw (done 2026-07-08)
- Landed as `scripts/research/regime/regime_debounce.py`: an N-bar confirmation
  (minimum dwell) with an optional filtered-probability floor (hysteresis). A
  raw state becomes the **committed regime** only after persisting `min_dwell`
  consecutive bars, each with filtered prob ≥ `min_prob`; a blip never confirms,
  a sustained change does (lagging by min_dwell−1). Strictly streaming/causal —
  `committed[t]` uses only bars ≤ t. Adds `committed_state` to the QR3.2 frame.
- Result on SPY (min_dwell=10 ≈ 2 weeks): raw **81 switches → committed 28
  (65% fewer)** over 1,180 days, occupancy preserved (44/33/23%); the genuine
  regime edges survive. Smooth trade-off: dwell 5/10/21 → 37/65/89% fewer
  switches. Committed plot + manifest.
- **Done when — verified:** 12 pytest cases — a one-bar blip does **not** switch
  the committed regime, a sustained change **does** (at exactly the dwell). Plus
  alternating blips never confirm, min_dwell=1 identity, direct multi-state
  transitions, probability-floor hysteresis, streaming causality (prefix vs full
  agree), and switch reduction on noisy input. black/flake8 clean.

#### QR3.4 ✅ Integrate with A5 `λ` (done 2026-07-08)
- Landed as `include/qse/factor/RegimeLambda.h` (header-only) + sample
  `config/regime_lambda.yaml`. The committed regime (QR3.3) selects the A5 λ:
  `RegimeLambda` loads a YAML `regime_lambda` sequence (state → λ) and its
  `apply(config, state)` returns a `PortfolioBuilder` config with
  `risk_aversion` set to that regime's λ, so a turbulent regime → larger λ →
  `−λ/2·wᵀΣw` dominates → min-variance / lower-gross posture. Map `[0, 5, 50]`
  (calm/elevated/turbulent).
- **Done when — verified both halves:** 5 gtest `RegimeLambdaTest` cases — a
  state-change YAML injection forces the engine to **lower portfolio variance**
  (calm λ=0 → turbulent λ=50) *and* **scale gross down** on a factor-exposed
  book (market-variance channel); plus YAML load/state-mapping with clamping,
  reject empty/negative, apply-changes-only-λ. AND a Jupyter notebook
  `docs/research/regime/regime_overlay.ipynb` plots the SPY equity curve colored
  by committed HMM regime (turbulent days cluster in the 2022/2025 drawdowns —
  exactly where the overlay de-risks). 259/259 ctest; clang-format/black/flake8
  clean.

**QR-P3 (Phase 14) complete** — features → causal filtered HMM → debounced
committed regime → A5 λ. Regime treated as risk control (its real job), an
unsupervised ML model wired into a live risk parameter, look-ahead avoided
throughout.

### QR-P4 — Execution Intelligence: OFI / VPIN (QR1)

*Reframed from standalone alpha to execution-timing filter, for two concrete
reasons: (1) faithful OFI needs real L2/L3 depth updates and our depth is
L1-reconstructed; (2) the OFI edge decays in seconds while our fill path is
REST polling at hundreds of ms — the signal is gone before we act. As a
toxicity filter in `OrderManager` it plays straight to the systems strength,
and the A/B audit decides whether it earns its place.*

#### QR-Data ✅ Settle the depth-data fork *first* (done 2026-07-08)
- **Decision: stay on L1-reconstructed depth.** Rationale — (1) data reality:
  the only inputs are L1 trade prints + daily OHLCV, no quote/depth feed, and
  the engine walks *reconstructed* depth (validated in aggregate against the A4
  square-root impact law); (2) latency reality: even with real MBO the OFI edge
  decays in seconds while the fill path is REST at 100s of ms, so OFI-as-alpha
  isn't executable here regardless; (3) cost/scope: real MBO (Databento/IEX
  DEEP) is a paid, heavy ingestion dependency out of scope for an
  execution-realism thesis.
- **Consequence (the caveat):** QR-P4 scopes OFI/VPIN as an execution-timing /
  toxicity filter in `OrderManager`, judged only by whether it improves fills
  in the A/B audit — **not** a standalone alpha. We can claim *"an approximate
  toxicity filter on L1 depth does/doesn't improve simulated fills"*, not *"OFI
  predicts returns."* Databento/IEX DEEP recorded as future work.
- **Done when — verified:** the decision + its honesty caveat are written into
  the thesis limitations section, `docs/thesis/limitations.md` §1 (which also
  consolidates the previously-scattered caveats: reconstructed depth, IEX-partial
  feed, survivorship, dividend gaps, regime coverage, provisional Sharpes).

#### QR1.1 ✅ OFI engine (done 2026-07-08)
- Landed as `include/qse/microstructure/OFICalculator.h` (header-only): the
  Cont-Kukanov-Stoikov Order Flow Imbalance on L1 quotes. Per event
  `OFI = ΔV_bid − ΔV_ask` with the conditional contributions — bid up → +new
  size, bid down → −prior size, flat → Δ (symmetric on the ask: up → +prior
  size, down → −new size). Sizes cast to double before differencing so the
  unsigned `Volume` (uint64) never underflows. Keeps a rolling-window sum for
  the live `OrderManager` filter (QR1.3); `event_ofi(...)` is a pure static.
- **Done when — verified:** 11 `OFITest` gtests — a hand-built tick sequence
  with known level changes reproduces the per-event OFI and the running sum;
  each of the six price-move cases (bid/ask × up/down/flat) in isolation, plus
  shrinking-size (no unsigned underflow), a combined bullish event, rolling
  eviction, first-snapshot-no-event, and reset. 270/270 ctest;
  clang-format/black/flake8 clean. Research doc: docs/research/execution/.

#### QR1.2 ✅ VPIN engine (done 2026-07-08)
- Landed as `include/qse/microstructure/VPINCalculator.h` (header-only):
  Easley-López de Prado-O'Hara VPIN in volume time — equal-volume buckets (a
  large trade split across buckets) → bulk-volume classification (`V_buy =
  V·Φ(ΔP/σ)` via the normal CDF, `Φ` from `erfc` for tail accuracy) → `VPIN =
  mean over n buckets of |V_buy − V_sell|/V = mean|2Φ(ΔP/σ)−1|`. σ estimated
  causally as the expanding std of ΔP, or a `fixed_sigma` for determinism.
  Balanced flow → VPIN ≈ 0, one-sided → ≈ 1.
- **Done when — verified:** 9 `VPINTest` gtests — VPIN on a synthetic volume
  series with a **known buy/sell split** (alternating ±1σ closes → VPIN =
  |2Φ(1)−1| ≈ 0.683 exactly with fixed σ, and within tolerance under estimated
  σ). Plus normal-CDF values, buy-fraction/imbalance (incl. σ≤0 → balanced),
  equal-volume bucketing + trade splitting, balanced→0, one-sided→~1,
  not-ready-until-n, reset. 279/279 ctest; clang-format/black/flake8 clean.

#### QR1.3 ✅ Toxicity filter in `OrderManager` (done 2026-07-08)
- Landed as `include/qse/microstructure/ToxicityFilter.h` (the VPIN+OFI decision
  policy) + an additive `OrderManager` hook: `enable_toxicity_filter` feeds a
  running VPIN/OFI from `process_tick` and exposes `current_vpin()`,
  `current_ofi()`, `is_toxic()` (no change to the fill path). The filter rests
  passive (delays crossing) only when flow is toxic **and** directionally
  favorable (buy wants OFI<0). The A/B experiment is the `toxicity_audit` tool +
  `scripts/analysis/toxicity_audit.py`: blind market orders vs the gated
  passive-then-fallback policy on the AAPL tick stream.
- **Done when — the audit decided, and it decided AGAINST (an honest negative):**
  on 1,893 orders the filter *increases* slippage (0.01168 vs blind 0.01000).
  Decomposition: 27/34 rested orders capture the spread (−$0.01), but the 7 that
  don't fall back after the toxic flow **ran away** (avg +$0.54 — the adverse tail)
  and swamp the captures; robust across thresholds 0.45–0.7 and horizons 10–40.
  The lesson: high VPIN predicts *continued* adverse movement, so resting passive
  into it is the wrong move. The microstructure literacy is proven; the naive
  filter does not earn its place on L1 data (any config-swept win needs QR-P2
  deflation). 286/286 ctest (7 `ToxicityFilterTest`), 4 pytest; gates clean.

**QR-P4 (Phase 15) complete** — QR-Data → OFI → VPIN → toxicity filter, with an
honest negative result the audit produced.

### QR-P5 — Learned Meta-Layer: meta-labeling (QR5) 🎓

*The one defensible ML addition, and the capstone: QR4's s-score still
decides the **side**; a classifier decides **whether to act and how big**,
trained on features from QR1 (VPIN/OFI), QR3 (regime), and s-score magnitude.
López de Prado meta-labeling — improves precision and sizes bets without ever
predicting direction. Gated behind QR-P2 by design: it is only trustworthy
under purging/embargo, reuses QR-P2's CPCV directly, and is judged by the
same DSR.*

#### QR5.1 ✅ Triple-barrier labels (done 2026-07-09)
- Landed as `scripts/research/meta/triple_barrier.py` (López de Prado, AFML
  ch. 3): per QR4 entry (t0, price p0, side s) three barriers on the return in
  the bet's direction `signed = s·(p/p0−1)` — profit-take (`signed ≥ pt` → label
  1), stop (`signed ≤ −sl` → label 0), vertical (neither within `max_holding` →
  label by sign at the horizon); first touch wins. The touch time t1 is the
  observation's **information window** `[t0, t1]`, emitted as `t0_idx`/`t1_idx`
  (via `label_windows`) so QR2.1 purge/embargo and QR5.2 sample uniqueness
  consume it directly. `extract_entry_events` pulls opens + long↔short flips
  from the QR4.5 positions.
- Real-data check: 748 QR4 entries labeled (pt=sl=3%, 10-bar horizon) →
  332 pt / 331 sl / 85 time, meta-label balance **0.497** (~50% wins), median
  3-bar holding (matches the OU half-life). The near-even win rate is exactly
  the motivation for meta-labeling. Committed distribution plot.
- **Done when — verified:** 12 pytest cases — barrier-touch labeling on
  hand-built paths (up-first → win, down-first → loss, timeout by sign), plus
  short side, first-touch precedence, exact-threshold touch, horizon clamping,
  invalid-param guards, entry-event extraction, and the info-window handoff.
  black/flake8 clean.

#### QR5.2 ✅ Sample uniqueness (done 2026-07-09)
- Landed as `scripts/research/meta/sample_uniqueness.py` (AFML ch. 4): from the
  QR5.1 `[t0_idx, t1_idx]` windows — `concurrency` (windows active per bar),
  `average_uniqueness` (mean of 1/c_t over an event's span ∈ (0,1]),
  `sample_weights` (ū, optionally |return|-scaled, normalized to mean 1) as the
  classifier weight, and `sequential_bootstrap` (draw favoring the least-
  overlapping). `weights_for_labels` wraps the labels frame per name.
- Real labels: the 748 entries are mostly non-overlapping (mean uniqueness
  0.984 → effective N ≈ 736/748) — a modest but principled correction here.
- **Done when — verified:** 12 pytest cases — heavily-overlapping samples get
  lower weight than isolated ones (triple-overlap ū=1/3 vs isolated ū=1); plus
  concurrency, the unit-interval bound, identical/partial overlap,
  return-attribution scaling, normalization, sequential bootstrap over-sampling
  the unique event, and a duplicate-index regression (a bug found on the pooled
  multi-name frame). black/flake8 clean.

#### QR5.3 ✅ Meta-model under purged CV (done 2026-07-09)
- Landed as `scripts/research/meta/meta_model.py` + `build_meta_dataset.py`: a
  dependency-free weighted logistic regression predicting `P(profitable)` from
  `{abs_sscore, sscore, kappa, regime (QR3), vol_21, vol_5, vol_ratio, dow}`
  (VPIN/OFI aren't available per-name at daily frequency — the daily volume
  ratio stands in; noted in limitations §1), trained on QR5.1 labels weighted by
  QR5.2 uniqueness. `purged_cpcv_splits` composes QR2 on the events' windows —
  time-ordered groups, C(N,k) folds, QR2.1 purge + a bar-space embargo (QR2.1's
  own embargo assumes obs-index==bar-index, false for custom windows).
- **Honest null (preview of QR5.5's DSR verdict):** 743 events × 8 features,
  balance 0.498; under purged CPCV (6 groups, k=2 → 15 splits, 3,715 OOS preds)
  CV accuracy **0.500 vs 0.502 majority baseline** — no edge. The point of the
  guardrails: a leaking CV might have flattered it; purged CPCV keeps it honest.
- **Done when — verified:** 11 pytest cases — the model trains through the
  harness and **no train/test fold overlaps** (disjoint event sets *and* no
  train window overlaps a test window, across (N,k) ∈ {(6,2),(8,3),(5,2)}), with
  correct split counts, every event tested C(N−1,k−1) times, bar-embargo, and
  deterministic end-to-end training; plus the logistic regression itself.
  black/flake8 clean.

#### QR5.4 ✅ Probability → size / gate (done 2026-07-09)
- Landed as `scripts/research/meta/meta_sizing.py`: the QR5.3 pooled purged-CPCV
  OOS `P(profitable)` per event becomes the bet *size*, re-emitted in the same
  `weights_YYYYMMDD.csv` format the C++ engine consumes (QR4.5). Three modes the
  QR5.5 A/B audit toggles — **off** (size 1 → reproduces the raw QR4.5 book
  exactly), **gate** (1 if P ≥ floor else 0), **size** (`clip((P−floor)/(1−floor),
  0, 1)`). Size is decided at entry and propagated over the position's held run;
  fractional positions are dollar-neutralized by allocating each side ∝ its sizes,
  which collapses to QR4.5's equal weighting when sizes are all 1 — so meta-off is
  provably the baseline.
- **On the real book:** off → 1,338 active days (= raw QR4.5); gate/size at floor
  0.5 → 301 active days (77% fewer), all net ~1e-16, 1,431 weight files each.
  Since QR5.3 found no CV edge, the gate at 0.5 drops ~half the bets ~at random —
  QR5.5's DSR judges whether that helps net-of-cost.
- **Done when — verified:** 8 pytest cases — same weight-file format
  (header + `symbol,weight`, |w| ≤ 10, net ~0), **meta-off bit-equal to the raw
  QR4.5 `weights_from_positions` book**, gate skips sub-floor bets, size scales by
  confidence, sizes propagate over the held run, dollar-neutrality holds for
  fractional sizes. black/flake8 clean; `weights_meta_*` output gitignored.

#### QR5.5 ✅ Judge it like everything else (done 2026-07-09)
- Landed as `scripts/research/meta/judge_meta.py` (+ `mda_importance` in
  `meta_model.py`); full result in `docs/research/meta/qr5_judge_summary.md`.
  Three leak-free lenses on the meta-layer:
  - **Engine B:** the QR5.4 meta-sized weight dirs run through the *same*
    `statarb_audit` full-depth fill model as QR4.7. `meta_off` reproduces the raw
    QR4.5 book (lands on QR4.7's stat_arb numbers — a cross-check); gate/size at
    floor 0.5 **crater** net Sharpe (50×: **0.69 → 0.17 / 0.16**) — ~82% fewer
    trades, none better-selected, so the book's diversification collapses.
  - **DSR for both:** deflating a 13-config meta search (mode × floor, cost-free
    paper PnL, as QR2.5), meta_off DSR **0.943** vs best meta-on (`gate@0.52`)
    **0.771** — no meta config's deflated Sharpe beats doing nothing.
  - **MDA under purged CV:** pooled purged-CV accuracy 0.500 vs 0.502 baseline;
    the only feature with positive importance is `sscore` (the primary signal's
    own sign, rediscovered), while every engineered meta feature (abs_sscore,
    regime, vols, kappa, vol_ratio) has ≤ 0 importance.
- **Verdict — honest null:** meta-labeling adds nothing here and naive
  application subtracts. The guardrails make that a *finding*, not a guess.
- **Done when — verified:** 6 pytest cases — depth-curve → net-return/Sharpe
  table (ranks modes, skips missing sizes), the meta-search family (off + mode ×
  floor grid), the deflated verdict (DSR for both, best-meta selection), and MDA
  (ranks an informative feature above noise; noise-only ≈ 0). black/flake8 clean;
  `results/meta_audit/` gitignored, summary + PNG committed.

### QR-X ✅ (optional) — Hierarchical Risk Parity (done 2026-07-09)
- Landed as `scripts/research/portfolio/allocators.py` (MVO min-variance, HRP
  cluster→quasi-diag→recursive-bisection, IVP, equal-weight) +
  `compare_allocators.py` (walk-forward OOS + CPCV blocks + tearsheet); full
  result in `docs/research/portfolio/hrp_vs_mvo_summary.md`. 252-day trailing
  covariance, rebalanced every 21 days over the 15-name QR4 universe (59
  rebalances, no lookahead), one OOS daily series per allocator, sliced into
  CPCV blocks for the temporal-stability read.
- **Honest three-part finding:** MVO **collapses** OOS (Sharpe −0.35, churns
  3.8× HRP — the Σ⁻¹ instability on a near-singular 15-tech-name covariance,
  even shorting into losses); HRP **repairs** it (0.65 Sharpe at 3.8× less
  turnover — robustness from clustering, forecasting nothing); but **1/N still
  wins** (0.90, zero turnover; DeMiguel–Garlappi–Uppal). HRP's value is risk
  control, not alpha — and CPCV lets us state the HRP < 1/N negative with
  confidence.
- **Done when — verified:** 11 pytest cases — allocator invariants (sum-to-1,
  IVP=1/σ², MVO=IVP when uncorrelated, HRP=IVP when uncorrelated, HRP long-only &
  diversifies a correlated cluster, MVO shorts where HRP can't, singular-Σ
  graceful) and the walk-forward harness (no lookahead, turnover, equal-weight
  zero churn, CPCV blocks partition the series). black/flake8 clean; outputs are
  the committed summary + PNG (no data artifacts).

---

## Track F — Presentation & Thesis (Phase 8)

### F1. ✅ Whitepaper README (done 2026-07-06)
- Full rewrite for the dual audience: "60-second version" findings table
  (phantom profit, impact exponents, arena/SPSC numbers, determinism), the
  flagship A/B audit section with the committed figures, Mermaid architecture
  diagram, engineering-quality section (gates + the real bugs they caught),
  Docker + native + reproduce-the-research quickstarts, repo map, and links
  to PROJECT_PHASES / TASK_BREAKDOWN / benchmarks / research artifacts.
- Done-when satisfied: a reader can build and run a backtest (and reproduce
  every research artifact) using only the README.

### F2. ✅ Jupyter walkthrough (done 2026-07-09)
- Landed as [`notebooks/qse_walkthrough.ipynb`](../notebooks/qse_walkthrough.ipynb)
  + `requirements-notebooks.txt`. Detects the repo root from wherever nbconvert
  runs, **builds (if needed) and runs `strategy_engine` via `subprocess`**
  (SMA 20/50 over the bundled AAPL ticks), loads its `equity_curve.csv` /
  `tradelog.csv` with the project's own `scripts/analysis/tearsheet.py`, and
  renders an inline tearsheet — metrics table (return, CAGR, Sharpe, max DD,
  Calmar, turnover) + charts (equity, underwater drawdown, rolling Sharpe).
- **Robust from a fresh clone:** if the C++ toolchain can't build the engine in
  the execution environment, the run-engine cell **falls back to a committed
  sample run** (`notebooks/sample/`, produced by this same engine) so the
  notebook always completes.
- **Done when — verified:** `jupyter nbconvert --to notebook --execute` exits 0
  on **both** paths — the real engine (19,184 equity points, 456 fills) and the
  forced fallback (binary hidden + `cmake` off `PATH`). The committed notebook
  carries no baked-in outputs; engine artifacts stay gitignored.

### F3. ✅ PDF one-pager (done 2026-07-09)
- Landed as [`scripts/analysis/onepager.py`](../scripts/analysis/onepager.py) →
  committed [`docs/QSE_one_pager.pdf`](QSE_one_pager.pdf). A single print-ready
  page built with matplotlib's PDF backend (same as the tearsheet — no new
  deps): a drawn architecture pipeline (Data → C++ engine w/ the full-depth book
  highlighted → Analysis, with live + research-track annotations), a two-column
  **key-results** table spanning every track (phantom profit, impact exponent,
  arena/SPSC, determinism, live paper, and the research negatives — stat arb,
  DSR, regime, toxicity, meta-labeling, HRP), the embedded flagship A/B slippage
  figure, and the repo link in the header + footer.
- **Done when — verified:** `docs/QSE_one_pager.pdf` exists and renders — a valid
  `%PDF-1.4`, exactly **one page**, visually checked (rasterized) for clean
  layout with no clipping or overlap. 3 pytest cases (valid single-page PDF,
  graceful placeholder when the figure is missing, the committed artifact is
  present + valid). black/flake8 clean.

### F4. Thesis write-up 🎓
- Suggested research question: **"How much does market-microstructure realism
  (full-depth book, VWAP fills, queue position) change measured performance of
  cross-sectional factor strategies versus naive fixed-slippage backtests?"**
- Methodology = Tracks A+B; experiments = same factor strategy under
  fixed-slippage vs full-depth fills across order sizes/turnover levels;
  results = tearsheets + impact curves from A4/B3.
- **Sharpened by Track QR** (do QR-P1/QR-P2 first): *"How much does
  market-microstructure realism change the measured performance of a
  cross-sectional statistical-arbitrage strategy — and does anything survive
  once its Sharpe is also deflated for the number of configurations
  tested?"* Methodology = QR-P1 + QR-P2; experiments = QR4 (and baselines)
  under Engine A vs Engine B across order sizes, each config deflated by
  CPCV/DSR; risk overlay and execution filter (QR-P3/P4) as ablations; the
  learned layer (QR-P5) as the capstone ablation. Whether the answer is
  "survives" or "doesn't," it's a genuine, defensible result.
- **Done when:** `docs/thesis/` contains a 25–40 page paper: intro, related
  work, architecture, methodology, results, limitations.
