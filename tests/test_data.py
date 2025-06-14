import os
import pandas as pd
import pytest
from scripts.download_data import fetch_crypto_data
from scripts.process_data import process_raw_data
from unittest.mock import patch

def create_mock_data():
    """Create mock market data for testing."""
    dates = pd.date_range(start='2024-01-01', periods=100, freq='h')
    data = {
        'open': [100.0 + i for i in range(100)],
        'high': [101.0 + i for i in range(100)],
        'low': [99.0 + i for i in range(100)],
        'close': [100.5 + i for i in range(100)],
        'volume': [1000.0 + i * 10 for i in range(100)]
    }
    return pd.DataFrame(data, index=dates)

class MockResponse:
    def json(self):
        return {'Time Series (60min)': create_mock_data().to_dict()}
    def raise_for_status(self):
        pass

@patch('scripts.download_data.requests.get')
def test_data_pipeline(mock_get):
    """Test the complete data pipeline from download to processing."""
    mock_get.return_value = MockResponse()

    # 1. Test download
    symbol = 'BTCUSD'
    raw_file = fetch_crypto_data(symbol=symbol, interval='60min', limit=100)
    assert raw_file is not None
    assert os.path.exists(raw_file)
    
    # Verify raw data format
    raw_df = pd.read_csv(raw_file, index_col=0, parse_dates=True)
    expected_columns = ['open', 'high', 'low', 'close', 'volume']
    assert list(raw_df.columns) == expected_columns
    assert isinstance(raw_df.index, pd.DatetimeIndex)
    
    # 2. Test processing
    processed_file = process_raw_data(symbol=symbol)
    assert processed_file is not None
    assert os.path.exists(processed_file)
    
    # 3. Validate processed data
    df = pd.read_parquet(processed_file)
    assert not df.isnull().values.any()  # Check for NaNs
    assert df.index.is_monotonic_increasing  # Check for correct sorting
    assert list(df.columns) == ['open', 'high', 'low', 'close', 'volume']
    
    # Verify data types
    assert isinstance(df.index, pd.DatetimeIndex)
    for col in ['open', 'high', 'low', 'close', 'volume']:
        assert pd.api.types.is_numeric_dtype(df[col])
    
    # Clean up created files
    os.remove(raw_file)
    os.remove(processed_file)

def test_download_data_error_handling():
    """Test error handling in data download."""
    # Test with invalid symbol
    result = fetch_crypto_data(symbol='INVALID_PAIR', interval='60min', limit=100)
    assert result is None

def test_process_data_error_handling():
    """Test error handling in data processing."""
    # Test with non-existent file
    result = process_raw_data(symbol='NONEXISTENT')
    assert result is None 