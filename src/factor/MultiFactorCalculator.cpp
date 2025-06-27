#include "qse/factor/MultiFactorCalculator.h"
#include "qse/math/StatsUtil.h"
#include <arrow/api.h>
#include <arrow/csv/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/writer.h>
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace qse;

namespace {

double pct_change(double now, double prev) {
    return prev == 0.0 ? 0.0 : (now - prev) / prev;
}

}   // anonymous

void MultiFactorCalculator::compute_factors(const std::string& in_csv,
                                            const std::string& out_parquet,
                                            const std::string& weights_yaml)
{
    /************ 1. Load daily OHLCV ************/
    auto table = load_arrow_table(in_csv);
    auto nrows = table->num_rows();
    if (nrows == 0) throw std::runtime_error("Empty price file");

    /************ 2. Momentum 12-1 (placeholder) ************/
    // TODO: replace with real 12-1 logic
    std::vector<double> mom(nrows, 0.0);
    for (int i = 21; i < nrows; ++i)   // 1-month gap, 12-month look-back
        mom[i] = pct_change(col<double>(table,"close", i),
                            col<double>(table,"close", i-252));

    /************ 3. Volatility ************/
    qse::math::RollingStdDev vol20(20), vol60(60);
    std::vector<double> vol20v(nrows), vol60v(nrows);
    for (int i = 0; i < nrows; ++i) {
        auto px = col<double>(table,"close", i);
        vol20v[i] = vol20(px);
        vol60v[i] = vol60(px);
    }

    /************ 4. Value proxy: 1 / P-B (placeholder) ************/
    std::vector<double> value(nrows);
    for (int i = 0; i < nrows; ++i) {
        double pb = col<double>(table,"pb", i);      // assumes pb column exists
        value[i]  = pb == 0.0 ? 0.0 : 1.0 / pb;
    }

    /************ 5. Winsorise + z-score ************/
    qse::math::winsorize(mom);
    qse::math::winsorize(vol20v);
    qse::math::winsorize(value);

    qse::math::zscore(mom);
    qse::math::zscore(vol20v);
    qse::math::zscore(value);

    /************ 6. Composite score ************/
    YAML::Node cfg = YAML::LoadFile(weights_yaml);
    double w_mom   = cfg["momentum"].as<double>();
    double w_vol   = cfg["vol"].as<double>();
    double w_val   = cfg["value"].as<double>();
    double w_sum   = w_mom + w_vol + w_val;
    if (std::abs(w_sum - 1.0) > 1e-6)
        throw std::runtime_error("Factor weights must sum to 1");

    std::vector<double> composite(nrows);
    for (int i = 0; i < nrows; ++i)
        composite[i] = w_mom*mom[i] + w_vol*vol20v[i] + w_val*value[i];

    /************ 7. Attach columns and dump Parquet ************/
    append_column(table, "mom_z",   mom);
    append_column(table, "vol20_z", vol20v);
    append_column(table, "val_z",   value);
    append_column(table, "alpha",   composite);

    save_parquet(table, out_parquet);
    std::cout << "Wrote " << out_parquet << std::endl;
}

std::shared_ptr<arrow::Table> MultiFactorCalculator::load_arrow_table(const std::string& csv_path) {
    // For now, create a simple mock table
    // In practice, you'd use Arrow's CSV reader
    arrow::MemoryPool* pool = arrow::default_memory_pool();
    
    // Create schema
    std::vector<std::shared_ptr<arrow::Field>> fields = {
        arrow::field("date", arrow::utf8()),
        arrow::field("open", arrow::float64()),
        arrow::field("high", arrow::float64()),
        arrow::field("low", arrow::float64()),
        arrow::field("close", arrow::float64()),
        arrow::field("volume", arrow::int64()),
        arrow::field("pb", arrow::float64())  // Price-to-book ratio
    };
    
    auto schema = std::make_shared<arrow::Schema>(fields);
    
    // Create mock data (in practice, read from CSV)
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    
    // Mock data for testing
    std::vector<std::string> dates = {"2023-01-01", "2023-01-02", "2023-01-03"};
    std::vector<double> closes = {100.0, 101.0, 102.0};
    std::vector<double> pbs = {1.5, 1.6, 1.7};
    
    // Create arrays (simplified for now)
    arrow::StringBuilder date_builder;
    arrow::DoubleBuilder close_builder;
    arrow::DoubleBuilder pb_builder;
    
    for (const auto& date : dates) {
        date_builder.Append(date);
    }
    for (double close : closes) {
        close_builder.Append(close);
    }
    for (double pb : pbs) {
        pb_builder.Append(pb);
    }
    
    std::shared_ptr<arrow::Array> date_array, close_array, pb_array;
    date_builder.Finish(&date_array);
    close_builder.Finish(&close_array);
    pb_builder.Finish(&pb_array);
    
    // Create a simple volume array with zeros
    arrow::Int64Builder volume_builder;
    for (size_t i = 0; i < dates.size(); ++i) {
        volume_builder.Append(1000); // Mock volume
    }
    std::shared_ptr<arrow::Array> volume_array;
    volume_builder.Finish(&volume_array);
    
    arrays = {date_array, close_array, close_array, close_array, close_array, 
              volume_array, pb_array};
    
    return arrow::Table::Make(schema, arrays);
}

void MultiFactorCalculator::save_parquet(const std::shared_ptr<arrow::Table>& table, 
                                        const std::string& path) {
    // For now, just log that we would save
    // In practice, use Arrow's Parquet writer
    std::cout << "Would save table with " << table->num_rows() << " rows to " << path << std::endl;
}

void MultiFactorCalculator::append_column(const std::shared_ptr<arrow::Table>& table,
                                         const std::string& name, 
                                         const std::vector<double>& data) {
    // For now, just log the column addition
    // In practice, create Arrow array and append to table
    std::cout << "Would append column '" << name << "' with " << data.size() << " values" << std::endl;
}

template<typename T>
T MultiFactorCalculator::col(const std::shared_ptr<arrow::Table>& table, 
                            const std::string& name, int row) {
    // Mock implementation - return test values
    if (name == "close") {
        return static_cast<T>(100.0 + row);
    } else if (name == "pb") {
        return static_cast<T>(1.5 + row * 0.1);
    }
    return static_cast<T>(0.0);
} 