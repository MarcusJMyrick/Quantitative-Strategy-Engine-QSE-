#!/usr/bin/env python3
"""
Script to organize scattered results files into timestamped directories.
"""

import os
import re
import shutil
from pathlib import Path
from collections import defaultdict

def extract_timestamp(filename):
    """Extract timestamp from filename like 'equity_AAPL_SMA_Crossover_20250625_212602_704.csv'"""
    match = re.search(r'_(\d{8}_\d{6}_\d{3})\.csv$', filename)
    return match.group(1) if match else None

def organize_results():
    """Organize scattered results files into timestamped directories."""
    results_dir = Path("results")
    
    if not results_dir.exists():
        print("Results directory does not exist.")
        return
    
    # Group files by timestamp
    timestamp_groups = defaultdict(list)
    
    # Find all CSV files in results directory (not in subdirectories)
    for file_path in results_dir.glob("*.csv"):
        if file_path.is_file():
            timestamp = extract_timestamp(file_path.name)
            if timestamp:
                timestamp_groups[timestamp].append(file_path)
    
    print(f"Found {len(timestamp_groups)} timestamp groups:")
    
    for timestamp, files in timestamp_groups.items():
        print(f"  {timestamp}: {len(files)} files")
        
        # Create timestamped directory
        timestamp_dir = results_dir / timestamp
        equity_dir = timestamp_dir / "equity_logs"
        tradelog_dir = timestamp_dir / "trade_logs"
        
        # Create directories
        equity_dir.mkdir(parents=True, exist_ok=True)
        tradelog_dir.mkdir(parents=True, exist_ok=True)
        
        # Move files to appropriate subdirectories
        for file_path in files:
            if file_path.name.startswith("equity_"):
                dest = equity_dir / file_path.name
            elif file_path.name.startswith("tradelog_"):
                dest = tradelog_dir / file_path.name
            else:
                # For any other files, put them in the main timestamp directory
                dest = timestamp_dir / file_path.name
            
            print(f"    Moving {file_path.name} -> {dest}")
            shutil.move(str(file_path), str(dest))
    
    print(f"\nOrganization complete! Files moved to {len(timestamp_groups)} timestamped directories.")

if __name__ == "__main__":
    organize_results() 