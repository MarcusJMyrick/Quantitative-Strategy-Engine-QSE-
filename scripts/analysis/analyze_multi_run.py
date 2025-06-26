#!/usr/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys
import glob
from pathlib import Path
import seaborn as sns

def analyze_single_symbol(equity_file, tradelog_file, symbol):
    """Analyze results for a single symbol."""
    try:
        # Read equity curve
        equity_df = pd.read_csv(equity_file)
        equity_df['datetime'] = pd.to_datetime(equity_df['timestamp'], unit='s')
        
        # Read trade log
        try:
            tradelog_df = pd.read_csv(tradelog_file)
            num_trades = len(tradelog_df)
        except:
            num_trades = 0
            tradelog_df = pd.DataFrame()
        
        # Calculate performance metrics
        initial_value = equity_df['equity'].iloc[0]
        final_value = equity_df['equity'].iloc[-1]
        total_return = (final_value / initial_value - 1) * 100
        
        # Calculate returns for Sharpe ratio
        equity_df['returns'] = equity_df['equity'].pct_change().fillna(0)
        if equity_df['returns'].std() > 0:
            sharpe_ratio = equity_df['returns'].mean() / equity_df['returns'].std() * np.sqrt(252 * 24 * 60)  # Assuming minute data
        else:
            sharpe_ratio = 0
        
        # Calculate max drawdown
        equity_df['peak'] = equity_df['equity'].expanding().max()
        equity_df['drawdown'] = (equity_df['equity'] - equity_df['peak']) / equity_df['peak'] * 100
        max_drawdown = equity_df['drawdown'].min()
        
        results = {
            'symbol': symbol,
            'initial_value': initial_value,
            'final_value': final_value,
            'total_return': total_return,
            'sharpe_ratio': sharpe_ratio,
            'max_drawdown': max_drawdown,
            'num_trades': num_trades,
            'num_data_points': len(equity_df),
            'equity_df': equity_df,
            'tradelog_df': tradelog_df
        }
        
        return results
        
    except Exception as e:
        print(f"Error analyzing {symbol}: {e}")
        return None

def create_individual_plots(results, timestamp, output_dir):
    """Create individual plots for each symbol."""
    output_dir = Path(output_dir)
    output_dir.mkdir(exist_ok=True)
    
    for symbol_data in results:
        if symbol_data is None:
            continue
            
        symbol = symbol_data['symbol']
        equity_df = symbol_data['equity_df']
        
        # Create individual equity curve plot
        plt.figure(figsize=(12, 6))
        plt.plot(equity_df['datetime'], equity_df['equity'], linewidth=2, color='blue')
        plt.title(f'{symbol} - Strategy Performance', fontsize=16)
        plt.xlabel('Time', fontsize=12)
        plt.ylabel('Portfolio Value ($)', fontsize=12)
        plt.grid(True, alpha=0.3)
        
        # Format y-axis
        plt.gca().yaxis.set_major_formatter(plt.FuncFormatter(lambda x, p: f'${x:,.0f}'))
        
        # Add statistics
        stats_text = (f'Initial: ${symbol_data["initial_value"]:,.0f}\n'
                     f'Final: ${symbol_data["final_value"]:,.0f}\n'
                     f'Return: {symbol_data["total_return"]:.2f}%\n'
                     f'Sharpe: {symbol_data["sharpe_ratio"]:.2f}\n'
                     f'Max DD: {symbol_data["max_drawdown"]:.2f}%\n'
                     f'Trades: {symbol_data["num_trades"]}')
        
        plt.text(0.02, 0.98, stats_text, transform=plt.gca().transAxes, 
                verticalalignment='top', bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))
        
        # Save individual plot
        output_file = output_dir / f'equity_curve_{symbol}_{timestamp}.png'
        plt.tight_layout()
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"Individual plot saved: {output_file}")

def create_summary_plot(results, timestamp, output_dir):
    """Create a summary plot with all symbols."""
    output_dir = Path(output_dir)
    
    # Filter out None results
    valid_results = [r for r in results if r is not None]
    
    if not valid_results:
        print("No valid results to plot")
        return
    
    # Create 2x2 subplot grid
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle(f'Multi-Symbol Strategy Performance - {timestamp}', fontsize=20)
    
    # Flatten axes for easy iteration
    axes = axes.flatten()
    
    for i, symbol_data in enumerate(valid_results):
        if i >= 4:  # Only plot first 4 symbols
            break
            
        ax = axes[i]
        equity_df = symbol_data['equity_df']
        symbol = symbol_data['symbol']
        
        # Plot equity curve
        ax.plot(equity_df['datetime'], equity_df['equity'], linewidth=2)
        ax.set_title(f'{symbol} - Return: {symbol_data["total_return"]:.2f}%')
        ax.set_ylabel('Portfolio Value ($)')
        ax.grid(True, alpha=0.3)
        ax.yaxis.set_major_formatter(plt.FuncFormatter(lambda x, p: f'${x:,.0f}'))
        
        # Add performance text
        perf_text = (f'Sharpe: {symbol_data["sharpe_ratio"]:.2f}\n'
                    f'Max DD: {symbol_data["max_drawdown"]:.2f}%\n'
                    f'Trades: {symbol_data["num_trades"]}')
        ax.text(0.02, 0.98, perf_text, transform=ax.transAxes, 
               verticalalignment='top', bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.7))
    
    # Hide unused subplots
    for i in range(len(valid_results), 4):
        axes[i].set_visible(False)
    
    plt.tight_layout()
    
    # Save summary plot
    output_file = output_dir / f'multi_symbol_summary_{timestamp}.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"Summary plot saved: {output_file}")
    return output_file

def create_performance_comparison(results, timestamp, output_dir):
    """Create a performance comparison chart."""
    output_dir = Path(output_dir)
    
    valid_results = [r for r in results if r is not None]
    if not valid_results:
        return
    
    # Extract data for comparison
    symbols = [r['symbol'] for r in valid_results]
    returns = [r['total_return'] for r in valid_results]
    sharpe_ratios = [r['sharpe_ratio'] for r in valid_results]
    max_drawdowns = [abs(r['max_drawdown']) for r in valid_results]  # Make positive for display
    num_trades = [r['num_trades'] for r in valid_results]
    
    # Create comparison plots
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle(f'Performance Comparison - {timestamp}', fontsize=20)
    
    # Returns comparison
    axes[0, 0].bar(symbols, returns, color=['green' if r >= 0 else 'red' for r in returns])
    axes[0, 0].set_title('Total Returns (%)')
    axes[0, 0].set_ylabel('Return (%)')
    axes[0, 0].grid(True, alpha=0.3)
    
    # Sharpe ratio comparison
    axes[0, 1].bar(symbols, sharpe_ratios, color=['green' if s >= 0 else 'red' for s in sharpe_ratios])
    axes[0, 1].set_title('Sharpe Ratios')
    axes[0, 1].set_ylabel('Sharpe Ratio')
    axes[0, 1].grid(True, alpha=0.3)
    
    # Max drawdown comparison
    axes[1, 0].bar(symbols, max_drawdowns, color='orange')
    axes[1, 0].set_title('Maximum Drawdown (%)')
    axes[1, 0].set_ylabel('Max Drawdown (%)')
    axes[1, 0].grid(True, alpha=0.3)
    
    # Number of trades comparison
    axes[1, 1].bar(symbols, num_trades, color='blue')
    axes[1, 1].set_title('Number of Trades')
    axes[1, 1].set_ylabel('Trade Count')
    axes[1, 1].grid(True, alpha=0.3)
    
    plt.tight_layout()
    
    # Save comparison plot
    output_file = output_dir / f'performance_comparison_{timestamp}.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"Performance comparison saved: {output_file}")

def print_summary_table(results, timestamp):
    """Print a summary table of all results."""
    valid_results = [r for r in results if r is not None]
    if not valid_results:
        return
    
    print(f"\n=== Performance Summary - {timestamp} ===")
    print(f"{'Symbol':<8} {'Initial':<12} {'Final':<12} {'Return%':<10} {'Sharpe':<8} {'MaxDD%':<8} {'Trades':<8}")
    print("-" * 80)
    
    for r in valid_results:
        print(f"{r['symbol']:<8} ${r['initial_value']:<11,.0f} ${r['final_value']:<11,.0f} "
              f"{r['total_return']:<9.2f} {r['sharpe_ratio']:<7.2f} {r['max_drawdown']:<7.2f} {r['num_trades']:<8}")
    
    # Calculate portfolio-level statistics
    total_initial = sum(r['initial_value'] for r in valid_results)
    total_final = sum(r['final_value'] for r in valid_results)
    portfolio_return = (total_final / total_initial - 1) * 100
    total_trades = sum(r['num_trades'] for r in valid_results)
    
    print("-" * 80)
    print(f"{'TOTAL':<8} ${total_initial:<11,.0f} ${total_final:<11,.0f} "
          f"{portfolio_return:<9.2f} {'N/A':<7} {'N/A':<7} {total_trades:<8}")

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 analyze_multi_run.py <timestamp_suffix>")
        print("Example: python3 analyze_multi_run.py 20241225_143022_123")
        sys.exit(1)
    
    timestamp = sys.argv[1]
    print(f"Analyzing multi-symbol run: {timestamp}")
    
    # Define symbols and file patterns
    symbols = ['AAPL', 'GOOG', 'MSFT', 'SPY']
    results_dir = Path('results')
    plots_dir = Path('plots')
    
    # Analyze each symbol
    results = []
    for symbol in symbols:
        equity_file = results_dir / f'equity_{symbol}_{timestamp}.csv'
        tradelog_file = results_dir / f'tradelog_{symbol}_{timestamp}.csv'
        
        if equity_file.exists():
            print(f"Analyzing {symbol}...")
            result = analyze_single_symbol(equity_file, tradelog_file, symbol)
            results.append(result)
        else:
            print(f"Warning: {equity_file} not found, skipping {symbol}")
            results.append(None)
    
    # Generate all plots and analysis
    create_individual_plots(results, timestamp, plots_dir)
    create_summary_plot(results, timestamp, plots_dir)
    create_performance_comparison(results, timestamp, plots_dir)
    print_summary_table(results, timestamp)
    
    print(f"\n=== Analysis Complete ===")
    print(f"Check the plots/ directory for generated charts")
    print(f"All files are tagged with timestamp: {timestamp}")

if __name__ == "__main__":
    main() 