#include "qse/exe/FactorExecutionEngine.h"

#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <arrow/table.h>
#include <arrow/array.h>
#include <arrow/api.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>

namespace qse {

using WeightMap = std::unordered_map<std::string, double>;

static WeightMap parse_csv(const std::string& path) {
    WeightMap out;
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        throw std::runtime_error("Could not open weight file: " + path);
    }
    std::string header;
    if (!std::getline(ifs, header)) {
        throw std::runtime_error("Empty weight file: " + path);
    }
    // Expect first two columns to be symbol and weight (case insensitive)
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string sym, wstr;
        if (!std::getline(ss, sym, ',')) continue;
        if (!std::getline(ss, wstr, ',')) {
            throw std::runtime_error("Malformed weights line: " + line);
        }
        double w = std::stod(wstr);
        if (std::isnan(w) || !std::isfinite(w)) {
            throw std::runtime_error("NaN/Inf weight for symbol " + sym);
        }
        out[sym] = w;
    }
    return out;
}

static WeightMap parse_parquet(const std::string& path) {
    WeightMap out;
    std::shared_ptr<arrow::io::ReadableFile> infile;
    auto res = arrow::io::ReadableFile::Open(path);
    if (!res.ok()) {
        throw std::runtime_error(res.status().ToString());
    }
    infile = *res;

    std::unique_ptr<parquet::arrow::FileReader> reader;
    auto st = parquet::arrow::OpenFile(infile, arrow::default_memory_pool(), &reader);
    if (!st.ok()) {
        throw std::runtime_error(st.ToString());
    }
    std::shared_ptr<arrow::Table> table;
    st = reader->ReadTable(&table);
    if (!st.ok()) {
        throw std::runtime_error(st.ToString());
    }

    auto sym_col = table->GetColumnByName("symbol");
    auto w_col = table->GetColumnByName("weight");
    if (!sym_col || !w_col) {
        throw std::runtime_error("Parquet weights file missing required columns");
    }

    for (int chunk = 0; chunk < sym_col->num_chunks(); ++chunk) {
        auto sym_arr = std::static_pointer_cast<arrow::StringArray>(sym_col->chunk(chunk));
        auto w_arr = std::static_pointer_cast<arrow::DoubleArray>(w_col->chunk(chunk));
        int64_t n = sym_arr->length();
        for (int64_t i = 0; i < n; ++i) {
            if (!sym_arr->IsValid(i) || !w_arr->IsValid(i)) {
                throw std::runtime_error("Null value in weights parquet file");
            }
            std::string sym = sym_arr->GetString(i);
            double w = w_arr->Value(i);
            if (std::isnan(w) || !std::isfinite(w)) {
                throw std::runtime_error("NaN/Inf weight for symbol " + sym);
            }
            out[sym] = w;
        }
    }
    return out;
}

WeightMap FactorExecutionEngine::load_weights(const std::string& path) {
    auto ext = std::filesystem::path(path).extension().string();
    if (ext == ".csv") {
        return parse_csv(path);
    } else if (ext == ".parquet" || ext == ".pq") {
        return parse_parquet(path);
    }
    throw std::runtime_error("Unsupported weight file extension: " + ext);
}

std::unordered_map<std::string, double> FactorExecutionEngine::fetch_holdings() {
    return {};
}

std::unordered_map<std::string, long long> FactorExecutionEngine::calc_target_shares(
    const std::unordered_map<std::string, double>& /*target_weights*/,
    const std::unordered_map<std::string, double>& /*current_holdings*/,
    double /*cash*/,
    const std::unordered_map<std::string, double>& /*prices*/) {
    return {};
}

void FactorExecutionEngine::submit_orders(const std::unordered_map<std::string, long long>& /*target_qty*/) {}

bool FactorExecutionEngine::should_rebalance(std::chrono::system_clock::time_point /*now*/) const {
    return false;
}

} // namespace qse 