#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include "qse/messaging/TickPublisher.h"
#include "qse/messaging/TickSubscriber.h"
#include "qse/data/Data.h"

using namespace qse;

void run_publisher() {
    try {
        TickPublisher publisher("tcp://*:5555");
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> price_dist(100.0, 200.0);
        std::uniform_int_distribution<> volume_dist(100, 1000);
        
        std::cout << "Publisher started. Press Ctrl+C to stop." << std::endl;
        
        for (int i = 0; i < 10; ++i) {
            // Create a sample tick
            Tick tick;
            tick.timestamp = std::chrono::system_clock::now();
            tick.price = price_dist(gen);
            tick.volume = volume_dist(gen);
            
            publisher.publish_tick("TICK_DATA", tick);
            
            // Create a sample bar
            Bar bar;
            bar.symbol = "AAPL";
            bar.timestamp = std::chrono::system_clock::now();
            bar.open = tick.price;
            bar.high = tick.price + 1.0;
            bar.low = tick.price - 1.0;
            bar.close = tick.price + 0.5;
            bar.volume = tick.volume;
            
            publisher.publish_bar("BAR_DATA", bar);
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Publisher error: " << e.what() << std::endl;
    }
}

void run_subscriber() {
    try {
        // Subscribe to both tick and bar topics
        TickSubscriber tick_subscriber("tcp://localhost:5555", "TICK_DATA");
        TickSubscriber bar_subscriber("tcp://localhost:5555", "BAR_DATA");
        
        // Set up callbacks
        tick_subscriber.set_tick_callback([](const Tick& tick) {
            std::cout << "Received tick: price=" << tick.price 
                      << ", volume=" << tick.volume << std::endl;
        });
        
        bar_subscriber.set_bar_callback([](const Bar& bar) {
            std::cout << "Received bar: " << bar.symbol 
                      << " O:" << bar.open << " H:" << bar.high 
                      << " L:" << bar.low << " C:" << bar.close << std::endl;
        });
        
        std::cout << "Subscriber started. Listening for messages..." << std::endl;
        
        // Listen for messages from both subscribers
        std::thread tick_thread([&]() {
            tick_subscriber.listen();
        });
        
        std::thread bar_thread([&]() {
            bar_subscriber.listen();
        });
        
        tick_thread.join();
        bar_thread.join();
        
    } catch (const std::exception& e) {
        std::cerr << "Subscriber error: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " [publisher|subscriber]" << std::endl;
        return 1;
    }
    
    std::string mode = argv[1];
    
    if (mode == "publisher") {
        run_publisher();
    } else if (mode == "subscriber") {
        run_subscriber();
    } else {
        std::cerr << "Invalid mode. Use 'publisher' or 'subscriber'" << std::endl;
        return 1;
    }
    
    return 0;
} 