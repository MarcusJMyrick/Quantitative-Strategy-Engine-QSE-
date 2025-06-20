#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <zmq.hpp>
#include "qse/data/ZeroMQDataReader.h"
#include "qse/data/Data.h"

// Include the generated Protocol Buffers header
#include "tick.pb.h"

using namespace qse;

class ZeroMQDataReaderTest : public ::testing::Test {
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

TEST_F(ZeroMQDataReaderTest, CanCreateZeroMQDataReader) {
    EXPECT_NO_THROW({
        ZeroMQDataReader reader("tcp://localhost:5560");
    });
}

TEST_F(ZeroMQDataReaderTest, ProtocolBuffersSerializationTest) {
    // Create a test tick
    Tick original_tick;
    original_tick.timestamp = std::chrono::system_clock::now();
    original_tick.price = 150.25;
    original_tick.volume = 1000;
    
    // Serialize using Protocol Buffers
    qse::messaging::Tick proto_tick;
    auto timestamp_s = std::chrono::duration_cast<std::chrono::seconds>(
        original_tick.timestamp.time_since_epoch()).count();
    proto_tick.set_timestamp_s(timestamp_s);
    proto_tick.set_price(original_tick.price);
    proto_tick.set_volume(original_tick.volume);
    
    std::string serialized;
    EXPECT_TRUE(proto_tick.SerializeToString(&serialized));
    
    // Deserialize using Protocol Buffers
    qse::messaging::Tick deserialized_proto;
    EXPECT_TRUE(deserialized_proto.ParseFromString(serialized));
    
    // Convert back to C++ struct
    Tick deserialized_tick;
    deserialized_tick.timestamp = std::chrono::system_clock::from_time_t(deserialized_proto.timestamp_s());
    deserialized_tick.price = deserialized_proto.price();
    deserialized_tick.volume = deserialized_proto.volume();
    
    // Verify data integrity
    EXPECT_DOUBLE_EQ(original_tick.price, deserialized_tick.price);
    EXPECT_EQ(original_tick.volume, deserialized_tick.volume);
    
    // Note: Timestamp comparison might be tricky due to precision, so we'll just check they're close
    auto time_diff = std::abs(std::chrono::duration_cast<std::chrono::seconds>(
        original_tick.timestamp - deserialized_tick.timestamp).count());
    EXPECT_LE(time_diff, 1); // Within 1 second
}

TEST_F(ZeroMQDataReaderTest, CanReceiveDataFromPublisher) {
    // Create a simple publisher in a separate thread
    std::thread publisher_thread([]() {
        try {
            zmq::context_t context(1);
            zmq::socket_t publisher(context, ZMQ_PUB);
            publisher.bind("tcp://*:5561");
            
            // Wait a bit for binding
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Create and send a test tick
            qse::messaging::Tick proto_tick;
            proto_tick.set_timestamp_s(std::time(nullptr));
            proto_tick.set_price(160.0);
            proto_tick.set_volume(2000);
            
            std::string serialized;
            proto_tick.SerializeToString(&serialized);
            
            zmq::message_t message(serialized.data(), serialized.size());
            publisher.send(message, zmq::send_flags::none);
            
            // Send end-of-stream
            std::string eos = "END_OF_STREAM";
            zmq::message_t eos_msg(eos.data(), eos.size());
            publisher.send(eos_msg, zmq::send_flags::none);
            
        } catch (const std::exception& e) {
            std::cerr << "Publisher error: " << e.what() << std::endl;
        }
    });
    
    // Create the data reader
    ZeroMQDataReader reader("tcp://localhost:5561", 2000); // 2 second timeout
    
    // Start receiving data
    reader.start_receiving();
    
    // Wait for publisher to finish
    publisher_thread.join();
    
    // Check that we received data
    const auto& ticks = reader.read_all_ticks();
    EXPECT_EQ(ticks.size(), 1);
    
    if (!ticks.empty()) {
        EXPECT_DOUBLE_EQ(ticks[0].price, 160.0);
        EXPECT_EQ(ticks[0].volume, 2000);
    }
    
    EXPECT_TRUE(reader.is_complete());
}

TEST_F(ZeroMQDataReaderTest, HandlesTimeoutGracefully) {
    // Create a reader with a short timeout
    ZeroMQDataReader reader("tcp://localhost:5562", 100); // 100ms timeout
    
    // Try to receive data (should timeout)
    reader.start_receiving();
    
    const auto& ticks = reader.read_all_ticks();
    EXPECT_EQ(ticks.size(), 0);
    EXPECT_FALSE(reader.is_complete());
} 