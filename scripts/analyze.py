import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path
import math

def analyze_and_get_data(filepath: Path):
    """
    Reads a single backtest equity curve file, computes performance,
    and returns the processed DataFrame and key metrics.
    """
    try:
        symbol = filepath.stem.split('_')[1]
        print(f"\n--- Analyzing results for symbol: {symbol} ---")

        df = pd.read_csv(filepath)
        df['timestamp'] = pd.to_datetime(df['timestamp'], unit='s')
        df.set_index('timestamp', inplace=True)

        returns = df['portfolio_value'].pct_change().dropna()
        
        sharpe_ratio = 0
        if not returns.empty and returns.std() != 0:
            sharpe_ratio = returns.mean() / returns.std() * np.sqrt(252)
        
        cumulative_returns = (1 + returns).cumprod()
        peak = cumulative_returns.expanding(min_periods=1).max()
        drawdown = (cumulative_returns - peak) / peak
        max_drawdown = drawdown.min()

        initial_capital = df['portfolio_value'].iloc[0]
        final_value = df['portfolio_value'].iloc[-1]
        total_return_pct = (final_value / initial_capital - 1) * 100

        print(f"Final Portfolio Value: ${final_value:,.2f}")
        print(f"Total Return: {total_return_pct:.2f}%")
        print(f"Annualized Sharpe Ratio: {sharpe_ratio:.2f}")
        print(f"Max Drawdown: {max_drawdown * 100:.2f}%")
        
        return symbol, df

    except Exception as e:
        print(f"Could not analyze file {filepath}. Error: {e}")
        return None, None

if __name__ == "__main__":
    results_dir = Path("results")
    plot_dir = Path("plots")
    plot_dir.mkdir(exist_ok=True)
    
    equity_files = sorted(list(results_dir.glob("equity_*.csv")))

    if not equity_files:
        print("No equity result files found in the 'results' directory.")
    else:
        print(f"Found {len(equity_files)} result files to analyze.")
        
        all_results = {}
        for filepath in equity_files:
            symbol, df = analyze_and_get_data(filepath)
            if symbol and df is not None:
                all_results[symbol] = df

        # --- Create a single figure with a grid of subplots ---
        if all_results:
            num_plots = len(all_results)
            # Create a grid that's as square as possible
            cols = math.ceil(math.sqrt(num_plots))
            rows = math.ceil(num_plots / cols)
            
            plt.style.use('seaborn-v0_8-darkgrid')
            fig, axes = plt.subplots(rows, cols, figsize=(cols * 6, rows * 5), squeeze=False)
            fig.suptitle('Multi-Asset Strategy Equity Curves', fontsize=20, y=1.02)
            
            # Flatten the 2D array of axes for easy iteration
            axes = axes.flatten()

            i = 0
            for symbol, df in all_results.items():
                ax = axes[i]
                df['portfolio_value'].plot(ax=ax)
                ax.set_title(f"Equity Curve for {symbol}")
                ax.set_ylabel("Portfolio Value ($)")
                ax.grid(True)
                i += 1

            # Hide any unused subplots in the grid
            for j in range(i, len(axes)):
                axes[j].set_visible(False)

            plt.tight_layout(rect=[0, 0, 1, 0.98])
            summary_plot_path = plot_dir / "equity_curves_summary.png"
            plt.savefig(summary_plot_path)
            plt.close()
            print(f"\nSummary plot with all equity curves saved to {summary_plot_path}")

    print("\n--- Python Performance Analysis Finished ---")