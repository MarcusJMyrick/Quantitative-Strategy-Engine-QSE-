import pandas as pd
import os
from datetime import datetime, timedelta

def generate_test_parquet():
    # Create test data directory if it doesn't exist
    os.makedirs('test_data', exist_ok=True)
    
    # Generate 100 hours of data starting from 2024-01-01
    start_time = datetime(2024, 1, 1)
    timestamps = [start_time + timedelta(hours=i) for i in range(100)]
    
    # Create a DataFrame with OHLCV data
    df = pd.DataFrame({
        'timestamp': timestamps,
        'open': [100.0 + i * 0.1 for i in range(100)],
        'high': [102.0 + i * 0.1 for i in range(100)],
        'low': [98.0 + i * 0.1 for i in range(100)],
        'close': [101.0 + i * 0.1 for i in range(100)],
        'volume': [1000 + i * 10 for i in range(100)]
    })
    
    # Convert timestamp to nanoseconds since epoch
    df['timestamp'] = pd.to_datetime(df['timestamp']).astype('int64')
    
    # Save as Parquet file
    output_path = os.path.join('test_data', 'test_bars.parquet')
    df.to_parquet(output_path, index=False)
    print(f"Generated test data at {output_path}")

if __name__ == '__main__':
    generate_test_parquet() 