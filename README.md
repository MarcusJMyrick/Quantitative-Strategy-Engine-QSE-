# High-Frequency Trading (HFT) Backtesting and Simulation Engine

A comprehensive system for backtesting and simulating high-frequency trading strategies.

## Phase 1: Data Acquisition and Processing

This phase handles the downloading and processing of financial data for backtesting.

### Prerequisites

- Python 3.8+
- pip (Python package manager)

### Setup

1. Create and activate a virtual environment:
```bash
python -m venv venv
source venv/bin/activate  # On Windows, use `venv\Scripts\activate`
```

2. Install dependencies:
```bash
pip install -r requirements.txt
```

### Usage

1. Download raw data:
```bash
python -m src.download_data
```

2. Process the raw data:
```bash
python -m src.process_data
```

### Testing

Run the test suite:
```bash
pytest tests/
```

### Data Format

The system processes data in the following format:

- Raw data (CSV):
  - open_time: Timestamp in milliseconds
  - open: Opening price
  - high: Highest price
  - low: Lowest price
  - close: Closing price
  - volume: Trading volume
  - Additional metadata columns

- Processed data (Parquet):
  - open_time: Datetime index
  - open: Opening price
  - high: Highest price
  - low: Lowest price
  - close: Closing price
  - volume: Trading volume

### Error Handling

The system includes comprehensive error handling for:
- Network issues during data download
- Invalid data formats
- Missing or corrupted files
- Data processing errors

## License

This project is licensed under the MIT License - see the LICENSE file for details.
