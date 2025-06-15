import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

def analyze_results(filepath="results/results.csv", plot_dir="plots"):
    """
    Reads the backtest results and computes/plots performance metrics.
    """
    print("\n--- Running Python Performance Analysis ---")
    
    if not os.path.exists(filepath):
        print(f"Error: The results file was not found at '{filepath}'")
        print("Please ensure you have run the C++ backtester first.")
        return

    df = pd.read_csv(filepath)
    df['timestamp'] = pd.to_datetime(df['timestamp'], unit='s')
    df.set_index('timestamp', inplace=True)

    # --- Calculate Key Performance Metrics ---
    returns = df['portfolio_value'].pct_change().dropna()
    
    # Sharpe Ratio (annualized, assuming daily data for simplicity)
    sharpe_ratio = 0
    if not returns.empty and returns.std() != 0:
        # 252 trading days in a year
        sharpe_ratio = returns.mean() / returns.std() * np.sqrt(252)
    
    # Max Drawdown
    cumulative_returns = (1 + returns).cumprod()
    peak = cumulative_returns.expanding(min_periods=1).max()
    drawdown = (cumulative_returns - peak) / peak
    max_drawdown = drawdown.min()

    print(f"\nTotal Return: {(df['portfolio_value'].iloc[-1] / df['portfolio_value'].iloc[0] - 1) * 100:.2f}%")
    print(f"Annualized Sharpe Ratio: {sharpe_ratio:.2f}")
    print(f"Max Drawdown: {max_drawdown * 100:.2f}%")

    # --- Plot the Equity Curve ---
    os.makedirs(plot_dir, exist_ok=True)
    plt.style.use('seaborn-v0_8-darkgrid')
    plt.figure(figsize=(12, 8))
    df['portfolio_value'].plot()
    plt.title("Portfolio Equity Curve: SMA Crossover Strategy", fontsize=16)
    plt.ylabel("Portfolio Value ($)", fontsize=12)
    plt.xlabel("Date", fontsize=12)
    plt.grid(True)
    
    plot_path = os.path.join(plot_dir, "equity_curve.png")
    plt.savefig(plot_path)
    print(f"\nEquity curve plot saved to {plot_path}")

if __name__ == "__main__":
    analyze_results() 