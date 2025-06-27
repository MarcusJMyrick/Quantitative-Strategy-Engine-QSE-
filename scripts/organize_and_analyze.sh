#!/bin/bash

# QSE Organize and Analyze Script
# Organizes results into proper directory structure and runs analysis

set -e

echo "📊 QSE Organize and Analyze"
echo "============================"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Find the most recent timestamp
latest_timestamp=$(ls results/ | grep -oE '[0-9]{8}_[0-9]{6}_[0-9]+' | sort | tail -1)

if [ -z "$latest_timestamp" ]; then
    echo "❌ No recent results found"
    exit 1
fi

print_status "Organizing results for timestamp: $latest_timestamp"

# Create organized directory structure
organized_dir="results/${latest_timestamp}"
equity_dir="${organized_dir}/equity_logs"
tradelog_dir="${organized_dir}/trade_logs"

mkdir -p "$equity_dir"
mkdir -p "$tradelog_dir"

print_status "Created directory structure: $organized_dir"

# Move equity files
print_status "Organizing equity files..."
for file in results/equity_*_${latest_timestamp}.csv; do
    if [ -f "$file" ]; then
        filename=$(basename "$file")
        # Remove timestamp suffix for organized naming
        new_name=$(echo "$filename" | sed "s/_${latest_timestamp}.csv/.csv/")
        cp "$file" "${equity_dir}/${new_name}"
        print_status "  Moved: $filename -> equity_logs/${new_name}"
    fi
done

# Move tradelog files
print_status "Organizing tradelog files..."
for file in results/tradelog_*_${latest_timestamp}.csv; do
    if [ -f "$file" ]; then
        filename=$(basename "$file")
        # Remove timestamp suffix for organized naming
        new_name=$(echo "$filename" | sed "s/_${latest_timestamp}.csv/.csv/")
        cp "$file" "${tradelog_dir}/${new_name}"
        print_status "  Moved: $filename -> trade_logs/${new_name}"
    fi
done

print_success "Results organized in: $organized_dir"

# Run analysis
print_status "Running analysis..."
python3 scripts/analysis/analyze_multi_strategy.py "$organized_dir"

print_success "Analysis complete!"
print_status "Check $organized_dir/plots/ for generated charts" 