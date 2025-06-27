#!/usr/bin/env python3

import os
import shutil
import glob
from pathlib import Path
import argparse
from datetime import datetime
import json

def create_organized_structure(base_dir, run_name=None):
    """
    Create an organized directory structure for a trading run.
    
    Args:
        base_dir: Base directory for the organized structure
        run_name: Name for this run (if None, uses timestamp)
    """
    if run_name is None:
        run_name = datetime.now().strftime("%Y%m%d_%H%M%S")
    
    run_dir = Path(base_dir) / run_name
    run_dir.mkdir(parents=True, exist_ok=True)
    
    # Create subdirectories
    subdirs = [
        "data/equity_curves",
        "data/trade_logs", 
        "data/raw_data",
        "plots/strategy_comparisons",
        "plots/performance_heatmaps",
        "plots/individual_strategies",
        "analysis/reports",
        "analysis/summaries",
        "logs",
        "config"
    ]
    
    for subdir in subdirs:
        (run_dir / subdir).mkdir(parents=True, exist_ok=True)
    
    return run_dir

def organize_csv_files(results_dir, organized_dir):
    """
    Organize CSV files from results directory into the organized structure.
    """
    results_path = Path(results_dir)
    organized_path = Path(organized_dir)
    
    # Find all CSV files
    csv_files = list(results_path.glob("*.csv"))
    
    equity_files = []
    trade_files = []
    
    for csv_file in csv_files:
        filename = csv_file.name
        
        if filename.startswith("equity_"):
            equity_files.append(csv_file)
        elif filename.startswith("tradelog_"):
            trade_files.append(csv_file)
    
    # Move equity files
    for equity_file in equity_files:
        dest = organized_path / "data" / "equity_curves" / equity_file.name
        shutil.copy2(equity_file, dest)
        print(f"📊 Moved equity file: {equity_file.name}")
    
    # Move trade log files
    for trade_file in trade_files:
        dest = organized_path / "data" / "trade_logs" / trade_file.name
        shutil.copy2(trade_file, dest)
        print(f"📈 Moved trade log: {trade_file.name}")
    
    return len(equity_files), len(trade_files)

def organize_plots(plots_dir, organized_dir):
    """
    Organize plot files into the organized structure.
    """
    plots_path = Path(plots_dir)
    organized_path = Path(organized_dir)
    
    if not plots_path.exists():
        print("⚠️  No plots directory found")
        return 0
    
    # Find all plot files
    plot_files = list(plots_path.glob("*.png")) + list(plots_path.glob("*.jpg")) + list(plots_path.glob("*.pdf"))
    
    for plot_file in plot_files:
        filename = plot_file.name.lower()
        
        # Determine destination based on filename
        if "comparison" in filename or "strategy" in filename:
            dest = organized_path / "plots" / "strategy_comparisons" / plot_file.name
        elif "heatmap" in filename or "performance" in filename:
            dest = organized_path / "plots" / "performance_heatmaps" / plot_file.name
        else:
            dest = organized_path / "plots" / "individual_strategies" / plot_file.name
        
        shutil.copy2(plot_file, dest)
        print(f"📊 Moved plot: {plot_file.name}")
    
    return len(plot_files)

def copy_config_files(organized_dir):
    """
    Copy configuration files to the organized structure.
    """
    organized_path = Path(organized_dir)
    config_files = [
        "config.yaml",
        "config/slippage.yaml",
        "requirements.txt",
        "CMakeLists.txt"
    ]
    
    copied = 0
    for config_file in config_files:
        if Path(config_file).exists():
            dest = organized_path / "config" / Path(config_file).name
            shutil.copy2(config_file, dest)
            print(f"⚙️  Copied config: {config_file}")
            copied += 1
    
    return copied

def generate_run_summary(organized_dir, equity_count, trade_count, plot_count, config_count):
    """
    Generate a summary report for the organized run.
    """
    organized_path = Path(organized_dir)
    
    summary = {
        "run_timestamp": datetime.now().isoformat(),
        "files_organized": {
            "equity_curves": equity_count,
            "trade_logs": trade_count,
            "plots": plot_count,
            "config_files": config_count
        },
        "directory_structure": {
            "data/equity_curves": "Portfolio value over time for each strategy",
            "data/trade_logs": "Detailed trade execution logs",
            "data/raw_data": "Original input data files",
            "plots/strategy_comparisons": "Performance comparisons across strategies",
            "plots/performance_heatmaps": "Heatmaps showing performance metrics",
            "plots/individual_strategies": "Individual strategy performance plots",
            "analysis/reports": "Detailed analysis reports",
            "analysis/summaries": "Summary statistics and metrics",
            "logs": "Execution logs and error reports",
            "config": "Configuration files used for this run"
        }
    }
    
    # Save summary as JSON
    summary_file = organized_path / "analysis" / "summaries" / "run_summary.json"
    with open(summary_file, 'w') as f:
        json.dump(summary, f, indent=2)
    
    # Save human-readable summary
    summary_txt = organized_path / "analysis" / "summaries" / "run_summary.txt"
    with open(summary_txt, 'w') as f:
        f.write("=== QSE Run Summary ===\n")
        f.write(f"Generated: {summary['run_timestamp']}\n\n")
        f.write("Files Organized:\n")
        f.write(f"  - Equity curves: {equity_count}\n")
        f.write(f"  - Trade logs: {trade_count}\n")
        f.write(f"  - Plots: {plot_count}\n")
        f.write(f"  - Config files: {config_count}\n\n")
        f.write("Directory Structure:\n")
        for dir_path, description in summary['directory_structure'].items():
            f.write(f"  - {dir_path}: {description}\n")
    
    print(f"📋 Generated summary: {summary_file}")
    print(f"📋 Generated summary: {summary_txt}")

def create_readme(organized_dir):
    """
    Create a README file for the organized directory.
    """
    organized_path = Path(organized_dir)
    readme_content = """# QSE Trading Run Results

This directory contains the organized results from a Quantitative Strategy Engine (QSE) trading run.

## Directory Structure

### 📊 Data
- **`data/equity_curves/`**: Portfolio value over time for each strategy
- **`data/trade_logs/`**: Detailed trade execution logs  
- **`data/raw_data/`**: Original input data files

### 📈 Plots
- **`plots/strategy_comparisons/`**: Performance comparisons across strategies
- **`plots/performance_heatmaps/`**: Heatmaps showing performance metrics
- **`plots/individual_strategies/`**: Individual strategy performance plots

### 📋 Analysis
- **`analysis/reports/`**: Detailed analysis reports
- **`analysis/summaries/`**: Summary statistics and metrics

### 📝 Logs & Config
- **`logs/`**: Execution logs and error reports
- **`config/`**: Configuration files used for this run

## Quick Start

1. **View Summary**: Check `analysis/summaries/run_summary.txt` for an overview
2. **Analyze Performance**: Look at equity curves in `data/equity_curves/`
3. **Review Trades**: Examine trade logs in `data/trade_logs/`
4. **Visualize Results**: Browse plots in the `plots/` subdirectories

## File Naming Convention

- **Equity files**: `equity_[SYMBOL]_[STRATEGY]_[TIMESTAMP].csv`
- **Trade logs**: `tradelog_[SYMBOL]_[STRATEGY]_[TIMESTAMP].csv`
- **Plots**: Strategy-specific naming with timestamps

## Next Steps

- Run analysis scripts on the organized data
- Compare performance across different runs
- Archive this directory for future reference
"""
    
    readme_file = organized_path / "README.md"
    with open(readme_file, 'w') as f:
        f.write(readme_content)
    
    print(f"📖 Created README: {readme_file}")

def main():
    parser = argparse.ArgumentParser(description="Organize QSE trading results into a clean directory structure")
    parser.add_argument(
        '--results-dir', 
        default='results', 
        help='Directory containing raw results (default: results)'
    )
    parser.add_argument(
        '--plots-dir', 
        default='plots', 
        help='Directory containing plot files (default: plots)'
    )
    parser.add_argument(
        '--output-dir', 
        default='organized_runs', 
        help='Base directory for organized runs (default: organized_runs)'
    )
    parser.add_argument(
        '--run-name', 
        default=None, 
        help='Name for this run (default: timestamp)'
    )
    parser.add_argument(
        '--clean', 
        action='store_true', 
        help='Clean up original files after organizing (use with caution!)'
    )
    
    args = parser.parse_args()
    
    print("🚀 QSE Results Organizer")
    print("=" * 50)
    
    # Create organized structure
    organized_dir = create_organized_structure(args.output_dir, args.run_name)
    print(f"📁 Created organized structure: {organized_dir}")
    
    # Organize files
    print("\n📦 Organizing files...")
    equity_count, trade_count = organize_csv_files(args.results_dir, organized_dir)
    plot_count = organize_plots(args.plots_dir, organized_dir)
    config_count = copy_config_files(organized_dir)
    
    # Generate summary
    print("\n📋 Generating summary...")
    generate_run_summary(organized_dir, equity_count, trade_count, plot_count, config_count)
    
    # Create README
    create_readme(organized_dir)
    
    # Clean up if requested
    if args.clean:
        print("\n🧹 Cleaning up original files...")
        if Path(args.results_dir).exists():
            shutil.rmtree(args.results_dir)
            print(f"🗑️  Removed: {args.results_dir}")
        if Path(args.plots_dir).exists():
            shutil.rmtree(args.plots_dir)
            print(f"🗑️  Removed: {args.plots_dir}")
    
    print("\n✅ Organization complete!")
    print(f"📁 Organized results available in: {organized_dir}")
    print(f"📊 Files organized: {equity_count + trade_count + plot_count + config_count} total")

if __name__ == "__main__":
    main() 