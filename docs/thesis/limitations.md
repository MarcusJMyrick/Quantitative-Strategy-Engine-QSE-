# Limitations (thesis section)

The credibility of this artifact rests on stating what it *cannot* claim as
plainly as what it can. This is the thesis Limitations section, maintained as
the work lands so the final write-up (F4) inherits an already-honest ledger.
The headline entry is the **depth-data fork** (QR-Data), settled here before the
OFI/VPIN engines are built.

---

## 1. The depth-data fork — decision and caveat (QR-Data)

**The fork.** A faithful Order-Flow-Imbalance (OFI) signal needs real
market-by-order or level-2/3 depth updates (every quote insertion, cancel, and
trade at each price level). This project has no such feed. Two paths were
possible:

- **(A) Stay on L1-reconstructed depth** — the trade-print tape (timestamp,
  price, volume) plus a *reconstructed* book: synthetic depth profiles seeded
  from volume, which the `OrderBookFullDepth` engine then walks. Honest result:
  *"an approximate toxicity filter improves fills in simulation"* — valid, but
  **not** "OFI predicts price."
- **(B) Source real MBO / L2** — e.g. **Databento MBO** or **IEX DEEP** — for a
  genuine OFI result at the cost of a paid, heavy data-engineering dependency.

**Decision: (A), L1-reconstructed depth.** Three reasons:

1. **Data reality.** The available inputs are L1 trade prints for a handful of
   names and daily OHLCV for the 15-name QR4 universe. There is no quote/depth
   feed. The engine's book is fed *reconstructed* depth (uniform/linear profiles
   seeded from volume), validated only in aggregate against the square-root
   impact law (A4: fitted exponent b = 0.569 vs 0.5, R² = 0.999).
2. **Latency reality.** Even *with* real MBO, the OFI edge decays in seconds,
   while this system's fill path is REST polling at hundreds of milliseconds
   (E2/E3). The signal is gone before an order lands — so OFI-as-alpha is not
   executable here regardless of the data.
3. **Cost / scope.** Real MBO is a paid feed and a substantial ingestion
   pipeline, out of scope for a portfolio artifact whose thesis is execution
   *realism*, not depth-data engineering.

**The caveat this imposes.** Consequently QR-P4 scopes OFI/VPIN as an
**execution-timing / toxicity filter** inside `OrderManager` — judged solely by
whether it improves fills in the A/B audit — and **not** as a standalone alpha.
We can honestly claim *"an approximate toxicity filter built on L1-reconstructed
depth does / does not improve simulated fills."* We explicitly **cannot** claim
*"OFI predicts returns."* Path (B) — Databento MBO / IEX DEEP — is recorded as
future work that would enable a genuine OFI result.

---

## 2. Order-book realism (the thesis core, and its edges)

- **Reconstructed depth, not observed depth.** The full-depth book is real code
  (FIFO queues, VWAP walks, queue position), but the *liquidity* it walks is
  synthetic — seeded from volume, not from an observed book. The impact-law fit
  (A4) shows the mechanism is sound; the absolute cost level depends on the
  seeding assumptions. In the QR4.7 stat-arb audit these are stated inline: the
  A-vs-B *gap growing with order size* is the robust finding, the absolute
  phantom-cost level is not.
- **Conservative queue assumption.** With only L1 data the true queue ahead of a
  resting order is invisible, so displayed size is modeled as always ahead of
  our order (the pessimistic case). Real fills would be at least this good.
- **No self-trade prevention.** The engine can match a strategy's own orders
  against each other; a production venue would prevent this.

## 3. Data provenance

- **IEX-partial volume.** The QR4 universe and the SPY regime proxy are fetched
  from Alpaca's **IEX** feed (SIP is not available on the plan). IEX prints
  ~2–3% of consolidated volume, so absolute volumes understate the tape.
  Volume *ratios* (regime `vol_ratio_63`) and returns are robust to this;
  absolute-liquidity uses (audit depth seeding) scale IEX volume toward a
  consolidated-ADV estimate, a documented approximation.
- **Survivorship / selection bias.** The QR4 universe is today's large-cap tech
  names applied retroactively — fine for the execution-realism-and-deflation
  thesis, not for claiming the signal generalizes to a point-in-time universe.
- **Dividend adjustment gaps.** Only corporate actions listed in
  `config/corporate_actions.csv` are back-adjusted; unlisted ex-dividend days
  carry a small artificial negative return (~0.2–0.7%/quarter for the payers),
  second-order against ~2% daily vol.
- **Regime window coverage.** IEX daily history begins mid-2020, so the regime
  overlay's SPY features start 2020-10 and **miss the March 2020 COVID crash**;
  they do capture the 2022 bear and the April 2025 selloff.

## 4. Strategy-research scope

- **Provisional Sharpes until deflation.** Every strategy Sharpe (QR4.7 Engine-B
  results, the baselines) is a *candidate*, not a result, until deflated for the
  number of configurations tried (QR-P2 DSR). This is a feature, not a caveat —
  but it means no headline Sharpe should be read as final without its DSR and
  trial count.
- **Small universe.** QR4 is deliberately proven on 15 names before any scaling
  to 100; the eigenportfolio factor structure and OU estimates are
  correspondingly low-dimensional.
- **Regime overlay is risk control, not alpha.** The HMM overlay is expected to
  cut drawdown, not add return; a distinct "crash" state did not even separate
  out over this window (calm/elevated/turbulent only). Reported, not assumed.

---

*Maintained across QR-P4/QR-P5; folded into the thesis Limitations chapter at
F4.*
