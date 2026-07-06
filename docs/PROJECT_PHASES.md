# QSE — Complete Phase Breakdown

*The Quantitative Strategy Engine: from Python-grade backtester to an
institutional-grade C++ trading system.*

This is the narrative companion to [TASK_BREAKDOWN.md](TASK_BREAKDOWN.md). The
breakdown is the execution checklist (chunks with pass/fail criteria); this
document explains **what each phase is, why it exists, how it was or will be
built, and what it proves** — including the phases that are already complete.

**Thesis research question:** *How much does market-microstructure realism —
a full-depth limit order book, VWAP fills, and queue position — change the
measured performance of trading strategies versus the fixed-slippage
assumptions of standard backtesters?*

---

## System at a glance

```
                       ┌────────────────────────────────────────────┐
  data/*.csv ─────────►│ CSVDataReader / ParquetDataReader          │
  (bars & ticks)       └──────────────┬─────────────────────────────┘
                                      │ ticks
  ZeroMQ + protobuf ──► TickSubscriber┤
  (distributed mode)                  ▼
                       ┌────────────────────────────────────────────┐
                       │ Backtester ── BarBuilder ── BarRouter      │
                       │  (tick loop, per-symbol bars, mark-to-mkt) │
                       └──────┬─────────────────────────┬───────────┘
                              │ bars/ticks              │ orders
                       ┌──────▼──────────┐    ┌─────────▼───────────┐
                       │ IStrategy       │    │ OrderManager        │
                       │  SMA crossover  │    │  naive fills  ──or──│
                       │  Pairs trading  │    │  OrderBookFullDepth │
                       │  Factor / multi │    │  (FIFO queues, VWAP │
                       └─────────────────┘    │   walk, queue pos)  │
                                              └─────────┬───────────┘
                                                        │ equity/tradelog CSVs
                       ┌────────────────────────────────▼───────────┐
                       │ Python analysis: tearsheet.py,             │
                       │ impact_study.py, (slippage_audit.py)       │
                       └────────────────────────────────────────────┘
```

**Current scale:** ~19k ticks replayed in 24 ms (~800k ticks/s) on Apple
Silicon; 207 C++ tests + 16 Python tests; CI green on every push
(ubuntu-latest, GCC 13 + Arrow 24 at C++20, cross-checked against local
Apple clang at C++17).

---

## Phase 1 — Foundational Realism ✅

**Goal:** make the simplest backtest honest.

- **Transaction costs.** `OrderManager` applies per-trade commission and a
  slippage adjustment to every fill, so PnL is net of costs from day one. A
  naive SMA run on SPY loses ~$490 on $100k — a realistic outcome that a
  cost-free backtester would report as profit.
- **O(1) indicators.** The SMA crossover strategy uses rolling
  `MovingAverage` / `MovingStandardDeviation` accumulators (constant-time
  update per bar) instead of recomputing windows — the difference between
  O(n·w) and O(n) over a full backtest.
- **Auditable output.** Every run writes `equity_*.csv` (mark-to-market curve)
  and `tradelog_*.csv` (every fill with price, quantity, cash), which the
  entire Python analysis layer consumes.

**Proves:** understanding that backtest PnL without costs is fiction.

## Phase 2 — Performance Engineering ✅

**Goal:** measure before optimizing; keep receipts.

- Profiled the engine with **Instruments** to find real bottlenecks rather
  than guessing.
- Eliminated per-bar copies, pre-allocated vectors, and tightened the SMA
  hot path. Historical benchmark: the 6,444-bar SPY backtest ran in
  **~3.8 ms** after optimization.
- Before/after evidence lives in `docs/benchmarks/01–03_*.png`.

**Proves:** disciplined performance work — profile, change one thing, measure.

## Phase 3 — Advanced Architecture ✅

**Goal:** the three structural jumps that separate a script from a system.

- **3.1 Multi-asset parallelism.** A hand-rolled C++ `ThreadPool` runs
  independent per-symbol backtests concurrently (`multi_symbol_engine`,
  `multi_strategy_engine`).
- **3.2 Tick-driven core.** The `Backtester` consumes raw ticks;
  `BarBuilder` aggregates them into time bars on the fly and `BarRouter`
  dispatches per-symbol bars, so bar-driven strategies (SMA, pairs) run
  unchanged on tick data. Handles out-of-order ticks, bar boundaries, and
  end-of-stream flush — all unit-tested.
- **3.3 Distributed mode.** `data_publisher` and `strategy_engine` are
  separate executables connected by **ZeroMQ** pub/sub with **protobuf**
  serialization (`TickPublisher` / `TickSubscriber` / `ZeroMQDataReader`) —
  the standard shape of production market-data infrastructure.

**Proves:** concurrency, event-driven design, and distributed-systems layout.

## Phase 4 — Quantitative Modeling ✅ (far beyond original spec)

**Goal:** breadth beyond trend-following; the original plan asked for a
"simple factor model" — what exists is a full cross-sectional research stack.

- **4.1 Statistical arbitrage.** `PairsTradingStrategy`: spread z-score entry
  and exit with rolling hedge ratio; pair discovery and diagnostics in
  `scripts/analysis/find_pairs.py` and friends.
- **4.2 Multi-factor pipeline.** The chain runs
  `UniverseFilter → MultiFactorCalculator → CrossSectionalRegression
  (robust variants tested) → ICMonitor (information-coefficient tracking,
  distribution-tested) → AlphaBlender (IC-weighted signal blending with
  weight-sum property tests) → RiskModel (multi-asset beta/covariance) →
  PortfolioBuilder`. Factor data flows through **Apache Arrow/Parquet**
  tables; configuration is YAML; `compute_factors` is the standalone tool.
- **4.3 Portfolio optimization.** `PortfolioBuilder` is a constrained
  **quadratic-programming optimizer**: maximizes blended alpha subject to
  gross/net exposure caps and beta neutrality, with projection back to the
  feasible set. The A5 extension (2026-07-06) adds true **mean-variance**:
  `−λ/2·wᵀΣw` with the single-factor covariance Σ = σ_m²ββᵀ + diag(σ_resid²)
  from RiskModel, λ in YAML; λ=0 reproduces pure alpha-maximization exactly
  and the λ-sweep traces a textbook efficient frontier
  (`docs/research/factor/efficient_frontier.png`). `FactorStrategy` + `FactorExecutionEngine` turn target
  weights into delta orders behind a rebalance guard (no churn below
  threshold), from `WeightsLoader`-provided daily weight files.

**Proves:** the vocabulary and mechanics of modern quant research — factors,
IC, blending, risk models, constrained portfolio construction.

## Phase 5 — Market Microstructure ✅ (the thesis core)

**Goal:** replace "fills happen at the price you asked" with a real market.

- **5.1 Full-depth order book.** `OrderBookFullDepth`: price-ordered levels
  (bids descending, asks ascending), each level a **FIFO queue** of orders
  with per-order sizes, O(1) queue-position lookup via position maps, and
  stable queue IDs for cancellation.
- **5.2 Impact-priced market orders (A2).** Market orders **walk the book**:
  consume the best level, then the next, paying the volume-weighted average —
  so a 10,000-share order pays measurably more per share than a 100-share
  order against the same book. Selected per run by the `fill_model` config
  flag (`naive` vs `full_depth`), keeping the A/B comparison one config edit
  apart.
- **5.3 Queue-position-aware limit fills (A3).** Passive orders join the
  **back** of the queue at their price. Trade prints consume the queue
  FIFO — an order fills only after the displayed size ahead of it is
  exhausted. Marketable limits take liquidity up to their limit price and
  rest the remainder. Cancels remove orders from the queue. Displayed quote
  size re-enters at the **front** of its level on refresh (the conservative
  queue assumption forced by L1 data — the real queue is invisible). Maker
  fills are attributed even when one strategy order trades against another.
- **5.4 Empirical impact study (A4).** `impact_sweep` + `impact_study.py`
  sweep order sizes 50→51,200 through the book against real AAPL tick prices
  under two synthetic depth profiles and fit the impact law
  `slippage = a·Q^b`:

  | Depth profile | Fitted b | Theory | R² |
  |---|---|---|---|
  | uniform (equal size/level) | **1.017** | 1.0 | 0.9999 |
  | linear (deepening book)    | **0.569** | 0.5 (square-root law) | 0.9989 |

  The exponent *emerges* from liquidity distribution — the linear cost model
  every naive backtester assumes is exactly the uniform-depth special case,
  and real markets (square-root law) are not that case.

**Proves:** microstructure literacy — priority rules, adverse selection,
market impact — implemented, not just cited.

## Phase 6 — Data Quality & Analysis ✅

**Goal:** institutional-grade inputs and outputs.

- **6.1 Tearsheet ✅ (B3).** `scripts/analysis/tearsheet.py`: annualized
  Sharpe, max drawdown, CAGR, Calmar, annualized turnover, rolling Sharpe,
  and alpha/beta OLS vs a benchmark, rendered to a 3-page PDF. All metrics
  are pure functions with 16 pytest cases asserting hand-computed values to
  4 decimals. Building it exposed and fixed three real engine bugs: equity
  curves were never recorded, three failbit hacks silenced stdout in every
  qse binary (now opt-in `QSE_DEBUG=1` via `qse/core/Debug.h`), and the
  strategy engine never tagged ticks with a symbol.
- **6.2 Missing-data handling ✅ (B1).** `forward_fill_ticks` in the Python
  pipeline forward-fills prices, zeroes missing volumes, and reports every
  repair instead of silently dropping rows; `CSVDataReader` counts
  unparseable rows (one bad row no longer aborts a load) and surfaces
  time-grid holes via `gap_count()`, with a data-quality warning at load.
- **6.3 Corporate actions ✅ (B2).** `corporate_actions.py` back-adjusts
  splits and dividends from `config/corporate_actions.csv` (real split
  history for five names), with factors computed on the raw series and
  compounded correctly across multiple events. Verified against the AAPL 4:1
  split (2020-08-31): prices ÷4, volumes ×4, and a buy-and-hold equity curve
  flat across the split date where the raw series shows a fake −75% crash.

**Proves:** results you can hand to a PM, from inputs you can trust.

## Phase 7 — DevOps & Reproducibility ✅

**Goal:** engineering hygiene that survives other people's machines.

- **7.1 CI ✅ (C1).** GitHub Actions on every push/PR: Ubuntu runner installs
  Arrow/Parquet from the official apt repo plus protobuf/ZeroMQ/yaml-cpp,
  checks out the Eigen submodule, builds Release, runs the full ctest suite
  (~2.5 min). Because Arrow 24 forces C++20 on GCC while local builds are
  C++17 Apple clang, **every push is cross-checked against two compilers and
  two language standards** — this caught five classes of portability bugs on
  day one.
- **7.2 Repo hygiene ✅ (C4).** Untracked stale binaries and ctest logs;
  `git status` stays clean through a full build + test cycle.
- **7.3 Formatting ✅ (C2).** `.clang-format` + `black`/`flake8` enforced by a
  dedicated CI job with pip-pinned tool versions (identical output local and
  CI); one-time mechanical reformat of the whole tree, all suites green after.
- **7.4 Static analysis ✅ (C3).** `clang-tidy` gate (bugprone/performance/
  modernize-use-override, warnings-as-errors) over every built TU, pinned
  version, dedicated CI job. The clean-up fixed 49 findings including a real
  bug — a missing return on `WeightsLoader`'s Arrow-success path (UB) — and
  17 silently ignored Arrow `Status` returns.
- **7.5 Docker ✅ (D1).** Multi-stage `Dockerfile`: stage 1 compiles on the
  same ubuntu:24.04 + Arrow toolchain as CI, stage 2 ships binaries + runtime
  libs + the Python analysis stack. `docker run -v "$PWD/out:/results" qse`
  runs the SMA demo and writes equity curve, tradelog, and tearsheet PDF to
  the host — with metrics identical to the native macOS run, doubling as a
  cross-platform determinism check.

**Proves:** the difference between code that works here and software that
works anywhere.

## Phase 8 — Low-Latency Engineering ✅ (Track G)

**Goal:** hardware-sympathy — the layer trading firms actually interview on.

### 8.1 Custom memory management: the arena allocator ✅ (G1)

*The problem.* Every `new`/`malloc` walks OS heap structures in unpredictable
time — 10 ns on a good day, thousands when it triggers a page fault or
context switch. In trading, **jitter** (variance, not mean) is what loses the
trade; per-order heap allocation in the fill path is a fatal flaw.

*The build.* `qse::Arena` — a bump allocator: one large contiguous block
requested up front; each allocation returns the current offset and bumps it
by the object size (two instructions, perfectly predictable); **no individual
deallocation ever** — the whole arena resets in one operation when the
session ends. Implemented as a custom allocator or C++17
`std::pmr::monotonic_buffer_resource` with instrumentation, then plugged into
`OrderBookFullDepth`'s level containers via `std::pmr`.

*The payoff (measured 2026-07-05).* Raw allocation path: 57–70 ns/op heap →
**3.5 ns/op arena (16–20×)**, stable to ~0.1 ns run-to-run where the heap
varies — the determinism is the point. End-to-end order-book workload (2,000
books × 200 levels, build + VWAP walk + destroy): 139 µs → **58 µs/book
(2.4×)** even though allocation is only part of each iteration, because live
orders pack sequentially and the prefetcher streams them through L1/L2 during
book walks. Full numbers: `docs/benchmarks/04_arena_allocator.md`.

### 8.2 Lock-free multi-threading: the SPSC ring buffer (G2)

*The problem.* Live mode needs a network thread (feeding ticks) and a
strategy thread (trading on them). Guard the hand-off with a `std::mutex` and
the network thread blocks whenever the strategy holds the lock — a frozen
feed during the exact moments the market is moving.

*The build.* `qse::SPSCRingBuffer<T>` — a fixed power-of-two array that wraps
around; the producer owns an atomic `write_index`, the consumer owns an
atomic `read_index`; neither ever locks. Two hardware details carry the
design:
  - **False sharing.** CPUs move memory in 64-byte cache lines. If the two
    indices share a line, each core's write invalidates the other core's L1
    copy on every operation — a silent, catastrophic slowdown. `alignas(64)`
    forces the indices onto separate lines.
  - **Memory ordering.** The hand-off uses acquire/release semantics
    (`std::memory_order_acquire`/`release`) so the consumer never observes an
    index advance before the payload write, while each side reads its own
    index relaxed.

*The result (measured 2026-07-06).* Two 10M-item stress tests (strict
ordering + checksum) pass and ThreadSanitizer certifies both consumer paths
race-free. The benchmark (`docs/benchmarks/05_spsc_ring_buffer.md`) reports
the honest pair of findings: raw throughput is at *parity* with a mutexed
queue in item-at-a-time chase mode (~23M items/s — both designs are bound by
the same cross-core cache-line round-trip), while the ring wins where it
matters: with a consumer doing 200ns of work per tick, producer push latency
is p99 **42ns vs 16,334ns** for the locked design (389×), worst case 71µs vs
**1.15ms** (lock-holder preemption), and 1.75× total wall time from pipeline
overlap. Market-data hand-off is a tail-latency problem, and locks have
unbounded tails by construction. Integrated as `qse::LiveTickPipeline`
(ZeroMQ subscriber thread → ring → strategy thread, dropped ticks counted) —
the prerequisite for Phase 10's live feed, tested end to end over real
ZeroMQ.

**Proves:** cache lines, atomics, and allocation behavior — the exact
territory of a C++ trading-systems interview.

## Phase 9 — The Business Proof: A/B Slippage Audit ✅ (Track H)

**Goal:** convert all the infrastructure above into one financial number.

*The problem.* Standard backtesters assume infinite liquidity: "buy 5,000
shares" fills instantly at the close. In reality the order eats through the
book, pays VWAP degradation, and may wipe out the strategy's edge. The profit
difference between those two worlds is invisible — until you measure it.

*The build (H1).* Run the **exact same strategy on the exact same data**
through two engine configurations that already exist behind the `fill_model`
flag:
  - **Engine A (naive):** instant fills, fixed slippage — the
    infinite-liquidity assumption every tutorial backtester makes.
  - **Engine B (institutional):** the Phase 5 book — market orders walk
    levels and pay true VWAP, limit orders wait their turn in the FIFO queue.

Repeat across order-size regimes (1×, 10×, 50×). `slippage_audit.py` overlays
the paired equity curves; the gap between them is the **phantom profit** —
the dollar amount the naive backtest hallucinated. The report (built on the
Phase 6 tearsheet machinery) states it as the headline: *"the naive backtest
overstates this strategy's Sharpe by X% at size Y because it ignores queue
position and depth."*

*The result (measured 2026-07-05).* Identical SMA signals (455 per run),
deterministic and reproducible:

| Shares/signal | Naive PnL | Real PnL | Phantom $ | Phantom $/share | Naive Sharpe | Real Sharpe |
|---|---|---|---|---|---|---|
| 1,000 | −$10,011 | −$18,033 | $8,000 | $0.018 | −2.12 | −3.71 |
| 5,000 | −$7,887 | −$112,856 | $105,000 | $0.046 | −0.50 | −4.59 |
| 25,000 | **+$152,866** | **−$660,831** | **$813,700** | $0.072 | **+1.93** | **−5.26** |

At scale, the naive backtester reports a Sharpe-1.9 *winner*; the realistic
engine shows a heavy loser. Per-share phantom cost grows with size, so the
distortion is superlinear — worst exactly where a profitable-looking strategy
would scale into. Artifacts: `docs/research/microstructure/slippage_audit.pdf`
+ `ab_audit_summary.md`.

**Proves:** the intersection the whole project aims at — quantitative
research claims, validated or destroyed by systems engineering. This is the
artifact to hand across the interview table, and the results chapter of the
thesis.

## Phase 10 — Live Trading Integration ⏳ (Track E)

**Goal:** demonstrate the engine is an execution system, not just a simulator.

- **10.1 `IExecutionHandler` ✅ (E1, done 2026-07-06).** Venue-agnostic
  contract (submit market/limit, cancel, replace, fill stream) shaped to map
  1:1 onto Alpaca's REST API; `SimulatedExecutionHandler` puts the real
  backtest fill engine behind it, verified equivalent to direct OrderManager
  use; gmock contract tests prove callers need only the interface.
- **10.2 Alpaca paper trading ✅ (E2, done 2026-07-06).**
  `AlpacaExecutionHandler` implements the E1 contract over the Alpaca paper
  REST API — submit/cancel/replace/query plus a polling fill stream —
  behind an injected `IHttpClient` (libcurl in production, gmock in tests,
  zero network in CI). Credentials only from the official APCA_* environment
  variables; the `alpaca_smoke --paper` tool performs the manual
  place-and-cancel dashboard verification.
- **10.3 Live mode ✅ (E3, done 2026-07-06).** The `live_engine` tool:
  Alpaca REST quote feed (producer thread; chosen over a websocket dependency
  — same architecture, transport swappable) → **SPSC ring buffer (Phase 8)**
  → `BarBuilder`/SMA strategy → `AlpacaExecutionHandler`, with local
  order/fill logs and an end-of-session per-order reconciliation against the
  venue. The engine is venue-agnostic (any drain source, any
  IExecutionHandler), so the strategy logic is identical between backtest and
  live — the entire point of event-driven design. Verified with mock-venue
  unit tests both ways (match and mismatch) plus a live market-hours
  session (2026-07-06): five crossover signals, five real paper fills, and a
  5/5 per-order reconciliation match against the venue.

**Proves:** full-stack capability — the same code path from historical CSV to
a live brokerage session.

## Phase 11 — Presentation & Thesis ⏳ (Track F)

**Goal:** make the work legible to recruiters, professors, and hiring managers.

- **11.1 Whitepaper README ✅ (F1, done 2026-07-06).** Full rewrite: findings
  table with every measured number, the A/B audit as the flagship section
  with committed figures, Mermaid architecture, engineering-quality evidence,
  and three quickstart paths (Docker, native, reproduce-the-research).
- **11.2 Notebook walkthrough (F2).** Jupyter demo: run the C++ engine,
  load results, render the tearsheet inline — executes clean via
  `nbconvert --execute`.
- **11.3 One-pager (F3).** Single PDF: architecture, key results, repo link.
- **11.4 Thesis write-up (F4).** 25–40 pages in `docs/thesis/`: introduction,
  related work (impact laws, queue models), system architecture (Phases 1–8),
  methodology (Phases 5, 9), results (impact exponents, phantom profit,
  Sharpe inflation), limitations (L1-reconstructed depth, no self-trade
  prevention, synthetic queue assumptions), future work.

**Proves:** communication — the skill that makes the other ten phases count.

---

## Results ledger (updated as phases land)

| Evidence | Value | Where |
|---|---|---|
| Tick replay throughput | ~800k ticks/s (19,184 ticks / 24 ms) | strategy_engine |
| Historical bar-backtest benchmark | ~3.8 ms / 6,444 bars | docs/benchmarks |
| Impact exponent, uniform depth | b = 1.017 (theory 1.0), R² 0.9999 | docs/research/microstructure |
| Impact exponent, linear depth | b = 0.569 (theory 0.5), R² 0.9989 | docs/research/microstructure |
| C++ tests | 207 (ctest, mac + Linux CI) | tests/cpp |
| Python tests | 16 (pytest, hand-computed metrics) | tests/python |
| CI wall time | ~2.5 min per push | GitHub Actions |
| Phantom profit at 25k sh/signal | $813,700 (naive Sharpe +1.93 vs real −5.26) | docs/research/microstructure |
| Arena vs heap allocation | 3.5 ns/op vs 57–70 ns/op (16–20×); book workload 2.4× | docs/benchmarks/04 |
| SPSC ring vs locked queue | p99 push 42ns vs 16,334ns (389×); max 71µs vs 1.15ms; TSan clean | docs/benchmarks/05 |
