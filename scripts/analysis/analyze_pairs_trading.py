#!/usr/bin/env python3

import os
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
import argparse
import json
from datetime import datetime
import numpy as np
from matplotlib.dates import DateFormatter
import matplotlib.dates as mdates

def load_pairs_trading_data(organized_dir):
    """
    Load PairsTrading specific data from organized directory.
    """
    organized_path = Path(organized_dir)
    
    # Find PairsTrading files
    equity_files = list(organized_path.glob("data/equity_curves/equity_PairsTrading_*.csv"))
    trade_files = list(organized_path.glob("data/trade_logs/tradelog_PairsTrading_*.csv"))
    
    equity_data = {}
    trade_data = {}
    
    # Load equity curves
    for file_path in equity_files:
        try:
            df = pd.read_csv(file_path)
            if not df.empty:
                # Extract symbol pair from filename
                filename = file_path.stem
                parts = filename.split('_')
                if len(parts) >= 4:
                    symbol_pair = f"{parts[2]}_{parts[3]}"  # e.g., AAPL_GOOG
                    equity_data[symbol_pair] = df
                    print(f"📊 Loaded equity: {symbol_pair} ({len(df)} rows)")
        except Exception as e:
            print(f"⚠️  Error loading {file_path}: {e}")
    
    # Load trade logs
    for file_path in trade_files:
        try:
            df = pd.read_csv(file_path)
            if not df.empty:
                # Extract symbol pair from filename
                filename = file_path.stem
                parts = filename.split('_')
                if len(parts) >= 4:
                    symbol_pair = f"{parts[2]}_{parts[3]}"  # e.g., AAPL_GOOG
                    trade_data[symbol_pair] = df
                    print(f"📈 Loaded trades: {symbol_pair} ({len(df)} trades)")
        except Exception as e:
            print(f"⚠️  Error loading {file_path}: {e}")
    
    return equity_data, trade_data

def analyze_pairs_trading_trades(trade_data):
    """
    Analyze PairsTrading specific trading patterns.
    """
    analysis = {}
    
    for symbol_pair, df in trade_data.items():
        if df.empty:
            continue
        
        # Basic statistics
        total_trades = len(df)
        
        # Defensive: check for required columns
        has_symbol = 'symbol' in df.columns
        has_side = 'side' in df.columns
        has_quantity = 'quantity' in df.columns
        has_pnl = 'pnl' in df.columns
        
        if not has_symbol:
            print(f"⚠️  Skipping symbol analysis for {symbol_pair}: missing 'symbol' column")
            symbol1 = symbol2 = 'Unknown'
            symbol1_trades = symbol2_trades = pd.DataFrame()
        else:
            symbols = symbol_pair.split('_')
            symbol1, symbol2 = symbols[0], symbols[1]
            symbol1_trades = df[df['symbol'] == symbol1]
            symbol2_trades = df[df['symbol'] == symbol2]
        
        # Count buy/sell for each symbol
        if has_side and not symbol1_trades.empty:
            symbol1_buys = len(symbol1_trades[symbol1_trades['side'] == 'BUY'])
            symbol1_sells = len(symbol1_trades[symbol1_trades['side'] == 'SELL'])
        else:
            symbol1_buys = symbol1_sells = 0
            if not has_side:
                print(f"⚠️  Skipping buy/sell count for {symbol_pair}: missing 'side' column")
        if has_side and not symbol2_trades.empty:
            symbol2_buys = len(symbol2_trades[symbol2_trades['side'] == 'BUY'])
            symbol2_sells = len(symbol2_trades[symbol2_trades['side'] == 'SELL'])
        else:
            symbol2_buys = symbol2_sells = 0
        
        # Calculate P&L by symbol
        symbol1_pnl = symbol1_trades['pnl'].sum() if has_pnl and not symbol1_trades.empty else 0
        symbol2_pnl = symbol2_trades['pnl'].sum() if has_pnl and not symbol2_trades.empty else 0
        total_pnl = symbol1_pnl + symbol2_pnl
        
        # Calculate volume by symbol
        symbol1_volume = symbol1_trades['quantity'].sum() if has_quantity and not symbol1_trades.empty else 0
        symbol2_volume = symbol2_trades['quantity'].sum() if has_quantity and not symbol2_trades.empty else 0
        
        # Analyze trade timing
        if 'timestamp' in df.columns:
            df['timestamp'] = pd.to_datetime(df['timestamp'], unit='s')
            df = df.sort_values('timestamp')
            
            # Calculate time between trades
            df['time_diff'] = df['timestamp'].diff()
            avg_time_between_trades = df['time_diff'].mean()
            
            # Find trading sessions (gaps > 1 hour)
            df['session_gap'] = df['time_diff'] > pd.Timedelta(hours=1)
            trading_sessions = df['session_gap'].sum() + 1
        else:
            avg_time_between_trades = None
            trading_sessions = 1
        
        analysis[symbol_pair] = {
            'total_trades': total_trades,
            'symbol1': {
                'symbol': symbol1,
                'buys': symbol1_buys,
                'sells': symbol1_sells,
                'pnl': symbol1_pnl,
                'volume': symbol1_volume
            },
            'symbol2': {
                'symbol': symbol2,
                'buys': symbol2_buys,
                'sells': symbol2_sells,
                'pnl': symbol2_pnl,
                'volume': symbol2_volume
            },
            'total_pnl': total_pnl,
            'avg_time_between_trades': avg_time_between_trades,
            'trading_sessions': trading_sessions,
            'trade_data': df
        }
    
    return analysis

def create_simple_pairs_analysis(trade_df, symbol_pair, output_dir):
    """
    Create simple, clear PairsTrading analysis plots based on the user's original function.
    """
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)
    
    symbols = symbol_pair.split('_')
    symbol1, symbol2 = symbols[0], symbols[1]
    
    # Check for required columns
    has_symbol = 'symbol' in trade_df.columns
    has_side = 'side' in trade_df.columns
    has_type = 'type' in trade_df.columns
    
    if not has_symbol:
        print(f"⚠️  Skipping simple analysis for {symbol_pair}: missing 'symbol' column")
        return
    
    # Separate trades by symbol
    symbol1_trades = trade_df[trade_df['symbol'] == symbol1]
    symbol2_trades = trade_df[trade_df['symbol'] == symbol2]
    
    print(f"\n=== PairsTrading Strategy Analysis: {symbol_pair} ===")
    print(f"Total trades: {len(trade_df)}")
    print(f"{symbol1} trades: {len(symbol1_trades)}")
    print(f"{symbol2} trades: {len(symbol2_trades)}")
    
    # Calculate basic statistics
    if len(symbol1_trades) > 0:
        print(f"Average {symbol1} price: ${symbol1_trades['price'].mean():.2f}")
    if len(symbol2_trades) > 0:
        print(f"Average {symbol2} price: ${symbol2_trades['price'].mean():.2f}")
    
    # Count buy vs sell orders
    if has_side:
        symbol1_buys = len(symbol1_trades[symbol1_trades['side'] == 'BUY'])
        symbol1_sells = len(symbol1_trades[symbol1_trades['side'] == 'SELL'])
        symbol2_buys = len(symbol2_trades[symbol2_trades['side'] == 'BUY'])
        symbol2_sells = len(symbol2_trades[symbol2_trades['side'] == 'SELL'])
    elif has_type:
        symbol1_buys = len(symbol1_trades[symbol1_trades['type'] == 'BUY'])
        symbol1_sells = len(symbol1_trades[symbol1_trades['type'] == 'SELL'])
        symbol2_buys = len(symbol2_trades[symbol2_trades['type'] == 'BUY'])
        symbol2_sells = len(symbol2_trades[symbol2_trades['type'] == 'SELL'])
    else:
        symbol1_buys = symbol1_sells = symbol2_buys = symbol2_sells = 0
        print(f"⚠️  Cannot determine buy/sell orders: missing 'side' or 'type' column")
    
    print(f"\n=== Order Distribution ===")
    print(f"{symbol1} - Buys: {symbol1_buys}, Sells: {symbol1_sells}")
    print(f"{symbol2} - Buys: {symbol2_buys}, Sells: {symbol2_sells}")
    
    # Show first few trades
    print(f"\n=== First 10 Trades ===")
    print(trade_df.head(10).to_string(index=False))
    
    # Create separate, detailed plots
    
    # 1. Trade Prices Over Time - Symbol 1
    plt.figure(figsize=(15, 8))
    
    if has_side:
        symbol1_buy_trades = symbol1_trades[symbol1_trades['side'] == 'BUY']
        symbol1_sell_trades = symbol1_trades[symbol1_trades['side'] == 'SELL']
    elif has_type:
        symbol1_buy_trades = symbol1_trades[symbol1_trades['type'] == 'BUY']
        symbol1_sell_trades = symbol1_trades[symbol1_trades['type'] == 'SELL']
    else:
        symbol1_buy_trades = symbol1_sell_trades = pd.DataFrame()
    
    plt.scatter(symbol1_buy_trades.index, symbol1_buy_trades['price'], 
               color='green', marker='^', s=50, label=f'{symbol1} Buy', alpha=0.7)
    plt.scatter(symbol1_sell_trades.index, symbol1_sell_trades['price'], 
               color='red', marker='v', s=50, label=f'{symbol1} Sell', alpha=0.7)
    plt.title(f'{symbol1} Trade Prices Over Time - {symbol_pair}')
    plt.xlabel('Trade Index')
    plt.ylabel('Price ($)')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(output_path / f'{symbol1}_trade_prices_{symbol_pair}.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    # 2. Trade Prices Over Time - Symbol 2
    plt.figure(figsize=(15, 8))
    
    if has_side:
        symbol2_buy_trades = symbol2_trades[symbol2_trades['side'] == 'BUY']
        symbol2_sell_trades = symbol2_trades[symbol2_trades['side'] == 'SELL']
    elif has_type:
        symbol2_buy_trades = symbol2_trades[symbol2_trades['type'] == 'BUY']
        symbol2_sell_trades = symbol2_trades[symbol2_trades['type'] == 'SELL']
    else:
        symbol2_buy_trades = symbol2_sell_trades = pd.DataFrame()
    
    plt.scatter(symbol2_buy_trades.index, symbol2_buy_trades['price'], 
               color='green', marker='^', s=50, label=f'{symbol2} Buy', alpha=0.7)
    plt.scatter(symbol2_sell_trades.index, symbol2_sell_trades['price'], 
               color='red', marker='v', s=50, label=f'{symbol2} Sell', alpha=0.7)
    plt.title(f'{symbol2} Trade Prices Over Time - {symbol_pair}')
    plt.xlabel('Trade Index')
    plt.ylabel('Price ($)')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(output_path / f'{symbol2}_trade_prices_{symbol_pair}.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    # 3. Combined Trade Activity
    plt.figure(figsize=(15, 10))
    
    plt.subplot(2, 1, 1)
    plt.scatter(symbol1_buy_trades.index, symbol1_buy_trades['price'], 
               color='green', marker='^', s=50, label=f'{symbol1} Buy', alpha=0.7)
    plt.scatter(symbol1_sell_trades.index, symbol1_sell_trades['price'], 
               color='red', marker='v', s=50, label=f'{symbol1} Sell', alpha=0.7)
    plt.title(f'{symbol1} Trade Activity - {symbol_pair}')
    plt.ylabel('Price ($)')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    plt.subplot(2, 1, 2)
    plt.scatter(symbol2_buy_trades.index, symbol2_buy_trades['price'], 
               color='blue', marker='^', s=50, label=f'{symbol2} Buy', alpha=0.7)
    plt.scatter(symbol2_sell_trades.index, symbol2_sell_trades['price'], 
               color='orange', marker='v', s=50, label=f'{symbol2} Sell', alpha=0.7)
    plt.title(f'{symbol2} Trade Activity - {symbol_pair}')
    plt.xlabel('Trade Index')
    plt.ylabel('Price ($)')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_path / f'combined_trade_activity_{symbol_pair}.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    # 4. Trade Volume Analysis
    if 'quantity' in trade_df.columns:
        plt.figure(figsize=(15, 12))
        
        # Plot 1: Individual trade volumes (line plot instead of bars)
        plt.subplot(3, 1, 1)
        plt.plot(symbol1_trades.index, symbol1_trades['quantity'], 
                color='green', alpha=0.7, linewidth=1, label=f'{symbol1} Volume')
        plt.title(f'{symbol1} Individual Trade Volumes - {symbol_pair}')
        plt.ylabel('Quantity')
        plt.legend()
        plt.grid(True, alpha=0.3)
        
        plt.subplot(3, 1, 2)
        plt.plot(symbol2_trades.index, symbol2_trades['quantity'], 
                color='blue', alpha=0.7, linewidth=1, label=f'{symbol2} Volume')
        plt.title(f'{symbol2} Individual Trade Volumes - {symbol_pair}')
        plt.ylabel('Quantity')
        plt.legend()
        plt.grid(True, alpha=0.3)
        
        # Plot 3: Cumulative volume over time
        plt.subplot(3, 1, 3)
        if 'timestamp' in trade_df.columns:
            # Sort by timestamp for proper cumulative calculation
            symbol1_sorted = symbol1_trades.sort_values('timestamp')
            symbol2_sorted = symbol2_trades.sort_values('timestamp')
            
            symbol1_cumulative = symbol1_sorted['quantity'].cumsum()
            symbol2_cumulative = symbol2_sorted['quantity'].cumsum()
            
            plt.plot(symbol1_sorted['timestamp'], symbol1_cumulative, 
                    color='green', linewidth=2, label=f'{symbol1} Cumulative Volume')
            plt.plot(symbol2_sorted['timestamp'], symbol2_cumulative, 
                    color='blue', linewidth=2, label=f'{symbol2} Cumulative Volume')
            plt.title(f'Cumulative Trading Volume Over Time - {symbol_pair}')
            plt.xlabel('Time')
            plt.ylabel('Cumulative Volume')
            plt.legend()
            plt.grid(True, alpha=0.3)
            plt.xticks(rotation=45)
        else:
            # Fallback to index-based cumulative if no timestamp
            symbol1_cumulative = symbol1_trades['quantity'].cumsum()
            symbol2_cumulative = symbol2_trades['quantity'].cumsum()
            
            plt.plot(symbol1_trades.index, symbol1_cumulative, 
                    color='green', linewidth=2, label=f'{symbol1} Cumulative Volume')
            plt.plot(symbol2_trades.index, symbol2_cumulative, 
                    color='blue', linewidth=2, label=f'{symbol2} Cumulative Volume')
            plt.title(f'Cumulative Trading Volume - {symbol_pair}')
            plt.xlabel('Trade Index')
            plt.ylabel('Cumulative Volume')
            plt.legend()
            plt.grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig(output_path / f'trade_volumes_{symbol_pair}.png', dpi=300, bbox_inches='tight')
        plt.close()
        
        # Create a separate volume statistics plot
        plt.figure(figsize=(15, 8))
        
        # Volume statistics
        plt.subplot(2, 2, 1)
        volume_stats = {
            f'{symbol1}': [
                symbol1_trades['quantity'].mean(),
                symbol1_trades['quantity'].median(),
                symbol1_trades['quantity'].std(),
                symbol1_trades['quantity'].max()
            ],
            f'{symbol2}': [
                symbol2_trades['quantity'].mean(),
                symbol2_trades['quantity'].median(),
                symbol2_trades['quantity'].std(),
                symbol2_trades['quantity'].max()
            ]
        }
        
        x = np.arange(4)
        width = 0.35
        plt.bar(x - width/2, volume_stats[symbol1], width, label=symbol1, alpha=0.7, color='green')
        plt.bar(x + width/2, volume_stats[symbol2], width, label=symbol2, alpha=0.7, color='blue')
        plt.title(f'Volume Statistics - {symbol_pair}')
        plt.xlabel('Statistic')
        plt.ylabel('Quantity')
        plt.xticks(x, ['Mean', 'Median', 'Std Dev', 'Max'])
        plt.legend()
        plt.grid(True, alpha=0.3)
        
        # Volume distribution
        plt.subplot(2, 2, 2)
        plt.hist(symbol1_trades['quantity'], bins=30, alpha=0.7, color='green', label=symbol1)
        plt.hist(symbol2_trades['quantity'], bins=30, alpha=0.7, color='blue', label=symbol2)
        plt.title(f'Volume Distribution - {symbol_pair}')
        plt.xlabel('Quantity')
        plt.ylabel('Frequency')
        plt.legend()
        plt.grid(True, alpha=0.3)
        
        # Total volume comparison
        plt.subplot(2, 2, 3)
        total_volumes = [symbol1_trades['quantity'].sum(), symbol2_trades['quantity'].sum()]
        plt.bar([symbol1, symbol2], total_volumes, color=['green', 'blue'], alpha=0.7)
        plt.title(f'Total Volume Comparison - {symbol_pair}')
        plt.ylabel('Total Volume')
        plt.grid(True, alpha=0.3)
        
        # Average volume per trade
        plt.subplot(2, 2, 4)
        avg_volumes = [symbol1_trades['quantity'].mean(), symbol2_trades['quantity'].mean()]
        plt.bar([symbol1, symbol2], avg_volumes, color=['green', 'blue'], alpha=0.7)
        plt.title(f'Average Volume per Trade - {symbol_pair}')
        plt.ylabel('Average Volume')
        plt.grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig(output_path / f'volume_statistics_{symbol_pair}.png', dpi=300, bbox_inches='tight')
        plt.close()
    
    # 5. Price Distribution
    plt.figure(figsize=(15, 8))
    
    plt.subplot(2, 1, 1)
    plt.hist(symbol1_trades['price'], bins=30, alpha=0.7, color='green', label=symbol1)
    plt.title(f'{symbol1} Price Distribution - {symbol_pair}')
    plt.xlabel('Price ($)')
    plt.ylabel('Frequency')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    plt.subplot(2, 1, 2)
    plt.hist(symbol2_trades['price'], bins=30, alpha=0.7, color='blue', label=symbol2)
    plt.title(f'{symbol2} Price Distribution - {symbol_pair}')
    plt.xlabel('Price ($)')
    plt.ylabel('Frequency')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_path / f'price_distributions_{symbol_pair}.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"📊 Created detailed analysis plots for {symbol_pair}:")
    print(f"  - {symbol1}_trade_prices_{symbol_pair}.png")
    print(f"  - {symbol2}_trade_prices_{symbol_pair}.png")
    print(f"  - combined_trade_activity_{symbol_pair}.png")
    if 'quantity' in trade_df.columns:
        print(f"  - trade_volumes_{symbol_pair}.png")
        print(f"  - volume_statistics_{symbol_pair}.png")
    print(f"  - price_distributions_{symbol_pair}.png")

def create_pairs_trading_plots(equity_data, trade_data, analysis, output_dir):
    """
    Create comprehensive PairsTrading visualization plots.
    """
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)
    
    for symbol_pair in trade_data.keys():
        if symbol_pair not in analysis:
            continue
            
        trade_df = analysis[symbol_pair]['trade_data']
        
        # Create simple, detailed analysis plots
        create_simple_pairs_analysis(trade_df, symbol_pair, output_path)
        
        # Also create the comprehensive plots (but simplified)
        symbols = symbol_pair.split('_')
        symbol1, symbol2 = symbols[0], symbols[1]
        
        # Defensive: check for required columns
        has_symbol = 'symbol' in trade_df.columns
        has_side = 'side' in trade_df.columns
        has_quantity = 'quantity' in trade_df.columns
        has_pnl = 'pnl' in trade_df.columns
        
        # Create a simplified comprehensive figure
        fig = plt.figure(figsize=(20, 16))
        
        # 1. Trading Activity Timeline
        plt.subplot(4, 2, 1)
        if has_symbol:
            symbol1_trades = trade_df[trade_df['symbol'] == symbol1]
            symbol2_trades = trade_df[trade_df['symbol'] == symbol2]
        else:
            symbol1_trades = symbol2_trades = pd.DataFrame()
            print(f"⚠️  Skipping symbol-based plots for {symbol_pair}: missing 'symbol' column")
        
        # Plot buy/sell points for symbol1
        if has_side and not symbol1_trades.empty:
            symbol1_buys = symbol1_trades[symbol1_trades['side'] == 'BUY']
            symbol1_sells = symbol1_trades[symbol1_trades['side'] == 'SELL']
            if not symbol1_buys.empty:
                plt.scatter(symbol1_buys['timestamp'], symbol1_buys['price'], 
                           color='green', marker='^', s=50, label=f'{symbol1} BUY', alpha=0.7)
            if not symbol1_sells.empty:
                plt.scatter(symbol1_sells['timestamp'], symbol1_sells['price'], 
                           color='red', marker='v', s=50, label=f'{symbol1} SELL', alpha=0.7)
        else:
            print(f"⚠️  Skipping buy/sell markers for {symbol_pair}: missing 'side' column or no trades")
        
        # Plot buy/sell points for symbol2
        if has_side and not symbol2_trades.empty:
            symbol2_buys = symbol2_trades[symbol2_trades['side'] == 'BUY']
            symbol2_sells = symbol2_trades[symbol2_trades['side'] == 'SELL']
            if not symbol2_buys.empty:
                plt.scatter(symbol2_buys['timestamp'], symbol2_buys['price'], 
                           color='blue', marker='^', s=50, label=f'{symbol2} BUY', alpha=0.7)
            if not symbol2_sells.empty:
                plt.scatter(symbol2_sells['timestamp'], symbol2_sells['price'], 
                           color='orange', marker='v', s=50, label=f'{symbol2} SELL', alpha=0.7)
        
        plt.title(f'PairsTrading Activity: {symbol_pair}')
        plt.xlabel('Time')
        plt.ylabel('Price')
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.xticks(rotation=45)
        
        # 2. Position Tracking
        plt.subplot(4, 2, 2)
        if has_quantity and has_symbol:
            symbol1_trades['position'] = symbol1_trades['quantity'].cumsum() if not symbol1_trades.empty else 0
            symbol2_trades['position'] = symbol2_trades['quantity'].cumsum() if not symbol2_trades.empty else 0
            plt.plot(symbol1_trades['timestamp'], symbol1_trades['position'], 
                    label=f'{symbol1} Position', linewidth=2)
            plt.plot(symbol2_trades['timestamp'], symbol2_trades['position'], 
                    label=f'{symbol2} Position', linewidth=2)
        else:
            print(f"⚠️  Skipping position tracking for {symbol_pair}: missing 'quantity' or 'symbol' column")
        plt.title(f'Position Tracking: {symbol_pair}')
        plt.xlabel('Time')
        plt.ylabel('Position Size')
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.xticks(rotation=45)
        
        # 3. Trade Volume Analysis
        plt.subplot(4, 2, 3)
        if has_quantity and has_symbol and 'timestamp' in trade_df.columns:
            trade_df['date'] = trade_df['timestamp'].dt.date
            daily_volume = trade_df.groupby(['date', 'symbol'])['quantity'].sum().unstack(fill_value=0)
            if not daily_volume.empty:
                daily_volume.plot(kind='bar', ax=plt.gca())
                plt.title(f'Daily Trading Volume: {symbol_pair}')
                plt.xlabel('Date')
                plt.ylabel('Volume')
                plt.legend()
                plt.xticks(rotation=45)
                plt.grid(True, alpha=0.3)
        else:
            print(f"⚠️  Skipping trade volume plot for {symbol_pair}: missing 'quantity', 'symbol', or 'timestamp' column")
        
        # 4. P&L Analysis
        plt.subplot(4, 2, 4)
        if has_pnl and 'timestamp' in trade_df.columns:
            trade_df['cumulative_pnl'] = trade_df['pnl'].cumsum()
            plt.plot(trade_df['timestamp'], trade_df['cumulative_pnl'], 
                    label='Cumulative P&L', linewidth=2, color='purple')
            if has_symbol:
                symbol1_pnl = symbol1_trades['pnl'].cumsum() if not symbol1_trades.empty else 0
                symbol2_pnl = symbol2_trades['pnl'].cumsum() if not symbol2_trades.empty else 0
                plt.plot(symbol1_trades['timestamp'], symbol1_pnl, 
                        label=f'{symbol1} P&L', alpha=0.7)
                plt.plot(symbol2_trades['timestamp'], symbol2_pnl, 
                        label=f'{symbol2} P&L', alpha=0.7)
            plt.title(f'P&L Analysis: {symbol_pair}')
            plt.xlabel('Time')
            plt.ylabel('P&L')
            plt.legend()
            plt.grid(True, alpha=0.3)
            plt.xticks(rotation=45)
        else:
            print(f"⚠️  Skipping P&L plot for {symbol_pair}: missing 'pnl' or 'timestamp' column")
        
        # 5. Trade Distribution
        plt.subplot(4, 2, 5)
        if 'timestamp' in trade_df.columns:
            trade_df['hour'] = trade_df['timestamp'].dt.hour
            hourly_trades = trade_df.groupby('hour').size()
            plt.bar(hourly_trades.index, hourly_trades.values, alpha=0.7)
            plt.title(f'Trade Distribution by Hour: {symbol_pair}')
            plt.xlabel('Hour of Day')
            plt.ylabel('Number of Trades')
            plt.grid(True, alpha=0.3)
        else:
            print(f"⚠️  Skipping trade distribution plot for {symbol_pair}: missing 'timestamp' column")
        
        # 6. Price Spread Analysis (if available)
        plt.subplot(4, 2, 6)
        if 'spread' in trade_df.columns:
            plt.hist(trade_df['spread'], bins=30, alpha=0.7, color='green')
            plt.title(f'Spread Distribution: {symbol_pair}')
            plt.xlabel('Spread')
            plt.ylabel('Frequency')
            plt.grid(True, alpha=0.3)
        elif has_quantity:
            plt.hist(trade_df['quantity'], bins=30, alpha=0.7, color='blue')
            plt.title(f'Trade Size Distribution: {symbol_pair}')
            plt.xlabel('Trade Size')
            plt.ylabel('Frequency')
            plt.grid(True, alpha=0.3)
        else:
            print(f"⚠️  Skipping spread/trade size plot for {symbol_pair}: missing 'spread' and 'quantity' columns")
        
        # 7. Equity Curve (if available)
        plt.subplot(4, 2, 7)
        if symbol_pair in equity_data:
            equity_df = equity_data[symbol_pair]
            if 'timestamp' in equity_df.columns and 'equity' in equity_df.columns:
                equity_df['timestamp'] = pd.to_datetime(equity_df['timestamp'], unit='s')
                plt.plot(equity_df['timestamp'], equity_df['equity'], 
                        label='Portfolio Value', linewidth=2, color='darkgreen')
                plt.title(f'Equity Curve: {symbol_pair}')
                plt.xlabel('Time')
                plt.ylabel('Portfolio Value')
                plt.grid(True, alpha=0.3)
                plt.xticks(rotation=45)
        
        # 8. Trade Frequency Analysis
        plt.subplot(4, 2, 8)
        if 'timestamp' in trade_df.columns:
            trade_df['trade_count'] = 1
            trade_df = trade_df.sort_values('timestamp')
            trade_df['rolling_trades'] = trade_df['trade_count'].rolling(window=20).sum()
            plt.plot(trade_df['timestamp'], trade_df['rolling_trades'], 
                    label='20-Trade Rolling Window', linewidth=2, color='red')
            plt.title(f'Trade Frequency: {symbol_pair}')
            plt.xlabel('Time')
            plt.ylabel('Trades in Rolling Window')
            plt.grid(True, alpha=0.3)
            plt.xticks(rotation=45)
        else:
            print(f"⚠️  Skipping trade frequency plot for {symbol_pair}: missing 'timestamp' column")
        
        plt.tight_layout()
        plt.savefig(output_path / f'pairs_trading_analysis_{symbol_pair}.png', 
                   dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"📊 Created comprehensive analysis plot: pairs_trading_analysis_{symbol_pair}.png")

def create_summary_plots(analysis, output_dir):
    """
    Create summary plots comparing all PairsTrading pairs.
    """
    output_path = Path(output_dir)
    
    # Summary statistics
    summary_data = []
    for symbol_pair, data in analysis.items():
        summary_data.append({
            'symbol_pair': symbol_pair,
            'total_trades': data['total_trades'],
            'total_pnl': data['total_pnl'],
            'symbol1_pnl': data['symbol1']['pnl'],
            'symbol2_pnl': data['symbol2']['pnl'],
            'symbol1_volume': data['symbol1']['volume'],
            'symbol2_volume': data['symbol2']['volume'],
            'trading_sessions': data['trading_sessions']
        })
    
    if not summary_data:
        return
    
    summary_df = pd.DataFrame(summary_data)
    
    # Create summary plots
    fig, axes = plt.subplots(2, 2, figsize=(15, 12))
    
    # 1. Total P&L by pair
    axes[0, 0].bar(summary_df['symbol_pair'], summary_df['total_pnl'], 
                   color=['green' if x > 0 else 'red' for x in summary_df['total_pnl']])
    axes[0, 0].set_title('Total P&L by Symbol Pair')
    axes[0, 0].set_ylabel('P&L')
    axes[0, 0].tick_params(axis='x', rotation=45)
    axes[0, 0].grid(True, alpha=0.3)
    
    # 2. Trade count by pair
    axes[0, 1].bar(summary_df['symbol_pair'], summary_df['total_trades'], color='blue', alpha=0.7)
    axes[0, 1].set_title('Total Trades by Symbol Pair')
    axes[0, 1].set_ylabel('Number of Trades')
    axes[0, 1].tick_params(axis='x', rotation=45)
    axes[0, 1].grid(True, alpha=0.3)
    
    # 3. P&L breakdown by symbol
    x = np.arange(len(summary_df))
    width = 0.35
    axes[1, 0].bar(x - width/2, summary_df['symbol1_pnl'], width, label='Symbol 1', alpha=0.7)
    axes[1, 0].bar(x + width/2, summary_df['symbol2_pnl'], width, label='Symbol 2', alpha=0.7)
    axes[1, 0].set_title('P&L Breakdown by Symbol')
    axes[1, 0].set_ylabel('P&L')
    axes[1, 0].set_xticks(x)
    axes[1, 0].set_xticklabels(summary_df['symbol_pair'], rotation=45)
    axes[1, 0].legend()
    axes[1, 0].grid(True, alpha=0.3)
    
    # 4. Volume comparison
    axes[1, 1].bar(x - width/2, summary_df['symbol1_volume'], width, label='Symbol 1', alpha=0.7)
    axes[1, 1].bar(x + width/2, summary_df['symbol2_volume'], width, label='Symbol 2', alpha=0.7)
    axes[1, 1].set_title('Trading Volume by Symbol')
    axes[1, 1].set_ylabel('Volume')
    axes[1, 1].set_xticks(x)
    axes[1, 1].set_xticklabels(summary_df['symbol_pair'], rotation=45)
    axes[1, 1].legend()
    axes[1, 1].grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_path / 'pairs_trading_summary.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"📊 Created summary plot: pairs_trading_summary.png")

def generate_pairs_trading_report(analysis, output_dir):
    """
    Generate a detailed PairsTrading report.
    """
    output_path = Path(output_dir)
    
    report = {
        'analysis_timestamp': datetime.now().isoformat(),
        'pairs_analysis': analysis,
        'summary': {
            'total_pairs': len(analysis),
            'total_trades': sum(data['total_trades'] for data in analysis.values()),
            'total_pnl': sum(data['total_pnl'] for data in analysis.values()),
            'best_performing_pair': None,
            'most_active_pair': None
        }
    }
    
    # Find best performers
    if analysis:
        pnl_by_pair = {k: v['total_pnl'] for k, v in analysis.items()}
        trades_by_pair = {k: v['total_trades'] for k, v in analysis.items()}
        
        report['summary']['best_performing_pair'] = max(pnl_by_pair, key=pnl_by_pair.get)
        report['summary']['most_active_pair'] = max(trades_by_pair, key=trades_by_pair.get)
    
    # Save JSON report
    with open(output_path / 'pairs_trading_report.json', 'w') as f:
        json.dump(report, f, indent=2, default=str)
    
    # Save human-readable report
    with open(output_path / 'pairs_trading_report.txt', 'w') as f:
        f.write("=== PairsTrading Analysis Report ===\n")
        f.write(f"Generated: {report['analysis_timestamp']}\n\n")
        
        f.write("SUMMARY:\n")
        f.write("=" * 50 + "\n")
        f.write(f"Total Pairs Analyzed: {report['summary']['total_pairs']}\n")
        f.write(f"Total Trades: {report['summary']['total_trades']}\n")
        f.write(f"Total P&L: {report['summary']['total_pnl']:.2f}\n")
        f.write(f"Best Performing Pair: {report['summary']['best_performing_pair']}\n")
        f.write(f"Most Active Pair: {report['summary']['most_active_pair']}\n\n")
        
        f.write("DETAILED ANALYSIS:\n")
        f.write("=" * 50 + "\n")
        
        for symbol_pair, data in analysis.items():
            f.write(f"\n{symbol_pair}:\n")
            f.write("-" * 30 + "\n")
            f.write(f"  Total Trades: {data['total_trades']}\n")
            f.write(f"  Total P&L: {data['total_pnl']:.2f}\n")
            f.write(f"  Trading Sessions: {data['trading_sessions']}\n")
            
            f.write(f"  {data['symbol1']['symbol']}:\n")
            f.write(f"    Buys: {data['symbol1']['buys']}, Sells: {data['symbol1']['sells']}\n")
            f.write(f"    P&L: {data['symbol1']['pnl']:.2f}\n")
            f.write(f"    Volume: {data['symbol1']['volume']:.0f}\n")
            
            f.write(f"  {data['symbol2']['symbol']}:\n")
            f.write(f"    Buys: {data['symbol2']['buys']}, Sells: {data['symbol2']['sells']}\n")
            f.write(f"    P&L: {data['symbol2']['pnl']:.2f}\n")
            f.write(f"    Volume: {data['symbol2']['volume']:.0f}\n")
            
            if data['avg_time_between_trades']:
                f.write(f"  Avg Time Between Trades: {data['avg_time_between_trades']}\n")
    
    print(f"📋 Generated PairsTrading report: {output_path / 'pairs_trading_report.txt'}")

def main():
    parser = argparse.ArgumentParser(description="Comprehensive PairsTrading analysis")
    parser.add_argument(
        '--organized-dir', 
        required=True,
        help='Path to organized results directory'
    )
    parser.add_argument(
        '--output-dir', 
        default='pairs_trading_analysis',
        help='Output directory for analysis results (default: pairs_trading_analysis)'
    )
    
    args = parser.parse_args()
    
    organized_path = Path(args.organized_dir)
    if not organized_path.exists():
        print(f"❌ Organized directory not found: {organized_path}")
        return
    
    print("🔍 PairsTrading Analysis")
    print("=" * 40)
    
    # Load PairsTrading data
    print("\n📊 Loading PairsTrading data...")
    equity_data, trade_data = load_pairs_trading_data(organized_path)
    
    if not trade_data:
        print("❌ No PairsTrading data found!")
        return
    
    # Analyze trading patterns
    print("\n📈 Analyzing PairsTrading patterns...")
    analysis = analyze_pairs_trading_trades(trade_data)
    
    # Create comprehensive plots
    print("\n📊 Creating detailed PairsTrading plots...")
    create_pairs_trading_plots(equity_data, trade_data, analysis, args.output_dir)
    
    # Create summary plots
    print("\n📊 Creating summary plots...")
    create_summary_plots(analysis, args.output_dir)
    
    # Generate report
    print("\n📋 Generating PairsTrading report...")
    generate_pairs_trading_report(analysis, args.output_dir)
    
    print(f"\n✅ PairsTrading analysis complete! Results saved to: {args.output_dir}")

if __name__ == "__main__":
    main() 