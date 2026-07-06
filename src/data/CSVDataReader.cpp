#include "qse/data/CSVDataReader.h"
#include "qse/core/Debug.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>
#include <algorithm>

namespace {

// Counts missing rows in a time grid: the expected spacing is the median of
// consecutive deltas, and any delta rounding to k spacings contributes k-1
// missing rows. Exact for grid data; a heuristic for event-time series.
template <typename Series, typename GetTs>
std::size_t count_grid_gaps(const Series& rows, GetTs get_ts) {
    if (rows.size() < 3) {
        return 0;
    }
    std::vector<long long> deltas;
    deltas.reserve(rows.size() - 1);
    for (std::size_t i = 1; i < rows.size(); ++i) {
        long long d = get_ts(rows[i]) - get_ts(rows[i - 1]);
        if (d > 0) {
            deltas.push_back(d);
        }
    }
    if (deltas.size() < 2) {
        return 0;
    }
    std::vector<long long> sorted = deltas;
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    const long long expected = sorted[sorted.size() / 2];
    if (expected <= 0) {
        return 0;
    }
    std::size_t gaps = 0;
    for (long long d : deltas) {
        if (d > expected + expected / 2) {
            gaps += static_cast<std::size_t>((d + expected / 2) / expected) - 1;
        }
    }
    return gaps;
}

} // namespace

namespace qse {

CSVDataReader::CSVDataReader(const std::string& file_path)
    : file_path_(file_path), symbol_override_("") {
    load_data();
}

CSVDataReader::CSVDataReader(const std::string& file_path, const std::string& symbol_override)
    : file_path_(file_path), symbol_override_(symbol_override) {
    load_data();
}

void CSVDataReader::load_data() {
    std::ifstream file(file_path_);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + file_path_);
    }

    std::string header_line;
    if (!std::getline(file, header_line)) {
        throw std::runtime_error("Cannot read header from file: " + file_path_);
    }

    // Detect file type based on header content
    bool isBar = (header_line.find("Open") != std::string::npos ||
                  header_line.find("open") != std::string::npos);

    std::string line;
    if (isBar) {
        if (qse_debug_enabled())
            std::cout << "Detected Bar data format in " << file_path_ << std::endl;
        while (std::getline(file, line)) {
            if (line.empty()) {
                continue; // trailing newlines are not data-quality problems
            }
            std::stringstream ss(line);
            std::string item;
            std::vector<std::string> tokens;
            while (std::getline(ss, item, ',')) {
                tokens.push_back(item);
            }
            if (tokens.size() < 6) {
                ++skipped_rows_;
                continue;
            }
            // A row with missing or non-numeric fields is counted and
            // skipped; one bad row must not abort the whole load
            try {
                Bar bar;
                bar.symbol = symbol_override_.empty() ? "UNKNOWN" : symbol_override_;
                bar.timestamp = qse::Timestamp(std::chrono::seconds(std::stoll(tokens[0])));
                bar.open = std::stod(tokens[1]);
                bar.high = std::stod(tokens[2]);
                bar.low = std::stod(tokens[3]);
                bar.close = std::stod(tokens[4]);
                bar.volume = std::stoull(tokens[5]);
                bars_.push_back(bar);
            } catch (const std::exception&) {
                ++skipped_rows_;
            }
        }

        std::sort(bars_.begin(), bars_.end(),
                  [](const Bar& a, const Bar& b) { return a.timestamp < b.timestamp; });
        gap_count_ = count_grid_gaps(bars_, [](const Bar& b) {
            return static_cast<long long>(b.timestamp.time_since_epoch().count());
        });
    } else {
        if (qse_debug_enabled())
            std::cout << "Detected Tick data format in " << file_path_ << std::endl;
        while (std::getline(file, line)) {
            if (line.empty()) {
                continue;
            }
            std::stringstream ss(line);
            std::string item;
            std::vector<std::string> tokens;
            while (std::getline(ss, item, ',')) {
                tokens.push_back(item);
            }
            try {
                if (tokens.size() >= 8) {
                    // Full tick format: timestamp,symbol,price,volume,bid,ask,bid_size,ask_size
                    Tick tick;
                    {
                        long long raw = std::stoll(tokens[0]);
                        if (raw < 10'000'000'000LL) {
                            raw *= 1000; // CSV provides seconds – promote to ms
                        }
                        tick.timestamp = qse::Timestamp(std::chrono::milliseconds(raw));
                    }
                    tick.symbol = symbol_override_.empty() ? tokens[1] : symbol_override_;
                    tick.price = std::stod(tokens[2]);
                    tick.volume = std::stoull(tokens[3]);
                    tick.bid = std::stod(tokens[4]);
                    tick.ask = std::stod(tokens[5]);
                    tick.bid_size = std::stoull(tokens[6]);
                    tick.ask_size = std::stoull(tokens[7]);
                    ticks_.push_back(tick);
                } else if (tokens.size() >= 3) {
                    // Legacy format: timestamp,price,volume
                    Tick tick;
                    {
                        long long raw = std::stoll(tokens[0]);
                        if (raw < 10'000'000'000LL) {
                            raw *= 1000;
                        }
                        tick.timestamp = qse::Timestamp(std::chrono::milliseconds(raw));
                    }
                    tick.symbol = symbol_override_.empty() ? "UNKNOWN" : symbol_override_;
                    tick.price = std::stod(tokens[1]);
                    tick.volume = std::stoull(tokens[2]);
                    tick.bid = tick.price; // Use price as bid/ask for legacy format
                    tick.ask = tick.price;
                    tick.bid_size = tick.volume; // Use volume as size for legacy format
                    tick.ask_size = tick.volume;
                    ticks_.push_back(tick);
                } else {
                    ++skipped_rows_;
                }
            } catch (const std::exception&) {
                ++skipped_rows_;
            }
        }

        // Sort ticks by timestamp
        std::sort(ticks_.begin(), ticks_.end(),
                  [](const Tick& a, const Tick& b) { return a.timestamp < b.timestamp; });
        gap_count_ = count_grid_gaps(ticks_, [](const Tick& t) {
            return static_cast<long long>(t.timestamp.time_since_epoch().count());
        });
    }

    if (skipped_rows_ > 0 || gap_count_ > 0) {
        std::cerr << "[CSVDataReader] Data quality warning for " << file_path_ << ": "
                  << skipped_rows_ << " unparseable row(s) skipped, " << gap_count_
                  << " missing row(s) in the time grid" << std::endl;
    }
}

// Return by const reference, as per the interface
const std::vector<qse::Tick>& CSVDataReader::read_all_ticks() const {
    return ticks_;
}

// Return by const reference, as per the interface
const std::vector<qse::Bar>& CSVDataReader::read_all_bars() const {
    return bars_;
}

} // namespace qse