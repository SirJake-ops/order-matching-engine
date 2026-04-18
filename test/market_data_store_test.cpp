#include "gtest/gtest.h"

#include "transport/MarketDataStore.h"

namespace {
    TEST(MarketDataStoreTest, UpdatePriceStoresAndReplacesSymbolSnapshot) {
        server::MarketDataStore store;

        const market::MarkPrice initial_price("AAPL", 100.25, 100.30, 100.27, 1500);
        const market::MarkPrice replacement_price("AAPL", 101.50, 101.55, 101.52, 1700);

        store.updatePrice(initial_price);
        store.updatePrice(replacement_price);

        const auto stored_price = store.getPriceForSymbol("AAPL");
        ASSERT_TRUE(stored_price.has_value());
        EXPECT_DOUBLE_EQ(stored_price->getBid(), replacement_price.getBid());
        EXPECT_DOUBLE_EQ(stored_price->getAsk(), replacement_price.getAsk());
        EXPECT_EQ(store.getAllPrices().size(), 1);
    }

    TEST(MarketDataStoreTest, UnknownSymbolReturnsEmptyOptional) {
        server::MarketDataStore store;

        EXPECT_FALSE(store.getPriceForSymbol("MISSING").has_value());
    }
}
