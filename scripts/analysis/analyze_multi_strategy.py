#!/usr/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys
import glob
from pathlib import Path
import seaborn as sns
import re
import argparse

def analyze_single_strategy_result(equity_df, tradelog_df, symbol, strategy):
    """Analyze results for a single strategy on a single symbol."""
    try:
        if equity_df.empty:
            print(f"Warning: Equity dataframe is empty for {strategy} on {symbol}")
            return None

        # Convert timestamp to datetime
        equity_df['datetime'] = pd.to_datetime(equity_df['timestamp'], unit='s')
        
        # Get trade count
        num_trades = len(tradelog_df)
        
        # Calculate performance metrics
        initial_value = equity_df['equity'].iloc[0]
        final_value = equity_df['equity'].iloc[-1]
        total_return = (final_value / initial_value - 1) * 100
        
        # Calculate returns for Sharpe ratio
        equity_df['returns'] = equity_df['equity'].pct_change().fillna(0)
        if equity_df['returns'].std() > 0:
            sharpe_ratio = equity_df['returns'].mean() / equity_df['returns'].std() * np.sqrt(252 * 24 * 60)
        else:
            sharpe_ratio = 0
        
        # Calculate max drawdown
        equity_df['peak'] = equity_df['equity'].expanding().max()
        equity_df['drawdown'] = (equity_df['equity'] - equity_df['peak']) / equity_df['peak'] * 100
        max_drawdown = equity_df['drawdown'].min()
        
        results = {
            'symbol': symbol,
            'strategy': strategy,
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
        print(f"Error analyzing {strategy} for {symbol}: {e}")
        return None

def find_strategy_results(run_dir):
    """
    Finds all equity and tradelog files in a specific run directory.
    """
    results = []
    
    if not run_dir.is_dir():
        print(f"Error: Could not find run directory {run_dir}")
        return results

    # The file names no longer have timestamps, so parsing is simpler
    for equity_file in run_dir.glob('equity_*.csv'):
        try:
            filename = equity_file.stem
            parts = filename.split('_')
            
            # e.g., equity_PairsTrading_AAPL_GOOG -> [equity, PairsTrading, AAPL, GOOG]
            # e.g., equity_AAPL_SMA_20_50 -> [equity, AAPL, SMA, 20, 50]
            if parts[1] == "PairsTrading":
                strategy = "PairsTrading"
                symbol = f"{parts[2]}_{parts[3]}"
                tradelog_filename = f"tradelog_PairsTrading_{symbol}.csv"
            else:
                symbol = parts[1]
                strategy = '_'.join(parts[2:])
                tradelog_filename = f"tradelog_{symbol}_{strategy}.csv"

            tradelog_file = run_dir / tradelog_filename
            
            if tradelog_file.exists():
                print(f"Found result for {strategy} on {symbol}")
                # Store the filename in the dataframe for later reference
                equity_df = pd.read_csv(equity_file)
                equity_df.attrs['filename'] = str(equity_file)
                
                tradelog_df = pd.read_csv(tradelog_file)
                tradelog_df.attrs['filename'] = str(tradelog_file)

                result = analyze_single_strategy_result(equity_df, tradelog_df, symbol, strategy)
                if result:
                    results.append(result)
            else:
                print(f"Warning: Found equity file but missing tradelog: {tradelog_file.name}")
        except Exception as e:
            print(f"Could not parse file {equity_file}: {e}")
            
    return results

def create_strategy_comparison_plots(results, timestamp, output_dir):
    """Create comparison plots across different strategies."""
    output_dir = Path(output_dir)
    
    if not results:
        print("No results to plot")
        return
    
    # Group results by symbol
    symbols = list(set(r['symbol'] for r in results))
    strategies = list(set(r['strategy'] for r in results))
    
    print(f"Found {len(symbols)} symbols: {symbols}")
    print(f"Found {len(strategies)} strategies: {strategies}")
    
    # Create strategy comparison for each symbol
    for symbol in symbols:
        symbol_results = [r for r in results if r['symbol'] == symbol]
        if len(symbol_results) < 2:
            continue
            
        # Create equity curve comparison
        plt.figure(figsize=(15, 8))
        
        colors = ['blue', 'red', 'green', 'orange', 'purple', 'brown']
        for i, result in enumerate(symbol_results):
            color = colors[i % len(colors)]
            plt.plot(result['equity_df']['datetime'], result['equity_df']['equity'], 
                    linewidth=2, color=color, label=f"{result['strategy']} ({result['total_return']:.2f}%)")
        
        plt.title(f'{symbol} - Strategy Performance Comparison', fontsize=16)
        plt.xlabel('Time', fontsize=12)
        plt.ylabel('Portfolio Value ($)', fontsize=12)
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.gca().yaxis.set_major_formatter(plt.FuncFormatter(lambda x, p: f'${x:,.0f}'))
        
        # Save plot
        output_file = output_dir / f'strategy_comparison_{symbol}_{timestamp}.png'
        plt.tight_layout()
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"Strategy comparison saved: {output_file}")

def create_performance_heatmap(results, timestamp, output_dir):
    """Create a heatmap showing performance across symbols and strategies."""
    output_dir = Path(output_dir)
    
    if not results:
        return
    
    # Create performance matrix
    symbols = sorted(list(set(r['symbol'] for r in results)))
    strategies = sorted(list(set(r['strategy'] for r in results)))
    
    # Create matrices for different metrics
    returns_matrix = np.zeros((len(strategies), len(symbols)))
    sharpe_matrix = np.zeros((len(strategies), len(symbols)))
    trades_matrix = np.zeros((len(strategies), len(symbols)))
    
    for result in results:
        symbol_idx = symbols.index(result['symbol'])
        strategy_idx = strategies.index(result['strategy'])
        
        returns_matrix[strategy_idx, symbol_idx] = result['total_return']
        sharpe_matrix[strategy_idx, symbol_idx] = result['sharpe_ratio']
        trades_matrix[strategy_idx, symbol_idx] = result['num_trades']
    
    # Create subplots for different metrics
    fig, axes = plt.subplots(1, 3, figsize=(20, 6))
    fig.suptitle(f'Strategy Performance Heatmaps - {timestamp}', fontsize=16)
    
    # Returns heatmap
    im1 = axes[0].imshow(returns_matrix, cmap='RdYlGn', aspect='auto')
    axes[0].set_title('Total Returns (%)')
    axes[0].set_xticks(range(len(symbols)))
    axes[0].set_yticks(range(len(strategies)))
    axes[0].set_xticklabels(symbols)
    axes[0].set_yticklabels(strategies)
    plt.colorbar(im1, ax=axes[0])
    
    # Add text annotations
    for i in range(len(strategies)):
        for j in range(len(symbols)):
            text = axes[0].text(j, i, f'{returns_matrix[i, j]:.1f}%',
                               ha="center", va="center", color="black", fontweight='bold')
    
    # Sharpe ratio heatmap
    im2 = axes[1].imshow(sharpe_matrix, cmap='RdYlGn', aspect='auto')
    axes[1].set_title('Sharpe Ratios')
    axes[1].set_xticks(range(len(symbols)))
    axes[1].set_yticks(range(len(strategies)))
    axes[1].set_xticklabels(symbols)
    axes[1].set_yticklabels(strategies)
    plt.colorbar(im2, ax=axes[1])
    
    # Add text annotations
    for i in range(len(strategies)):
        for j in range(len(symbols)):
            text = axes[1].text(j, i, f'{sharpe_matrix[i, j]:.2f}',
                               ha="center", va="center", color="black", fontweight='bold')
    
    # Number of trades heatmap
    im3 = axes[2].imshow(trades_matrix, cmap='Blues', aspect='auto')
    axes[2].set_title('Number of Trades')
    axes[2].set_xticks(range(len(symbols)))
    axes[2].set_yticks(range(len(strategies)))
    axes[2].set_xticklabels(symbols)
    axes[2].set_yticklabels(strategies)
    plt.colorbar(im3, ax=axes[2])
    
    # Add text annotations
    for i in range(len(strategies)):
        for j in range(len(symbols)):
            text = axes[2].text(j, i, f'{int(trades_matrix[i, j])}',
                               ha="center", va="center", color="black", fontweight='bold')
    
    plt.tight_layout()
    
    # Save heatmap
    output_file = output_dir / f'performance_heatmap_{timestamp}.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"Performance heatmap saved: {output_file}")

def create_best_strategy_summary(results, timestamp, output_dir):
    """Create a summary showing the best performing strategy for each symbol."""
    output_dir = Path(output_dir)
    
    if not results:
        return
    
    # Group by symbol and find best strategy
    symbols = list(set(r['symbol'] for r in results))
    best_strategies = []
    
    for symbol in symbols:
        symbol_results = [r for r in results if r['symbol'] == symbol]
        if symbol_results:
            # Find best by return
            best_by_return = max(symbol_results, key=lambda x: x['total_return'])
            best_strategies.append(best_by_return)
    
    if not best_strategies:
        return
    
    # Create summary plot
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle(f'Best Strategy Performance by Symbol - {timestamp}', fontsize=20)
    
    axes = axes.flatten()
    
    for i, result in enumerate(best_strategies):
        if i >= 4:  # Only plot first 4
            break
            
        ax = axes[i]
        equity_df = result['equity_df']
        symbol = result['symbol']
        strategy = result['strategy']
        
        # Plot equity curve
        ax.plot(equity_df['datetime'], equity_df['equity'], linewidth=2)
        ax.set_title(f'{symbol} - {strategy}\nReturn: {result["total_return"]} {result["total_return"]}')
        ax.set_ylabel('Portfolio Value ($)')
        ax.grid(True, alpha=0.3)
        ax.yaxis.set_major_formatter(plt.FuncFormatter(lambda x, p: f'${x:,.0f}'))
        
        # Add performance text
        perf_text = (f'Sharpe: {result["sharpe_ratio"]:.2f}\n'
                    f'Max DD: {result["max_drawdown"]:.2f}%\n'
                    f'Trades: {result["num_trades"]}')
        ax.text(0.02, 0.98, perf_text, transform=ax.transAxes, 
               verticalalignment='top', bbox=dict(boxstyle='round', facecolor='lightgreen', alpha=0.7))
    
    # Hide unused subplots
    for i in range(len(best_strategies), 4):
        axes[i].set_visible(False)
    
    plt.tight_layout()
    
    # Save summary plot
    output_file = output_dir / f'best_strategies_summary_{timestamp}.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"Best strategies summary saved: {output_file}")

def print_detailed_summary_table(results, timestamp):
    """Print a detailed summary table of all results."""
    if not results:
        return
    
    print(f"\n=== Multi-Strategy Performance Summary - {timestamp} ===")
    print(f"{'Symbol':<12} {'Strategy':<15} {'Initial':<12} {'Final':<12} {'Return%':<10} {'Sharpe':<8} {'MaxDD%':<8} {'Trades':<8}")
    print("-" * 100)
    
    # Sort by symbol, then by return
    sorted_results = sorted(results, key=lambda x: (x['symbol'], -x['total_return']))
    
    for r in sorted_results:
        print(f"{r['symbol']:<12} {r['strategy']:<15} ${r['initial_value']:<11,.0f} ${r['final_value']:<11,.0f} "
              f"{r['total_return']:<9.2f} {r['sharpe_ratio']:<7.2f} {r['max_drawdown']:<7.2f} {r['num_trades']:<8}")
    
    # Summary by strategy
    print("\n" + "="*100)
    print("BEST PERFORMING STRATEGY BY SYMBOL:")
    print("="*100)
    
    symbols = list(set(r['symbol'] for r in results))
    for symbol in symbols:
        symbol_results = [r for r in results if r['symbol'] == symbol]
        if symbol_results:
            best = max(symbol_results, key=lambda x: x['total_return'])
            print(f"{symbol:<12} {best['strategy']:<15} {best['total_return']:>8.2f}% "
                  f"{best['sharpe_ratio']:>7.2f} {best['max_drawdown']:>7.2f}% {best['num_trades']:>8}")

def analyze_and_plot(strategy_results, output_dir):
    """Generate all plots and summaries for a given set of results."""
    if not strategy_results:
        print("No strategy results to analyze.")
        return

    # Use the first result's timestamp as a representative for the run
    # This assumes all files in the directory share a similar naming scheme
    first_equity_file = Path(strategy_results[0]['equity_df'].attrs['filename'])
    run_timestamp = first_equity_file.parent.name
    
    print(f"\n--- Analysis Summary for Run: {run_timestamp} ---")

    # Create plots and summaries
    create_strategy_comparison_plots(strategy_results, run_timestamp, output_dir)
    create_performance_heatmap(strategy_results, run_timestamp, output_dir)
    create_best_strategy_summary(strategy_results, run_timestamp, output_dir)
    
    # Print detailed table to console
    print_detailed_summary_table(strategy_results, run_timestamp)

def main():
    """Main function to find and analyze strategy results."""
    parser = argparse.ArgumentParser(description="Analyze multi-strategy backtest results.")
    parser.add_argument(
        'run_directory',
        nargs='?',
        default=None,
        help="Specific run directory to analyze (e.g., results/20240101_123000_000). If not provided, analyzes the latest run."
    )
    args = parser.parse_args()

    results_root = Path("results")
    
    if not results_root.exists():
        print(f"Error: Top-level results directory '{results_root}' not found.")
        sys.exit(1)

    if args.run_directory:
        target_dir = Path(args.run_directory)
        if not target_dir.is_dir():
            print(f"Error: Specified run directory '{target_dir}' does not exist.")
            sys.exit(1)
    else:
        # Find the most recent run directory
        run_dirs = sorted([d for d in results_root.iterdir() if d.is_dir()], reverse=True)
        if not run_dirs:
            print(f"Error: No run directories found in '{results_root}'.")
            sys.exit(1)
        target_dir = run_dirs[0]

    print(f"--- Analyzing backtest results in: {target_dir} ---")
    
    # Find and analyze the results in the target directory
    # Note: The original find_strategy_results might need adjustment if the file naming has changed.
    # This implementation assumes the file naming inside the run folder is consistent with the old one.
    strategy_results = find_strategy_results(target_dir)
    
    # The output plots will be saved inside the run directory itself
    analyze_and_plot(strategy_results, target_dir)

if __name__ == "__main__":
    main() 