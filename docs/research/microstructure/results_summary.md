# Market Impact Study

*Generated 2026-07-05 by `scripts/analysis/impact_study.py`*

## Setup

- 200 price samples drawn evenly from `data/raw_ticks_AAPL.csv`
- Synthetic quote (one-tick half-spread) with 800 depth levels behind the touch
- Market buy orders swept from 50 to 51,200 shares through
  `OrderBookFullDepth::fill_market`; slippage measured as the VWAP's
  distance past the touch, in bps of the mid price
- Power law `slippage = a * Q^b` fitted by OLS on log-log means over the
  asymptotic tail (Q >= 1,600); smaller orders reach only a
  few discrete levels, which biases the local slope upward

## Fitted impact exponents

| Depth profile | Fitted b | Theory | R² | Points |
|---|---|---|---|---|
| linear | 0.569 | 0.5 | 0.9989 | 6 |
| uniform | 1.017 | 1.0 | 0.9999 | 6 |

## Interpretation

The impact exponent is a property of how liquidity is distributed
through the book, and the walked-book simulation recovers the
theoretical value for each profile:

- **uniform** depth (equal size at every level) makes cumulative depth
  grow linearly with distance, so cost grows linearly with order size
  (b ≈ 1) — the assumption implicitly made by the legacy linear
  slippage coefficient.
- **linear** depth (size growing with distance from the touch) makes
  cumulative depth grow quadratically, which yields the square-root
  law (b ≈ 0.5) that empirical studies consistently report for real
  markets.

This motivates the `fill_model: full_depth` backtest mode: a flat or
linear cost model mis-prices exactly the large orders where impact
matters most, while the book walk prices them by construction.

![Impact curves](impact_curve.png)
