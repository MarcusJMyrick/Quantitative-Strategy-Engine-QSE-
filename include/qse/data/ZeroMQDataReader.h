#pragma once

#include "qse/data/IDataReader.h"
#include <zmq.hpp>
#include <string>
#include <memory>
#include <vector>

namespace qse {

/**
 * @brief DataReader implementation that receives data via ZeroMQ
 * 
 * This class connects to a ZeroMQ publisher and receives tick data
 * in real-time, implementing the IDataReader interface.
 */
class ZeroMQDataReader : public IDataReader {
public:
    /**
     * @brief Constructor
     * @param endpoint ZeroMQ endpoint to connect to (e.g., "tcp://localhost:5555")
     * @param timeout_ms Timeout for receiving messages in milliseconds
     */
    explicit ZeroMQDataReader(const std::string& endpoint, int timeout_ms = 5000);
    
    /**
     * @brief Destructor
     */
    ~ZeroMQDataReader();
    
    /**
     * @brief Read all ticks from the ZeroMQ stream
     * @return Vector of received ticks
     */
    const std::vector<Tick>& read_all_ticks() const override;
    
    /**
     * @brief Read all bars from the ZeroMQ stream
     * @return Vector of received bars (empty for now, as we're focusing on ticks)
     */
    const std::vector<Bar>& read_all_bars() const override;
    
    /**
     * @brief Start receiving data from the publisher
     * This method will block until all data is received or timeout occurs
     */
    void start_receiving() const;
    
    /**
     * @brief Check if data reception is complete
     * @return true if all data has been received
     */
    bool is_complete() const;

private:
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> socket_;
    std::string endpoint_;
    int timeout_ms_;
    
    mutable std::vector<Tick> received_ticks_;
    mutable std::vector<Bar> received_bars_;
    mutable bool data_loaded_;
    mutable bool reception_complete_;
    
    /**
     * @brief Deserialize a Protocol Buffers Tick message
     * @param data Serialized data
     * @return Deserialized Tick object
     */
    Tick deserialize_tick(const std::string& data) const;
    
    /**
     * @brief Convert Protocol Buffers timestamp to C++ timestamp
     * @param timestamp_s Unix timestamp in seconds
     * @return C++ timestamp
     */
    std::chrono::system_clock::time_point convert_timestamp(int64_t timestamp_s) const;
};

} // namespace qse 