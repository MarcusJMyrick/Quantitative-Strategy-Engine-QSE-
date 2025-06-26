# Data Management Scripts

Scripts for downloading, processing, and organizing data.

## Scripts

### `download_data.py`
**Download market data**
- Downloads tick data from various sources
- Supports multiple symbols and time periods
- Handles data format conversion

### `process_data.py`
**Process raw data**
- Converts raw data to required formats
- Handles data cleaning and validation
- Generates processed datasets

### `organize_results.py`
**Organize backtest results**
- Moves scattered CSV files into timestamped directories
- Creates organized structure: `results/YYYYMMDD_HHMMSS_MMM/`
- Separates equity logs and trade logs

### `organize_plots.py`
**Organize generated plots**
- Moves scattered PNG files into timestamped directories
- Creates organized structure: `plots/YYYYMMDD_HHMMSS_MMM/`
- Handles files without timestamps

## Usage

```bash
# Download new data
./scripts/run.sh data download

# Process data
./scripts/run.sh data process

# Organize results
./scripts/run.sh data organize
```

## Data Structure

### Results Organization
```
results/
├── 20250625_210221_912/
│   ├── equity_logs/
│   │   ├── equity_AAPL_SMA_Crossover.csv
│   │   └── equity_GOOG_FillTracking.csv
│   └── trade_logs/
│       ├── tradelog_AAPL_SMA_Crossover.csv
│       └── tradelog_GOOG_FillTracking.csv
└── ...
```

### Plots Organization
```
plots/
├── misc/                    # Files without timestamps
├── 20250625_210248_621/     # Timestamped plot groups
└── ...
```

## Dependencies

- pandas
- yfinance (for data download)
- pathlib 