#include "qse/messaging/TickSubscriber.h"
#include <iostream>
#include <sstream>
#include <chrono>

namespace qse {

TickSubscriber::TickSubscriber(const std::string& endpoint, const std::string& topic)
    : endpoint_(endpoint), topic_(topic), running_(false) {
    try {
        context_ = std::make_unique<zmq::context_t>(1);
        socket_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_SUB);
        
        if (!topic_.empty()) {
            socket_->set(zmq::sockopt::subscribe, topic_);
        } else {
            socket_->set(zmq::sockopt::subscribe, "");
        }
        
        socket_->connect(endpoint_);
        std::cout << "TickSubscriber connected to: " << endpoint_ << std::endl;
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
            
            auto topic_result = socket_->recv(topic_msg);
            if (!topic_result) {
                continue;
            }
            
            auto data_result = socket_->recv(data_msg);
            if (!data_result) {
                continue;
            }
            
            std::string topic(static_cast<char*>(topic_msg.data()), topic_msg.size());
            std::string data(static_cast<char*>(data_msg.data()), data_msg.size());
            
            process_message(topic, data);
            
        } catch (const zmq::error_t& e) {
            if (e.num() == ETERM) {
                std::cout << "Received termination signal" << std::endl;
                break;
            }
            std::cerr << "Error receiving message: " << e.what() << std::endl;
        }
    }
}

bool TickSubscriber::try_receive() {
    try {
        zmq::message_t topic_msg;
        zmq::message_t data_msg;
        
        auto topic_result = socket_->recv(topic_msg, zmq::recv_flags::dontwait);
        if (!topic_result) {
            return false;
        }
        
        auto data_result = socket_->recv(data_msg, zmq::recv_flags::dontwait);
        if (!data_result) {
            return false;
        }
        
        std::string topic(static_cast<char*>(topic_msg.data()), topic_msg.size());
        std::string data(static_cast<char*>(data_msg.data()), data_msg.size());
        
        process_message(topic, data);
        return true;
        
    } catch (const zmq::error_t& e) {
        if (e.num() == EAGAIN) {
            return false; // No message available
        }
        std::cerr << "Error receiving message: " << e.what() << std::endl;
        return false;
    }
}

void TickSubscriber::stop() {
    running_ = false;
}

void TickSubscriber::process_message(const std::string& topic, const std::string& data) {
    if (topic == "TICK" && tick_callback_) {
        Tick tick = deserialize_tick(data);
        tick_callback_(tick);
    } else if (topic == "BAR" && bar_callback_) {
        Bar bar = deserialize_bar(data);
        bar_callback_(bar);
    } else if (topic == "ORDER" && order_callback_) {
        Order order = deserialize_order(data);
        order_callback_(order);
    } else {
        std::cout << "Received message with topic: " << topic 
                  << ", data: " << data << std::endl;
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