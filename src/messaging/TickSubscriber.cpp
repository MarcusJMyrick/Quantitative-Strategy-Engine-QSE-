#include "qse/messaging/TickSubscriber.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>

namespace qse {

TickSubscriber::TickSubscriber(const std::string& endpoint, const std::string& topic)
    : endpoint_(endpoint), topic_(topic), running_(false) {
    try {
        context_ = std::make_unique<zmq::context_t>(1);
        socket_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_SUB);
        
        if (!topic_.empty()) {
            std::cout << "[SUBSCRIBER] Subscribing to topic: '" << topic_ << "'" << std::endl;
            socket_->set(zmq::sockopt::subscribe, topic_);
        } else {
            std::cout << "[SUBSCRIBER] Subscribing to ALL topics." << std::endl;
            socket_->set(zmq::sockopt::subscribe, "");
        }
        
        socket_->connect(endpoint_);
        std::cout << "[SUBSCRIBER] Connected to: " << endpoint_ << std::endl;
    } catch (const zmq::error_t& e) {
        std::cerr << "Failed to initialize TickSubscriber: " << e.what() << std::endl;
        throw;
    }
}

TickSubscriber::~TickSubscriber() {
    stop();
    if (socket_) {
        socket_->close();
    }
    if (context_) {
        context_->close();
    }
}

void TickSubscriber::set_tick_callback(TickCallback callback) {
    tick_callback_ = callback;
}

void TickSubscriber::set_bar_callback(BarCallback callback) {
    bar_callback_ = callback;
}

void TickSubscriber::set_order_callback(OrderCallback callback) {
    order_callback_ = callback;
}

void TickSubscriber::listen() {
    running_ = true;
    std::cout << "Starting to listen for messages..." << std::endl;
    
    while (running_) {
        try {
            zmq::message_t topic_msg;
            zmq::message_t data_msg;
            
            // Add timeout to prevent infinite blocking
            auto topic_result = socket_->recv(topic_msg, zmq::recv_flags::dontwait);
            if (!topic_result) {
                // No message available, check if we should stop
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            auto data_result = socket_->recv(data_msg, zmq::recv_flags::dontwait);
            if (!data_result) {
                continue;
            }
            
            std::string topic(static_cast<char*>(topic_msg.data()), topic_msg.size());
            std::string data(static_cast<char*>(data_msg.data()), data_msg.size());
            
            std::cout << "Received message with topic: " << topic << std::endl;
            process_message(topic, data);
            
        } catch (const zmq::error_t& e) {
            if (e.num() == ETERM) {
                std::cout << "Received termination signal" << std::endl;
                break;
            }
            if (e.num() == EAGAIN) {
                // No message available, continue
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            std::cerr << "Error receiving message: " << e.what() << std::endl;
        }
    }
    
    std::cout << "Listen loop finished." << std::endl;
}

bool TickSubscriber::try_receive() {
    zmq::message_t topic_msg;
    
    // Use a blocking receive with a small timeout to make this simpler
    socket_->set(zmq::sockopt::rcvtimeo, 10); // Set 10ms timeout
    
    auto topic_result = socket_->recv(topic_msg);
    if (!topic_result.has_value() || topic_result.value() == 0) {
        return false; // Timed out, no message received
    }
    
    std::string received_topic(static_cast<char*>(topic_msg.data()), topic_msg.size());
    std::cout << "[SUBSCRIBER] Received first message part (topic): '" << received_topic << "'" << std::endl;

    if (!socket_->get(zmq::sockopt::rcvmore)) {
        std::cout << "[SUBSCRIBER] ERROR: Message did not have a second part." << std::endl;
        return false;
    }

    zmq::message_t data_msg;
    auto data_result = socket_->recv(data_msg, zmq::recv_flags::none);
    if(data_result.has_value()) {
         std::cout << "[SUBSCRIBER] Received second message part (payload) of size " << data_result.value() << std::endl;
         std::string data(static_cast<char*>(data_msg.data()), data_msg.size());
         process_message(received_topic, data);
         return true;
    }
    
    return false;
}

void TickSubscriber::stop() {
    running_ = false;
}

void TickSubscriber::process_message(const std::string& topic, const std::string& data) {
    std::cout << "process_message called with topic: '" << topic << "'" << std::endl;
    
    // FIX: Match the topics used in your test ("TICK_DATA", "BAR_DATA", etc.)
    if (topic == "TICK_DATA" && tick_callback_) {
        std::cout << "Matched TICK_DATA, calling tick callback" << std::endl;
        Tick tick = deserialize_tick(data);
        tick_callback_(tick);
    } else if (topic == "BAR_DATA" && bar_callback_) {
        std::cout << "Matched BAR_DATA, calling bar callback" << std::endl;
        Bar bar = deserialize_bar(data);
        bar_callback_(bar);
    } else if (topic == "ORDER_DATA" && order_callback_) {
        std::cout << "Matched ORDER_DATA, calling order callback" << std::endl;
        Order order = deserialize_order(data);
        order_callback_(order);
    } else {
        // This is a good catch-all for debugging unknown topics
        std::cout << "Received message with unhandled topic: '" << topic << "'" << std::endl;
        std::cout << "Available callbacks - tick: " << (tick_callback_ ? "yes" : "no") 
                  << ", bar: " << (bar_callback_ ? "yes" : "no") 
                  << ", order: " << (order_callback_ ? "yes" : "no") << std::endl;
    }
}

Tick TickSubscriber::deserialize_tick(const std::string& data) {
    std::istringstream iss(data);
    std::string token;
    Tick tick;
    
    // Parse timestamp
    std::getline(iss, token, ',');
    auto timestamp_s = std::stoll(token);
    tick.timestamp = std::chrono::system_clock::from_time_t(timestamp_s);
    
    // Parse price
    std::getline(iss, token, ',');
    tick.price = std::stod(token);
    
    // Parse volume
    std::getline(iss, token, ',');
    tick.volume = std::stoull(token);
    
    return tick;
}

Bar TickSubscriber::deserialize_bar(const std::string& data) {
    std::istringstream iss(data);
    std::string token;
    Bar bar;
    
    // Parse timestamp
    std::getline(iss, token, ',');
    auto timestamp_s = std::stoll(token);
    bar.timestamp = std::chrono::system_clock::from_time_t(timestamp_s);
    
    // Parse symbol
    std::getline(iss, bar.symbol, ',');
    
    // Parse OHLCV
    std::getline(iss, token, ',');
    bar.open = std::stod(token);
    
    std::getline(iss, token, ',');
    bar.high = std::stod(token);
    
    std::getline(iss, token, ',');
    bar.low = std::stod(token);
    
    std::getline(iss, token, ',');
    bar.close = std::stod(token);
    
    std::getline(iss, token, ',');
    bar.volume = std::stoull(token);
    
    return bar;
}

Order TickSubscriber::deserialize_order(const std::string& data) {
    std::istringstream iss(data);
    std::string token;
    Order order;
    
    // Parse order_id
    std::getline(iss, order.order_id, ',');
    
    // Parse symbol
    std::getline(iss, order.symbol, ',');
    
    // Parse type
    std::getline(iss, token, ',');
    order.type = static_cast<Order::Type>(std::stoi(token));
    
    // Parse side
    std::getline(iss, token, ',');
    order.side = static_cast<Order::Side>(std::stoi(token));
    
    // Parse price
    std::getline(iss, token, ',');
    order.price = std::stod(token);
    
    // Parse quantity
    std::getline(iss, token, ',');
    order.quantity = std::stoull(token);
    
    // Parse status
    std::getline(iss, token, ',');
    order.status = static_cast<Order::Status>(std::stoi(token));
    
    // Parse timestamp
    std::getline(iss, token, ',');
    auto timestamp_s = std::stoll(token);
    order.timestamp = std::chrono::system_clock::from_time_t(timestamp_s);
    
    return order;
}

} // namespace qse 