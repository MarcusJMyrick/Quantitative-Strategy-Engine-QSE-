import requests
import pandas as pd
from typing import Optional
import logging
import os
from datetime import datetime, timedelta

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# You can get a free API key from https://www.alphavantage.co/support/#api-key
ALPHA_VANTAGE_API_KEY = os.getenv('ALPHA_VANTAGE_API_KEY', 'demo')

def fetch_crypto_data(symbol: str = 'BTCUSD', interval: str = '60min', limit: int = 100) -> Optional[str]:
    """
    Fetches historical crypto data from Alpha Vantage and saves it.
    
    Args:
        symbol (str): Trading pair symbol (e.g., 'BTCUSD')
        interval (str): Time interval (e.g., '60min', 'daily')
        limit (int): Number of records to fetch
    
    Returns:
        str: Path to the saved raw data file, or None if failed
    """
    try:
        # Alpha Vantage uses different interval formats
        interval_map = {
            '1h': '60min',
            '1d': 'daily',
            '1m': '1min'
        }
        interval = interval_map.get(interval, interval)
        
        url = f"https://www.alphavantage.co/query?function=CRYPTO_INTRADAY&symbol={symbol}&market=USD&interval={interval}&apikey={ALPHA_VANTAGE_API_KEY}&outputsize=compact"
        logger.info(f"Fetching data from {url}")
        
        response = requests.get(url)
        response.raise_for_status()
        
        data = response.json()
        
        if 'Error Message' in data:
            logger.error(f"API Error: {data['Error Message']}")
            return None
            
        if 'Time Series Crypto' not in data:
            logger.error(f"Unexpected API response format: {data}")
            return None
            
        # Convert to DataFrame
        time_series = data['Time Series Crypto']
        df = pd.DataFrame.from_dict(time_series, orient='index')
        
        # Rename columns
        df.columns = ['open', 'high', 'low', 'close', 'volume']
        
        # Convert index to datetime
        df.index = pd.to_datetime(df.index)
        
        # Sort by timestamp
        df.sort_index(inplace=True)
        
        # Save raw data
        output_file = f"raw_{symbol}_data.csv"
        df.to_csv(output_file)
        logger.info(f"Successfully downloaded raw data for {symbol} to {output_file}")
        
        return output_file
        
    except requests.exceptions.RequestException as e:
        logger.error(f"Error fetching data: {str(e)}")
        return None
    except Exception as e:
        logger.error(f"Unexpected error: {str(e)}")
        return None

if __name__ == "__main__":
    # Example usage
    fetch_crypto_data(symbol='BTCUSD', interval='60min', limit=100) 