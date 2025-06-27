#include "gtest/gtest.h"
#include "qse/math/StatsUtil.h"

TEST(FactorMathTest, RollingStd) {
    qse::math::RollingStdDev r(4);
    std::vector<double> v = {1,2,3,4};
    double last = 0;
    for (auto x : v) last = r(x);
    // σ of {1,2,3,4} = √1.25 ≈ 1.118
    EXPECT_NEAR(last, 1.1180, 1e-3);
} 