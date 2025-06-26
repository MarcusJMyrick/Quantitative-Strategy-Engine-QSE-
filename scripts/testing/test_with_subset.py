#!/usr/bin/env python3
"""
Create a smaller test data subset for faster testing
"""

import csv
import os
from pathlib import Path

def create_test_subset(input_file, output_file, max_ticks=1000):
    """Create a smaller subset of tick data for testing."""
    if not os.path.exists(input_file):
        print(f"❌ Input file not found: {input_file}")
        return False
    
    print(f"📝 Creating test subset: {os.path.basename(input_file)} -> {os.path.basename(output_file)}")
    
    with open(input_file, 'r') as infile, open(output_file, 'w', newline='') as outfile:
        reader = csv.DictReader(infile)
        writer = csv.DictWriter(outfile, fieldnames=reader.fieldnames)
        
        writer.writeheader()
        
        tick_count = 0
        for row in reader:
            writer.writerow(row)
            tick_count += 1
            if tick_count >= max_ticks:
                break
    
    print(f"✅ Created subset with {tick_count} ticks")
    return True

def main():
    """Create test subsets for all tick files."""
    print("🔧 Creating test data subsets for faster testing...")
    print()
    
    data_dir = Path("data")
    test_dir = Path("test_data")
    test_dir.mkdir(exist_ok=True)
    
    # Create subsets with 1000 ticks each (much faster for testing)
    tick_files = list(data_dir.glob("raw_ticks_*.csv"))
    
    for filepath in tick_files:
        output_file = test_dir / f"test_{filepath.name}"
        create_test_subset(filepath, output_file, max_ticks=1000)
    
    print()
    print("📊 Test Data Summary:")
    print("-" * 40)
    print(f"Original data: {len(tick_files)} files")
    print(f"Test subsets: {len(list(test_dir.glob('test_*.csv')))} files")
    print(f"Ticks per subset: 1,000")
    print(f"Total test ticks: {len(tick_files) * 1000:,}")
    print()
    
    # Estimate runtime for test data
    total_test_ticks = len(tick_files) * 1000
    estimated_seconds = total_test_ticks / 1000  # 1000 ticks/sec processing
    estimated_seconds = max(estimated_seconds, 10)  # Minimum 10 seconds
    
    print("⏱️  Estimated Runtime for Test Data:")
    print("-" * 40)
    print(f"Conservative: {estimated_seconds:.1f} seconds")
    print(f"Optimistic: {estimated_seconds * 0.5:.1f} seconds")
    print()
    
    print("🚀 To run with test data:")
    print("   ./scripts/ultra_quick_build.sh test_data/test_raw_ticks_SPY.csv")

if __name__ == "__main__":
    main() 