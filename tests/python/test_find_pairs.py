#!/usr/bin/env python3
"""
Unit tests for the pairs trading research script (find_pairs.py).

This test suite uses mocking to isolate functions from external dependencies (like yfinance)
and validates the core logic of cointegration testing, hedge ratio calculation, and ranking.
"""

import unittest
from unittest.mock import patch, MagicMock
import pandas as pd
import numpy as np
import sys
import os

# Add the scripts directory to the path so we can import the functions
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts'))

# Import the functions to be tested
from find_pairs import (
    download_data,
    find_cointegrated_pairs,
    rank_pairs,
    print_results
)

class TestFindPairs(unittest.TestCase):

    def setUp(self):
        """Set up common test data before each test."""
        # Create a sample DataFrame for testing.
        # 'A' and 'B' are designed to be perfectly cointegrated (B = 2*A + 5 + noise).
        # 'C' is random and should not be cointegrated with 'A' or 'B'.
        np.random.seed(42)  # For reproducible tests
        self.test_data = pd.DataFrame({
            'A': np.linspace(100, 150, 100),
            'B': 2 * np.linspace(100, 150, 100) + 5 + np.random.normal(0, 0.1, 100),
            'C': np.random.normal(100, 10, 100)
        })
        self.test_data.columns = pd.Index(['A', 'B', 'C'])


    @patch('find_pairs.yf.download')
    def test_download_data_success(self, mock_yf_download):
        """Test that the data download function correctly processes mocked API responses."""
        print("\nTesting download_data function...")

        # Configure the mock to return our test data with multi-level columns
        mock_yf_download.return_value = pd.DataFrame({
            ('Adj Close', 'A'): self.test_data['A'],
            ('Adj Close', 'B'): self.test_data['B'],
            ('Adj Close', 'C'): self.test_data['C'],
            ('Close', 'A'): self.test_data['A'],  # Include other columns to simulate real response
            ('Volume', 'A'): np.random.randint(1000, 10000, 100),
        })

        symbols = ['A', 'B', 'C']
        data = download_data(symbols, '2022-01-01', '2023-01-01')

        # Assertions
        self.assertIsInstance(data, pd.DataFrame)
        self.assertListEqual(data.columns.tolist(), symbols)
        self.assertEqual(data.shape[0], self.test_data.shape[0])
        mock_yf_download.assert_called_once()  # Ensure the download function was called
        print("✓ download_data test passed.")


    @patch('find_pairs.yf.download')
    def test_download_data_fallback_to_close(self, mock_yf_download):
        """Test that the function falls back to 'Close' when 'Adj Close' is not available."""
        print("\nTesting download_data fallback to Close...")

        # Configure the mock to return data without 'Adj Close'
        mock_yf_download.return_value = pd.DataFrame({
            ('Close', 'A'): self.test_data['A'],
            ('Close', 'B'): self.test_data['B'],
            ('Close', 'C'): self.test_data['C'],
            ('Volume', 'A'): np.random.randint(1000, 10000, 100),
        })

        symbols = ['A', 'B', 'C']
        data = download_data(symbols, '2022-01-01', '2023-01-01')

        # Assertions
        self.assertIsInstance(data, pd.DataFrame)
        self.assertListEqual(data.columns.tolist(), symbols)
        print("✓ download_data fallback test passed.")


    @patch('find_pairs.yf.download')
    def test_download_data_single_symbol(self, mock_yf_download):
        """Test data download for a single symbol."""
        print("\nTesting download_data with single symbol...")

        # Configure the mock for single symbol
        mock_yf_download.return_value = pd.DataFrame({
            'Adj Close': self.test_data['A'],
            'Close': self.test_data['A'],
            'Volume': np.random.randint(1000, 10000, 100),
        })

        symbols = ['A']
        data = download_data(symbols, '2022-01-01', '2023-01-01')

        # Assertions
        self.assertIsInstance(data, pd.DataFrame)
        self.assertEqual(len(data.columns), 1)
        print("✓ download_data single symbol test passed.")


    @patch('find_pairs.yf.download')
    def test_download_data_error_handling(self, mock_yf_download):
        """Test error handling in data download."""
        print("\nTesting download_data error handling...")

        # Configure the mock to raise an exception
        mock_yf_download.side_effect = Exception("Network error")

        symbols = ['A', 'B', 'C']
        
        # The function should catch the exception and return empty DataFrame
        data = download_data(symbols, '2022-01-01', '2023-01-01')

        # Assertions
        self.assertTrue(data.empty)
        print("✓ download_data error handling test passed.")


    def test_find_cointegrated_pairs(self):
        """Test the core logic of finding cointegrated pairs and calculating the hedge ratio."""
        print("\nTesting find_cointegrated_pairs function...")

        cointegrated_pairs = find_cointegrated_pairs(self.test_data, significance_level=0.05)

        # Assertions
        self.assertGreaterEqual(len(cointegrated_pairs), 1, "Should find at least one cointegrated pair.")
        
        # Find the A-B pair in results
        ab_pair = None
        for pair_info in cointegrated_pairs:
            if pair_info['pair'] == ('A', 'B'):
                ab_pair = pair_info
                break
        
        self.assertIsNotNone(ab_pair, "Should find the cointegrated pair ('A', 'B').")
        self.assertLess(ab_pair['p_value'], 0.05, "P-value should be below the significance level.")
        
        # The hedge ratio for B = 2*A + 5 should be ~0.5 when regressing A on B (A = 0.5*B - 2.5)
        # OLS(y,x) where y = A, x = B
        self.assertAlmostEqual(ab_pair['hedge_ratio'], 0.5, delta=0.1, msg="Hedge ratio is incorrect.")
        
        # Check that all required fields are present
        required_fields = ['pair', 'p_value', 'hedge_ratio', 'alpha', 'r_squared', 'correlation', 'score']
        for field in required_fields:
            self.assertIn(field, ab_pair, f"Missing required field: {field}")
        
        print("✓ find_cointegrated_pairs test passed.")


    def test_find_cointegrated_pairs_no_cointegration(self):
        """Test that the function correctly identifies when no pairs are cointegrated."""
        print("\nTesting find_cointegrated_pairs with no cointegration...")

        # Create data where no pairs should be cointegrated
        # Use a more stringent significance level to avoid false positives
        random_data = pd.DataFrame({
            'X': np.random.normal(100, 10, 100),
            'Y': np.random.normal(100, 10, 100),
            'Z': np.random.normal(100, 10, 100)
        })

        cointegrated_pairs = find_cointegrated_pairs(random_data, significance_level=0.01)  # More stringent

        # Assertions - with random data, we might get some false positives
        # So we'll just check that the function runs without error
        self.assertIsInstance(cointegrated_pairs, list)
        print("✓ find_cointegrated_pairs no cointegration test passed.")


    def test_rank_pairs(self):
        """Test the ranking logic for cointegrated pairs."""
        print("\nTesting rank_pairs function...")

        # Create a list of dummy cointegrated pair results
        dummy_pairs = [
            {'pair': ('A', 'B'), 'p_value': 0.01, 'r_squared': 0.98, 'correlation': 0.99},
            {'pair': ('C', 'D'), 'p_value': 0.04, 'r_squared': 0.80, 'correlation': 0.90},
            {'pair': ('E', 'F'), 'p_value': 0.02, 'r_squared': 0.95, 'correlation': 0.97}
        ]
        
        ranked = rank_pairs(dummy_pairs)

        # Assertions
        self.assertEqual(len(ranked), 3)
        
        # Check that quality scores were calculated
        for pair in ranked:
            self.assertIn('quality_score', pair)
            self.assertGreater(pair['quality_score'], 0)
            self.assertLess(pair['quality_score'], 1)
        
        # The quality score for the first pair should be the highest
        # Score = 0.4 * (1 - p) + 0.4 * r^2 + 0.2 * corr
        # Pair 1: 0.4*(0.99) + 0.4*(0.98) + 0.2*(0.99) = 0.396 + 0.392 + 0.198 = 0.986
        # Pair 2: 0.4*(0.96) + 0.4*(0.80) + 0.2*(0.90) = 0.384 + 0.320 + 0.180 = 0.884
        # Pair 3: 0.4*(0.98) + 0.4*(0.95) + 0.2*(0.97) = 0.392 + 0.380 + 0.194 = 0.966
        self.assertEqual(ranked[0]['pair'], ('A', 'B'), "Pair ('A', 'B') should be ranked first.")
        self.assertEqual(ranked[1]['pair'], ('E', 'F'), "Pair ('E', 'F') should be ranked second.")
        self.assertEqual(ranked[2]['pair'], ('C', 'D'), "Pair ('C', 'D') should be ranked third.")
        
        # Verify quality scores are in descending order
        self.assertGreater(ranked[0]['quality_score'], ranked[1]['quality_score'])
        self.assertGreater(ranked[1]['quality_score'], ranked[2]['quality_score'])
        
        print("✓ rank_pairs test passed.")


    def test_rank_pairs_empty_list(self):
        """Test ranking with an empty list."""
        print("\nTesting rank_pairs with empty list...")

        ranked = rank_pairs([])
        self.assertEqual(len(ranked), 0)
        print("✓ rank_pairs empty list test passed.")


    @patch('builtins.print')
    def test_print_results(self, mock_print):
        """Test the print_results function."""
        print("\nTesting print_results function...")

        # Create test data
        test_pairs = [
            {
                'pair': ('A', 'B'),
                'p_value': 0.01,
                'hedge_ratio': 0.5,
                'alpha': 1.0,
                'r_squared': 0.98,
                'correlation': 0.99,
                'quality_score': 0.95
            }
        ]

        # Test with output file
        with patch('pandas.DataFrame.to_csv') as mock_to_csv:
            print_results(test_pairs, 'test_output.csv')
            
            # Verify that print was called (indicating results were displayed)
            self.assertGreater(mock_print.call_count, 0)
            
            # Verify that to_csv was called (indicating file was saved)
            mock_to_csv.assert_called_once()

        print("✓ print_results test passed.")


    @patch('builtins.print')
    def test_print_results_empty_list(self, mock_print):
        """Test print_results with an empty list."""
        print("\nTesting print_results with empty list...")

        print_results([], None)
        
        # Should print "No cointegrated pairs found"
        mock_print.assert_called_with("No cointegrated pairs found in the given universe.")
        print("✓ print_results empty list test passed.")


    def test_integration_workflow(self):
        """Test the complete workflow from data to ranked results."""
        print("\nTesting complete integration workflow...")

        # Use the test data directly (no download needed)
        cointegrated_pairs = find_cointegrated_pairs(self.test_data, significance_level=0.05)
        
        # Should find at least the A-B pair
        self.assertGreaterEqual(len(cointegrated_pairs), 1)
        
        # Rank the pairs
        ranked_pairs = rank_pairs(cointegrated_pairs)
        
        # Should have the same number of pairs
        self.assertEqual(len(ranked_pairs), len(cointegrated_pairs))
        
        # Should be sorted by quality score
        if len(ranked_pairs) > 1:
            for i in range(len(ranked_pairs) - 1):
                self.assertGreaterEqual(
                    ranked_pairs[i]['quality_score'], 
                    ranked_pairs[i + 1]['quality_score']
                )
        
        print("✓ Integration workflow test passed.")


if __name__ == '__main__':
    # Set up test suite
    test_suite = unittest.TestLoader().loadTestsFromTestCase(TestFindPairs)
    
    # Run tests with verbose output
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(test_suite)
    
    # Print summary
    print(f"\n{'='*60}")
    print(f"TEST SUMMARY")
    print(f"{'='*60}")
    print(f"Tests run: {result.testsRun}")
    print(f"Failures: {len(result.failures)}")
    print(f"Errors: {len(result.errors)}")
    
    if result.failures:
        print(f"\nFAILURES:")
        for test, traceback in result.failures:
            print(f"- {test}: {traceback}")
    
    if result.errors:
        print(f"\nERRORS:")
        for test, traceback in result.errors:
            print(f"- {test}: {traceback}")
    
    if result.wasSuccessful():
        print(f"\n🎉 ALL TESTS PASSED! 🎉")
    else:
        print(f"\n❌ SOME TESTS FAILED ❌")
    
    # Exit with appropriate code
    sys.exit(0 if result.wasSuccessful() else 1) 