#!/bin/bash
# QSE Script Runner
# Usage: ./scripts/run.sh <category> <script_name>

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

case "$1" in
    "build")
        case "$2" in
            "quick") bash "$SCRIPT_DIR/build/quick_build.sh" ;;
            "ultra") bash "$SCRIPT_DIR/build/ultra_quick_build.sh" ;;
            "full") bash "$SCRIPT_DIR/build/build.sh" ;;
            *) echo "Available build scripts: quick, ultra, full" ;;
        esac
        ;;
    "analyze")
        case "$2" in
            "multi") python3 "$SCRIPT_DIR/analysis/analyze_multi_strategy.py" ;;
            "single") python3 "$SCRIPT_DIR/analysis/analyze.py" ;;
            "plot") python3 "$SCRIPT_DIR/analysis/plot_current_run.py" ;;
            *) echo "Available analysis scripts: multi, single, plot" ;;
        esac
        ;;
    "data")
        case "$2" in
            "download") python3 "$SCRIPT_DIR/data/download_data.py" ;;
            "process") python3 "$SCRIPT_DIR/data/process_data.py" ;;
            "organize") python3 "$SCRIPT_DIR/data/organize_results.py" ;;
            *) echo "Available data scripts: download, process, organize" ;;
        esac
        ;;
    "test")
        case "$2" in
            "quick") bash "$SCRIPT_DIR/testing/test.sh" ;;
            "subset") python3 "$SCRIPT_DIR/testing/test_with_subset.py" ;;
            "perf") bash "$SCRIPT_DIR/testing/performance_test.sh" ;;
            *) echo "Available test scripts: quick, subset, perf" ;;
        esac
        ;;
    "run")
        case "$2" in
            "multi") bash "$SCRIPT_DIR/execution/run_multi_symbol.sh" ;;
            "dev") bash "$SCRIPT_DIR/execution/dev_mode.sh" ;;
            *) echo "Available execution scripts: multi, dev" ;;
        esac
        ;;
    *)
        echo "QSE Script Runner"
        echo "Usage: $0 <category> <script_name>"
        echo ""
        echo "Categories:"
        echo "  build     - Build scripts (quick, ultra, full)"
        echo "  analyze   - Analysis scripts (multi, single, plot)"
        echo "  data      - Data management (download, process, organize)"
        echo "  test      - Testing scripts (quick, subset, perf)"
        echo "  run       - Execution scripts (multi, dev)"
        ;;
esac
