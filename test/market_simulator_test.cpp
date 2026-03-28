#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "events/event_bus.h"
#include "simulation/MarketSimulator.h"

namespace {
    TEST(MarketSimulatorTest, UpdatePublishesOneEventPerSymbolAndReturnsPrices) {
        events::event_bus bus;
        std::vector<std::string> published_messages;
        bus.subscribe("market_data", [&](const std::string &message) {
            published_messages.push_back(message);
        });

        const std::vector<std::string> symbols = {"AAPL", "BTC/USD", "EUR/USD"};
        const market::MarketSimulator simulator(bus, symbols);

        const auto prices = simulator.update();

        ASSERT_EQ(prices.size(), symbols.size());
        ASSERT_EQ(published_messages.size(), symbols.size());
        for (std::size_t index = 0; index < symbols.size(); ++index) {
            EXPECT_EQ(prices[index].getSymbol(), symbols[index]);
            EXPECT_NE(published_messages[index].find("SYMBOL:" + symbols[index]), std::string::npos);
            EXPECT_NE(published_messages[index].find("BID:"), std::string::npos);
        }
    }
}
