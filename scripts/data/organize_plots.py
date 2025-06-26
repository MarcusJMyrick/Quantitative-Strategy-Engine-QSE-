#!/usr/bin/env python3
"""
Script to organize scattered plot files into timestamped directories.
"""

import os
import re
import shutil
from pathlib import Path
from collections import defaultdict

def extract_timestamp(filename):
    """Extract timestamp from filename like 'best_strategies_summary_20250625_211237_186.png'"""
    match = re.search(r'_(\d{8}_\d{6}_\d{3})\.png$', filename)
    return match.group(1) if match else None

def organize_plots():
    """Organize scattered plot files into timestamped directories."""
    plots_dir = Path("plots")
    
    if not plots_dir.exists():
        print("Plots directory does not exist.")
        return
    
    # Group files by timestamp
    timestamp_groups = defaultdict(list)
    other_files = []
    
    # Find all PNG files in plots directory (not in subdirectories)
    for file_path in plots_dir.glob("*.png"):
        if file_path.is_file():
            timestamp = extract_timestamp(file_path.name)
            if timestamp:
                timestamp_groups[timestamp].append(file_path)
            else:
                other_files.append(file_path)
    
    print(f"Found {len(timestamp_groups)} timestamp groups:")
    
    for timestamp, files in timestamp_groups.items():
        print(f"  {timestamp}: {len(files)} files")
        
        # Create timestamped directory
        timestamp_dir = plots_dir / timestamp
        timestamp_dir.mkdir(parents=True, exist_ok=True)
        
        # Move files to timestamp directory
        for file_path in files:
            dest = timestamp_dir / file_path.name
            print(f"    Moving {file_path.name} -> {dest}")
            shutil.move(str(file_path), str(dest))
    
    # Handle files without timestamps
    if other_files:
        print(f"\nFound {len(other_files)} files without timestamps:")
        other_dir = plots_dir / "misc"
        other_dir.mkdir(parents=True, exist_ok=True)
        
        for file_path in other_files:
            dest = other_dir / file_path.name
            print(f"    Moving {file_path.name} -> {dest}")
            shutil.move(str(file_path), str(dest))
    
    print(f"\nOrganization complete! Files moved to {len(timestamp_groups)} timestamped directories.")

if __name__ == "__main__":
    organize_plots() 