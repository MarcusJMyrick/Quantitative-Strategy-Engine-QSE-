#!/bin/bash

# QSE Organize and Analyze Script
# This script organizes trading results and optionally runs analysis

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
RESULTS_DIR="results"
PLOTS_DIR="plots"
OUTPUT_DIR="organized_runs"
RUN_NAME=""
CLEAN=false
ANALYZE=false

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -r, --results-dir DIR    Directory containing raw results (default: results)"
    echo "  -p, --plots-dir DIR      Directory containing plot files (default: plots)"
    echo "  -o, --output-dir DIR     Base directory for organized runs (default: organized_runs)"
    echo "  -n, --name NAME          Name for this run (default: timestamp)"
    echo "  -c, --clean              Clean up original files after organizing"
    echo "  -a, --analyze            Run analysis on organized results"
    echo "  -h, --help               Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                                    # Basic organization with timestamp"
    echo "  $0 -n my_strategy_test               # Named run"
    echo "  $0 -c -a                             # Clean and analyze"
    echo "  $0 -r build/results -o my_runs       # Custom directories"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -r|--results-dir)
            RESULTS_DIR="$2"
            shift 2
            ;;
        -p|--plots-dir)
            PLOTS_DIR="$2"
            shift 2
            ;;
        -o|--output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -n|--name)
            RUN_NAME="$2"
            shift 2
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -a|--analyze)
            ANALYZE=true
            shift
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Main script
echo "🚀 QSE Organize and Analyze Script"
echo "=================================="

# Check if results directory exists
if [[ ! -d "$RESULTS_DIR" ]]; then
    print_error "Results directory '$RESULTS_DIR' not found!"
    print_warning "Make sure you've run the trading engine first."
    exit 1
fi

# Check if there are any CSV files to organize
CSV_COUNT=$(find "$RESULTS_DIR" -name "*.csv" | wc -l)
if [[ $CSV_COUNT -eq 0 ]]; then
    print_warning "No CSV files found in '$RESULTS_DIR'"
    print_warning "The trading engine may not have generated any results yet."
fi

# Build the organizer command
ORGANIZER_CMD="python3 scripts/organize_results.py"
ORGANIZER_CMD="$ORGANIZER_CMD --results-dir $RESULTS_DIR"
ORGANIZER_CMD="$ORGANIZER_CMD --plots-dir $PLOTS_DIR"
ORGANIZER_CMD="$ORGANIZER_CMD --output-dir $OUTPUT_DIR"

if [[ -n "$RUN_NAME" ]]; then
    ORGANIZER_CMD="$ORGANIZER_CMD --run-name $RUN_NAME"
fi

if [[ "$CLEAN" == true ]]; then
    ORGANIZER_CMD="$ORGANIZER_CMD --clean"
    print_warning "Will clean up original files after organizing!"
fi

# Run the organizer
print_status "Running organizer..."
print_status "Command: $ORGANIZER_CMD"
echo ""

eval $ORGANIZER_CMD

if [[ $? -eq 0 ]]; then
    print_success "Organization completed successfully!"
else
    print_error "Organization failed!"
    exit 1
fi

# Run analysis if requested
if [[ "$ANALYZE" == true ]]; then
    echo ""
    print_status "Running analysis on organized results..."
    
    # Find the most recent organized run
    if [[ -n "$RUN_NAME" ]]; then
        ORGANIZED_RUN="$OUTPUT_DIR/$RUN_NAME"
    else
        # Find the most recent timestamped directory
        ORGANIZED_RUN=$(find "$OUTPUT_DIR" -maxdepth 1 -type d -name "*_*_*" | sort | tail -n 1)
    fi
    
    if [[ -z "$ORGANIZED_RUN" || ! -d "$ORGANIZED_RUN" ]]; then
        print_error "Could not find organized run directory!"
        exit 1
    fi
    
    print_status "Analyzing: $ORGANIZED_RUN"
    
    # Run analysis on the organized data
    ANALYSIS_CMD="python3 scripts/analysis/analyze.py --data-dir $ORGANIZED_RUN/data"
    print_status "Analysis command: $ANALYSIS_CMD"
    echo ""
    
    eval $ANALYSIS_CMD
    
    if [[ $? -eq 0 ]]; then
        print_success "Analysis completed successfully!"
    else
        print_error "Analysis failed!"
        exit 1
    fi
fi

echo ""
print_success "All operations completed!"
print_status "Check the organized results in: $OUTPUT_DIR" 