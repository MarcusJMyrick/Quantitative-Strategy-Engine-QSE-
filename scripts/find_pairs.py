#!/usr/bin/env python3
"""
Pairs Trading Research Script

This script performs the research phase of a pairs trading strategy by:
1. Downloading historical price data for a universe of symbols
2. Testing all possible pairs for cointegration using Engle-Granger test
3. Calculating hedge ratios for cointegrated pairs
4. Outputting results for use in the C++ strategy implementation

Author: QSE Development Team
Date: 2024
"""

import yfinance as yf
import statsmodels.api as sm
from statsmodels.tsa.stattools import coint
import itertools
import pandas as pd
import numpy as np
from typing import List, Tuple, Optional
import argparse
import sys
from datetime import datetime, timedelta


def download_data(symbols: List[str], start_date: str, end_date: str) -> pd.DataFrame:
    """
    Download historical price data for the given symbols.
    
    Args:
        symbols: List of ticker symbols
        start_date: Start date in 'YYYY-MM-DD' format
        end_date: End date in 'YYYY-MM-DD' format
        
    Returns:
        DataFrame with adjusted close prices for all symbols
    """
    try:
        print(f"Downloading historical data for {len(symbols)} symbols...")
        data = yf.download(symbols, start=start_date, end=end_date, progress=False)
        # Prefer 'Adj Close', but fallback to 'Close' if not present
        if isinstance(data.columns, pd.MultiIndex):
            if 'Adj Close' in data.columns.get_level_values(0):
                data = data['Adj Close']
            elif 'Close' in data.columns.get_level_values(0):
                data = data['Close']
            else:
                raise ValueError("Neither 'Adj Close' nor 'Close' found in downloaded data.")
        else:
            # Single symbol
            if 'Adj Close' in data.columns:
                data = pd.DataFrame(data['Adj Close'])
            elif 'Close' in data.columns:
                data = pd.DataFrame(data['Close'])
            else:
                raise ValueError("Neither 'Adj Close' nor 'Close' found in downloaded data.")
        data.dropna(inplace=True)
        
        print(f"Successfully downloaded data for {len(data.columns)} symbols")
        print(f"Data shape: {data.shape}")
        
        return data
    except Exception as e:
        print(f"Error downloading data: {e}")
        return pd.DataFrame()


def find_cointegrated_pairs(data: pd.DataFrame, significance_level: float = 0.05) -> List[dict]:
    """
    Find cointegrated pairs of assets and their hedge ratios.
    
    Args:
        data: DataFrame with price data (symbols as columns)
        significance_level: P-value threshold for cointegration test
        
    Returns:
        List of dictionaries containing pair information
    """
    symbols = data.columns.tolist()
    pairs = list(itertools.combinations(symbols, 2))
    
    print(f"Testing {len(pairs)} pairs for cointegration...")
    
    cointegrated_pairs = []
    
    for symbol1, symbol2 in pairs:
        # Skip if either symbol has insufficient data
        if data[symbol1].isna().sum() > len(data) * 0.1 or data[symbol2].isna().sum() > len(data) * 0.1:
            continue
            
        # Calculate correlation for additional insight
        correlation = data[symbol1].corr(data[symbol2])
        
        # Perform Engle-Granger cointegration test
        try:
            score, p_value, _ = coint(data[symbol1], data[symbol2])
            
            # If p-value is below significance level, the pair is cointegrated
            if p_value < significance_level:
                # Calculate hedge ratio using OLS regression
                # Regress symbol1 on symbol2: symbol1 = alpha + beta * symbol2
                model = sm.OLS(data[symbol1], sm.add_constant(data[symbol2])).fit()
                hedge_ratio = model.params.iloc[1]
                alpha = model.params.iloc[0]
                r_squared = model.rsquared
                
                cointegrated_pairs.append({
                    'pair': (symbol1, symbol2),
                    'p_value': p_value,
                    'hedge_ratio': hedge_ratio,
                    'alpha': alpha,
                    'r_squared': r_squared,
                    'correlation': correlation,
                    'score': score
                })
                
                print(f"✓ Found cointegrated pair: {symbol1}-{symbol2} (p={p_value:.4f}, β={hedge_ratio:.4f})")
                
        except Exception as e:
            print(f"Error testing pair {symbol1}-{symbol2}: {e}")
            continue
    
    return cointegrated_pairs


def rank_pairs(cointegrated_pairs: List[dict]) -> List[dict]:
    """
    Rank cointegrated pairs by quality metrics.
    
    Args:
        cointegrated_pairs: List of cointegrated pair dictionaries
        
    Returns:
        Sorted list of pairs ranked by quality
    """
    if not cointegrated_pairs:
        return []
    
    # Calculate quality score based on multiple factors
    for pair in cointegrated_pairs:
        # Lower p-value is better (more significant)
        p_score = 1 - pair['p_value']
        
        # Higher R-squared is better (better fit)
        r_score = pair['r_squared']
        
        # Higher absolute correlation is better
        corr_score = abs(pair['correlation'])
        
        # Combined quality score (weighted average)
        pair['quality_score'] = 0.4 * p_score + 0.4 * r_score + 0.2 * corr_score
    
    # Sort by quality score (descending)
    ranked_pairs = sorted(cointegrated_pairs, key=lambda x: x['quality_score'], reverse=True)
    
    return ranked_pairs


def print_results(ranked_pairs: List[dict], output_file: Optional[str] = None):
    """
    Print and optionally save the results.
    
    Args:
        ranked_pairs: Ranked list of cointegrated pairs
        output_file: Optional file path to save results
    """
    if not ranked_pairs:
        print("No cointegrated pairs found in the given universe.")
        return
    
    print(f"\n{'='*80}")
    print(f"COINTEGRATION ANALYSIS RESULTS")
    print(f"{'='*80}")
    print(f"Found {len(ranked_pairs)} cointegrated pairs:")
    print()
    
    # Print header
    print(f"{'Rank':<4} {'Pair':<15} {'P-Value':<10} {'Hedge Ratio':<12} {'R²':<8} {'Corr':<8} {'Quality':<8}")
    print("-" * 80)
    
    results_data = []
    
    for i, pair_info in enumerate(ranked_pairs, 1):
        pair_str = f"{pair_info['pair'][0]}-{pair_info['pair'][1]}"
        
        print(f"{i:<4} {pair_str:<15} {pair_info['p_value']:<10.4f} "
              f"{pair_info['hedge_ratio']:<12.4f} {pair_info['r_squared']:<8.4f} "
              f"{pair_info['correlation']:<8.4f} {pair_info['quality_score']:<8.4f}")
        
        # Store for potential file output
        results_data.append({
            'rank': i,
            'symbol1': pair_info['pair'][0],
            'symbol2': pair_info['pair'][1],
            'p_value': pair_info['p_value'],
            'hedge_ratio': pair_info['hedge_ratio'],
            'alpha': pair_info['alpha'],
            'r_squared': pair_info['r_squared'],
            'correlation': pair_info['correlation'],
            'quality_score': pair_info['quality_score']
        })
    
    print(f"\n{'='*80}")
    print("RECOMMENDED PAIR FOR TRADING:")
    print(f"{'='*80}")
    
    if ranked_pairs:
        best_pair = ranked_pairs[0]
        print(f"Symbol 1: {best_pair['pair'][0]}")
        print(f"Symbol 2: {best_pair['pair'][1]}")
        print(f"Hedge Ratio (β): {best_pair['hedge_ratio']:.6f}")
        print(f"Alpha (α): {best_pair['alpha']:.6f}")
        print(f"P-Value: {best_pair['p_value']:.6f}")
        print(f"R-Squared: {best_pair['r_squared']:.6f}")
        print(f"Correlation: {best_pair['correlation']:.6f}")
        print(f"Quality Score: {best_pair['quality_score']:.6f}")
        
        # Save to file if requested
        if output_file:
            df = pd.DataFrame(results_data)
            df.to_csv(output_file, index=False)
            print(f"\nResults saved to: {output_file}")


def main():
    """Main function to run the pairs trading research."""
    parser = argparse.ArgumentParser(description='Find cointegrated pairs for pairs trading strategy')
    
    # Default symbols organized by sector for better cointegration potential
    default_symbols = [
        'PEP', 'KO',      # Beverages
        'ADBE', 'CRM',    # Software
        'JPM', 'MS',      # Banking
        'SPY', 'IVV',     # S&P 500 ETFs
        'GLD', 'SLV',     # Precious Metals
        'AAPL', 'MSFT',   # Tech Giants
        'GOOG', 'AMZN'    # Tech Giants
    ]
    
    parser.add_argument('--symbols', nargs='+', default=default_symbols,
                       help='List of symbols to test (default: sector-specific pairs)')
    parser.add_argument('--start-date', default='2020-01-01',
                       help='Start date for data download (YYYY-MM-DD)')
    parser.add_argument('--end-date', default='2024-01-01',
                       help='End date for data download (YYYY-MM-DD)')
    parser.add_argument('--significance', type=float, default=0.05,
                       help='Significance level for cointegration test (default: 0.05)')
    parser.add_argument('--output', type=str, default='pairs_results.csv',
                       help='Output file for results (default: pairs_results.csv)')
    
    args = parser.parse_args()
    
    print("="*80)
    print("PAIRS TRADING RESEARCH - COINTEGRATION ANALYSIS")
    print("="*80)
    print(f"Symbols: {args.symbols}")
    print(f"Date range: {args.start_date} to {args.end_date}")
    print(f"Significance level: {args.significance}")
    print()
    
    # Download data
    data = download_data(args.symbols, args.start_date, args.end_date)
    
    if data.empty:
        print("Failed to download data. Exiting.")
        sys.exit(1)
    
    # Find cointegrated pairs
    cointegrated_pairs = find_cointegrated_pairs(data, args.significance)
    
    if not cointegrated_pairs:
        print("No cointegrated pairs found. Try:")
        print("1. Using different symbols")
        print("2. Adjusting the significance level")
        print("3. Using a different time period")
        sys.exit(1)
    
    # Rank pairs by quality
    ranked_pairs = rank_pairs(cointegrated_pairs)
    
    # Print results
    print_results(ranked_pairs, args.output)
    
    print(f"\nAnalysis completed successfully!")
    print(f"Use the top-ranked pair for your C++ PairsTradingStrategy implementation.")


if __name__ == '__main__':
    main() 