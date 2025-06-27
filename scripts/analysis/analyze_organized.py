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

def load_equity_curves(equity_dir):
    """
    Load all equity curves from the organized directory.
    """
    equity_files = list(Path(equity_dir).glob("*.csv"))
    equity_data = {}
    
    for file_path in equity_files:
        try:
            df = pd.read_csv(file_path)
            if not df.empty:
                # Extract strategy and symbol from filename
                filename = file_path.stem
                
                # Handle special cases
                if filename == "equity_AAPL_current" or filename == "equity_GOOG" or filename == "equity_MSFT" or filename == "equity_SPY":
                    # These are simple files without strategy info
                    symbol = filename.split('_', 1)[1]  # Remove "equity_" prefix
                    strategy = "Unknown"
                    key = f"{symbol}_{strategy}"
                elif filename.startswith('equity_PairsTrading'):
                    strategy = 'PairsTrading'
                    parts = filename.split('_')
                    if len(parts) >= 4:
                        symbol = f"{parts[2]}_{parts[3]}"  # AAPL_GOOG
                    else:
                        symbol = "Unknown"
                    key = f"{symbol}_{strategy}"
                else:
                    # Regular format: equity_SYMBOL_STRATEGY_TIMESTAMP
                    parts = filename.split('_')
                    if len(parts) >= 3:
                        symbol = parts[1]
                        strategy = parts[2]
                        key = f"{symbol}_{strategy}"
                    else:
                        print(f"⚠️  Skipping {filename}: cannot parse filename")
                        continue
                
                equity_data[key] = df
                print(f"📊 Loaded: {key} ({len(df)} rows)")
        except Exception as e:
            print(f"⚠️  Error loading {file_path}: {e}")
    
    return equity_data

def load_trade_logs(trade_dir):
    """
    Load all trade logs from the organized directory.
    """
    trade_files = list(Path(trade_dir).glob("*.csv"))
    trade_data = {}
    
    for file_path in trade_files:
        try:
            df = pd.read_csv(file_path)
            if not df.empty:
                # Extract strategy and symbol from filename
                filename = file_path.stem
                
                # Handle special cases
                if filename == "tradelog_AAPL" or filename == "tradelog_GOOG" or filename == "tradelog_MSFT" or filename == "tradelog_SPY":
                    # These are simple files without strategy info
                    symbol = filename.split('_', 1)[1]  # Remove "tradelog_" prefix
                    strategy = "Unknown"
                    key = f"{symbol}_{strategy}"
                elif filename.startswith('tradelog_PairsTrading'):
                    strategy = 'PairsTrading'
                    parts = filename.split('_')
                    if len(parts) >= 4:
                        symbol = f"{parts[2]}_{parts[3]}"  # AAPL_GOOG
                    else:
                        symbol = "Unknown"
                    key = f"{symbol}_{strategy}"
                else:
                    # Regular format: tradelog_SYMBOL_STRATEGY_TIMESTAMP
                    parts = filename.split('_')
                    if len(parts) >= 3:
                        symbol = parts[1]
                        strategy = parts[2]
                        key = f"{symbol}_{strategy}"
                    else:
                        print(f"⚠️  Skipping {filename}: cannot parse filename")
                        continue
                
                trade_data[key] = df
                print(f"📈 Loaded: {key} ({len(df)} trades)")
        except Exception as e:
            print(f"⚠️  Error loading {file_path}: {e}")
    
    return trade_data

def calculate_performance_metrics(equity_data):
    """
    Calculate performance metrics for each strategy.
    """
    metrics = {}
    
    for key, df in equity_data.items():
        if df.empty:
            continue
            
        # Ensure we have the required columns
        if 'timestamp' not in df.columns or 'equity' not in df.columns:
            print(f"⚠️  Skipping {key}: missing required columns")
            continue
        
        # Convert timestamp to datetime if needed
        if df['timestamp'].dtype == 'object':
            df['timestamp'] = pd.to_datetime(df['timestamp'], unit='s')
        
        # Calculate metrics
        initial_equity = float(df['equity'].iloc[0])
        final_equity = float(df['equity'].iloc[-1])
        total_return = (final_equity - initial_equity) / initial_equity * 100
        
        # Calculate daily returns for volatility
        df['daily_return'] = df['equity'].pct_change()
        volatility = float(df['daily_return'].std() * np.sqrt(252) * 100)  # Annualized
        
        # Calculate Sharpe ratio (assuming risk-free rate of 0)
        if volatility > 0:
            sharpe_ratio = float((total_return / 100) / (volatility / 100))
        else:
            sharpe_ratio = 0.0
        
        # Maximum drawdown
        df['cummax'] = df['equity'].cummax()
        df['drawdown'] = (df['equity'] - df['cummax']) / df['cummax'] * 100
        max_drawdown = float(df['drawdown'].min())
        
        metrics[key] = {
            'total_return_pct': float(total_return),
            'volatility_pct': float(volatility),
            'sharpe_ratio': float(sharpe_ratio),
            'max_drawdown_pct': float(max_drawdown),
            'final_equity': float(final_equity),
            'initial_equity': float(initial_equity),
            'trading_days': int(len(df))
        }
    
    return metrics

def analyze_trades(trade_data):
    """
    Analyze trading activity for each strategy.
    """
    trade_analysis = {}
    
    for key, df in trade_data.items():
        if df.empty:
            continue
        
        # Basic trade statistics
        total_trades = int(len(df))
        
        # Count buy/sell trades
        if 'side' in df.columns:
            buy_trades = int(len(df[df['side'] == 'BUY']))
            sell_trades = int(len(df[df['side'] == 'SELL']))
        else:
            buy_trades = sell_trades = 0
        
        # Calculate total volume and P&L
        if 'quantity' in df.columns:
            total_volume = float(df['quantity'].sum())
        else:
            total_volume = 0.0
        
        if 'pnl' in df.columns:
            total_pnl = float(df['pnl'].sum())
            avg_pnl_per_trade = float(df['pnl'].mean())
        else:
            total_pnl = avg_pnl_per_trade = 0.0
        
        trade_analysis[key] = {
            'total_trades': total_trades,
            'buy_trades': buy_trades,
            'sell_trades': sell_trades,
            'total_volume': total_volume,
            'total_pnl': total_pnl,
            'avg_pnl_per_trade': avg_pnl_per_trade
        }
    
    return trade_analysis

def create_performance_plots(equity_data, metrics, output_dir):
    """
    Create performance visualization plots.
    """
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)
    
    # 1. Equity Curves Comparison
    plt.figure(figsize=(15, 10))
    
    # Group by strategy
    strategies = {}
    for key, df in equity_data.items():
        if df.empty:
            continue
        strategy = key.split('_', 1)[1]  # Get strategy part
        if strategy not in strategies:
            strategies[strategy] = []
        strategies[strategy].append((key, df))
    
    # Plot each strategy
    for i, (strategy, data_list) in enumerate(strategies.items()):
        plt.subplot(2, 2, i+1)
        
        for key, df in data_list:
            if 'timestamp' in df.columns and 'equity' in df.columns:
                # Normalize to start at 100
                normalized_equity = df['equity'] / df['equity'].iloc[0] * 100
                plt.plot(df['timestamp'], normalized_equity, label=key.split('_')[0], alpha=0.7)
        
        plt.title(f'{strategy} Strategy Performance')
        plt.xlabel('Time')
        plt.ylabel('Normalized Equity (%)')
        plt.legend()
        plt.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_path / 'strategy_performance_comparison.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    # 2. Performance Metrics Heatmap
    if metrics:
        plt.figure(figsize=(12, 8))
        
        # Create metrics DataFrame
        metrics_df = pd.DataFrame.from_dict(metrics, orient='index')
        
        # Select key metrics for heatmap
        heatmap_data = metrics_df[['total_return_pct', 'volatility_pct', 'sharpe_ratio', 'max_drawdown_pct']]
        
        # Create heatmap
        sns.heatmap(heatmap_data.T, annot=True, fmt='.2f', cmap='RdYlGn', center=0)
        plt.title('Performance Metrics Heatmap')
        plt.xlabel('Strategy-Symbol')
        plt.ylabel('Metric')
        plt.xticks(rotation=45, ha='right')
        plt.tight_layout()
        plt.savefig(output_path / 'performance_metrics_heatmap.png', dpi=300, bbox_inches='tight')
        plt.close()
    
    # 3. Return Distribution
    plt.figure(figsize=(12, 8))
    
    returns = []
    labels = []
    
    for key, df in equity_data.items():
        if not df.empty and 'equity' in df.columns:
            daily_returns = df['equity'].pct_change().dropna()
            returns.extend(daily_returns)
            labels.extend([key] * len(daily_returns))
    
    if returns:
        returns_df = pd.DataFrame({'return': returns, 'strategy': labels})
        
        plt.subplot(2, 2, 1)
        for strategy in returns_df['strategy'].unique():
            strategy_returns = returns_df[returns_df['strategy'] == strategy]['return']
            plt.hist(strategy_returns, alpha=0.5, label=strategy, bins=30)
        plt.title('Return Distribution by Strategy')
        plt.xlabel('Daily Return')
        plt.ylabel('Frequency')
        plt.legend()
        
        plt.subplot(2, 2, 2)
        returns_df.boxplot(column='return', by='strategy', ax=plt.gca())
        plt.title('Return Distribution Box Plot')
        plt.suptitle('')  # Remove default title
        
        plt.tight_layout()
        plt.savefig(output_path / 'return_distribution.png', dpi=300, bbox_inches='tight')
        plt.close()

def generate_analysis_report(metrics, trade_analysis, output_dir):
    """
    Generate a comprehensive analysis report.
    """
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)
    
    # Create summary report
    report = {
        'analysis_timestamp': datetime.now().isoformat(),
        'performance_metrics': metrics,
        'trading_analysis': trade_analysis,
        'summary': {
            'total_strategies': len(metrics),
            'total_trading_combinations': len(trade_analysis),
            'best_performer': None,
            'worst_performer': None
        }
    }
    
    # Find best and worst performers
    if metrics:
        returns = {k: v['total_return_pct'] for k, v in metrics.items()}
        if returns:
            report['summary']['best_performer'] = max(returns, key=returns.get)
            report['summary']['worst_performer'] = min(returns, key=returns.get)
    
    # Save JSON report
    with open(output_path / 'analysis_report.json', 'w') as f:
        json.dump(report, f, indent=2)
    
    # Save human-readable report
    with open(output_path / 'analysis_report.txt', 'w') as f:
        f.write("=== QSE Organized Analysis Report ===\n")
        f.write(f"Generated: {report['analysis_timestamp']}\n\n")
        
        f.write("PERFORMANCE SUMMARY:\n")
        f.write("=" * 50 + "\n")
        if metrics:
            f.write(f"{'Strategy':<30} {'Return%':<10} {'Vol%':<8} {'Sharpe':<8} {'MaxDD%':<8}\n")
            f.write("-" * 70 + "\n")
            for key, metric in metrics.items():
                f.write(f"{key:<30} {metric['total_return_pct']:<10.2f} {metric['volatility_pct']:<8.2f} "
                       f"{metric['sharpe_ratio']:<8.2f} {metric['max_drawdown_pct']:<8.2f}\n")
        
        f.write("\nTRADING ACTIVITY:\n")
        f.write("=" * 50 + "\n")
        if trade_analysis:
            f.write(f"{'Strategy':<30} {'Trades':<8} {'Buy':<6} {'Sell':<6} {'Volume':<10} {'P&L':<12}\n")
            f.write("-" * 70 + "\n")
            for key, analysis in trade_analysis.items():
                f.write(f"{key:<30} {analysis['total_trades']:<8} {analysis['buy_trades']:<6} "
                       f"{analysis['sell_trades']:<6} {analysis['total_volume']:<10.0f} "
                       f"{analysis['total_pnl']:<12.2f}\n")
    
    print(f"📋 Generated analysis report: {output_path / 'analysis_report.txt'}")

def main():
    parser = argparse.ArgumentParser(description="Analyze organized QSE trading results")
    parser.add_argument(
        '--organized-dir', 
        required=True,
        help='Path to organized results directory'
    )
    parser.add_argument(
        '--output-dir', 
        default='analysis_output',
        help='Output directory for analysis results (default: analysis_output)'
    )
    
    args = parser.parse_args()
    
    organized_path = Path(args.organized_dir)
    if not organized_path.exists():
        print(f"❌ Organized directory not found: {organized_path}")
        return
    
    print("🔍 QSE Organized Analysis")
    print("=" * 40)
    
    # Load data
    print("\n📊 Loading equity curves...")
    equity_data = load_equity_curves(organized_path / "data" / "equity_curves")
    
    print("\n📈 Loading trade logs...")
    trade_data = load_trade_logs(organized_path / "data" / "trade_logs")
    
    if not equity_data:
        print("❌ No equity data found!")
        return
    
    # Calculate metrics
    print("\n📊 Calculating performance metrics...")
    metrics = calculate_performance_metrics(equity_data)
    
    print("\n📈 Analyzing trading activity...")
    trade_analysis = analyze_trades(trade_data)
    
    # Generate plots
    print("\n📊 Creating performance plots...")
    create_performance_plots(equity_data, metrics, args.output_dir)
    
    # Generate report
    print("\n📋 Generating analysis report...")
    generate_analysis_report(metrics, trade_analysis, args.output_dir)
    
    print(f"\n✅ Analysis complete! Results saved to: {args.output_dir}")

if __name__ == "__main__":
    main() 