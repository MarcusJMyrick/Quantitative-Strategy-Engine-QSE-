# QSE Task Breakdown — Roadmap to a Thesis-Grade System

Each chunk is independently completable and has an explicit **Done when** test. Work top to
bottom within a track; tracks are mostly independent of each other.

**Recommended order:** A1 → C1 → C4 → A2 → B3 → A3 → A4 → B1 → B2 → D1 → C2 → C3 → E1 → E2 → E3 → F1 → F2 → F3 → F4

---

## Current state (audited 2026-07-04)

| Roadmap phase | Actual status |
|---|---|
| 1–3 (costs, perf, threading, ticks, ZeroMQ) | ✅ Done, tested |
| 4.1 Pairs trading | ✅ Done, tested |
| 4.2 Factor model | ✅ Done **far beyond spec**: MultiFactorCalculator, UniverseFilter, CrossSectionalRegression, ICMonitor, AlphaBlender, RiskModel, PortfolioBuilder (QP), FactorExecutionEngine, rebalance guard, YAML config |
| 4.3 Portfolio optimizer | ✅ Mostly done (constrained QP exists); mean-variance extension optional (A5) |
| **OrderBookFullDepth** | ✅ Committed 2026-07-04: all 38 tests pass (PriceLevel, QueuePosition, Impact) |
| 5 Data & tearsheet | ❌ analyze.py has only Sharpe + max drawdown; no ffill, no corporate actions, no PDF |
| 6 CI / format / lint | 🟡 CI green as of 2026-07-04 (C1+C4 done); formatting (C2) and clang-tidy (C3) remain |
| 7 Live trading | ❌ Not started |
| 8 Presentation | ❌ Not started |
| Docker | ❌ No Dockerfile |

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

### B1. Missing-data handling (forward-fill)
- Python: `process_data.py` gains ffill + gap report. C++: `CSVDataReader` /
  `ParquetDataReader` tolerate and flag missing bars instead of silently
  misbehaving.
- **Done when:** pytest on a synthetic gappy CSV verifies filled values, and a
  gtest confirms the reader surfaces the gap count.

### B2. Corporate actions handler
- `CorporateActionsHandler` (C++ or Python — pick one layer, don't do both)
  applying split/dividend back-adjustments from an actions CSV.
- **Done when:** test with a known event (e.g., AAPL 4:1 split 2020-08-31)
  shows pre-split prices ÷4, volumes ×4, and an equity curve unchanged across
  the split date for a buy-and-hold backtest.

### B3. Institutional tearsheet
- Extend `scripts/analysis/analyze.py`: Calmar, annualized turnover, rolling
  Sharpe (63d window) plot, benchmark-relative plot vs SPY with alpha/beta
  regression, PDF export (matplotlib `PdfPages` is enough).
- **Done when:** pytest feeds a synthetic equity curve with hand-computed
  Sharpe/Calmar/turnover and asserts each metric to 4 decimals; running on a
  real backtest emits `tearsheet.pdf`.

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
