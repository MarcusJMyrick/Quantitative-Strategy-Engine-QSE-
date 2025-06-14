import pandas as pd
import logging
from typing import Optional
from pathlib import Path

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

def process_raw_data(symbol: str = 'BTCUSD') -> Optional[str]:
    """
    Cleans raw data and saves it as a Parquet file.
    
    Args:
        symbol (str): Trading pair symbol (e.g., 'BTCUSD')
    
    Returns:
        str: Path to the processed parquet file, or None if failed
    """
    try:
        input_file = f"raw_{symbol}_data.csv"
        if not Path(input_file).exists():
            logger.error(f"Input file {input_file} not found")
            return None
            
        logger.info(f"Processing data from {input_file}")
        
        # Read the raw data
        df = pd.read_csv(input_file, index_col=0, parse_dates=True)
        
        # Convert numeric columns to appropriate types
        for col in ['open', 'high', 'low', 'close', 'volume']:
            df[col] = pd.to_numeric(df[col])
        
        # Drop rows with any missing values
        df.dropna(inplace=True)
        
        # Sort by timestamp
        df.sort_index(inplace=True)
        
        # Save to Parquet
        output_file = f"processed_{symbol}_data.parquet"
        df.to_parquet(output_file)
        logger.info(f"Successfully processed data and saved to {output_file}")
        
        return output_file
        
    except Exception as e:
        logger.error(f"Error processing data: {str(e)}")
        return None

if __name__ == "__main__":
    # Example usage
    process_raw_data() 