#!/bin/sh
# Default container command: run the sample backtest and write everything to
# /results (mount a host directory there: docker run -v "$PWD/out:/results" qse)
set -e
cd /app
mkdir -p /results

echo "=== QSE sample backtest: SMA 20/50 on AAPL minute ticks ==="
./bin/strategy_engine
cp equity_curve.csv tradelog.csv /results/

echo "=== Generating tearsheet ==="
python3 scripts/analysis/tearsheet.py \
    --equity /results/equity_curve.csv \
    --tradelog /results/tradelog.csv \
    --benchmark data/raw_AAPL.csv \
    --out /results/tearsheet.pdf \
    --title "SMA 20/50 AAPL (Docker)"

echo "=== Done: equity_curve.csv, tradelog.csv, tearsheet.pdf in /results ==="
