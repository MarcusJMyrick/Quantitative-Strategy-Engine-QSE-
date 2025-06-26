#include "qse/data/ZeroMQDataReader.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <array>

// Include the generated Protocol Buffers header
#include "tick.pb.h"

#define ZMQ_READER_DEBUG 0

#if ZMQ_READER_DEBUG
    #define ZMQ_LOG(expr) do { expr; } while(0)
#else
    #define ZMQ_LOG(expr) do {} while(0)
#endif

namespace qse {

constexpr int ZMQ_HWM       = 100000;           // high-water mark
constexpr int ZMQ_BUF_SIZE  = 4 * 1024 * 1024; // 4 MB OS buffer

ZeroMQDataReader::ZeroMQDataReader(const std::string& endpoint, int timeout_ms)
    : endpoint_(endpoint), timeout_ms_(timeout_ms), data_loaded_(false), reception_complete_(false) {
    try {
        context_ = std::make_unique<zmq::context_t>(1);
        socket_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_SUB);
        
        // Set subscription filter (empty string means receive all messages)
        socket_->set(zmq::sockopt::subscribe, "");
        
        // High-water mark & buffer tuning (Stage-2 optimisation)
        socket_->set(zmq::sockopt::rcvhwm,  ZMQ_HWM);
        socket_->set(zmq::sockopt::rcvbuf, ZMQ_BUF_SIZE);

        // Non-blocking – we'll poll manually
        socket_->set(zmq::sockopt::rcvtimeo, 0);
        
        // Connect to the publisher
        socket_->connect(endpoint_);
        ZMQ_LOG(std::cout << "ZeroMQDataReader connected to: " << endpoint_ << std::endl;);
        
    } catch (const zmq::error_t& e) {
        std::cerr << "Failed to initialize ZeroMQDataReader: " << e.what() << std::endl;
        throw;
    }
}

ZeroMQDataReader::~ZeroMQDataReader() {
    if (socket_) {
        socket_->close();
    }
    if (context_) {
        context_->close();
    }
}

const std::vector<Tick>& ZeroMQDataReader::read_all_ticks() const {
    if (!data_loaded_) {
        start_receiving();
    }
    return received_ticks_;
}

const std::vector<Bar>& ZeroMQDataReader::read_all_bars() const {
    if (!data_loaded_) {
        start_receiving();
    }
    return received_bars_;
}

void ZeroMQDataReader::start_receiving() const {
    if (data_loaded_) {
        return; // Already loaded
    }
    
    ZMQ_LOG(std::cout << "Starting to receive data from publisher..." << std::endl;);
    
    try {
        // Stage-3 optimisation: poll-based async loop (1-ms timeout)
        zmq::pollitem_t pi{ *socket_, 0, ZMQ_POLLIN, 0 };

        while (!reception_complete_) {
            zmq::poll(&pi, 1, 1); // 1 ms poll
            if (!(pi.revents & ZMQ_POLLIN)) {
                continue; // nothing ready
            }

            zmq::message_t message;
            socket_->recv(message, zmq::recv_flags::none);
            
            // Extract message data (zero-copy string view)
            std::string data(static_cast<char*>(message.data()), message.size());
            
            // Check for end-of-stream message
            if (data == "END_OF_STREAM") {
                ZMQ_LOG(std::cout << "Received end-of-stream signal" << std::endl;);
                reception_complete_ = true;
                break;
            }
            
            // Deserialize the tick data
            try {
                Tick tick = deserialize_tick(data);
                received_ticks_.push_back(tick);
                
                received_ticks_.push_back(tick);
#if ZMQ_READER_DEBUG
                ZMQ_LOG(std::cout << "Received tick: price=" << tick.price 
                                   << ", volume=" << tick.volume << std::endl;);
#endif
            } catch (const std::exception& e) {
                std::cerr << "Failed to deserialize tick: " << e.what() << std::endl;
                continue;
            }
        }
        
        data_loaded_ = true;
        ZMQ_LOG(std::cout << "Data reception completed. Received " << received_ticks_.size() << " ticks." << std::endl;);
        
    } catch (const zmq::error_t& e) {
        if (e.num() == EAGAIN) {
            ZMQ_LOG(std::cout << "Timeout reached while waiting for data" << std::endl;);
        } else {
            std::cerr << "Error receiving data: " << e.what() << std::endl;
        }
        data_loaded_ = true;
    }
}

bool ZeroMQDataReader::is_complete() const {
    return reception_complete_;
}

Tick ZeroMQDataReader::deserialize_tick(const std::string& data) const {
    // Parse the Protocol Buffers message
    qse::messaging::Tick proto_tick;
    if (!proto_tick.ParseFromString(data)) {
        throw std::runtime_error("Failed to parse Protocol Buffers message");
    }
    
    // Convert to C++ Tick struct
    Tick tick;
    tick.timestamp = convert_timestamp(proto_tick.timestamp_s());
    tick.price = proto_tick.price();
    tick.volume = proto_tick.volume();
    
    return tick;
}

std::chrono::system_clock::time_point ZeroMQDataReader::convert_timestamp(int64_t timestamp_s) const {
    return std::chrono::system_clock::from_time_t(timestamp_s);
}

} // namespace qse 