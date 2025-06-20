import os
import shutil
import pytest
from scripts.analyze import analyze_results

def setup_module(module):
    # Ensure results and plots directories exist
    os.makedirs('results', exist_ok=True)
    os.makedirs('plots', exist_ok=True)
    # Create a minimal results.csv for testing
    with open('results/results.csv', 'w') as f:
        f.write('timestamp,portfolio_value\n')
        f.write('1700000000,100000\n')
        f.write('1700003600,100500\n')
        f.write('1700007200,101000\n')

def teardown_module(module):
    # Clean up generated plot
    try:
        os.remove('plots/equity_curve.png')
    except FileNotFoundError:
        pass
    # Optionally clean up test results.csv
    # os.remove('results/results.csv')

def test_analyze_creates_plot():
    analyze_results(filepath='results/results.csv', plot_dir='plots')
    assert os.path.exists('plots/equity_curve.png'), 'Equity curve plot was not created.' 