// E2: AlpacaExecutionHandler unit tests against a mock HTTP layer - zero
// network. Verifies the REST mapping (endpoints, auth headers, payloads),
// response parsing, and the polling fill stream.

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "qse/exe/AlpacaExecutionHandler.h"

#include <memory>
#include <vector>

using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Return;

namespace {

class MockHttpClient : public qse::IHttpClient {
public:
    MOCK_METHOD(qse::HttpResponse, get,
                (const std::string& url, const std::vector<std::string>& headers), (override));
    MOCK_METHOD(qse::HttpResponse, post,
                (const std::string& url, const std::vector<std::string>& headers,
                 const std::string& body),
                (override));
    MOCK_METHOD(qse::HttpResponse, patch,
                (const std::string& url, const std::vector<std::string>& headers,
                 const std::string& body),
                (override));
    MOCK_METHOD(qse::HttpResponse, del,
                (const std::string& url, const std::vector<std::string>& headers), (override));
};

class AlpacaHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        http_ = std::make_shared<MockHttpClient>();
        handler_ = std::make_unique<qse::AlpacaExecutionHandler>(http_, "test-key", "test-secret",
                                                                 "https://paper.test");
    }

    std::shared_ptr<MockHttpClient> http_;
    std::unique_ptr<qse::AlpacaExecutionHandler> handler_;
};

qse::HttpResponse json_response(long status, std::string body) {
    return qse::HttpResponse{status, std::move(body)};
}

} // namespace

TEST_F(AlpacaHandlerTest, RequiresCredentials) {
    EXPECT_THROW(qse::AlpacaExecutionHandler(http_, "", "secret"), std::runtime_error);
    EXPECT_THROW(qse::AlpacaExecutionHandler(http_, "key", ""), std::runtime_error);
}

TEST_F(AlpacaHandlerTest, MarketOrderPostsCorrectPayloadWithAuth) {
    EXPECT_CALL(*http_, post("https://paper.test/v2/orders",
                             AllOf(Contains("APCA-API-KEY-ID: test-key"),
                                   Contains("APCA-API-SECRET-KEY: test-secret")),
                             AllOf(HasSubstr("\"symbol\":\"AAPL\""), HasSubstr("\"qty\":\"100\""),
                                   HasSubstr("\"side\":\"buy\""), HasSubstr("\"type\":\"market\""),
                                   HasSubstr("\"time_in_force\":\"day\""))))
        .WillOnce(Return(json_response(200, R"({"id":"alpaca-1","status":"accepted"})")));

    qse::OrderId id = handler_->submit_market_order("AAPL", qse::Order::Side::BUY, 100);
    EXPECT_EQ(id, "alpaca-1");
}

TEST_F(AlpacaHandlerTest, LimitOrderCarriesPriceAndTimeInForce) {
    EXPECT_CALL(*http_,
                post(_, _,
                     AllOf(HasSubstr("\"type\":\"limit\""), HasSubstr("\"limit_price\":\"150."),
                           HasSubstr("\"side\":\"sell\""), HasSubstr("\"time_in_force\":\"gtc\""))))
        .WillOnce(Return(json_response(200, R"({"id":"alpaca-2","status":"new"})")));

    qse::OrderId id = handler_->submit_limit_order("AAPL", qse::Order::Side::SELL, 50, 150.0,
                                                   qse::Order::TimeInForce::GTC);
    EXPECT_EQ(id, "alpaca-2");
}

TEST_F(AlpacaHandlerTest, RejectionReturnsEmptyId) {
    EXPECT_CALL(*http_, post(_, _, _))
        .WillOnce(Return(json_response(422, R"({"message":"insufficient buying power"})")));
    EXPECT_TRUE(handler_->submit_market_order("AAPL", qse::Order::Side::BUY, 1000000).empty());
}

TEST_F(AlpacaHandlerTest, CancelHitsDeleteEndpoint) {
    EXPECT_CALL(*http_, del("https://paper.test/v2/orders/alpaca-1", _))
        .WillOnce(Return(json_response(204, "")));
    EXPECT_TRUE(handler_->cancel_order("alpaca-1"));

    EXPECT_CALL(*http_, del("https://paper.test/v2/orders/gone", _))
        .WillOnce(Return(json_response(404, R"({"message":"order not found"})")));
    EXPECT_FALSE(handler_->cancel_order("gone"));
}

TEST_F(AlpacaHandlerTest, ReplacePatchesAndReturnsNewId) {
    EXPECT_CALL(*http_,
                patch("https://paper.test/v2/orders/alpaca-1", _,
                      AllOf(HasSubstr("\"qty\":\"200\""), HasSubstr("\"limit_price\":\"151."))))
        .WillOnce(Return(json_response(200, R"({"id":"alpaca-1b","status":"new"})")));

    EXPECT_EQ(handler_->replace_order("alpaca-1", 200, 151.0), "alpaca-1b");
}

TEST_F(AlpacaHandlerTest, GetOrderMapsFieldsAndStatuses) {
    EXPECT_CALL(*http_, get("https://paper.test/v2/orders/alpaca-3", _))
        .WillOnce(Return(json_response(200, R"({
            "id":"alpaca-3","symbol":"MSFT","side":"sell","type":"limit",
            "qty":"75","filled_qty":"30","filled_avg_price":"402.5",
            "limit_price":"402.75","status":"partially_filled",
            "time_in_force":"gtc"})")));

    auto order = handler_->get_order("alpaca-3");
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->symbol, "MSFT");
    EXPECT_EQ(order->side, qse::Order::Side::SELL);
    EXPECT_EQ(order->type, qse::Order::Type::LIMIT);
    EXPECT_EQ(order->quantity, 75);
    EXPECT_EQ(order->filled_quantity, 30);
    EXPECT_DOUBLE_EQ(order->avg_fill_price, 402.5);
    EXPECT_DOUBLE_EQ(order->limit_price, 402.75);
    EXPECT_EQ(order->status, qse::Order::Status::PARTIALLY_FILLED);
    EXPECT_EQ(order->time_in_force, qse::Order::TimeInForce::GTC);
}

TEST_F(AlpacaHandlerTest, PollFillsEmitsOnlyNewQuantity) {
    std::vector<qse::Fill> fills;
    handler_->set_fill_callback([&fills](const qse::Fill& fill) { fills.push_back(fill); });

    EXPECT_CALL(*http_, post(_, _, _))
        .WillOnce(Return(json_response(200, R"({"id":"alpaca-4","status":"accepted"})")));
    handler_->submit_market_order("AAPL", qse::Order::Side::BUY, 100);

    // Poll 1: 40 of 100 filled -> one Fill for 40
    EXPECT_CALL(*http_, get("https://paper.test/v2/orders/alpaca-4", _))
        .WillOnce(Return(json_response(200, R"({
            "id":"alpaca-4","symbol":"AAPL","side":"buy","type":"market",
            "qty":"100","filled_qty":"40","filled_avg_price":"199.5",
            "status":"partially_filled","time_in_force":"day"})")))
        .RetiresOnSaturation();
    EXPECT_EQ(handler_->poll_fills(), 1u);
    ASSERT_EQ(fills.size(), 1u);
    EXPECT_EQ(fills[0].quantity, 40);
    EXPECT_DOUBLE_EQ(fills[0].price, 199.5);
    EXPECT_EQ(fills[0].side, "BUY");

    // Poll 2: unchanged -> no duplicate fill
    EXPECT_CALL(*http_, get("https://paper.test/v2/orders/alpaca-4", _))
        .WillOnce(Return(json_response(200, R"({
            "id":"alpaca-4","symbol":"AAPL","side":"buy","type":"market",
            "qty":"100","filled_qty":"40","filled_avg_price":"199.5",
            "status":"partially_filled","time_in_force":"day"})")))
        .RetiresOnSaturation();
    EXPECT_EQ(handler_->poll_fills(), 0u);
    EXPECT_EQ(fills.size(), 1u);

    // Poll 3: fully filled -> one Fill for the remaining 60, order untracked
    EXPECT_CALL(*http_, get("https://paper.test/v2/orders/alpaca-4", _))
        .WillOnce(Return(json_response(200, R"({
            "id":"alpaca-4","symbol":"AAPL","side":"buy","type":"market",
            "qty":"100","filled_qty":"100","filled_avg_price":"199.6",
            "status":"filled","time_in_force":"day"})")));
    EXPECT_EQ(handler_->poll_fills(), 1u);
    ASSERT_EQ(fills.size(), 2u);
    EXPECT_EQ(fills[1].quantity, 60);

    // Poll 4: terminal order was untracked -> no HTTP call at all
    EXPECT_EQ(handler_->poll_fills(), 0u);
}
