#!/usr/bin/env python3
"""
Analyze data availability for PairsTrading strategy with aggressive parameters
"""

import pandas as pd
import numpy as np
from datetime import datetime
import os

def analyze_pairs_aggressive():
    print("🔍 PairsTrading Strategy - Aggressive Parameters Analysis")
    print("=" * 60)
    
    # More aggressive configuration
    symbol1 = "AAPL"
    symbol2 = "GOOG"
    spread_window = 10  # Reduced from 20
    entry_threshold = 1.0  # Reduced from 2.0
    exit_threshold = 0.2  # Reduced from 0.5
    
    print(f"🔧 Aggressive Strategy Configuration:")
    print(f"  Symbol 1: {symbol1}")
    print(f"  Symbol 2: {symbol2}")
    print(f"  Spread Window: {spread_window} bars (was 20)")
    print(f"  Entry Threshold: {entry_threshold} (z-score, was 2.0)")
    print(f"  Exit Threshold: {exit_threshold} (z-score, was 0.5)")
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
    
    # Simulate spread calculation with aggressive parameters
    print("📊 Spread Analysis with Aggressive Parameters:")
    
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
    
    # Calculate rolling statistics with aggressive window
    merged['spread_mean'] = merged['spread'].rolling(window=spread_window).mean()
    merged['spread_std'] = merged['spread'].rolling(window=spread_window).std()
    merged['z_score'] = (merged['spread'] - merged['spread_mean']) / merged['spread_std']
    
    # Count potential signals with aggressive thresholds
    valid_data = merged.dropna()
    high_z_score = valid_data[valid_data['z_score'] > entry_threshold]
    low_z_score = valid_data[valid_data['z_score'] < -entry_threshold]
    exit_signals = valid_data[abs(valid_data['z_score']) < exit_threshold]
    
    print(f"  Valid data points: {len(valid_data)}")
    print(f"  High z-score signals (> {entry_threshold}): {len(high_z_score)}")
    print(f"  Low z-score signals (< -{entry_threshold}): {len(low_z_score)}")
    print(f"  Exit signals (< {exit_threshold}): {len(exit_signals)}")
    
    total_signals = len(high_z_score) + len(low_z_score)
    
    if total_signals > 0:
        print(f"🎯 AGGRESSIVE TRADING SIGNALS DETECTED!")
        print(f"   Entry signals: {total_signals}")
        print(f"   Exit signals: {len(exit_signals)}")
        print(f"   Signal frequency: {total_signals/len(valid_data)*100:.1f}% of bars")
    else:
        print(f"⚠️  STILL NO TRADING SIGNALS DETECTED")
        print(f"   Even with aggressive parameters")
    
    print()
    
    # Compare with original conservative parameters
    print("📊 Comparison: Conservative vs Aggressive Parameters:")
    print("  Conservative (original):")
    print(f"    - Entry threshold: 2.0 → {len(valid_data[valid_data['z_score'] > 2.0])} signals")
    print(f"    - Exit threshold: 0.5 → {len(valid_data[abs(valid_data['z_score']) < 0.5])} signals")
    print("  Aggressive (new):")
    print(f"    - Entry threshold: {entry_threshold} → {total_signals} signals")
    print(f"    - Exit threshold: {exit_threshold} → {len(exit_signals)} signals")
    
    print()
    
    # Show sample data with signals highlighted
    print("📋 Sample Spread Data (Last 20 bars):")
    sample = merged.tail(20)[['timestamp', f'{symbol1}_close', f'{symbol2}_close', 'spread', 'z_score']]
    
    for _, row in sample.iterrows():
        timestamp = row['timestamp']
        z_score = row['z_score']
        
        if abs(z_score) > entry_threshold:
            signal_type = "🟢 LONG" if z_score < -entry_threshold else "🔴 SHORT"
            print(f"  {timestamp} | {row[f'{symbol1}_close']:.2f} | {row[f'{symbol2}_close']:.2f} | {row['spread']:.2f} | {z_score:.3f} {signal_type}")
        elif abs(z_score) < exit_threshold:
            print(f"  {timestamp} | {row[f'{symbol1}_close']:.2f} | {row[f'{symbol2}_close']:.2f} | {row['spread']:.2f} | {z_score:.3f} 🟡 EXIT")
        else:
            print(f"  {timestamp} | {row[f'{symbol1}_close']:.2f} | {row[f'{symbol2}_close']:.2f} | {row['spread']:.2f} | {z_score:.3f}")
    
    print()
    print("🎯 Conclusion:")
    if total_signals > 0:
        print(f"✅ With aggressive parameters, you should see {total_signals} PairsTrading signals!")
        print(f"   This is {total_signals/len(valid_data)*100:.1f}% of all bars - very active trading!")
        print("   The strategy should generate trades with these parameters.")
    else:
        print("⚠️  Even with aggressive parameters, no signals detected.")
        print("   This suggests the symbols are too highly correlated.")
        print("   Consider using different symbol pairs with more divergence.")

if __name__ == "__main__":
    analyze_pairs_aggressive() 