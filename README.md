# Quantitative Strategy Engine (QSE)

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/your_username/Quantitative-Strategy-Engine)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A high-performance, low-latency backtesting engine built in C++ and Python for developing and analyzing quantitative trading strategies.

## About The Project

The Quantitative Strategy Engine (QSE) was created to simulate a professional quantitative trading environment, addressing the core requirements for a Quantitative Developer role. It tackles the challenge of processing large financial datasets, executing trading logic in a performant manner, and providing robust analytics for strategy evaluation.

The project's architecture is bifurcated to leverage the strengths of both C++ and Python:
* **C++ Core Engine**: An event-driven backtester optimized for speed and low-latency execution of trading logic.
* **Python Analysis Suite**: A collection of scripts for sourcing and cleaning market data, analyzing strategy performance, and generating visualizations.

## Key Features

* **High-Performance C++ Core**: Event-driven architecture to process market data tick-by-tick or bar-by-bar.
* **Data Processing Pipeline**: Python scripts to download, clean, and store historical market data in an efficient Parquet format.
* **Strategy Interface**: Abstract C++ classes allow for easy implementation of new trading strategies.
* **Order Management Simulation**: Simulates order execution, position tracking, and real-time PnL calculation.
* **Advanced Analytics**: Python-based performance analysis including Sharpe Ratio, Max Drawdown, and equity curve visualization.

## Technology Stack

### Backend (Core Engine)
* **C++17**: For performance-critical backtesting logic.
* **Apache Arrow**: For efficiently reading Parquet data files in C++.
* **CMake**: For cross-platform build automation.

### Frontend (Data & Analysis)
* **Python 3.8+**: For data handling, analysis, and orchestration.
* **Pandas**: For data manipulation and cleaning.
* **PyArrow**: For working with the Parquet file format.
* **Matplotlib**: For plotting and visualizing results.
* **Pytest**: For testing the data pipeline.

## Getting Started

Follow these instructions to get a local copy up and running.

### Prerequisites

* A C++17 compliant compiler (GCC, Clang, or MSVC)
* CMake (version 3.15 or higher)
* Python 3.8+
* Git

### Installation

1.  **Clone the repository:**
    *(Note: The repository name `Quantitative-Strategy-Engine` is a common convention for multi-word project names on GitHub.)*
    ```sh
    git clone [https://github.com/your_username/Quantitative-Strategy-Engine.git](https://github.com/your_username/Quantitative-Strategy-Engine.git)
    cd Quantitative-Strategy-Engine
    ```

2.  **Set up the Python virtual environment:**
    ```sh
    python -m venv venv
    source venv/bin/activate  # On Windows: `venv\Scripts\activate`
    pip install -r requirements.txt
    ```
    *(You will need to create a `requirements.txt` file containing `pandas`, `pyarrow`, `matplotlib`, `pytest`, etc.)*

3.  **Build the C++ backtester:**
    ```sh
    mkdir build
    cd build
    cmake ..
    cmake --build .  # Or use `make` on Linux/macOS
    cd ..
    ```

## Usage

The project workflow is executed in three stages.

1.  **Acquire and Process Data:**
    Run the Python script to download and clean the market data.
    ```sh
    python scripts/process_data.py
    ```
    This will generate a `processed_data.parquet` file.

2.  **Run the Backtest:**
    Execute the compiled C++ engine. It will read the processed data, run the hard-coded strategy, and output a `results.csv` file.
    ```sh
    ./build/backtester
    ```

3.  **Analyze the Results:**
    Run the Python analysis script to generate performance metrics and visualizations.
    ```sh
    python scripts/analyze.py
    ```
    This will print performance stats to the console and save an `equity_curve.png` chart.

## Testing

To run the integrated tests for the Python data pipeline, use `pytest`.

```sh
pytest
