# Live Alpaca paper-trading — verification evidence

Committed proof that the live path (Alpaca paper venue) actually runs, closing
the gap that the earlier "5/5 reconciled" claim lived only in commit messages.
Only the **SMA crossover** demo strategy is wired to the live venue; the Track-QR
research strategies are backtest/audit-only (see the note at the bottom).

## Re-run — 2026-07-09 23:43 ET (market closed)

Verified live against the paper account (`paper-api.alpaca.markets`):

- **Account:** `ACTIVE`, cash `$99,687.24`, buying power `$399,631.55` — a real
  API response, not a mock.
- **Clock:** `is_open: false` (23:43 ET Thursday; next open 2026-07-10 09:30 ET),
  so this run exercises **connectivity + the order lifecycle**, not fills.
- **`alpaca_smoke --paper`** (full log: [`alpaca_smoke_20260709.log`](alpaca_smoke_20260709.log)):
  submitted a 1-share AAPL buy **limit @ $1.00** (far below market, cannot fill),
  confirmed it `accepted` / `PENDING` (order id `fd4ebe77-…-abb4980eaac9`), then
  `cancelled` it (status → `CANCELLED`). The full submit → query → cancel round
  trip against the real venue, with nothing left resting.

## What still needs market hours

The end-to-end **fill + reconciliation** session — `live_engine --paper` polling
live quotes, the SMA strategy crossing, real fills, and per-order local-vs-venue
reconciliation — requires the market open. Reproduce at/after 09:30 ET:

```bash
set -a; source .env; set +a
./build/live_engine --paper --minutes 15    # quotes -> strategy -> venue -> reconcile
```

## Scope note — the research strategies are not wired to the live venue

The Track-QR strategies (eigen stat arb, momentum, meta-labeling, HRP) emit
`weights_YYYYMMDD.csv` books consumed by the **C++ backtest / Engine-B audit**
tools — they were built to be *judged* (realistic fills + Deflated Sharpe), and
the honest verdict was that none shows a durable, cost-surviving edge. There is
no pipeline connecting those weight files to `AlpacaExecutionHandler`, so the
research strategies have never traded live and are not currently set up to. The
only strategy exercised against Alpaca is the `SMACrossoverStrategy` demo above.
