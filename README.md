# Quantitative Strategy Engine (QSE)

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/MarcusJMyrick/Quantitative-Strategy-Engine-QSE-)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A high-performance, multi-threaded, event-driven backtesting engine in C++ designed to develop, test, and analyze quantitative trading strategies across a universe of assets.

---

## About The Project

This project simulates a professional quantitative trading workflow, from data handling to parallelized strategy simulation and performance analysis. It showcases a robust architecture that emphasizes performance, accuracy, and scalability.

The system is built on two core principles:

1.  **High-Performance C++ Core:** An event-driven engine designed for low-latency backtesting. It features a modern, thread-safe `ThreadPool` to run simulations for multiple assets concurrently, drastically reducing research time.
2.  **Flexible Python Ecosystem:** A suite of Python scripts for data acquisition, processing, and advanced analysis. It calculates key performance metrics and generates consolidated visualizations for easy comparison of results across different assets and strategies.

---

## Sample Multi-Asset Analysis

The engine can run backtests on multiple symbols in parallel. The `analyze.py` script then consolidates all the results into a single, comprehensive summary plot and a detailed performance report for each asset.

![Multi-Asset Equity Curve Summary](docs/plots/equity_curves_summary.png)

| Symbol | Total Return | Sharpe Ratio | Max Drawdown |
| :----: | :----------: | :----------: | :----------: |
| **SPY**|    -0.41%    |    -0.47     |    -0.42%    |
| **AAPL**|   -0.23%     |    -0.19     |    -0.36%    |
| **GOOG**|    +1.46%    |     0.46     |    -0.47%    |
| **MSFT**|   -0.18%     |    -0.58     |    -0.20%    |

---

### Key Engineering Features

* **Multi-Threaded Architecture:** Utilizes a modern C++ `ThreadPool` to run multiple backtests concurrently, maximizing CPU utilization and dramatically speeding up the research cycle.
* **High-Performance C++ Core:**
    * **O(1) Algorithm:** The SMA calculation uses an efficient "running sum" algorithm, ensuring constant-time complexity.
    * **Optimized Memory Management:** Eliminates slow memory reallocations in hot loops by using `reserve()` and passing large data objects by `const` reference, as verified by profiling with Apple's Instruments.
* **Realistic Cost Modeling:** The `OrderManager` accurately simulates per-trade `commission` and `slippage`, leading to more realistic PnL calculations.
* **Detailed Event Logging:** The backtester generates unique, thread-safe output files for each asset, including a full `tradelog_SYMBOL.csv` and an `equity_SYMBOL.csv` curve.
* **Comprehensive Python Suite:**
    * **Data Pipeline:** Scripts to download and process market data for multiple symbols, with built-in retry logic and API rate-limit handling.
    * **Performance Analytics:** The `analyze.py` script automatically finds all backtest results, calculates key metrics (Sharpe Ratio, Max Drawdown), and generates a consolidated summary plot.
* **Robust Testing:**
    * **C++:** A full suite of unit and integration tests using `Google Test` and `Google Mock`.
    * **Python:** A placeholder for `pytest` tests.
* **Modern C++ & Build System:**
    * Written in modern C++17.
    * Uses `CMake` for a professional, cross-platform build system.
    * Includes separate `build.sh` (full process) and `test.sh` (dev loop) scripts for an efficient workflow.

---

### Technology Stack

* **C++ Core Engine:** C++17, CMake, Google Test/Mock
* **Python Ecosystem:** Python 3.8+, Pandas, NumPy, Matplotlib
* **High-Performance Libraries:** Apache Arrow, Parquet (planned for data I/O)

---

### Getting Started & Usage Workflow

Follow these steps to get a local copy up and running.

#### Prerequisites

* A C++17 compliant compiler (Clang on macOS is perfect)
* CMake (3.14+)
* Python (3.8+)
* **Homebrew (macOS):**
    ```sh
    # For build tools and dependencies
    brew install cmake git python
    # For Arrow/Parquet dependencies (optional but recommended)
    brew install apache-arrow lz4 thrift
    ```

#### Automated Workflow

This project includes automated scripts for a streamlined development experience.

1.  **Clone the Repository:**
    ```sh
    git clone [https://github.com/MarcusJMyrick/Quantitative-Strategy-Engine-QSE-.git](https://github.com/MarcusJMyrick/Quantitative-Strategy-Engine-QSE-.git)
    cd Quantitative-Strategy-Engine-QSE-
    ```

2.  **Set Up the API Key:**
    * Create a file named `.env` in the project root.
    * Add your Alpha Vantage API key to it:
        ```
        ALPHA_VANTAGE_API_KEY=YOUR_API_KEY_HERE
        ```

3.  **Run the Full Process (Build Script):**
    * The `./build.sh` script does everything: downloads data, builds the C++ code, runs all tests, and executes the final multi-asset backtest.
    * First, make the script executable:
        ```sh
        chmod +x build.sh
        ```
    * Then, run it:
        ```sh
        ./build.sh
        ```

4.  **Run the Analysis:**
    * After the C++ application finishes, run the Python analysis script to generate the summary report and plots.
        ```sh
        python3 scripts/analyze.py
        ```

#### C++ Development Workflow (Test Script)

When you are actively working on the C++ code, you don't need to re-download the data every time. Use the `test.sh` script for a much faster build-and-test loop.

1.  **Make the script executable:**
    ```sh
    chmod +x test.sh
    ```
2.  **Run the test script:**
    ```sh
    ./test.sh
    ```

---
### License

Distributed under the MIT License. See `LICENSE` for more information.

