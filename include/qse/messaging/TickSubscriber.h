#pragma once

#include <zmq.hpp>
#include <string>
#include <memory>
#include <functional>
#include "qse/data/Data.h"

namespace qse {

/**
 * @brief Subscribes to tick data from publishers via ZeroMQ
 * 
 * This class handles the reception and deserialization of tick data
 * from publishers using ZeroMQ's PUB-SUB pattern.
 */
class TickSubscriber {
public:
    // Callback function types
    using TickCallback = std::function<void(const Tick&)>;
    using BarCallback = std::function<void(const Bar&)>;
    using OrderCallback = std::function<void(const Order&)>;
    
    /**
     * @brief Constructor
     * @param endpoint ZeroMQ endpoint (e.g., "tcp://localhost:5555")
     * @param topic Optional topic filter (e.g., "TICK" or "BAR")
     */
    explicit TickSubscriber(const std::string& endpoint, const std::string& topic = "");
    
    /**
     * @brief Destructor
     */
    ~TickSubscriber();
    
    /**
     * @brief Set callback for tick messages
     * @param callback Function to call when a tick is received
     */
    void set_tick_callback(TickCallback callback);
    
    /**
     * @brief Set callback for bar messages
     * @param callback Function to call when a bar is received
     */
    void set_bar_callback(BarCallback callback);
    
    /**
     * @brief Set callback for order messages
     * @param callback Function to call when an order is received
     */
    void set_order_callback(OrderCallback callback);
    
    /**
     * @brief Start listening for messages (blocking)
     */
    void listen();
    
    /**
     * @brief Try to receive a message (non-blocking)
     * @return true if a message was received, false otherwise
     */
    bool try_receive();
    
    /**
     * @brief Stop listening for messages
     */
    void stop();

private:
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> socket_;
    std::string endpoint_;
    std::string topic_;
    
    // Callbacks
    TickCallback tick_callback_;
    BarCallback bar_callback_;
    OrderCallback order_callback_;
    
    bool running_;
    
    // Helper methods for deserialization
    Tick deserialize_tick(const std::string& data);
    Bar deserialize_bar(const std::string& data);
    Order deserialize_order(const std::string& data);
    
    // Helper method to process received message
    void process_message(const std::string& topic, const std::string& data);
};

} // namespace qse 