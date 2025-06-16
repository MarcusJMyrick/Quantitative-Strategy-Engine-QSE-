# In scripts/download_data.py

import requests
import pandas as pd
import logging
import os
from pathlib import Path
from dotenv import load_dotenv
load_dotenv() # Loads environment variables from a .env file

# --- Configuration ---
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# Get API key from environment variable, with a fallback to 'demo'
ALPHA_VANTAGE_API_KEY = os.getenv('ALPHA_VANTAGE_API_KEY')
if not ALPHA_VANTAGE_API_KEY:
    logger.warning("ALPHA_VANTAGE_API_KEY not set. Using 'demo' key, which is highly limited.")
    ALPHA_VANTAGE_API_KEY = 'demo'

# --- New Function for Stocks ---
def fetch_stock_data(symbol: str = 'SPY') -> str | None:
    """
    Fetches historical daily stock data from Alpha Vantage and saves it as a CSV.
    --- USES THE FREE TIME_SERIES_DAILY ENDPOINT ---
    """
    Path("data").mkdir(exist_ok=True)
    output_file = Path(f"data/raw_{symbol}.csv")
    
    # --- CHANGE 1: Use the free endpoint ---
    url = (f"https://www.alphavantage.co/query?function=TIME_SERIES_DAILY"
           f"&symbol={symbol}&outputsize=full&apikey={ALPHA_VANTAGE_API_KEY}")
    
    logger.info(f"Fetching daily data for {symbol} using free endpoint...")
    
    try:
        response = requests.get(url)
        response.raise_for_status()
        data = response.json()

        if 'Error Message' in data or 'Time Series (Daily)' not in data:
            logger.error(f"API Error or unexpected format: {data}")
            return None
        
        df = pd.DataFrame.from_dict(data['Time Series (Daily)'], orient='index')
        
        # --- CHANGE 2: Clean the different column names from this endpoint ---
        df.rename(columns={
            '1. open': 'open',
            '2. high': 'high',
            '3. low': 'low',
            '4. close': 'close',
            '5. volume': 'volume'
        }, inplace=True)
        
        df.index = pd.to_datetime(df.index)
        df.sort_index(inplace=True)
        
        for col in ['open', 'high', 'low', 'close', 'volume']:
             df[col] = pd.to_numeric(df[col])

        # --- CHANGE 3: We only need these columns now ---
        df[['open', 'high', 'low', 'close', 'volume']].to_csv(output_file)
        
        logger.info(f"Successfully downloaded raw data for {symbol} to {output_file}")
        return str(output_file)

    except requests.exceptions.RequestException as e:
        logger.error(f"Error fetching data: {str(e)}")
        return None
    except Exception as e:
        logger.error(f"An unexpected error occurred: {str(e)}")
        return None


if __name__ == "__main__":
    # --- CHANGE: Call the new function for the 'SPY' symbol ---
    fetch_stock_data(symbol='SPY')