import os
from unittest.mock import patch

import pandas as pd
import pytest

from src.download_data import fetch_crypto_data
from src.process_data import process_raw_data


def create_mock_data():
    """Create a simple mock DataFrame for testing."""
    dates = pd.date_range(start="2024-01-01", periods=100, freq="h")
    data = {
        "open": range(100),
        "high": range(100),
        "low": range(100),
        "close": range(100),
        "volume": range(100),
    }
    return pd.DataFrame(data, index=dates)


@patch("src.download_data.requests.get")
def test_data_pipeline(mock_get):
    """Test the complete data pipeline from download to processing."""

    # Prepare mock response
    mock_data = create_mock_data()
    time_series = {
        dt.strftime("%Y-%m-%d %H:%M:%S"): row.to_dict()
        for dt, row in mock_data.iterrows()
    }

    mock_response = type(
        "Response",
        (),
        {
            "json": lambda self: {"Time Series Crypto": time_series},
            "raise_for_status": lambda self: None,
        },
    )()
    mock_get.return_value = mock_response

    # 1. Test download
    symbol = "BTCUSD"
    raw_file = fetch_crypto_data(symbol=symbol, interval="60min", limit=100)
    assert raw_file is not None
    assert os.path.exists(raw_file)

    # Verify raw data format
    raw_df = pd.read_csv(raw_file, index_col=0, parse_dates=True)
    expected_columns = ["open", "high", "low", "close", "volume"]
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
    assert list(df.columns) == ["open", "high", "low", "close", "volume"]

    # Verify data types
    assert isinstance(df.index, pd.DatetimeIndex)
    for col in ["open", "high", "low", "close", "volume"]:
        assert pd.api.types.is_numeric_dtype(df[col])

    # Clean up created files
    os.remove(raw_file)
    os.remove(processed_file)


@patch("src.download_data.requests.get")
def test_download_data_error_handling(mock_get):
    """Test error handling in data download."""

    mock_get.side_effect = Exception("API error")
    result = fetch_crypto_data(
        symbol="INVALID_PAIR", interval="60min", limit=100
    )
    assert result is None


def test_process_data_error_handling():
    """Test error handling in data processing."""
    # Test with non-existent file
    result = process_raw_data(symbol="NONEXISTENT")
    assert result is None
