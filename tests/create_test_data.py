import pandas as pd
import os
import numpy as np
from datetime import datetime, timedelta

def generate_test_parquet():
    # Create test data directory if it doesn't exist
    os.makedirs('test_data', exist_ok=True)
    
    # Generate 100 hours of data starting from 2024-01-01
    start_time = datetime(2024, 1, 1)
    timestamps = [start_time + timedelta(hours=i) for i in range(100)]
    
    # Create a more realistic price series with trends and reversals
    np.random.seed(42)  # For reproducibility
    base_price = 100.0
    prices = [base_price]
    
    # Generate prices with random walk and trends
    for i in range(1, 100):
        # Add some trend and random movement
        change = np.random.normal(0.2, 0.5)  # Mean 0.2 (upward bias), std 0.5
        # Add some periodic behavior
        change += 0.3 * np.sin(i / 10)  # Periodic component
        new_price = prices[-1] + change
        prices.append(new_price)
    
    # Create a DataFrame with OHLCV data
    df = pd.DataFrame({
        'timestamp': timestamps,
        'open': prices,
        'high': [p + abs(np.random.normal(0, 0.5)) for p in prices],
        'low': [p - abs(np.random.normal(0, 0.5)) for p in prices],
        'close': [p + np.random.normal(0, 0.2) for p in prices],
        'volume': [1000 + int(np.random.normal(0, 100)) for _ in range(100)]
    })
    
    # Ensure high is highest and low is lowest
    df['high'] = df[['open', 'high', 'close']].max(axis=1)
    df['low'] = df[['open', 'low', 'close']].min(axis=1)
    
    # Convert timestamp to nanoseconds since epoch
    df['timestamp'] = pd.to_datetime(df['timestamp']).astype('int64')
    
    # Save as Parquet file
    output_path = os.path.join('test_data', 'test_bars.parquet')
    df.to_parquet(output_path, index=False)
    print(f"Generated test data at {output_path}")

if __name__ == '__main__':
    generate_test_parquet() 