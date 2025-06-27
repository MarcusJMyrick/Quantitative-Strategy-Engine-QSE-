#include "gtest/gtest.h"
#include <yaml-cpp/yaml.h>
#include <sstream>

TEST(FactorCalcTest, CompositeWeights) {
    std::stringstream ss;
    ss << "momentum: 0.5\nvol: 0.3\nvalue: 0.2\n";
    YAML::Node n = YAML::Load(ss.str());
    double sum = n["momentum"].as<double>() +
                 n["vol"].as<double>() +
                 n["value"].as<double>();
    EXPECT_DOUBLE_EQ(sum, 1.0);
} 