import pandas as pd
import logging
from pathlib import Path

from corporate_actions import adjust_for_corporate_actions, load_actions

# --- Configuration ---
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

ACTIONS_FILE = Path("config/corporate_actions.csv")


def forward_fill_ticks(df: pd.DataFrame) -> tuple[pd.DataFrame, dict]:
    """Clean a tick DataFrame (price/volume, timestamp index) without
    silently discarding data.

    Missing prices are forward-filled from the last valid observation and
    missing volumes become 0 (a filled row represents "no trade printed", not
    a trade of unknown size). Rows before the first valid price cannot be
    filled and are dropped. Returns (clean_df, report) where the report
    counts every repair so gaps are surfaced instead of hidden:

        prices_filled, volumes_filled, leading_rows_dropped, grid_gaps

    grid_gaps counts missing rows in the timestamp grid, inferred from the
    median spacing (exact for grid data, heuristic for event-time ticks).
    """
    df = df.copy()
    for col in ("price", "volume"):
        df[col] = pd.to_numeric(df[col], errors="coerce")
    df.sort_index(inplace=True)

    # Rows before the first valid price have nothing to fill from
    valid_started = df["price"].notna().cummax()
    leading_dropped = int((~valid_started).sum())
    df = df[valid_started]

    prices_filled = int(df["price"].isna().sum())
    volumes_filled = int(df["volume"].isna().sum())
    df["price"] = df["price"].ffill()
    df["volume"] = df["volume"].fillna(0)

    grid_gaps = 0
    deltas = pd.Series(df.index).diff().dropna()
    deltas = deltas[deltas > 0]
    if len(deltas) >= 2:
        expected = deltas.median()
        if expected > 0:
            spacings = (deltas / expected).round()
            grid_gaps = int((spacings[spacings > 1] - 1).sum())

    report = {
        "prices_filled": prices_filled,
        "volumes_filled": volumes_filled,
        "leading_rows_dropped": leading_dropped,
        "grid_gaps": grid_gaps,
    }
    return df, report


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
        
        # Repair missing values instead of silently dropping rows, and log
        # what was repaired so data problems stay visible
        df, report = forward_fill_ticks(df)
        if any(report.values()):
            logger.warning(f"Data quality for {symbol}: {report}")

        # Back-adjust for splits/dividends so returns across event dates
        # reflect economics, not bookkeeping
        if ACTIONS_FILE.exists():
            actions = load_actions(ACTIONS_FILE)
            df, ca_report = adjust_for_corporate_actions(df, actions, symbol)
            for event in ca_report:
                logger.info(f"Corporate action applied: {event}")

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