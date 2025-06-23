#include "qse/messaging/TickPublisher.h"
#include <iostream>
#include <sstream>
#include <chrono>

namespace qse {

TickPublisher::TickPublisher(const std::string& endpoint) 
    : endpoint_(endpoint) {
    try {
        context_ = std::make_unique<zmq::context_t>(1);
        socket_ = std::make_unique<zmq::socket_t>(*context_, ZMQ_PUB);
        socket_->bind(endpoint_);
        std::cout << "TickPublisher bound to: " << endpoint_ << std::endl;
    } catch (const zmq::error_t& e) {
        std::cerr << "Failed to initialize TickPublisher: " << e.what() << std::endl;
        throw;
    }
}

TickPublisher::~TickPublisher() {
    if (socket_) {
        socket_->close();
    }
    if (context_) {
        context_->close();
    }
}

void TickPublisher::publish_tick(const std::string& topic, const Tick& tick) {
    try {
        std::string serialized = serialize_tick(tick);
        
        std::cout << "[PUBLISHER] Sending " << topic.size() << "-byte topic: '" << topic << "'" << std::endl;
        std::cout << "[PUBLISHER] Sending " << serialized.size() << "-byte payload." << std::endl;
        
        zmq::message_t topic_msg(topic.data(), topic.size());
        zmq::message_t data_msg(serialized.data(), serialized.size());
        
        socket_->send(topic_msg, zmq::send_flags::sndmore);
        socket_->send(data_msg, zmq::send_flags::none);
        
        std::cout << "Published tick: price=" << tick.price 
                  << ", volume=" << tick.volume << std::endl;
    } catch (const zmq::error_t& e) {
        std::cerr << "Failed to publish tick: " << e.what() << std::endl;
    }
}

void TickPublisher::publish_bar(const std::string& topic, const Bar& bar) {
    try {
        std::string serialized = serialize_bar(bar);
        
        std::cout << "[PUBLISHER] Sending " << topic.size() << "-byte topic: '" << topic << "'" << std::endl;
        std::cout << "[PUBLISHER] Sending " << serialized.size() << "-byte payload." << std::endl;
        
        zmq::message_t topic_msg(topic.data(), topic.size());
        zmq::message_t data_msg(serialized.data(), serialized.size());
        
        socket_->send(topic_msg, zmq::send_flags::sndmore);
        socket_->send(data_msg, zmq::send_flags::none);
        
        std::cout << "Published bar: " << bar.symbol 
                  << " O:" << bar.open << " H:" << bar.high 
                  << " L:" << bar.low << " C:" << bar.close << std::endl;
    } catch (const zmq::error_t& e) {
        std::cerr << "Failed to publish bar: " << e.what() << std::endl;
    }
}

void TickPublisher::publish_order(const std::string& topic, const Order& order) {
    try {
        std::string serialized = serialize_order(order);
        zmq::message_t topic_msg(topic.data(), topic.size());
        zmq::message_t data_msg(serialized.data(), serialized.size());
        
        socket_->send(topic_msg, zmq::send_flags::sndmore);
        socket_->send(data_msg, zmq::send_flags::none);
        
        std::cout << "Published order: " << order.order_id 
                  << " " << order.symbol << std::endl;
    } catch (const zmq::error_t& e) {
        std::cerr << "Failed to publish order: " << e.what() << std::endl;
    }
}

std::string TickPublisher::serialize_tick(const Tick& tick) {
    // Simple CSV-like serialization for now
    // In a real implementation, you'd use Protocol Buffers
    std::ostringstream oss;
    auto timestamp_s = std::chrono::duration_cast<std::chrono::seconds>(
        tick.timestamp.time_since_epoch()).count();
    oss << timestamp_s << "," << tick.price << "," << tick.volume;
    return oss.str();
}

std::string TickPublisher::serialize_bar(const Bar& bar) {
    std::ostringstream oss;
    auto timestamp_s = std::chrono::duration_cast<std::chrono::seconds>(
        bar.timestamp.time_since_epoch()).count();
    oss << timestamp_s << "," << bar.symbol << "," << bar.open 
        << "," << bar.high << "," << bar.low << "," << bar.close 
        << "," << bar.volume;
    return oss.str();
}

std::string TickPublisher::serialize_order(const Order& order) {
    std::ostringstream oss;
    auto timestamp_s = std::chrono::duration_cast<std::chrono::seconds>(
        order.timestamp.time_since_epoch()).count();
    oss << order.order_id << "," << order.symbol << "," 
        << static_cast<int>(order.type) << "," << static_cast<int>(order.side)
        << "," << order.limit_price << "," << order.quantity 
        << "," << static_cast<int>(order.status) << "," << timestamp_s;
    return oss.str();
}

} // namespace qse 