#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <optional>
#include "qse/messaging/TickPublisher.h"
#include "qse/messaging/TickSubscriber.h"
#include "qse/data/Data.h"

using namespace qse;

// A simple test fixture is sufficient.
class MessagingTest : public ::testing::Test {};

TEST_F(MessagingTest, CanCreatePublisherAndSubscriber) {
    // Using "inproc" is faster and more reliable for self-contained tests
    const std::string endpoint = "inproc://creation_test";
    EXPECT_NO_THROW({
        TickPublisher publisher(endpoint);
        TickSubscriber subscriber(endpoint);
    });
}

TEST_F(MessagingTest, CanPublishAndReceiveTick) {
    const std::string endpoint = "tcp://127.0.0.1:5555";  // Use TCP instead of inproc
    const std::string topic = "TICK_DATA"; // Define a clear topic

    // 1. Define the data you want to send
    Tick sent_tick;
    sent_tick.timestamp = std::chrono::system_clock::now();
    sent_tick.price = 150.25;
    sent_tick.volume = 1000;

    // 2. Set up subscriber with the correct topic
    std::optional<Tick> received_tick;
    std::atomic<bool> message_received{false};
    
    TickSubscriber subscriber(endpoint, topic); // Use the topic variable
    subscriber.set_tick_callback([&](const Tick& tick) {
        received_tick = tick;
        message_received = true;
    });

    // 3. Run the publisher in a separate thread.
    std::thread publisher_thread([&]() {
        TickPublisher publisher(endpoint);
        // Give the subscriber time to connect before we publish
        // This solves the "Slow Joiner" problem.
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        // FIX: Pass the topic when publishing
        publisher.publish_tick(topic, sent_tick); // Use the topic variable
    });

    // 4. The main test thread acts as the subscriber.
    // Use try_receive() in a loop with timeout instead of blocking listen()
    auto start = std::chrono::steady_clock::now();
    int receive_attempts = 0;
    while (!message_received && 
           std::chrono::steady_clock::now() - start < std::chrono::milliseconds(3000)) {
        bool received = subscriber.try_receive();
        receive_attempts++;
        if (receive_attempts % 100 == 0) {
            std::cout << "Receive attempts: " << receive_attempts 
                      << ", message_received: " << message_received << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::cout << "Final receive attempts: " << receive_attempts 
              << ", message_received: " << message_received << std::endl;

    // 5. Clean up the publisher thread.
    publisher_thread.join();

    // 6. Assert that we received the message and its contents are correct.
    ASSERT_TRUE(message_received) << "Test failed: Message was not received.";
    ASSERT_TRUE(received_tick.has_value()) << "Test failed: No tick data received.";
    
    EXPECT_DOUBLE_EQ(received_tick->price, sent_tick.price);
    EXPECT_EQ(received_tick->volume, sent_tick.volume);
}

TEST_F(MessagingTest, CanPublishAndReceiveBar) {
    const std::string endpoint = "tcp://127.0.0.1:5556";  // Use TCP instead of inproc, different port
    const std::string topic = "BAR_DATA"; // Define a clear topic

    // 1. Define the bar data
    Bar sent_bar;
    sent_bar.symbol = "MSFT";
    sent_bar.timestamp = std::chrono::system_clock::now();
    sent_bar.open = 150.0;
    sent_bar.high = 151.0;
    sent_bar.low = 149.0;
    sent_bar.close = 150.5;
    sent_bar.volume = 5000;

    // 2. Set up subscriber with the correct topic
    std::optional<Bar> received_bar;
    std::atomic<bool> message_received{false};
    
    TickSubscriber subscriber(endpoint, topic); // Use the topic variable
    subscriber.set_bar_callback([&](const Bar& bar) {
        received_bar = bar;
        message_received = true;
    });

    // 3. Run the publisher in the background
    std::thread publisher_thread([&]() {
        TickPublisher publisher(endpoint);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Give subscriber time to connect
        // FIX: Pass the topic when publishing
        publisher.publish_bar(topic, sent_bar); // Use the topic variable
    });

    // 4. Subscribe and receive in the main thread
    auto start = std::chrono::steady_clock::now();
    int receive_attempts = 0;
    while (!message_received && 
           std::chrono::steady_clock::now() - start < std::chrono::milliseconds(3000)) {
        bool received = subscriber.try_receive();
        receive_attempts++;
        if (receive_attempts % 100 == 0) {
            std::cout << "Receive attempts: " << receive_attempts 
                      << ", message_received: " << message_received << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::cout << "Final receive attempts: " << receive_attempts 
              << ", message_received: " << message_received << std::endl;

    // 5. Clean up
    publisher_thread.join();

    // 6. Assert results
    ASSERT_TRUE(message_received) << "Test failed: Bar message was not received.";
    ASSERT_TRUE(received_bar.has_value()) << "Test failed: No bar data received.";
    EXPECT_EQ(received_bar->symbol, sent_bar.symbol);
    EXPECT_EQ(received_bar->close, sent_bar.close);
}