#include <gtest/gtest.h>
#include "qse/factor/ICMonitor.h"
#include <arrow/array.h>
#include <arrow/table.h>
#include <arrow/builder.h>
#include <random>

using namespace qse;

TEST(ICMonitorDistributionTest, UncorrelatedSeriesMeanICZero) {
    const int days = 200;
    const int assets = 20;
    std::mt19937 rng(999);
    std::normal_distribution<double> nd(0.0,1.0);
    std::vector<std::string> dates;
    std::vector<double> factor; std::vector<double> ret;
    for(int d=0; d<days; ++d){
        std::string date = "2023-03-"+std::to_string(1+d);
        for(int a=0;a<assets;++a){
            dates.push_back(date);
            factor.push_back(nd(rng));
            ret.push_back(nd(rng));
        }
    }
    arrow::StringBuilder db; db.AppendValues(dates);
    arrow::DoubleBuilder fb, rb;
    fb.AppendValues(factor);
    rb.AppendValues(ret);
    std::shared_ptr<arrow::Array> da, fa, ra; db.Finish(&da); fb.Finish(&fa); rb.Finish(&ra);
    auto schema = arrow::schema({arrow::field("date",arrow::utf8()),arrow::field("factor",arrow::float64()),arrow::field("ret",arrow::float64())});
    auto tbl = arrow::Table::Make(schema,{da,fa,ra});

    ICMonitor icm; auto res = icm.compute_ic(tbl,"factor","ret","date",60);
    double sum=0.0; int n=0; for(double v: res.daily_ic){ if(!std::isnan(v)){ sum+=v; ++n;} }
    ASSERT_GT(n,0);
    double mean_ic = sum/n;
    EXPECT_NEAR(mean_ic,0.0,0.05);
} 