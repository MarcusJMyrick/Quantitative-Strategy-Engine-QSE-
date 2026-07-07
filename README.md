# QSE — Quantitative Strategy Engine

[![CI](https://github.com/MarcusJMyrick/Quantitative-Strategy-Engine-QSE-/actions/workflows/ci.yml/badge.svg)](https://github.com/MarcusJMyrick/Quantitative-Strategy-Engine-QSE-/actions)

A C++ event-driven backtesting and execution-research engine built around one
question: **how much of a backtest's profit is real, and how much is an
artifact of ignoring market microstructure?**

Most backtesters fill orders instantly at the quoted price. QSE simulates a
full-depth limit order book — FIFO queues at every price level, VWAP-priced
market orders that walk the book, limit orders that wait in queue behind
displayed liquidity — and then **measures the difference**. The engine is
paired with the low-latency machinery real trading systems use (arena
allocation, lock-free queues), a **live paper-trading mode** that runs the
same strategy code against a real venue, and the software discipline both
demand (253 C++ / 57 Python tests, three CI gates, cross-platform
determinism).

On top of the execution engine sits a **quantitative-research track** (in
progress): an eigenportfolio statistical-arbitrage pipeline built under one
rule — a strategy is *viable* only if it survives the realistic fill engine
**and** clears a Sharpe ratio deflated for the number of configurations
tried. Everything else is a candidate, not a result.

---

## The 60-second version

| Finding | Number | Where |
|---|---|---|
| **Phantom profit** a naive backtester hallucinates at 25k shares/signal | **$813,700** (naive says +$153k & Sharpe **+1.93**; realistic engine says −$661k & Sharpe **−5.26**) | [A/B slippage audit](docs/research/microstructure/ab_audit_summary.md) |
| Execution cost grows **superlinearly** with order size | 1.8¢ → 4.6¢ → 7.2¢ per share at 1k/5k/25k | same |
| Simulated market impact matches theory | fitted exponent **b = 0.569** vs square-root law 0.5 (R² = 0.999); linear-impact profile b = 1.017 vs 1.0 | [impact study](docs/research/microstructure/results_summary.md) |
| Arena allocator vs `new`/`delete` | **3.5 ns vs 57–70 ns per allocation (16–20×)**; 2.4× on the order-book workload | [benchmark 04](docs/benchmarks/04_arena_allocator.md) |
| Lock-free SPSC ring vs locked queue (producer push, working consumer) | p99 **42 ns vs 16,334 ns (389×)**; worst case 71 µs vs **1.15 ms**; ThreadSanitizer-clean | [benchmark 05](docs/benchmarks/05_spsc_ring_buffer.md) |
| Tick replay throughput | 19,184 ticks in **24 ms** (~800k ticks/s) | `strategy_engine` |
| Cross-platform determinism | Docker (GCC/x86-64) reproduces native (clang/arm64) metrics **exactly** — Sharpe −2.404, 456 trades | [Dockerfile](Dockerfile) |
| **Live paper trading verified end-to-end** | 15-min market-hours session: 5 crossover signals → 5 real fills → **5/5 orders reconciled** local == venue | `live_engine` (Alpaca REST → lock-free ring → strategy → venue) |
| Mean-variance optimizer traces a textbook efficient frontier | alpha 0.32 → 0.003, portfolio σ 0.35 → 0.002 across a 20-point λ-sweep | [factor research](docs/research/factor/) |
| Market factor structure via random-matrix theory | market eigenvalue clears the Marchenko-Pastur noise edge (λ+ = 2.25) in **100% of 1,432 rolling windows** (median 47% of variance); MP retains 1–2 factors while "explain 55%" wobbles 1–4 | [stat-arb research](docs/research/statarb/README.md) |

---

## The flagship result: the A/B slippage audit

The same 455 SMA-crossover signals, the same data, the same cash — run through
two fill models that differ in nothing else:

- **Engine A (naive):** orders fill instantly at the mid. No spread, no
  impact, no queue. The standard tutorial-backtester assumption.
- **Engine B (institutional):** orders go through the full-depth book — they
  pay the touch, walk the seeded depth profile, and receive the true VWAP of
  the liquidity they consume.

![A/B slippage audit](docs/research/microstructure/slippage_audit.png)

| Shares/signal | Naive PnL | Real PnL | Phantom $ | Naive Sharpe | Real Sharpe |
|---|---|---|---|---|---|
| 1,000 | −$10,011 | −$18,033 | $8,000 | −2.12 | −3.71 |
| 5,000 | −$7,887 | −$112,856 | $105,000 | −0.50 | −4.59 |
| 25,000 | **+$152,866** | **−$660,831** | **$813,700** | **+1.93** | **−5.26** |

At scale, the naive backtester reports a fundable Sharpe-1.9 strategy; the
realistic engine shows a heavy loser. Per-share cost grows with size, so the
distortion is worst at exactly the size a profitable-looking strategy would
scale into. Fully deterministic — rerunning the C++ driver and the Python
audit reproduces these numbers bit-for-bit.

The impact model behind Engine B isn't hand-tuned: sweeping order sizes
through the book recovers the theoretical impact exponent of each depth
profile, including the **square-root law** (b ≈ 0.5, R² > 0.998) that
empirical market studies consistently report:

<p align="center"><img src="docs/research/microstructure/impact_curve.png" width="560"></p>

---

## The research track: strategy discovery under statistical guardrails

The A/B audit above is exactly why "profitable in a paper account" means
nothing: paper fills are near-mid, instant, and infinitely deep. So the
research track (Track QR, in progress) holds every strategy to a harder bar —
**survive Engine B's realistic fills *and* clear a Sharpe deflated for the
number of configurations tried** (CPCV + Deflated Sharpe Ratio, the
López de Prado machinery). The honest headline it aims for is not "a money
printer" but *"after realistic fills and a search-deflated Sharpe, this
survives / does not survive"* — and a defensible negative is a result.

The flagship strategy is **Avellaneda-Lee eigenportfolio stat arb**: rolling
PCA on a 15-name large-cap tech universe, mean-reverting OU residuals,
dollar-neutral s-score trading. Built so far:

- **Universe (QR4.1):** raw daily bars back-adjusted through the audited
  corporate-actions pipeline (the AAPL 4:1 split day reads +3.4% adjusted vs
  the −75% crash in raw data), every repair counted, and a test-enforced
  **as-of contract** — appending future data leaves every emitted row
  bit-identical. 1,432 × 15 standardized matrix, zero NaNs.
- **Factor extraction (QR4.2):** rolling 60-day PCA with the retained-factor
  count set by **random-matrix theory**, not by hand: eigenvalues below the
  Marchenko-Pastur noise edge `λ+ = (1+√(N/T))² = 2.25` are indistinguishable
  from noise and dropped. The market mode clears the edge in every window; a
  second factor is real only episodically — and the popular
  "explain 55% of variance" rule retains 1–4 factors over the same sample,
  which is precisely the arbitrariness the cutoff removes.

![Rolling eigenvalue spectrum vs the Marchenko-Pastur noise edge](docs/research/statarb/eigen_spectrum.png)

- **Idiosyncratic residuals (QR4.3):** each name's return is regressed on the
  retained factors; the residual is the idiosyncratic part and its cumulative
  sum is the mean-reverting process the strategy trades. Factors explain a
  median **50.9%** of each name's daily variance — the other half is tradeable
  residual — and the residuals come out orthogonal to their factors to
  **7×10⁻¹⁵**, the correctness guarantee the next stage relies on.
- **OU fit → s-score (QR4.4):** each residual is fit to a mean-reverting
  Ornstein-Uhlenbeck process, with a speed filter that discards names too slow
  to revert within the window. The resulting s-score is genuinely
  standardized — pooled **mean 0.01, std 0.95** — an end-to-end calibration
  check on the whole PCA → residual → OU chain, with a median mean-reversion
  half-life of **5.8 days**.

![Pooled s-score distribution and the speed filter](docs/research/statarb/ou_sscore.png)

Next in the pipeline: s-score → dollar-neutral weights → the same Engine B
gauntlet as everything else, with the Sharpe deflated for every configuration
tested along the way. Full plan:
[Track QR in TASK_BREAKDOWN.md](docs/TASK_BREAKDOWN.md).

---

## Architecture

```mermaid
flowchart LR
    subgraph ingest [Data layer]
        CSV["CSV / Parquet readers<br/>(malformed-row tolerance,<br/>time-grid gap detection)"]
        PY["Python pipeline<br/>(forward-fill, corporate actions)"]
        ZMQ["ZeroMQ tick feed"]
    end
    subgraph core [C++ engine]
        RING["SPSC lock-free ring<br/>(network thread hand-off)"]
        BB["BarBuilder"]
        STRAT["Strategies<br/>SMA / Pairs / Multi-factor"]
        OM["OrderManager"]
        BOOK["Full-depth order book<br/>FIFO queues - VWAP walks<br/>queue-position limit fills<br/>arena-backed containers"]
    end
    subgraph live [Live mode]
        FEED["Alpaca REST quote feed<br/>(producer thread)"]
        EXE["IExecutionHandler<br/>Simulated / Alpaca paper venue<br/>(order + fill reconciliation)"]
    end
    subgraph out [Analysis layer]
        LOGS["equity + tradelog CSVs"]
        TEAR["tearsheet.py<br/>(Sharpe, Calmar, turnover,<br/>alpha/beta, PDF)"]
        RES["research artifacts<br/>(impact study, A/B audit,<br/>stat-arb universe + PCA)"]
    end
    PY --> CSV --> BB --> STRAT
    ZMQ --> RING --> STRAT
    FEED --> RING
    STRAT --> OM --> BOOK --> LOGS --> TEAR --> RES
    STRAT --> EXE
```

**The microstructure core** (the thesis differentiator):
- `OrderBookFullDepth` — price levels with FIFO order queues, O(1) queue-position
  lookups, market orders that walk levels and return `(filled, VWAP)`,
  per-order fill attribution so strategy orders consumed as makers get credited
- **Queue-position-aware limit fills** — limit orders join the back of the
  queue at their level; trade prints consume the FIFO ahead of them before
  they fill; displayed L1 liquidity is conservatively modeled as always ahead
- Both fill models live behind one config flag, which is what makes the A/B
  audit a controlled experiment

**The quant stack** (beyond the microstructure work): a full cross-sectional
factor pipeline — `UniverseFilter → MultiFactorCalculator →
CrossSectionalRegression → ICMonitor → AlphaBlender → RiskModel →
PortfolioBuilder → FactorExecutionEngine` — plus pairs trading and
SMA-crossover strategies, all YAML-configured and unit-tested.
`PortfolioBuilder` is a constrained QP (net/gross exposure, beta neutrality)
with a true **mean-variance objective**: `−λ/2·wᵀΣw` applied as an O(n)
single-factor covariance operator, where λ = 0 reproduces pure
alpha-maximization bit-for-bit and the λ-sweep traces a textbook
[efficient frontier](docs/research/factor/).

**The live layer** (same strategy code, real venue): a venue-agnostic
`IExecutionHandler` contract with two implementations — the simulated engine
behind it, and `AlpacaExecutionHandler` over the paper REST API (HTTP behind
an injectable seam, so CI tests the full order lifecycle with zero network).
`live_engine` wires Alpaca quote polling → the lock-free ring → BarBuilder →
strategy → venue, logs every order and fill locally, and reconciles per-order
fill quantities against the venue at session end. Verified in a live
market-hours session: 5 signals, 5 fills, 5/5 reconciled.

**The low-latency layer:**
- [`qse::Arena`](include/qse/core/Arena.h) — fixed-capacity bump allocator
  exposed as a `std::pmr::memory_resource`; one contiguous block up front, a
  pointer bump per allocation, O(1) wholesale reset, instrumented
  (high-water mark, allocation counts). The order book's containers are
  pmr-backed, so a book built on an arena packs its nodes contiguously for
  cache locality. Measured: **16–20× faster allocation**, 2.4× on the real
  book-building workload.
- [`qse::SPSCRingBuffer`](include/qse/core/SPSCRingBuffer.h) — lock-free
  single-producer/single-consumer ring with `alignas(64)` on both indices
  *and* each side's cached view of the opposite index (false-sharing defense),
  acquire/release hand-off, and a batched `consume_all` drain.
  [`LiveTickPipeline`](include/qse/messaging/LiveTickPipeline.h) wires it
  between the ZeroMQ network thread and the strategy thread with visible
  drop-counting backpressure. The benchmark reports the honest result:
  throughput parity with a mutexed queue in chase mode (both are bound by the
  same cache-line round-trip) — the win is **tail latency**, where the locked
  design's p99 is 389× worse and its worst case tops a millisecond
  (lock-holder preemption). Market-data hand-off is a tail-latency problem.

---

## Engineering quality

- **253 C++ tests** (GoogleTest, includes two 10M-item lock-free stress tests
  with strict ordering + checksum) and **80 Python tests** (pytest, metrics
  asserted against hand-computed values — including the stat-arb research
  layer, where a causality test proves appending future data leaves every
  emitted row bit-identical, the Marchenko-Pastur cutoff is verified to
  retain 0 factors on pure noise and exactly the planted factor otherwise,
  idiosyncratic residuals are checked orthogonal to their factors to machine
  precision, and the OU estimator recovers known (κ, m, σ) from a simulated
  mean-reverting path)
- **Three CI gates on every push:** build + full test suite, `clang-format`/
  `black`/`flake8`, and a `clang-tidy` static-analysis gate
  (warnings-as-errors) — all tool versions pip-pinned so local == CI
- **Two compilers, two standards, two Arrow versions:** every push builds on
  Apple clang (C++17, Arrow 20) and GCC 13 (C++20, Arrow 24), which has
  caught real portability bugs including API deprecations invisible locally
- **ThreadSanitizer** certifies the lock-free ring race-free over 10M items
  on both consumer paths
- **Determinism as a feature:** the Docker image reproduces native results
  exactly, and every research artifact is reproducible run-to-run
- **The gates caught real bugs**, including undefined behavior (a missing
  return on `WeightsLoader`'s success path), 17 silently ignored Arrow
  `Status` returns, an equity-recording path that had never been wired (every
  historical equity CSV was header-only), and three `failbit` hacks that
  silenced stdout in every binary

---

## Run it

### Docker (zero setup, any OS)

```bash
git clone --recurse-submodules <repository-url>
cd Quantitative-Strategy-Engine-QSE-
docker build -t qse .
docker run -v "$PWD/out:/results" qse
```

That single run executes the SMA 20/50 backtest over the bundled AAPL
minute-tick data and writes `equity_curve.csv`, `tradelog.csv`, and a 3-page
performance `tearsheet.pdf` into `./out/`.

### Native build (macOS / Linux)

```bash
# deps: cmake, a C++17 compiler, arrow+parquet, protobuf, zeromq, yaml-cpp, libcurl
git clone --recurse-submodules <repository-url>
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
cd build && ctest              # 253 tests
./strategy_engine              # sample backtest (from repo root)
```

### Live paper trading (Alpaca)

```bash
set -a; source .env; set +a    # APCA_API_KEY_ID / APCA_API_SECRET_KEY (paper)
./build/live_engine --paper --minutes 10   # quote feed → strategy → venue → reconcile
```

Runs the same strategy code path as the backtest against the Alpaca paper
venue, logging orders/fills locally and reconciling them against the venue at
session end. Guarded to paper-only; refuses to start without `--paper` and
credentials.

### Reproduce the research

```bash
python3 -m venv venv && ./venv/bin/pip install -r requirements.txt
./venv/bin/python scripts/analysis/impact_study.py          # impact exponents
./venv/bin/python scripts/analysis/slippage_audit.py        # phantom-profit audit
./venv/bin/python scripts/analysis/tearsheet.py --equity equity_curve.csv \
    --tradelog tradelog.csv --benchmark data/raw_AAPL.csv --out tearsheet.pdf
./venv/bin/python scripts/research/statarb/build_universe.py  # stat-arb returns matrix
./venv/bin/python scripts/research/statarb/rolling_pca.py     # rolling PCA + MP cutoff
./venv/bin/python scripts/research/statarb/residuals.py       # idiosyncratic residuals
./venv/bin/python scripts/research/statarb/ou_sscore.py       # OU fit + s-score
./build/arena_bench && ./build/spsc_bench                   # latency benchmarks
./build/spsc_tsan_stress                                    # TSan certification
```

---

## Repository map

```
include/qse/, src/       C++17/20 engine: core, data, order, strategy, factor,
                         messaging, exe, live + tools (impact_sweep, ab_audit,
                         live_engine, alpaca_smoke, benches)
tests/cpp/               253 GoogleTest cases incl. mocks and stress tests
scripts/analysis/        tearsheet, impact study, slippage audit
scripts/data/            download/process pipeline, forward-fill, corporate actions
scripts/research/statarb/ eigenportfolio stat arb: universe builder, rolling PCA
tests/python/            57 pytest cases with hand-computed expected values
docs/research/           committed research artifacts (microstructure, factor, statarb)
docs/benchmarks/         benchmark write-ups with reproduction commands
docs/PROJECT_PHASES.md   full narrative: every phase, design rationale, results
docs/TASK_BREAKDOWN.md   execution checklist with per-task done-when criteria
config/                  YAML strategy configs + real corporate-actions history
```

## Documentation

- **[PROJECT_PHASES.md](docs/PROJECT_PHASES.md)** — the whitepaper: all
  phases in detail (execution engine 1–11, research track 12–16), including
  the design rationale for every component above
- **[TASK_BREAKDOWN.md](docs/TASK_BREAKDOWN.md)** — the engineering log:
  testable chunks, each with an explicit done-when criterion and its outcome
- **[Benchmarks](docs/benchmarks/)** · **Research:
  [microstructure](docs/research/microstructure/) ·
  [factor](docs/research/factor/) ·
  [stat arb](docs/research/statarb/README.md)**

## Status

**Complete:** microstructure engine, factor stack (incl. mean-variance
optimizer), data-quality pipeline, research artifacts, low-latency layer,
CI/format/lint gates, Docker, and **live Alpaca paper-trading** (verified
end-to-end in a market-hours session).

**In progress:** the quantitative-research track (Track QR) — the
eigenportfolio stat-arb universe and rolling PCA are done; next are OU
residual modeling, dollar-neutral signals, and the CPCV/Deflated-Sharpe
validation layer that judges them. The thesis write-up follows once those
land. The full plan and its state live in
[TASK_BREAKDOWN.md](docs/TASK_BREAKDOWN.md).
