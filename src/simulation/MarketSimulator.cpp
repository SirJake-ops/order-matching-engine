#include "MarketSimulator.h"
#include "pricing/PriceGenerator.h"
#include "events/event_bus.h"
#include <string>

namespace market {
    MarketSimulator::MarketSimulator(events::event_bus& bus, std::vector<std::string> symbols)
        : _bus(bus), _symbols(std::move(symbols)) {}

    void MarketSimulator::update() const {
        for (const auto& symbol : _symbols) {
            auto price = PriceGenerator::generatePrice(symbol);
            std::string msg = "SYMBOL:" + price.getSymbol() + " BID:" + std::to_string(price.getBid());
            _bus.publish("market_data", msg);
        }
    }
}