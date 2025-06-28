#include <gtest/gtest.h>
#include "qse/factor/RiskModel.h"
#include <arrow/array.h>
#include <arrow/table.h>
#include <arrow/builder.h>

using namespace qse;

static std::shared_ptr<arrow::Table> build_multi_asset() {
    // Two assets, 100 dates each
    const int n = 100;
    std::vector<std::string> dates;
    std::vector<std::string> assets;
    std::vector<double> ret, mkt;

    for (int i = 0; i < n; ++i) {
        std::string d = "2023-02-" + std::to_string(1 + i);
        double m = std::sin(i*0.1)*0.01;
        // asset X beta 1.0, asset Y beta -0.5
        for (int a=0;a<2;++a){
            dates.push_back(d);
            assets.push_back(a==0?"X":"Y");
            mkt.push_back(m);
            double beta = (a==0?1.0:-0.5);
            ret.push_back(beta*m);
        }
    }
    // Build Arrow columns
    arrow::StringBuilder db, ab; db.AppendValues(dates); ab.AppendValues(assets);
    arrow::DoubleBuilder rb, mb; rb.AppendValues(ret); mb.AppendValues(mkt);
    std::shared_ptr<arrow::Array> da, aa, ra, ma;
    db.Finish(&da); ab.Finish(&aa); rb.Finish(&ra); mb.Finish(&ma);
    auto schema = arrow::schema({
        arrow::field("date", arrow::utf8()),
        arrow::field("asset", arrow::utf8()),
        arrow::field("ret", arrow::float64()),
        arrow::field("mkt", arrow::float64())});
    return arrow::Table::Make(schema,{da,aa,ra,ma});
}

TEST(RiskModelMultiAssetTest, AppendBetaAddsColumns) {
    auto tbl = build_multi_asset();
    RiskModel rm;
    auto out = rm.append_beta(tbl,"asset","date","ret","mkt");
    ASSERT_NE(out,nullptr);
    EXPECT_NE(out->schema()->GetFieldByName("beta"), nullptr);
    EXPECT_NE(out->schema()->GetFieldByName("resid_sigma"), nullptr);
} 