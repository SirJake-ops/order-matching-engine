#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "events/event_bus.h"
#include "orderbook/OrderBookManager.h"
#include "simulation/MarketSimulator.h"
#include "transport/MarketDataStore.h"
#include "transport/server.h"

namespace {
    volatile std::sig_atomic_t stop_requested = 0;

    void request_stop(const int) {
        stop_requested = 1;
    }
} // namespace

int main() {
    std::signal(SIGINT, request_stop);
    std::signal(SIGTERM, request_stop);

    events::event_bus bus;
    auto market_data_store = std::make_shared<server::MarketDataStore>();

    bus.subscribe("market_data", [](const std::string &msg) {
        std::cout << "[BOOK UPDATER] Received: " << msg << std::endl;
    });

    const std::vector<std::string> symbols = {
        "AAPL", "MSFT", "NVDA", "AMZN", "GOOGL", "META", "TSLA", "JPM", "BAC",
        "XOM", "SPY", "QQQ", "BTC/USD", "ETH/USD", "SOL/USD", "EUR/USD", "GBP/USD"
    };

    auto order_book_manager = std::make_shared<orderbook_manager::OrderBookManager>(symbols);
    const market::MarketSimulator simulator(bus, symbols);
    server::Server http_server(market_data_store, order_book_manager, 8080);

    std::cout << "Starting Market Data Service..." << std::endl;
    std::jthread server_thread([&http_server](const std::stop_token stop_token) {
        http_server.run(stop_token);
    });

    while (!stop_requested) {
        for (const auto &price: simulator.update()) {
            market_data_store->updatePrice(price);
            http_server.broadcastPriceUpdate(price);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    server_thread.request_stop();
    std::cout << "Market Data Service stopped." << std::endl;
}
