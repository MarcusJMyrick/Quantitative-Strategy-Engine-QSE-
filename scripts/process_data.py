import pandas as pd
import logging
from pathlib import Path

# --- Configuration ---
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

def process_raw_data(symbol: str) -> str | None:
    """
    Cleans a raw CSV data file from the 'data' directory and saves it as a 
    more efficient Parquet file.
    """
    try:
        input_file = Path(f"data/raw_{symbol}.csv")
        if not input_file.exists():
            logger.error(f"Input file {input_file} not found. Please run download_data.py for this symbol first.")
            return None
            
        logger.info(f"Processing raw data from {input_file}...")
        
        # Read the raw data, setting the first column as the index and parsing dates.
        df = pd.read_csv(input_file, index_col=0, parse_dates=True)
        
        # Rename the index to 'timestamp' to match C++ expectations.
        df.index.name = 'timestamp'

        # Ensure all data columns have the correct numeric type.
        for col in ['open', 'high', 'low', 'close', 'volume']:
            df[col] = pd.to_numeric(df[col], errors='coerce') # 'coerce' will turn non-numeric values into NaN
        
        # Drop any rows with missing data and sort by date.
        df.dropna(inplace=True)
        df.sort_index(inplace=True)
        
        # Define the output path for the processed Parquet file.
        output_file = Path(f"data/{symbol}.parquet")
        df.to_parquet(output_file)
        
        logger.info(f"Successfully processed data for {symbol} and saved to {output_file}")
        return str(output_file)
        
    except Exception as e:
        logger.error(f"An unexpected error occurred while processing data for {symbol}: {str(e)}")
        return None

if __name__ == "__main__":
    # --- Process data for all symbols required by the C++ application ---
    symbols_to_process = ["SPY", "AAPL", "GOOG", "MSFT"]
    
    logger.info("Starting data processing for all symbols...")
    
    for symbol in symbols_to_process:
        process_raw_data(symbol=symbol)
        
    logger.info("All data processing attempted.")