# QSE Task Breakdown — Roadmap to a Thesis-Grade System

Each chunk is independently completable and has an explicit **Done when** test. Work top to
bottom within a track; tracks are mostly independent of each other. The narrative companion
to this checklist — full phase descriptions including completed work — is
[PROJECT_PHASES.md](PROJECT_PHASES.md).

**Remaining work, recommended order:** Track QR (QR-P1 → QR-P2 → QR-P3 → QR-P4 → QR-P5) → F2 → F3 → F4
(QR leads: it is the large majority of the remaining effort, and F2/F3 are results showcases — built
after QR they tell the sharpened survives-or-doesn't story instead of presenting the pre-QR system
while the thesis tells the QR story. F2/F3 have no upstream dependency and are cheap, so they *may*
be pulled forward at any point — but only if built strategy-agnostic (notebook loops over whatever
strategies exist; one-pager templated on the results ledger), never hardcoded to the current SMA
results, or they get rebuilt after QR anyway. F4 stays last: it consumes the QR results directly.)
**Completed so far:** A1 → C1 → C4 → A2 → A3 → A4 → B3 → H1 → B1 → B2 → D1 → C2 → C3 → G1 → G2 → F1 → E1 → E2 → E3 → A5 → QR4.1 → QR4.2 → QR4.3

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
| QR Quantitative research (stat arb, CPCV/DSR, regime, OFI/VPIN, meta-labeling) | 🔄 In progress — QR4.1 universe + QR4.2 rolling PCA + QR4.3 residuals done 2026-07-06 (MP retains 1–2 factors; ~51% factor-explained variance; residuals orthogonal to 7e-15) |

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

#### QR4.4 OU fit + s-score
- Fit AR(1) to the discrete cumulative residual `X_{n+1} = a + b·X_n + ζ`
  (OLS). Back out, with step `Δt` (e.g. 1/252): mean-reversion speed
  `κ = −ln(b)/Δt`, equilibrium level `m = a/(1−b)`, equilibrium std
  `σ_eq = √( Var(ζ)/(1−b²) )`, and **s-score** `s_i = (X_i − m_i)/σ_eq,i`.
- **Speed filter (critical):** require mean-reversion time `τ = 1/κ` short vs
  the window — reject names where reversion is slower than ~half the
  estimation window (`b` too close to 1). Slow reversion = spurious signal.
- **Done when:** on a simulated OU path with known `(κ, m, σ)`, the estimator
  recovers them within tolerance; names with `b→1` are filtered out.

#### QR4.5 Signals + dollar-neutral weights
- Avellaneda-Lee default rules (**starting points only — these thresholds are
  exactly the overfitting-prone knobs QR-P2 must protect**): open long if
  `s < −1.25`; open short if `s > +1.25`; close long if `s > −0.50`; close
  short if `s < +0.75` (asymmetric = drift-aware). Convert active positions
  into **dollar-neutral daily target weights** (gross cap, net ≈ 0), written
  to the same weight-file format `WeightsLoader` / `FactorExecutionEngine`
  already consume.
- **Done when:** the Python pipeline emits daily target-weight files the C++
  engine loads unchanged; net exposure ≈ 0 each day within tolerance.

#### QR4.6 Cheap baselines (the floor)
- Add two baselines to `MultiFactorCalculator` for honest comparison:
  cross-sectional short-term reversal (buy losers / sell winners) and 12-1
  momentum. If the eigen stat arb can't beat *reversal* net of Engine B
  costs, that's a finding — the fancy version isn't earning its complexity.
- **Done when:** all three (stat arb, reversal, momentum) run through the
  same harness and produce comparable tearsheets.

#### QR4.7 Survive Engine B
- Run QR4 through `ab_audit` at 1×/10×/50× size regimes (Engine A naive vs
  Engine B full-depth). Generate the tearsheet.
- **Done when:** `ab_audit` + `tearsheet.py` run clean; the summary states the
  net Sharpe under **Engine B** at each size. (Positive-but-modest is the
  win; a clean negative with the phantom-cost decomposition is still a
  result.)
- **Note:** the Sharpe reported here is provisional — a candidate, not a
  result — until QR2.5 deflates it for the parameter search and the tearsheet
  carries the DSR + trial count.

### QR-P2 — The Truth Serum: CPCV + Deflated Sharpe (QR2) 🎓

*Financial time series have heavy serial correlation, so vanilla k-fold CV
leaks the future into the past — and if you test 1,000 threshold
combinations, the best is almost always a fluke. CPCV produces a distribution
of out-of-sample Sharpes; the DSR consumes that distribution's variance plus
the trial count. Build before tuning anything: it judges QR4 and everything
after, including the ML layer. This is the section a skeptical PM checks
first.*

#### QR2.1 Purge + embargo primitives
- Purging: drop training samples whose label/evaluation window overlaps the
  test window. Embargo: drop a fraction of training samples immediately
  *after* each test block, to stop forward leakage from serial correlation.
- **Done when:** `pytest` proves no training index's evaluation window
  overlaps any test window, and the embargo removes exactly the configured
  fraction.

#### QR2.2 Combinatorial path generation (CPCV)
- Partition the series into `N` blocks; choose `k` as test, rest as train →
  `C(N,k)` splits. Reconstruct the `C(N−1,k−1)` full out-of-sample **backtest
  paths** from the recombined test blocks. Each path is a complete equity
  curve → a *distribution* of Sharpes, not a single point estimate.
- **Done when:** a test confirms the correct number of splits/paths for small
  `(N,k)` and that every observation appears in the test set the expected
  number of times.

#### QR2.3 Trial registry
- Every backtest variation (each s-score threshold set, window length, factor
  count, etc.) logs its params **and its return series** to a registry, so
  the variance of Sharpe across trials is computable. This is infrastructure
  — the DSR is only meaningful if trial count and dispersion are real.
- **Done when:** running a parameter sweep populates a registry directory; a
  loader reconstructs `{params → return series}` for all trials.

#### QR2.4 PSR → DSR
- **PSR** (Probabilistic Sharpe Ratio), the building block:
  `PSR(SR*) = Z[ (SR − SR*)·√(n−1) / √(1 − skew·SR + ((kurt−1)/4)·SR²) ]`
  where `n` = number of returns, `skew`/`kurt` from the return distribution,
  `Z` the standard-normal CDF — probability the true Sharpe exceeds `SR*`.
- **DSR** = `PSR(SR*₀)` with the benchmark set to the **expected max Sharpe
  under the null** across `N` trials:
  `SR*₀ = √(V[SR]) · [ (1−γ)·Z⁻¹(1 − 1/N) + γ·Z⁻¹(1 − 1/(N·e)) ]`
  where `V[SR]` = variance of Sharpes across trials, `γ ≈ 0.5772`
  (Euler-Mascheroni), `Z⁻¹` inverse normal, `e` Euler's number.
- Files: new `scripts/research/validation/deflated_sharpe.py`
- **Done when:** the script takes the trial registry and computes DSR, and a
  `pytest` shows **100 random-noise strategy variations severely deflate**
  the final DSR vs a single-hypothesis run — the penalty for multiple testing
  is visible and correct.

#### QR2.5 Wire QR4 through it
- Run QR4's parameter search under CPCV; report the DSR of the chosen config
  from the path distribution.
- **Done when:** QR4's tearsheet carries a DSR line and the number of trials
  it was deflated against.

### QR-P3 — Risk Architecture: HMM regime overlay (QR3)

*Honest expectation: regime models detect the regime after it's underway and
overfit easily — **don't expect added return.** What they reliably do is cut
drawdown (flip to min-variance / reduce gross in high-vol states), lifting
Sharpe by shrinking the denominator. Risk management, correctly attributed.*

#### QR3.1 Regime features
- Engineer causal features: rolling realized vol, bid-ask spread expansion,
  volume profile, maybe realized-vol-of-vol. All strictly as-of (no future
  data).
- **Done when:** a feature Parquet with a documented no-look-ahead alignment;
  test that the feature at time `t` uses only data `≤ t`.

#### QR3.2 Gaussian HMM (fit causally)
- Fit a Gaussian HMM (`scikit-learn`/`hmmlearn`) with `K` states. **States
  are discovered, then labeled by inspection** (sort by emission variance →
  "low / high / crash"); you don't get to name them a priori, and the "crash"
  state may or may not emerge — report honestly.
- **Correctness trap (the sophistication point):** for any live/backtest use,
  use **filtered** state probabilities (info up to `t`) — Viterbi/**smoothed**
  states use the *whole* series and are a look-ahead bug. Fit on a
  rolling/expanding window, never once over all history.
- **Done when:** a test asserts the live state at `t` is unchanged when
  future data is appended (i.e. it's genuinely filtered, not smoothed).

#### QR3.3 Anti-whipsaw
- Regimes flip-flopping cause turnover. Add a minimum dwell time / hysteresis
  so `λ` only moves on a persistent state change.
- **Done when:** a test shows a one-bar state blip does *not* trigger a `λ`
  change; a sustained change does.

#### QR3.4 Integrate with A5 `λ`
- Map state → `λ` in the YAML config (high-vol/crash → larger `λ` →
  PortfolioBuilder drives toward min-variance weights and lower gross).
- **Done when:** a `gtest` confirms a state-change YAML injection forces the
  C++ engine to scale down gross exposure / shift to a minimum-variance
  posture; a Jupyter notebook plots the SPY equity curve colored by HMM
  state.

### QR-P4 — Execution Intelligence: OFI / VPIN (QR1)

*Reframed from standalone alpha to execution-timing filter, for two concrete
reasons: (1) faithful OFI needs real L2/L3 depth updates and our depth is
L1-reconstructed; (2) the OFI edge decays in seconds while our fill path is
REST polling at hundreds of ms — the signal is gone before we act. As a
toxicity filter in `OrderManager` it plays straight to the systems strength,
and the A/B audit decides whether it earns its place.*

#### QR-Data Settle the depth-data fork *first*
- Decide before building: stay on **L1-reconstructed** depth (honest result:
  *"an approximate toxicity filter improves fills in simulation"* — valid,
  but not "OFI predicts price"), or source real MBO/depth (**Databento MBO**,
  **IEX DEEP**) for a genuine OFI result. Document the choice as a stated
  limitation either way.
- **Done when:** the data decision + its honesty caveat is written into the
  thesis limitations section.

#### QR1.1 OFI engine
- Per tick interval, `OFI_t = ΔV_bid,t − ΔV_ask,t` with the conditional
  bid/ask size changes from the original spec (add to size on favorable price
  move, subtract prior size on adverse move, else the delta).
- **Done when:** a `gtest` reproduces OFI on a hand-built tick sequence with
  known level changes.

#### QR1.2 VPIN engine
- Equal-**volume** buckets; bulk-volume classification (standardized price
  change through the normal CDF splits each bucket into buy/sell volume);
  `VPIN = mean over n buckets of |V_buy − V_sell| / V`.
- **Done when:** a `gtest` computes VPIN on a synthetic volume series with
  known buy/sell split within tolerance.

#### QR1.3 Toxicity filter in `OrderManager`
- `OrderManager` reads OFI/VPIN state and **delays crossing the spread**
  (rests passive / waits) when localized VPIN flags toxic flow, instead of
  firing a blind market order.
- **Done when:** the A/B audit shows a **measurable slippage reduction** vs
  the blind-market-order baseline on the same signals/data (the filter earns
  its place or it doesn't — the audit decides).

### QR-P5 — Learned Meta-Layer: meta-labeling (QR5) 🎓

*The one defensible ML addition, and the capstone: QR4's s-score still
decides the **side**; a classifier decides **whether to act and how big**,
trained on features from QR1 (VPIN/OFI), QR3 (regime), and s-score magnitude.
López de Prado meta-labeling — improves precision and sizes bets without ever
predicting direction. Gated behind QR-P2 by design: it is only trustworthy
under purging/embargo, reuses QR-P2's CPCV directly, and is judged by the
same DSR.*

#### QR5.1 Triple-barrier labels
- For each QR4 entry signal, set an upper barrier (profit-take), lower
  barrier (stop), and vertical barrier (time limit). Label = which is hit
  first, framed as meta-labels: **1** if the primary (s-score) bet would have
  been profitable, **0** otherwise.
- **Done when:** `pytest` verifies barrier-touch labeling on hand-built price
  paths (up-first, down-first, timeout).

#### QR5.2 Sample uniqueness
- Overlapping label windows violate IID. Weight samples by average uniqueness
  (concurrency-adjusted) and/or sequential bootstrap so overlapping events
  don't dominate training.
- **Done when:** a test shows heavily-overlapping samples receive lower
  weight than isolated ones.

#### QR5.3 Meta-model under purged CV
- Train a classifier (start simple — logistic / gradient-boosted trees,
  **not** a deep net) on features `{s-score magnitude, regime state (QR3),
  VPIN/OFI (QR1), spread, recent vol, time-of-day}` to predict
  `P(profitable)`. Validate **only** through QR-P2's CPCV — no leakage.
- **Done when:** the model trains through the purged CV harness; a `pytest`
  confirms no train/test overlap in any fold.

#### QR5.4 Probability → size / gate
- Map `P(profitable)` to bet size (or a gate: skip trades below a probability
  floor). Feed sized/gated signals into the A5 PortfolioBuilder path.
- **Done when:** the meta-layer produces the same weight-file format the
  engine consumes; a config flag toggles meta-sizing on/off for A/B.

#### QR5.5 Judge it like everything else
- Run meta-labeled QR4 through `ab_audit` (Engine B) and report **DSR** — did
  the learned layer improve net-of-cost, deflated Sharpe, or just add trials
  to overfit against?
- **Done when:** the tearsheet compares meta-on vs meta-off under Engine B
  with DSR for both; feature importance via **MDA under purged CV** ranks
  which features actually carried information.

### QR-X (optional) — Hierarchical Risk Parity
- Same intellectual spirit, small scope, reuses existing infrastructure: HRP
  clusters the correlation matrix (hierarchical tree) and allocates by
  recursive bisection, avoiding the unstable matrix inversion mean-variance
  needs for near-singular covariance. Running **A5 MVO vs HRP out-of-sample**
  on the QR4 universe is a clean, self-contained research question — and
  hierarchical clustering is ML-adjacent without any return prediction.
- **Done when (if pursued):** both allocators run on the same
  universe/windows; a tearsheet compares out-of-sample Sharpe and turnover,
  judged under CPCV.

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

### F2. Jupyter walkthrough
- Notebook: run C++ engine via subprocess → load results → inline tearsheet.
- **Done when:** `jupyter nbconvert --execute` succeeds from a fresh clone.

### F3. PDF one-pager
- Architecture + key results + repo link.
- **Done when:** the PDF exists in `docs/` and renders correctly.

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
