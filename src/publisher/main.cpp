#include <iostream>
#include <chrono>
#include <thread>
#include <zmq.hpp>
#include "qse/data/CSVDataReader.h"
#include "qse/data/Data.h"

// Include the generated Protocol Buffers header
#include "tick.pb.h"

using namespace qse;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <data_file_path>" << std::endl;
        std::cout << "Example: " << argv[0] << " ../data/raw_ticks_AAPL.csv" << std::endl;
        return 1;
    }

    std::string data_file_path = argv[1];
    
    try {
        // Create ZeroMQ context and publisher socket
        zmq::context_t context(1);
        zmq::socket_t publisher(context, ZMQ_PUB);
        
        // Bind to address
        std::string endpoint = "tcp://*:5555";
        publisher.bind(endpoint);
        std::cout << "DataPublisher bound to: " << endpoint << std::endl;
        
        // Load data using existing CSVDataReader
        CSVDataReader data_reader(data_file_path);
        const std::vector<Tick>& ticks = data_reader.read_all_ticks();
        
        if (ticks.empty()) {
            std::cout << "No tick data found in file: " << data_file_path << std::endl;
            return 1;
        }
        
        std::cout << "Loaded " << ticks.size() << " ticks from " << data_file_path << std::endl;
        std::cout << "Starting to broadcast data..." << std::endl;
        
        // Loop through ticks and broadcast them
        for (const auto& tick : ticks) {
            // Create Protocol Buffers Tick message
            qse::messaging::Tick proto_tick;
            
            // Set fields from C++ Tick struct
            auto timestamp_s = std::chrono::duration_cast<std::chrono::seconds>(
                tick.timestamp.time_since_epoch()).count();
            proto_tick.set_timestamp_s(timestamp_s);
            proto_tick.set_price(tick.price);
            proto_tick.set_volume(tick.volume);
            
            // Serialize the message
            std::string serialized_tick;
            if (!proto_tick.SerializeToString(&serialized_tick)) {
                std::cerr << "Failed to serialize tick" << std::endl;
                continue;
            }
            
            // Send over the socket
            zmq::message_t message(serialized_tick.data(), serialized_tick.size());
            publisher.send(message, zmq::send_flags::none);
            
            std::cout << "Published tick: price=" << tick.price 
                      << ", volume=" << tick.volume 
                      << ", timestamp=" << timestamp_s << std::endl;
            
            // Add a small sleep to simulate a live feed
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Send end-of-stream message
        std::string eos_message = "END_OF_STREAM";
        zmq::message_t eos_msg(eos_message.data(), eos_message.size());
        publisher.send(eos_msg, zmq::send_flags::none);
        
        std::cout << "Data broadcast completed. Sent " << ticks.size() << " ticks." << std::endl;
        
        // Keep the publisher alive for a moment to ensure all messages are sent
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 