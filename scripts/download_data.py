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

# --- NEW: Function to fetch high-frequency (tick) data ---
def fetch_tick_data(symbol: str, max_retries: int = 3) -> str | None:
    """
    Fetches 1-minute intraday data and saves it in a tick format (timestamp,price,volume).
    """
    # Save to a new file to distinguish from daily bars.
    output_file = Path(f"data/raw_ticks_{symbol}.csv")
    Path("data").mkdir(exist_ok=True)
    
    # --- CHANGE: Use TIME_SERIES_INTRADAY endpoint for 1-minute data ---
    # Note: 'outputsize=full' gets up to ~2 weeks of 1-min data with a paid key.
    # The 'demo' key is limited to the most recent day.
    url = (f"https://www.alphavantage.co/query?function=TIME_SERIES_INTRADAY"
           f"&symbol={symbol}&interval=1min&outputsize=full&apikey={ALPHA_VANTAGE_API_KEY}")
    
    for attempt in range(max_retries):
        logger.info(f"Fetching 1-min intraday data for {symbol} (Attempt {attempt + 1}/{max_retries})...")
        try:
            response = requests.get(url, timeout=30)
            response.raise_for_status()
            data = response.json()

            # The key in the JSON response is different for intraday data
            data_key = 'Time Series (1min)'
            if data_key in data:
                df = pd.DataFrame.from_dict(data[data_key], orient='index')
                df.rename(columns={
                    '1. open': 'open', '2. high': 'high', '3. low': 'low', 
                    '4. close': 'close', '5. volume': 'volume'
                }, inplace=True)
                
                df.index = pd.to_datetime(df.index)
                df.sort_index(inplace=True)
                for col in df.columns:
                    df[col] = pd.to_numeric(df[col])

                # --- CHANGE: Convert to the C++ engine's expected tick format ---
                # We use the 'close' price for the interval as the tick 'price'.
                tick_df = pd.DataFrame()
                tick_df['price'] = df['close']
                tick_df['volume'] = df['volume']
                
                # Convert the datetime index to a Unix timestamp (integer seconds)
                tick_df.index = (df.index.astype(int) // 10**9)
                tick_df.index.name = 'timestamp' # Name the index column

                tick_df.to_csv(output_file)
                logger.info(f"SUCCESS: Downloaded raw tick data for {symbol} to {output_file}")
                return str(output_file)
            
            api_message = data.get('Information', str(data))
            logger.warning(f"API call for {symbol} failed: {api_message}. Retrying after a longer wait...")
            time.sleep(60)

        except requests.exceptions.RequestException as e:
            logger.error(f"Network error for {symbol} on attempt {attempt + 1}: {str(e)}")
            time.sleep(20)
    
    logger.error(f"FAILED: Could not download tick data for {symbol} after {max_retries} attempts.")
    return None

if __name__ == "__main__":
    symbols_to_download = ["SPY", "AAPL", "GOOG", "MSFT"]
    
    for symbol in symbols_to_download:
        # --- Call the new function to get tick data ---
        fetch_tick_data(symbol=symbol)
        
        # Respect the API rate limits
        if symbol != symbols_to_download[-1]:
             logger.info("Waiting 15 seconds before next API call...")
             time.sleep(15)

    logger.info("All tick data downloads attempted.")