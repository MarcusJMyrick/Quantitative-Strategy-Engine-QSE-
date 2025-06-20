import pandas as pd
import os
import numpy as np
from datetime import datetime, timedelta
from pathlib import Path

def generate_test_data():
    """
    Generates a realistic test dataset and saves it in two formats:
    1. A Parquet file for future, high-performance data readers.
    2. A simple CSV file to support the existing C++ unit tests.
    """
    output_dir = Path("test_data")
    output_dir.mkdir(exist_ok=True)
    
    # Generate 100 hours of data starting from 2024-01-01
    start_time = datetime(2024, 1, 1)
    timestamps = [start_time + timedelta(hours=i) for i in range(100)]
    
    # Create a more realistic price series with trends and reversals
    np.random.seed(42)  # For reproducibility
    base_price = 100.0
    prices = [base_price]
    
    # Generate prices with random walk and trends
    for i in range(1, 100):
        change = np.random.normal(0.2, 0.5)  # Upward bias with noise
        change += 0.3 * np.sin(i / 10)      # Add a periodic component
        new_price = prices[-1] + change
        prices.append(max(1, new_price)) # Ensure price doesn't go below 1
    
    # Create a DataFrame with OHLCV data
    df = pd.DataFrame({
        'timestamp_dt': timestamps,
        'open': prices,
        'high': [p + abs(np.random.normal(0, 0.5)) for p in prices],
        'low': [p - abs(np.random.normal(0, 0.5)) for p in prices],
        'close': [p + np.random.normal(0, 0.2) for p in prices],
        'volume': [1000 + int(np.random.rand() * 200) for _ in range(100)]
    })
    
    # Ensure high is the highest price and low is the lowest
    df['high'] = df[['open', 'high', 'close']].max(axis=1)
    df['low'] = df[['open', 'low', 'close']].min(axis=1)
    
    # --- 1. Save as Parquet file (for future use) ---
    df_parquet = df.copy()
    # Convert timestamp to nanoseconds since epoch for Parquet
    df_parquet['timestamp'] = pd.to_datetime(df_parquet['timestamp_dt']).astype('int64')
    output_path_parquet = output_dir / 'test_bars.parquet'
    df_parquet[['timestamp', 'open', 'high', 'low', 'close', 'volume']].to_parquet(output_path_parquet, index=False)
    print(f"Generated Parquet test data at {output_path_parquet}")

    # --- 2. Save as CSV file (for current C++ tests) ---
    df_csv = df.copy()
    # Convert timestamp to seconds since epoch for the CSV reader
    df_csv['timestamp'] = (pd.to_datetime(df_csv['timestamp_dt']).astype('int64') // 10**9)
    output_path_csv = output_dir / 'test_data.csv'
    df_csv[['timestamp', 'open', 'high', 'low', 'close', 'volume']].to_csv(output_path_csv, index=False)
    print(f"Generated CSV test data at {output_path_csv}")


if __name__ == '__main__':
    generate_test_data()