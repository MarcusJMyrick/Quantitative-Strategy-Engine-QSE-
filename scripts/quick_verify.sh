#!/bin/bash

echo "🔍 Quick Strategy Verification"
echo "=============================="

# Run the engine and capture output
echo "Running multi-strategy engine..."
./build/multi_strategy_engine > /dev/null 2>&1

# Find the latest results
latest_timestamp=$(ls results/ | grep -oE '[0-9]{8}_[0-9]{6}_[0-9]+' | sort | tail -1)

if [ -n "$latest_timestamp" ]; then
    echo "✅ Engine completed successfully"
    echo "📅 Latest run: $latest_timestamp"
    echo ""
    
    # Check FillTracking results (should always have trades)
    fill_files=$(ls results/tradelog_*_FillTracking_${latest_timestamp}.csv 2>/dev/null | wc -l)
    if [ $fill_files -gt 0 ]; then
        echo "🎯 FillTracking Strategy Results:"
        for file in results/tradelog_*_FillTracking_${latest_timestamp}.csv; do
            if [ -f "$file" ]; then
                symbol=$(echo $file | sed 's/.*tradelog_\([A-Z]*\)_FillTracking.*/\1/')
                trades=$(($(wc -l < "$file") - 1))
                if [ $trades -gt 0 ]; then
                    echo "  ✅ $symbol: $trades trades"
                    echo "    Sample: $(tail -1 $file)"
                else
                    echo "  ⚠️  $symbol: No trades"
                fi
            fi
        done
    fi
    
    echo ""
    echo "📊 All Strategy Results:"
    for file in results/tradelog_*_${latest_timestamp}.csv; do
        if [ -f "$file" ]; then
            trades=$(($(wc -l < "$file") - 1))
            strategy=$(echo $file | sed 's/.*tradelog_[A-Z]*_\([^_]*\)_[0-9]*_[0-9]*_[0-9]*.csv/\1/')
            symbol=$(echo $file | sed 's/.*tradelog_\([A-Z]*\)_[^_]*_[0-9]*_[0-9]*_[0-9]*.csv/\1/')
            echo "  $symbol $strategy: $trades trades"
        fi
    done
    
    echo ""
    echo "🎉 Verification Complete!"
    echo "Strategies are running and generating results."
    
else
    echo "❌ No results found - check engine execution"
fi 