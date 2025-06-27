#!/usr/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

def analyze_pairs_trading():
    """Analyze the PairsTrading strategy results."""
    
    # Find the most recent PairsTrading tradelog file
    results_dir = Path("results")
    pairs_files = list(results_dir.glob("tradelog_PairsTrading_AAPL_GOOG_*.csv"))
    
    if not pairs_files:
        print("No PairsTrading tradelog files found!")
        return
    
    # Get the most recent file
    latest_file = max(pairs_files, key=lambda x: x.stat().st_mtime)
    print(f"Analyzing: {latest_file}")
    
    # Read the tradelog
    trades_df = pd.read_csv(latest_file)
    
    if len(trades_df) <= 1:  # Only header
        print("No trades found in the file!")
        return
    
    print(f"\n=== PairsTrading Strategy Analysis ===")
    print(f"Total trades: {len(trades_df)}")
    
    # Analyze trades by symbol
    aapl_trades = trades_df[trades_df['symbol'] == 'AAPL']
    goog_trades = trades_df[trades_df['symbol'] == 'GOOG']
    
    print(f"AAPL trades: {len(aapl_trades)}")
    print(f"GOOG trades: {len(goog_trades)}")
    
    # Calculate basic statistics
    print(f"\n=== Trade Statistics ===")
    print(f"Average AAPL price: ${aapl_trades['price'].mean():.2f}")
    print(f"Average GOOG price: ${goog_trades['price'].mean():.2f}")
    
    # Count buy vs sell orders
    aapl_buys = len(aapl_trades[aapl_trades['type'] == 'BUY'])
    aapl_sells = len(aapl_trades[aapl_trades['type'] == 'SELL'])
    goog_buys = len(goog_trades[goog_trades['type'] == 'BUY'])
    goog_sells = len(goog_trades[goog_trades['type'] == 'SELL'])
    
    print(f"\n=== Order Distribution ===")
    print(f"AAPL - Buys: {aapl_buys}, Sells: {aapl_sells}")
    print(f"GOOG - Buys: {goog_buys}, Sells: {goog_sells}")
    
    # Show first few trades
    print(f"\n=== First 10 Trades ===")
    print(trades_df.head(10).to_string(index=False))
    
    # Create a simple plot of trade prices over time
    plt.figure(figsize=(15, 8))
    
    # Plot AAPL trades
    aapl_buy_trades = aapl_trades[aapl_trades['type'] == 'BUY']
    aapl_sell_trades = aapl_trades[aapl_trades['type'] == 'SELL']
    
    plt.subplot(2, 1, 1)
    plt.scatter(aapl_buy_trades.index, aapl_buy_trades['price'], 
               color='green', marker='^', s=50, label='AAPL Buy', alpha=0.7)
    plt.scatter(aapl_sell_trades.index, aapl_sell_trades['price'], 
               color='red', marker='v', s=50, label='AAPL Sell', alpha=0.7)
    plt.title('AAPL Trade Prices Over Time')
    plt.ylabel('Price ($)')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # Plot GOOG trades
    goog_buy_trades = goog_trades[goog_trades['type'] == 'BUY']
    goog_sell_trades = goog_trades[goog_trades['type'] == 'SELL']
    
    plt.subplot(2, 1, 2)
    plt.scatter(goog_buy_trades.index, goog_buy_trades['price'], 
               color='green', marker='^', s=50, label='GOOG Buy', alpha=0.7)
    plt.scatter(goog_sell_trades.index, goog_sell_trades['price'], 
               color='red', marker='v', s=50, label='GOOG Sell', alpha=0.7)
    plt.title('GOOG Trade Prices Over Time')
    plt.xlabel('Trade Index')
    plt.ylabel('Price ($)')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig('pairs_trading_analysis.png', dpi=300, bbox_inches='tight')
    plt.show()
    
    print(f"\nPlot saved as: pairs_trading_analysis.png")

if __name__ == "__main__":
    analyze_pairs_trading() 