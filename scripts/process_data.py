# In scripts/process_data.py

import pandas as pd
import logging
from pathlib import Path

# --- Configuration ---
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

def process_raw_data(symbol: str = 'SPY') -> str | None:
    """
    Cleans raw data from the 'data' directory and saves it as a Parquet file.
    """
    try:
        # --- CHANGE: Look for the raw file inside the 'data' directory ---
        input_file = Path(f"data/raw_{symbol}.csv")
        if not input_file.exists():
            logger.error(f"Input file {input_file} not found. Please run download_data.py first.")
            return None
            
        logger.info(f"Processing data from {input_file}")
        
        # Read the raw data
        df = pd.read_csv(input_file, index_col=0, parse_dates=True)
        
        # Rename the index to 'timestamp' to match C++ expectations
        df.index.name = 'timestamp'

        # Ensure columns are numeric
        for col in ['open', 'high', 'low', 'close', 'volume']:
            df[col] = pd.to_numeric(df[col])
        
        df.dropna(inplace=True)
        df.sort_index(inplace=True)
        
        # --- CHANGE: Save the final parquet file to the correct path and name ---
        output_file = Path(f"data/{symbol}.parquet")
        df.to_parquet(output_file)
        
        logger.info(f"Successfully processed data and saved to {output_file}")
        return str(output_file)
        
    except Exception as e:
        logger.error(f"Error processing data: {str(e)}")
        return None

if __name__ == "__main__":
    # --- CHANGE: Process the 'SPY' data ---
    process_raw_data(symbol='SPY')