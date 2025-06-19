import pandas as pd
import logging
from pathlib import Path

# --- Configuration ---
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# --- NEW: Function to process raw tick data ---
def process_raw_tick_data(symbol: str) -> str | None:
    """
    Cleans a raw CSV tick data file and saves it as a Parquet file.
    """
    try:
        # Look for the new raw tick data file.
        input_file = Path(f"data/raw_ticks_{symbol}.csv")
        if not input_file.exists():
            logger.error(f"Input file {input_file} not found. Please run download_data.py for this symbol first.")
            return None
            
        logger.info(f"Processing raw tick data from {input_file}...")
        
        # Read the raw tick data, setting the 'timestamp' column as the index.
        df = pd.read_csv(input_file, index_col='timestamp')
        
        # Ensure data columns have the correct numeric type.
        for col in ['price', 'volume']:
            df[col] = pd.to_numeric(df[col], errors='coerce')
        
        df.dropna(inplace=True)
        df.sort_index(inplace=True)
        
        # --- CHANGE: Save to a new Parquet file for ticks ---
        output_file = Path(f"data/ticks_{symbol}.parquet")
        df.to_parquet(output_file)
        
        logger.info(f"Successfully processed tick data for {symbol} and saved to {output_file}")
        return str(output_file)
        
    except Exception as e:
        logger.error(f"An unexpected error occurred while processing tick data for {symbol}: {str(e)}")
        return None

if __name__ == "__main__":
    symbols_to_process = ["SPY", "AAPL", "GOOG", "MSFT"]
    
    logger.info("Starting tick data processing for all symbols...")
    
    for symbol in symbols_to_process:
        # --- Call the new function to process tick data ---
        process_raw_tick_data(symbol=symbol)
        
    logger.info("All tick data processing attempted.")