# QSE Task Breakdown — Roadmap to a Thesis-Grade System

Each chunk is independently completable and has an explicit **Done when** test. Work top to
bottom within a track; tracks are mostly independent of each other. The narrative companion
to this checklist — full phase descriptions including completed work — is
[PROJECT_PHASES.md](PROJECT_PHASES.md).

**Remaining work, recommended order:** D1 → C2 → C3 → G1 → G2 → E1 → E2 → E3 → F1 → F2 → F3 → F4 (A5 optional)
**Completed so far:** A1 → C1 → C4 → A2 → A3 → A4 → B3 → H1 → B1

---

## Current state (audited 2026-07-04)

| Roadmap phase | Actual status |
|---|---|
| 1–3 (costs, perf, threading, ticks, ZeroMQ) | ✅ Done, tested |
| 4.1 Pairs trading | ✅ Done, tested |
| 4.2 Factor model | ✅ Done **far beyond spec**: MultiFactorCalculator, UniverseFilter, CrossSectionalRegression, ICMonitor, AlphaBlender, RiskModel, PortfolioBuilder (QP), FactorExecutionEngine, rebalance guard, YAML config |
| 4.3 Portfolio optimizer | ✅ Mostly done (constrained QP exists); mean-variance extension optional (A5) |
| **OrderBookFullDepth** | ✅ Committed 2026-07-04: all 38 tests pass (PriceLevel, QueuePosition, Impact) |
| 5 Data & tearsheet | ✅ Track B complete 2026-07-05 (B1 ffill, B2 corporate actions, B3 tearsheet) |
| 6 CI / format / lint | 🟡 CI green as of 2026-07-04 (C1+C4 done); formatting (C2) and clang-tidy (C3) remain |
| 7 Live trading | ❌ Not started |
| 8 Presentation | ❌ Not started |
| Docker | ❌ No Dockerfile |
| G Low-latency engineering (arena, SPSC) | ❌ New track (added 2026-07-05) — not started |
| H A/B slippage audit | ✅ Done 2026-07-05 — phantom profit $8k/$105k/$814k at 1k/5k/25k shares |

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

### A5 (optional). Mean-variance extension of PortfolioBuilder
- Add a risk-aversion term (`α·w − λ/2·wᵀΣw`) using RiskModel's covariance;
  expose λ in YAML.
- **Done when:** gtest shows λ=0 reproduces current weights and λ→∞ drives
  weights toward minimum variance; sweep script plots an efficient frontier.

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

### C2. Formatting
- `.clang-format` (LLVM base, 100 col) + one-time reformat commit; `black` +
  `flake8` config for `scripts/`; CI job that runs `clang-format --dry-run
  --Werror` and `black --check`.
- **Done when:** CI fails on an unformatted file, passes on the tree.

### C3. Static analysis
- `.clang-tidy` (start narrow: `bugprone-*`, `performance-*`,
  `modernize-use-override`); fix or suppress existing findings; add CI job.
- **Done when:** `run-clang-tidy` over `src/` exits 0 in CI.

### C4. ✅ Repo hygiene (done 2026-07-04; root analyze_pairs_trading.py left in place — differs from scripts/analysis copy, needs manual merge)
- `.gitignore` for `build/`, `venv/`, `Testing/`, `*.log`, `organized_runs/`,
  `test_output/`; `git rm --cached` the tracked `Testing/Temporary/LastTest.log`;
  move stray root files (`bar_debug.log`, `analyze_pairs_trading.py` duplicate,
  `direct_test`) into place or delete.
- **Done when:** `git status` is clean after a full build + test run.

---

## Track D — Docker

### D1. Multi-stage Dockerfile
- Stage 1 (build): `ubuntu:24.04` + g++, cmake, libarrow/parquet, protobuf,
  libzmq, yaml-cpp → compile. Stage 2 (runtime): copy binaries + runtime libs +
  Python analysis env. `docker-compose.yml` optional for the pub/sub demo.
- **Done when:** `docker build -t qse . && docker run qse` completes a sample
  backtest and writes results to a mounted volume, on a machine with only Docker.
- Note: also gives you a Linux build environment matching CI (C1), which is
  why D1 pairs well with Track C.

---

## Track E — Live Paper Trading (Phase 7)

### E1. `IExecutionHandler` interface
- `submit_order / cancel_order / on_fill` interface; adapt current backtest
  fill logic into a `SimulatedExecutionHandler` implementing it; strategies
  depend only on the interface.
- **Done when:** all existing tests pass with the simulated handler injected,
  plus new gmock-based tests for the interface contract.

### E2. `AlpacaExecutionHandler`
- REST calls to Alpaca paper API (libcurl + nlohmann/json); API keys from
  `.env`/environment, never committed.
- **Done when:** unit tests run against a mock HTTP layer (no network in CI);
  a manual `--paper` smoke test places and cancels one order in the Alpaca
  dashboard.

### E3. Live mode
- `qse_app --mode live`: Alpaca market-data websocket → existing `BarBuilder`
  → strategy → `AlpacaExecutionHandler`. Reuses the tick pipeline you already
  built for ZeroMQ.
- **Done when:** SMA strategy runs against live paper data for a session and
  the resulting Alpaca paper fills reconcile with the local trade log.

---

## Track G — Low-Latency Engineering 🎓

*Hardware-level engineering: heap jitter, cache locality, false sharing, atomic
memory ordering. Each chunk produces a committed benchmark artifact, not just code.*

### G1. Arena (bump) allocator for the hot path
- Implement `qse::Arena`: one large contiguous block requested up front, a
  single offset pointer "bumped" per allocation, **no per-object deallocation**
  — the whole arena resets in one operation at end of session/backtest. Either
  a custom bump allocator or `std::pmr::monotonic_buffer_resource` wrapped
  with instrumentation (bytes used, high-water mark, allocation count).
- Adopt in the hot path: back `OrderBookFullDepth`'s per-level containers with
  `std::pmr` equivalents so resting orders pack contiguously — the point is
  L1/L2 cache locality, and the fill benchmark must show it.
- Files: new `include/qse/core/Arena.h`, `OrderBookFullDepth.h/.cpp`,
  new `tests/cpp/ArenaTest.cpp`, microbenchmark tool in `src/tools/`
- **Done when:** gtest covers alignment guarantees, bump arithmetic,
  exhaustion policy, and reset semantics; an allocation microbenchmark (≥1M
  allocations, arena vs `new`/`delete`) plus a before/after full-depth fill
  benchmark are committed to `docs/benchmarks/04_arena_allocator.md`; full
  suite still green on mac and CI.

### G2. SPSC lock-free ring buffer
- Implement `qse::SPSCRingBuffer<T>`: fixed power-of-two capacity; the
  producer owns an atomic `write_index`, the consumer owns an atomic
  `read_index`; **`alignas(64)` on each index** so the two cores never share a
  cache line (false sharing kills exactly this structure); acquire/release
  ordering on the hand-off, relaxed loads on the owner's own index.
- Integrate: the ZeroMQ subscriber (network thread, producer) hands ticks to
  the strategy thread (consumer) through the ring instead of any locked
  structure — this is the prerequisite for live mode (E3), where a blocked
  network thread means dropped market data.
- Files: new `include/qse/core/SPSCRingBuffer.h`, the subscriber/live-feed
  path, new `tests/cpp/SPSCRingBufferTest.cpp`
- **Done when:** unit tests cover empty/full/wraparound/ordering; a two-thread
  stress test moves ≥10M items with a checksum match and runs clean under
  ThreadSanitizer (`-fsanitize=thread`); a throughput benchmark vs a
  `std::mutex`+`std::queue` baseline is committed to
  `docs/benchmarks/05_spsc_ring_buffer.md`.

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

## Track F — Presentation & Thesis (Phase 8)

### F1. Whitepaper README
- Architecture diagram (Mermaid), headline benchmark numbers, tearsheet
  screenshot, quickstart, link to this file.
- **Done when:** a reader can build + run a backtest using only the README.

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
- **Done when:** `docs/thesis/` contains a 25–40 page paper: intro, related
  work, architecture, methodology, results, limitations.
