# QSE Distributed System

The Quantitative Strategy Engine now supports a distributed architecture where data publishing and strategy execution are separated into different processes, enabling real-time data streaming and scalable processing.

## Architecture Overview

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Data Source   │───▶│  DataPublisher  │───▶│ StrategyEngine  │
│   (CSV Files)   │    │   (ZeroMQ PUB)  │    │  (ZeroMQ SUB)   │
└─────────────────┘    └─────────────────┘    └─────────────────┘
                                │                        │
                                ▼                        ▼
                       ┌─────────────────┐    ┌─────────────────┐
                       │  Protocol       │    │  Trading        │
                       │  Buffers        │    │  Strategy       │
                       │  Serialization  │    │  Execution      │
                       └─────────────────┘    └─────────────────┘
```

## Components

### 1. DataPublisher (`src/publisher/main.cpp`)
- **Purpose**: Reads data from files and broadcasts it via ZeroMQ
- **Input**: CSV files with tick data
- **Output**: Protocol Buffers serialized messages over ZeroMQ
- **Protocol**: PUB-SUB pattern

### 2. StrategyEngine (`src/engine/main.cpp`)
- **Purpose**: Receives data and executes trading strategies
- **Input**: ZeroMQ messages from DataPublisher
- **Output**: Trading decisions and performance metrics
- **Protocol**: PUB-SUB pattern

### 3. ZeroMQDataReader (`include/qse/data/ZeroMQDataReader.h`)
- **Purpose**: Implements IDataReader interface for ZeroMQ data reception
- **Features**: Automatic deserialization, timeout handling, end-of-stream detection

## Installation & Setup

### Prerequisites
```bash
# Install ZeroMQ and Protocol Buffers
brew install zeromq protobuf

# Build the project
mkdir build && cd build
cmake ..
make -j4
```

### Data Format
The DataPublisher expects CSV files with the following format:
```csv
timestamp,price,volume
1640995200,150.25,1000
1640995260,150.50,1200
1640995320,150.75,1100
```

## Usage

### Manual Operation

1. **Start the DataPublisher**:
```bash
cd build
./data_publisher ../data/raw_ticks_AAPL.csv
```

2. **Start the StrategyEngine** (in another terminal):
```bash
cd build
./strategy_engine tcp://localhost:5555
```

### Automated Testing

Run the integration test script:
```bash
./scripts/test_distributed_system.sh
```

## Protocol Details

### Message Format
All messages use Protocol Buffers for efficient serialization:

```protobuf
message Tick {
  int64 timestamp_s = 1;  // Unix timestamp in seconds
  double price = 2;
  uint64 volume = 3;
  string symbol = 4;      // Optional symbol identifier
}
```

### Communication Flow
1. **DataPublisher** reads CSV file
2. **DataPublisher** serializes each tick using Protocol Buffers
3. **DataPublisher** sends serialized data via ZeroMQ PUB socket
4. **StrategyEngine** receives data via ZeroMQ SUB socket
5. **StrategyEngine** deserializes data using Protocol Buffers
6. **StrategyEngine** processes data through trading strategy
7. **DataPublisher** sends "END_OF_STREAM" signal when complete

### End-of-Stream Handling
The DataPublisher sends a special "END_OF_STREAM" message when all data has been transmitted, allowing the StrategyEngine to know when processing is complete.

## Configuration

### ZeroMQ Endpoints
- **Default**: `tcp://localhost:5555`
- **Custom**: Can be changed by modifying the endpoint parameter

### Timeouts
- **Default**: 5000ms (5 seconds)
- **Custom**: Can be set in ZeroMQDataReader constructor

### Data Rate
- **Default**: 100ms between ticks (simulates live feed)
- **Custom**: Modify sleep duration in DataPublisher

## Performance Considerations

### Latency
- **ZeroMQ**: Sub-millisecond message passing
- **Protocol Buffers**: Efficient binary serialization
- **Network**: TCP/IP for reliable delivery

### Throughput
- **Single Publisher**: Can handle thousands of ticks per second
- **Multiple Subscribers**: ZeroMQ supports multiple strategy engines
- **Memory**: Minimal memory footprint per message

### Scalability
- **Horizontal**: Add more StrategyEngine instances
- **Vertical**: Increase data rate and processing capacity
- **Load Balancing**: ZeroMQ handles message distribution automatically

## Error Handling

### Network Issues
- **Connection Loss**: Automatic reconnection attempts
- **Timeout**: Configurable timeout with graceful degradation
- **Message Loss**: TCP ensures reliable delivery

### Data Issues
- **Invalid Format**: Error logging and message skipping
- **Serialization Errors**: Detailed error messages
- **Missing Data**: Graceful handling of incomplete datasets

## Monitoring & Debugging

### Logs
Both components provide detailed logging:
- **DataPublisher**: Shows published ticks and completion status
- **StrategyEngine**: Shows received data and trading decisions

### Integration Test
The test script provides comprehensive validation:
- **Process Management**: Automatic startup and cleanup
- **Success Detection**: Checks for completion messages
- **Error Reporting**: Detailed failure analysis

## Future Enhancements

### Planned Features
- **Multiple Data Sources**: Support for live market data feeds
- **Message Persistence**: Redis/RabbitMQ for message queuing
- **Load Balancing**: Multiple publishers and subscribers
- **Compression**: Message compression for high-volume data
- **Authentication**: Security and access control

### Advanced Patterns
- **Request-Reply**: For interactive queries
- **Pipeline**: For multi-stage processing
- **Pub-Sub with Topics**: For filtered subscriptions
- **Router-Dealer**: For load balancing

## Troubleshooting

### Common Issues

1. **Publisher Won't Start**
   - Check if port 5555 is available
   - Verify data file exists and is readable
   - Check ZeroMQ installation

2. **Engine Won't Connect**
   - Ensure publisher is running
   - Check endpoint URL format
   - Verify network connectivity

3. **No Data Received**
   - Check subscription filters
   - Verify message format
   - Check timeout settings

4. **Performance Issues**
   - Increase buffer sizes
   - Reduce sleep intervals
   - Use faster network connections

### Debug Commands
```bash
# Check if ZeroMQ is working
zmq_dump tcp://localhost:5555

# Monitor network traffic
tcpdump -i lo0 port 5555

# Check process status
ps aux | grep -E "(data_publisher|strategy_engine)"
```

## API Reference

### DataPublisher
```cpp
// Command line usage
./data_publisher <data_file_path>

// Example
./data_publisher ../data/raw_ticks_AAPL.csv
```

### StrategyEngine
```cpp
// Command line usage
./strategy_engine <zmq_endpoint>

// Example
./strategy_engine tcp://localhost:5555
```

### ZeroMQDataReader
```cpp
// Constructor
ZeroMQDataReader(const std::string& endpoint, int timeout_ms = 5000);

// Methods
const std::vector<Tick>& read_all_ticks() const;
const std::vector<Bar>& read_all_bars() const;
void start_receiving();
bool is_complete() const;
```

This distributed architecture provides the foundation for scalable, real-time quantitative trading systems with clear separation of concerns and robust communication protocols. 