#pragma once
#include "Data.h"
#include <string>
#include <vector>

class DataReader {
public:
    DataReader(const std::string& filepath);
    std::vector<MarketData> readData();
    
private:
    std::string filepath_;
}; 