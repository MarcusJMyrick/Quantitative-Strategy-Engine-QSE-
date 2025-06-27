#!/usr/bin/env python3
"""
Analyze data availability for PairsTrading strategy
"""

import pandas as pd
import numpy as np
from datetime import datetime
import os

def analyze_pairs_data():
    print("🔍 PairsTrading Strategy Data Analysis")
    print("=" * 50)
    
    # Configuration from the engine
    symbol1 = "AAPL"
    symbol2 = "GOOG"
    spread_window = 20  # From the engine configuration
    entry_threshold = 2.0
    exit_threshold = 0.5
    
    print(f"Strategy Configuration:")
    print(f"  Symbol 1: {symbol1}")
    print(f"  Symbol 2: {symbol2}")
    print(f"  Spread Window: {spread_window} bars")
    print(f"  Entry Threshold: {entry_threshold} (z-score)")
    print(f"  Exit Threshold: {exit_threshold} (z-score)")
    print()
    
    # Load data
    data_files = {
        symbol1: f"data/raw_ticks_{symbol1}.csv",
        symbol2: f"data/raw_ticks_{symbol2}.csv"
    }
    
    data = {}
    for symbol, file_path in data_files.items():
        if not os.path.exists(file_path):
            print(f"❌ Data file not found: {file_path}")
            return
        
        print(f"📊 Loading {symbol} data...")
        df = pd.read_csv(file_path)
        print(f"  Rows: {len(df):,}")
        print(f"  Time range: {df['timestamp'].min()} to {df['timestamp'].max()}")
        
        # Convert timestamps to datetime for analysis
        df['datetime'] = pd.to_datetime(df['timestamp'], unit='s')
        data[symbol] = df
    
    print()
    
    # Analyze bar generation
    print("📈 Bar Generation Analysis:")
    
    # Group by minute (60-second bars as configured in BarBuilder)
    bars = {}
    for symbol, df in data.items():
        # Group by minute
        df['minute'] = df['datetime'].dt.floor('min')
        
        # Create bars manually
        minute_bars = df.groupby('minute').agg({
            'price': ['first', 'max', 'min', 'last'],
            'volume': 'sum'
        }).reset_index()
        
        # Flatten column names
        minute_bars.columns = ['timestamp', 'open', 'high', 'low', 'close', 'volume']
        bars[symbol] = minute_bars
        
        print(f"  {symbol}: {len(minute_bars)} bars generated")
    
    print()
    
    # Find common time range
    common_start = max(bars[symbol1]['timestamp'].min(), bars[symbol2]['timestamp'].min())
    common_end = min(bars[symbol1]['timestamp'].max(), bars[symbol2]['timestamp'].max())
    
    print(f"🕐 Common Time Range:")
    print(f"  Start: {common_start}")
    print(f"  End: {common_end}")
    print(f"  Duration: {common_end - common_start}")
    
    # Filter to common range
    common_bars = {}
    for symbol, bar_df in bars.items():
        mask = (bar_df['timestamp'] >= common_start) & (bar_df['timestamp'] <= common_end)
        common_bars[symbol] = bar_df[mask].copy()
        print(f"  {symbol} common bars: {len(common_bars[symbol])}")
    
    print()
    
    # Check if enough data for PairsTrading
    min_bars_needed = spread_window + 10  # Need spread_window + some extra for signals
    
    if len(common_bars[symbol1]) < min_bars_needed or len(common_bars[symbol2]) < min_bars_needed:
        print(f"❌ INSUFFICIENT DATA for PairsTrading Strategy")
        print(f"   Need at least {min_bars_needed} bars, but have:")
        print(f"   {symbol1}: {len(common_bars[symbol1])} bars")
        print(f"   {symbol2}: {len(common_bars[symbol2])} bars")
        return
    
    print(f"✅ SUFFICIENT DATA for PairsTrading Strategy")
    print(f"   Have {len(common_bars[symbol1])} bars (need {min_bars_needed})")
    print()
    
    # Simulate spread calculation
    print("📊 Spread Analysis:")
    
    # Align bars by timestamp
    merged = pd.merge(
        common_bars[symbol1][['timestamp', 'close']].rename(columns={'close': f'{symbol1}_close'}),
        common_bars[symbol2][['timestamp', 'close']].rename(columns={'close': f'{symbol2}_close'}),
        on='timestamp',
        how='inner'
    )
    
    print(f"  Aligned bars: {len(merged)}")
    
    if len(merged) < spread_window:
        print(f"❌ Not enough aligned bars for spread calculation")
        return
    
    # Calculate spread (using hedge_ratio = 1.0 as configured)
    hedge_ratio = 1.0
    merged['spread'] = merged[f'{symbol1}_close'] - (hedge_ratio * merged[f'{symbol2}_close'])
    
    # Calculate rolling statistics
    merged['spread_mean'] = merged['spread'].rolling(window=spread_window).mean()
    merged['spread_std'] = merged['spread'].rolling(window=spread_window).std()
    merged['z_score'] = (merged['spread'] - merged['spread_mean']) / merged['spread_std']
    
    # Count potential signals
    valid_data = merged.dropna()
    high_z_score = valid_data[valid_data['z_score'] > entry_threshold]
    low_z_score = valid_data[valid_data['z_score'] < -entry_threshold]
    exit_signals = valid_data[abs(valid_data['z_score']) < exit_threshold]
    
    print(f"  Valid data points: {len(valid_data)}")
    print(f"  High z-score signals (> {entry_threshold}): {len(high_z_score)}")
    print(f"  Low z-score signals (< -{entry_threshold}): {len(low_z_score)}")
    print(f"  Exit signals (< {exit_threshold}): {len(exit_signals)}")
    
    if len(high_z_score) > 0 or len(low_z_score) > 0:
        print(f"✅ POTENTIAL TRADING SIGNALS DETECTED!")
        print(f"   Entry signals: {len(high_z_score) + len(low_z_score)}")
        print(f"   Exit signals: {len(exit_signals)}")
    else:
        print(f"⚠️  NO TRADING SIGNALS DETECTED")
        print(f"   This could be due to:")
        print(f"   - Insufficient price movement")
        print(f"   - High correlation between {symbol1} and {symbol2}")
        print(f"   - Conservative entry thresholds")
    
    print()
    
    # Show sample data
    print("📋 Sample Spread Data:")
    sample = merged.tail(10)[['timestamp', f'{symbol1}_close', f'{symbol2}_close', 'spread', 'z_score']]
    print(sample.to_string(index=False))
    
    print()
    print("🎯 Conclusion:")
    if len(high_z_score) > 0 or len(low_z_score) > 0:
        print("✅ Your data SHOULD generate PairsTrading signals")
        print("   The strategy is likely not generating trades due to:")
        print("   - Implementation details in the strategy")
        print("   - Bar alignment issues")
        print("   - Order execution conditions")
    else:
        print("⚠️  Your data may not have enough price divergence")
        print("   Consider:")
        print("   - Using different symbol pairs")
        print("   - Lowering entry thresholds")
        print("   - Using longer time periods")

if __name__ == "__main__":
    analyze_pairs_data() 