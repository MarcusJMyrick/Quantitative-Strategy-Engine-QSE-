# In scripts/download_data.py

import requests
import pandas as pd
import logging
import os
from pathlib import Path
from dotenv import load_dotenv
import time

# --- Configuration ---
load_dotenv() 

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

ALPHA_VANTAGE_API_KEY = os.getenv('ALPHA_VANTAGE_API_KEY')
if not ALPHA_VANTAGE_API_KEY or ALPHA_VANTAGE_API_KEY == 'your_api_key_here':
    logger.warning("ALPHA_VANTAGE_API_KEY not set. Using 'demo' key, which is highly limited.")
    ALPHA_VANTAGE_API_KEY = 'demo'

# --- Function to fetch stock data with retry logic ---
def fetch_stock_data(symbol: str, max_retries: int = 3) -> str | None:
    """
    Fetches historical daily stock data from Alpha Vantage with retry logic.
    """
    output_file = Path(f"data/raw_{symbol}.csv")
    Path("data").mkdir(exist_ok=True)
    
    url = (f"https://www.alphavantage.co/query?function=TIME_SERIES_DAILY"
           f"&symbol={symbol}&outputsize=full&apikey={ALPHA_VANTAGE_API_KEY}")
    
    for attempt in range(max_retries):
        logger.info(f"Fetching daily data for {symbol} (Attempt {attempt + 1}/{max_retries})...")
        try:
            response = requests.get(url, timeout=30) # Add a timeout
            response.raise_for_status()
            data = response.json()

            if 'Time Series (Daily)' in data:
                df = pd.DataFrame.from_dict(data['Time Series (Daily)'], orient='index')
                df.rename(columns={
                    '1. open': 'open', '2. high': 'high', '3. low': 'low', 
                    '4. close': 'close', '5. volume': 'volume'
                }, inplace=True)
                
                df.index = pd.to_datetime(df.index)
                df.sort_index(inplace=True)
                for col in df.columns:
                    df[col] = pd.to_numeric(df[col])

                df.to_csv(output_file)
                logger.info(f"SUCCESS: Downloaded raw data for {symbol} to {output_file}")
                return str(output_file) # Exit function on success
            
            # If we are here, the API returned a message, likely a rate limit warning
            api_message = data.get('Information', str(data))
            logger.warning(f"API call for {symbol} failed: {api_message}. Retrying after a longer wait...")
            time.sleep(60) # Wait a full minute if rate-limited

        except requests.exceptions.RequestException as e:
            logger.error(f"Network error for {symbol} on attempt {attempt + 1}: {str(e)}")
            time.sleep(20) # Wait before retrying on network error
    
    logger.error(f"FAILED: Could not download data for {symbol} after {max_retries} attempts.")
    return None

if __name__ == "__main__":
    symbols_to_download = ["SPY", "AAPL", "GOOG", "MSFT"]
    
    for symbol in symbols_to_download:
        fetch_stock_data(symbol=symbol)
        
        # Always wait between calls to be respectful of the free API limits
        if symbol != symbols_to_download[-1]:
             logger.info("Waiting 15 seconds before next API call...")
             time.sleep(15)

    logger.info("All data downloads attempted.")