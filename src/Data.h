#pragma once
#include <string>
#include <vector>
#include <chrono>

struct MarketData {
    std::chrono::system_clock::time_point timestamp;
    double open;
    double high;
    double low;
    double close;
    double volume;
};

struct Order {
    enum class Type { BUY, SELL };
    enum class Status { PENDING, FILLED, CANCELLED };
    
    Type type;
    Status status;
    double price;
    double quantity;
    std::chrono::system_clock::time_point timestamp;
}; 