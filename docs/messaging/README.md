# QSE Messaging System

The Quantitative Strategy Engine (QSE) now includes a real-time messaging system built on ZeroMQ and Protocol Buffers for distributed data streaming and processing.

## Overview

The messaging system provides:
- **TickPublisher**: Publishes market data (ticks, bars, orders) to subscribers
- **TickSubscriber**: Receives and processes market data from publishers
- **Protocol Buffers**: Efficient serialization for network transmission
- **ZeroMQ**: High-performance messaging infrastructure

## Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Data Source   │───▶│  TickPublisher  │───▶│ TickSubscriber  │
│   (CSV, etc.)   │    │   (ZeroMQ PUB)  │    │  (ZeroMQ SUB)   │
└─────────────────┘    └─────────────────┘    └─────────────────┘
                                │                        │
                                ▼                        ▼
                       ┌─────────────────┐    ┌─────────────────┐
                       │   Strategy 1    │    │   Strategy 2    │
                       │   (Backtest)    │    │   (Live)        │
                       └─────────────────┘    └─────────────────┘
```

## Dependencies

### Required Libraries
- **ZeroMQ**: High-performance messaging library
- **Protocol Buffers**: Efficient data serialization

### Installation (macOS)
```bash
# Install ZeroMQ
brew install zeromq

# Install Protocol Buffers
brew install protobuf
```

## Usage

### Building
```bash
mkdir build && cd build
cmake ..
make -j4
```

### Running the Example

1. **Start the subscriber** (in one terminal):
```bash
./messaging_example subscriber
```

2. **Start the publisher** (in another terminal):
```bash
./messaging_example publisher
```

### Programmatic Usage

#### Publishing Data
```cpp
#include "qse/messaging/TickPublisher.h"

// Create a publisher
qse::TickPublisher publisher("tcp://*:5555");

// Publish a tick
qse::Tick tick;
tick.timestamp = std::chrono::system_clock::now();
tick.price = 150.25;
tick.volume = 1000;
publisher.publish_tick(tick);

// Publish a bar
qse::Bar bar;
bar.symbol = "AAPL";
bar.timestamp = std::chrono::system_clock::now();
bar.open = 150.0;
bar.high = 151.0;
bar.low = 149.0;
bar.close = 150.5;
bar.volume = 5000;
publisher.publish_bar(bar);
```

#### Subscribing to Data
```cpp
#include "qse/messaging/TickSubscriber.h"

// Create a subscriber
qse::TickSubscriber subscriber("tcp://localhost:5555");

// Set up callbacks
subscriber.set_tick_callback([](const qse::Tick& tick) {
    std::cout << "Received tick: price=" << tick.price 
              << ", volume=" << tick.volume << std::endl;
});

subscriber.set_bar_callback([](const qse::Bar& bar) {
    std::cout << "Received bar: " << bar.symbol 
              << " O:" << bar.open << " H:" << bar.high 
              << " L:" << bar.low << " C:" << bar.close << std::endl;
});

// Start listening (blocking)
subscriber.listen();

// Or try to receive (non-blocking)
while (subscriber.try_receive()) {
    // Process received messages
}
```

## Message Types

### Tick Messages
- **Topic**: "TICK"
- **Data**: timestamp, price, volume
- **Use Case**: Real-time price updates

### Bar Messages
- **Topic**: "BAR"
- **Data**: timestamp, symbol, OHLCV data
- **Use Case**: Time-based aggregations

### Order Messages
- **Topic**: "ORDER"
- **Data**: order details, status, timestamps
- **Use Case**: Order management and tracking

## Protocol Buffers Schema

The messaging system uses Protocol Buffers for efficient serialization:

```protobuf
syntax = "proto3";

package qse.messaging;

message Tick {
  int64 timestamp_s = 1;  // Unix timestamp in seconds
  double price = 2;
  uint64 volume = 3;
  string symbol = 4;      // Optional symbol identifier
}

message Bar {
  int64 timestamp_s = 1;  // Unix timestamp in seconds
  string symbol = 2;
  double open = 3;
  double high = 4;
  double low = 5;
  double close = 6;
  uint64 volume = 7;
}

message Order {
  // ... order fields
}
```

## Integration with QSE

The messaging system integrates seamlessly with the existing QSE components:

### DataReader Integration
```cpp
// Create a data reader that publishes to ZeroMQ
class StreamingDataReader : public qse::IDataReader {
private:
    qse::TickPublisher publisher_;
    
public:
    const std::vector<qse::Tick>& read_all_ticks() const override {
        // Read from source and publish each tick
        for (const auto& tick : ticks_) {
            publisher_.publish_tick(tick);
        }
        return ticks_;
    }
};
```

### Strategy Integration
```cpp
// Create a strategy that receives from ZeroMQ
class StreamingStrategy : public qse::IStrategy {
private:
    qse::TickSubscriber subscriber_;
    
public:
    StreamingStrategy() {
        subscriber_.set_tick_callback([this](const qse::Tick& tick) {
            this->on_tick(tick);
        });
    }
    
    void on_tick(const qse::Tick& tick) override {
        // Process incoming tick data
        // Implement trading logic here
    }
};
```

## Performance Considerations

- **ZeroMQ**: Provides high-throughput, low-latency messaging
- **Protocol Buffers**: Efficient binary serialization
- **Non-blocking**: Use `try_receive()` for high-frequency applications
- **Multiple subscribers**: ZeroMQ supports multiple subscribers per publisher

## Future Enhancements

- **Message persistence**: Add message queuing and persistence
- **Load balancing**: Implement multiple publishers/subscribers
- **Message filtering**: Add topic-based filtering
- **Compression**: Add message compression for high-volume data
- **Authentication**: Add security and authentication mechanisms 