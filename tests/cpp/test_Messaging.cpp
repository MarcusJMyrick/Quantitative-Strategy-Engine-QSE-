#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "qse/messaging/TickPublisher.h"
#include "qse/messaging/TickSubscriber.h"
#include "qse/data/Data.h"

using namespace qse;

class MessagingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Give some time for sockets to bind/unbind
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    void TearDown() override {
        // Give some time for cleanup
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

TEST_F(MessagingTest, CanCreatePublisherAndSubscriber) {
    EXPECT_NO_THROW({
        TickPublisher publisher("tcp://*:5556");
        TickSubscriber subscriber("tcp://localhost:5556");
    });
}

TEST_F(MessagingTest, CanPublishAndReceiveTick) {
    TickPublisher publisher("tcp://*:5557");
    TickSubscriber subscriber("tcp://localhost:5557");
    
    Tick received_tick;
    bool tick_received = false;
    
    subscriber.set_tick_callback([&](const Tick& tick) {
        received_tick = tick;
        tick_received = true;
    });
    
    // Start subscriber in a separate thread
    std::thread subscriber_thread([&]() {
        subscriber.listen();
    });
    
    // Give subscriber time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Publish a tick
    Tick test_tick;
    test_tick.timestamp = std::chrono::system_clock::now();
    test_tick.price = 150.25;
    test_tick.volume = 1000;
    
    publisher.publish_tick(test_tick);
    
    // Wait for message to be received
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Stop subscriber
    subscriber.stop();
    subscriber_thread.join();
    
    EXPECT_TRUE(tick_received);
    EXPECT_DOUBLE_EQ(received_tick.price, test_tick.price);
    EXPECT_EQ(received_tick.volume, test_tick.volume);
}

TEST_F(MessagingTest, CanPublishAndReceiveBar) {
    TickPublisher publisher("tcp://*:5558");
    TickSubscriber subscriber("tcp://localhost:5558");
    
    Bar received_bar;
    bool bar_received = false;
    
    subscriber.set_bar_callback([&](const Bar& bar) {
        received_bar = bar;
        bar_received = true;
    });
    
    // Start subscriber in a separate thread
    std::thread subscriber_thread([&]() {
        subscriber.listen();
    });
    
    // Give subscriber time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Publish a bar
    Bar test_bar;
    test_bar.symbol = "AAPL";
    test_bar.timestamp = std::chrono::system_clock::now();
    test_bar.open = 150.0;
    test_bar.high = 151.0;
    test_bar.low = 149.0;
    test_bar.close = 150.5;
    test_bar.volume = 5000;
    
    publisher.publish_bar(test_bar);
    
    // Wait for message to be received
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Stop subscriber
    subscriber.stop();
    subscriber_thread.join();
    
    EXPECT_TRUE(bar_received);
    EXPECT_EQ(received_bar.symbol, test_bar.symbol);
    EXPECT_DOUBLE_EQ(received_bar.open, test_bar.open);
    EXPECT_DOUBLE_EQ(received_bar.high, test_bar.high);
    EXPECT_DOUBLE_EQ(received_bar.low, test_bar.low);
    EXPECT_DOUBLE_EQ(received_bar.close, test_bar.close);
    EXPECT_EQ(received_bar.volume, test_bar.volume);
}

TEST_F(MessagingTest, NonBlockingReceive) {
    TickPublisher publisher("tcp://*:5559");
    TickSubscriber subscriber("tcp://localhost:5559");
    
    // Try to receive without any messages (should return false)
    EXPECT_FALSE(subscriber.try_receive());
    
    // Publish a tick
    Tick test_tick;
    test_tick.timestamp = std::chrono::system_clock::now();
    test_tick.price = 160.0;
    test_tick.volume = 2000;
    
    publisher.publish_tick(test_tick);
    
    // Give some time for message to arrive
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Now try to receive (should return true)
    EXPECT_TRUE(subscriber.try_receive());
} 