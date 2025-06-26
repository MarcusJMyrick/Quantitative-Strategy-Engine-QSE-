#!/usr/bin/env python3
"""
Runtime estimation script for QSE
Analyzes data files and estimates processing time before running the engine.
"""

import os
import csv
import time
from pathlib import Path

def analyze_data_file(filepath):
    """Analyze a tick data file and return statistics."""
    if not os.path.exists(filepath):
        return None
    
    tick_count = 0
    start_time = None
    end_time = None
    
    with open(filepath, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            tick_count += 1
            timestamp = int(row['timestamp'])
            
            if start_time is None:
                start_time = timestamp
            end_time = timestamp
    
    if tick_count == 0:
        return None
    
    duration_seconds = end_time - start_time
    duration_days = duration_seconds / (24 * 3600)
    
    return {
        'file': os.path.basename(filepath),
        'tick_count': tick_count,
        'start_time': start_time,
        'end_time': end_time,
        'duration_seconds': duration_seconds,
        'duration_days': duration_days,
        'ticks_per_second': tick_count / duration_seconds if duration_seconds > 0 else 0
    }

def estimate_runtime():
    """Estimate runtime based on data analysis."""
    print("🔍 Analyzing data files for runtime estimation...")
    print()
    
    # Analyze all tick data files
    data_dir = Path("data")
    tick_files = list(data_dir.glob("raw_ticks_*.csv"))
    
    if not tick_files:
        print("❌ No tick data files found in data/ directory")
        return
    
    total_ticks = 0
    total_duration_days = 0
    
    print("📊 Data File Analysis:")
    print("-" * 60)
    
    for filepath in tick_files:
        stats = analyze_data_file(filepath)
        if stats:
            print(f"📁 {stats['file']}:")
            print(f"   Ticks: {stats['tick_count']:,}")
            print(f"   Duration: {stats['duration_days']:.1f} days")
            print(f"   Rate: {stats['ticks_per_second']:.1f} ticks/sec")
            print()
            
            total_ticks += stats['tick_count']
            total_duration_days = max(total_duration_days, stats['duration_days'])
    
    print("📈 Summary:")
    print("-" * 60)
    print(f"Total Ticks: {total_ticks:,}")
    print(f"Data Span: {total_duration_days:.1f} days")
    print(f"Files: {len(tick_files)}")
    print()
    
    # Runtime estimation based on processing speed
    # Conservative estimates based on typical performance
    ticks_per_second_processing = 1000  # Conservative estimate
    estimated_seconds = total_ticks / ticks_per_second_processing
    estimated_minutes = estimated_seconds / 60
    
    print("⏱️  Runtime Estimates:")
    print("-" * 60)
    print(f"Conservative: {estimated_minutes:.1f} minutes")
    print(f"Optimistic: {estimated_minutes * 0.5:.1f} minutes")
    print(f"Pessimistic: {estimated_minutes * 2:.1f} minutes")
    print()
    
    # Recommendations
    print("💡 Recommendations:")
    print("-" * 60)
    if estimated_minutes > 2:
        print("⚠️  Long runtime expected. Consider:")
        print("   • Using a smaller data subset for testing")
        print("   • Running with progress monitoring")
        print("   • Using the ultra_quick_build.sh script")
    else:
        print("✅ Reasonable runtime expected")
    
    print()
    print("🚀 Ready to proceed? Run:")
    print("   ./scripts/ultra_quick_build.sh")

if __name__ == "__main__":
    estimate_runtime() 