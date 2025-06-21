#pragma once

#include <zmq.hpp>
#include <string>
#include <memory>
#include "qse/data/Data.h"

namespace qse {

/**
 * @brief Publishes tick data to subscribers via ZeroMQ
 * 
 * This class handles the serialization and publishing of tick data
 * to multiple subscribers using ZeroMQ's PUB-SUB pattern.
 */
class TickPublisher {
public:
    /**
     * @brief Constructor
     * @param endpoint ZeroMQ endpoint (e.g., "tcp://*:5555")
     */
    explicit TickPublisher(const std::string& endpoint);
    
    /**
     * @brief Destructor
     */
    ~TickPublisher();
    
    /**
     * @brief Publish a tick to all subscribers
     * @param topic The topic to publish on
     * @param tick The tick data to publish
     */
    void publish_tick(const std::string& topic, const Tick& tick);
    
    /**
     * @brief Publish a bar to all subscribers
     * @param topic The topic to publish on
     * @param bar The bar data to publish
     */
    void publish_bar(const std::string& topic, const Bar& bar);
    
    /**
     * @brief Publish an order to all subscribers
     * @param topic The topic to publish on
     * @param order The order data to publish
     */
    void publish_order(const std::string& topic, const Order& order);

private:
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> socket_;
    std::string endpoint_;
    
    // Helper methods for serialization
    std::string serialize_tick(const Tick& tick);
    std::string serialize_bar(const Bar& bar);
    std::string serialize_order(const Order& order);
};

} // namespace qse 