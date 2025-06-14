import pandas as pd
import os

def generate_test_parquet():
    """Generates a simple, predictable parquet file for C++ tests."""
    data = {
        'timestamp': pd.to_datetime(['2024-01-01 12:00:00', '2024-01-01 12:01:00']),
        'open': [100.0, 101.0],
        'high': [102.5, 102.8],
        'low': [99.5, 100.5],
        'close': [101.2, 101.9],
        'volume': [1000.0, 1200.0]
    }
    df = pd.DataFrame(data)

    # Ensure the test_data directory exists
    output_dir = "test_data"
    os.makedirs(output_dir, exist_ok=True)
    
    output_path = os.path.join(output_dir, "test_bars.parquet")
    df.to_parquet(output_path)
    print(f"Generated test data at {output_path}")

if __name__ == "__main__":
    generate_test_parquet() 