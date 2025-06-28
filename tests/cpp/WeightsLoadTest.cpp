#include <gtest/gtest.h>
#include "qse/exe/FactorExecutionEngine.h"
#include <fstream>
#include <cstdio>

using namespace qse;

TEST(WeightsLoadTest, CsvToMap) {
    // Create a temporary CSV file
    const char* fname = "temp_weights.csv";
    std::ofstream ofs(fname);
    ofs << "symbol,weight\n";
    ofs << "AAPL,0.1\n";
    ofs << "GOOG,-0.1\n";
    ofs << "MSFT,0.0\n";
    ofs.close();

    ExecConfig cfg;
    FactorExecutionEngine engine(cfg, nullptr);
    auto wmap = engine.load_weights(fname);

    EXPECT_EQ(wmap.size(), 3);
    EXPECT_NEAR(wmap["AAPL"], 0.1, 1e-9);
    EXPECT_NEAR(wmap["GOOG"], -0.1, 1e-9);
    EXPECT_NEAR(wmap["MSFT"], 0.0, 1e-9);

    std::remove(fname);
} 