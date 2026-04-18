#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <vector>

#include "events/event_bus.h"
#include "simulation/MarketSimulator.h"
#include "orderbook/OrderBookSimulator.h"
#include "transport/MarketDataStore.h"
#include "transport/server.h"

int main() {
    events::event_bus bus;
    market::OrderBookSimulator book;
    auto market_data_store = std::make_shared<server::MarketDataStore>();

    bus.subscribe("market_data", [](const std::string &msg) {
        std::cout << "[BOOK UPDATER] Received: " << msg << std::endl;
    });

    const std::vector<std::string> symbols = {
        "AAPL",
        "MSFT",
        "NVDA",
        "AMZN",
        "GOOGL",
        "META",
        "TSLA",
        "JPM",
        "BAC",
        "XOM",
        "SPY",
        "QQQ",
        "BTC/USD",
        "ETH/USD",
        "SOL/USD",
        "EUR/USD",
        "GBP/USD"
    };
    const market::MarketSimulator simulator(bus, symbols);
    server::Server http_server(market_data_store, 8080);

    std::cout << "Starting Market Data Service..." << std::endl;

    std::thread server_thread([&http_server]() {
        http_server.run();
    });

    while (true) {
        for (const auto &price: simulator.update()) {
            book.onPriceUpdate(price);
            market_data_store->updatePrice(price);
            http_server.broadcastPriceUpdate(price);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    server_thread.join();
}
