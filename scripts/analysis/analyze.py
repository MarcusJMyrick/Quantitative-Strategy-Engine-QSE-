import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path
import math

def plot_ticks_vs_bars(save_path: Path):
    """Generates and saves a plot showing raw ticks aggregating into OHLC bars."""
    # 1. Create synthetic tick data
    base_time = pd.Timestamp("2024-01-01 09:30:00")
    timestamps = [base_time + pd.Timedelta(seconds=x) for x in np.arange(0, 60, 0.5)]
    price = 100 + np.cumsum(np.random.randn(len(timestamps)) * 0.1)
    ticks_df = pd.DataFrame({"price": price}, index=timestamps)

    # 2. Aggregate into 10-second bars
    bars_df = ticks_df["price"].resample("10S").ohlc()

    # 3. Plotting
    fig, ax = plt.subplots(figsize=(12, 6))
    ax.plot(
        ticks_df.index,
        ticks_df["price"],
        marker=".",
        linestyle="-",
        label="Ticks",
        alpha=0.6,
    )

    # Plot OHLC bars
    for idx, row in bars_df.iterrows():
        ax.plot(
            [idx, idx + pd.Timedelta(seconds=10)],
            [row["open"], row["open"]],
            color="grey",
        )  # Open
        ax.plot(
            [idx, idx + pd.Timedelta(seconds=10)],
            [row["close"], row["close"]],
            color="black",
        )  # Close
        ax.vlines(
            idx + pd.Timedelta(seconds=5),
            ymin=row["low"],
            ymax=row["high"],
            color="black",
            linewidth=1,
        )

    ax.set_title("Tick Aggregation into 10-Second OHLC Bars")
    ax.set_ylabel("Price")
    ax.set_xlabel("Time")
    ax.legend(["Ticks", "Bar Open", "Bar Close", "Bar High-Low"])
    ax.grid(True)
    fig.savefig(save_path)
    plt.close(fig)
    print(f"Chart saved: {save_path}")


def plot_order_book_snapshot(save_path: Path):
    """Generates a synthetic order book depth chart."""
    # 1. Synthetic book data
    bids = {"price": np.arange(100, 99, -0.01), "size": np.random.randint(50, 200, 100)}
    asks = {
        "price": np.arange(100.01, 101.01, 0.01),
        "size": np.random.randint(50, 200, 100),
    }
    bid_df = pd.DataFrame(bids)
    ask_df = pd.DataFrame(asks)
    bid_df["cumulative_size"] = bid_df["size"].cumsum()
    ask_df["cumulative_size"] = ask_df["size"].cumsum()

    # 2. Plotting
    fig, ax = plt.subplots(figsize=(12, 6))
    ax.fill_between(
        bid_df["price"],
        bid_df["cumulative_size"],
        step="pre",
        alpha=0.4,
        label="Bids",
    )
    ax.fill_between(
        ask_df["price"],
        ask_df["cumulative_size"],
        step="pre",
        alpha=0.4,
        label="Asks",
    )

    # Mark a hypothetical market order fill
    fill_price = 100.02
    fill_size = 350
    ax.axvline(fill_price, color="red", linestyle="--", label="Market Buy Fill Price")
    ax.axhline(fill_size, color="red", linestyle="--")

    ax.set_title("Order Book Depth Snapshot")
    ax.set_xlabel("Price")
    ax.set_ylabel("Cumulative Size")
    ax.legend()
    ax.grid(True)
    fig.savefig(save_path)
    plt.close(fig)
    print(f"Chart saved: {save_path}")


def plot_slippage_impact(save_path: Path):
    """Generates a plot comparing equity curves with and without slippage."""
    # 1. Synthetic equity curves
    days = 100
    base_returns = np.random.randn(days) * 0.01
    no_slippage_equity = 100000 * (1 + base_returns).cumprod()
    slippage_cost = 0.0005  # 0.05% per trade
    slippage_equity = 100000 * (1 + base_returns - slippage_cost).cumprod()

    # 2. Plotting
    fig, ax = plt.subplots(figsize=(12, 6))
    ax.plot(no_slippage_equity, label="P&L without Slippage")
    ax.plot(slippage_equity, label="P&L with Slippage")

    ax.set_title("Impact of Slippage on Equity Curve")
    ax.set_xlabel("Time (Trades)")
    ax.set_ylabel("Portfolio Value ($)")
    ax.legend()
    ax.grid(True)
    fig.savefig(save_path)
    plt.close(fig)
    print(f"Chart saved: {save_path}")


def generate_documentation_plots():
    """Main function to generate all plots for documentation."""
    doc_plot_dir = Path("docs/plots")
    doc_plot_dir.mkdir(exist_ok=True)
    print("\n--- Generating Documentation Plots ---")
    plot_ticks_vs_bars(doc_plot_dir / "ticks_to_bars.png")
    plot_order_book_snapshot(doc_plot_dir / "order_book_snapshot.png")
    plot_slippage_impact(doc_plot_dir / "slippage_impact.png")


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

    # Generate additional plots for documentation
    generate_documentation_plots()

    print("\n--- Python Performance Analysis Finished ---")