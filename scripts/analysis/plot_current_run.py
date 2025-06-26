#!/usr/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt
from pathlib import Path

def plot_current_equity_curve():
    """Plot the equity curve from the current strategy engine run."""
    
    # Read the equity curve data
    try:
        df = pd.read_csv('equity_curve.csv')
        print(f"Loaded equity curve with {len(df)} data points")
        print(f"Columns: {list(df.columns)}")
        print(f"Data range: {df['equity'].min():.2f} to {df['equity'].max():.2f}")
        
        # Convert timestamp to datetime (assuming seconds since epoch)
        df['datetime'] = pd.to_datetime(df['timestamp'], unit='s')
        
        # Create the plot
        plt.figure(figsize=(12, 6))
        plt.plot(df['datetime'], df['equity'], linewidth=2, color='blue')
        plt.title('Current Strategy Run - AAPL Equity Curve', fontsize=16)
        plt.xlabel('Time', fontsize=12)
        plt.ylabel('Portfolio Value ($)', fontsize=12)
        plt.grid(True, alpha=0.3)
        
        # Format y-axis to show dollar amounts
        plt.gca().yaxis.set_major_formatter(plt.FuncFormatter(lambda x, p: f'${x:,.0f}'))
        
        # Add some statistics as text
        initial_value = df['equity'].iloc[0]
        final_value = df['equity'].iloc[-1]
        total_return = (final_value / initial_value - 1) * 100
        
        stats_text = f'Initial: ${initial_value:,.0f}\nFinal: ${final_value:,.0f}\nReturn: {total_return:.2f}%'
        plt.text(0.02, 0.98, stats_text, transform=plt.gca().transAxes, 
                verticalalignment='top', bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))
        
        # Save the plot
        output_path = 'plots/current_run_equity_curve.png'
        plt.tight_layout()
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"Equity curve plot saved to: {output_path}")
        print(f"Strategy performance:")
        print(f"  Initial capital: ${initial_value:,.2f}")
        print(f"  Final value: ${final_value:,.2f}")
        print(f"  Total return: {total_return:.2f}%")
        print(f"  Data points: {len(df)}")
        
        return True
        
    except Exception as e:
        print(f"Error plotting equity curve: {e}")
        return False

def check_trade_log():
    """Check if any trades were executed."""
    try:
        df = pd.read_csv('tradelog.csv')
        if len(df) > 0:
            print(f"Trade log contains {len(df)} trades")
            print(df.head())
        else:
            print("No trades were executed during this run")
            print("This is normal for conservative strategies that don't find suitable entry points")
    except Exception as e:
        print(f"Error reading trade log: {e}")

if __name__ == "__main__":
    print("=== Current Strategy Run Analysis ===")
    plot_current_equity_curve()
    print("\n=== Trade Analysis ===")
    check_trade_log() 