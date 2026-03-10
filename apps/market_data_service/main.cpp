#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

#include "events/event_bus.h"
#include "simulation/MarketSimulator.h"
#include "orderbook/OrderBookSimulator.h"

[[noreturn]] int main() {
    events::event_bus bus;
    market::OrderBookSimulator book;

    bus.subscribe("market_data", [](const std::string& msg) {
        std::cout << "[BOOK UPDATER] Received: " << msg << std::endl;
    });

    std::vector<std::string> symbols = {"AAPL", "GOOGL", "MSFT"};
    market::MarketSimulator simulator(bus, symbols);

    std::cout << "Starting Market Data Service..." << std::endl;

    while (true) {
        simulator.update();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Service stopped." << std::endl;
}