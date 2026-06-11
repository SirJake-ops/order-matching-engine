#include "gtest/gtest.h"

#include "orderbook/OrderBookManager.h"

namespace {
using orderbook::Order;
using orderbook::OrderBook;
using orderbook::OrderType;
using orderbook::Side;
using orderbook_manager::OrderBookManager;

Order makeOrder(const std::string &id, const Side side, const double price,
                const std::int64_t quantity, const std::string &symbol = "AAPL",
                const std::uint64_t timestamp = 1,
                const OrderType type = OrderType::LIMIT) {
    return Order{id, symbol, side, type, price, quantity, timestamp};
}

TEST(OrderBookManagerTest, ConstructorCreatesBookForConfiguredSymbol) {
    OrderBookManager manager("AAPL");

    const OrderBook &book = manager.get_orderbook("AAPL");

    EXPECT_FALSE(book.bestBid().has_value());
    EXPECT_FALSE(book.bestAsk().has_value());
}

TEST(OrderBookManagerTest, AddOrderRoutesOrderToConfiguredSymbolBook) {
    OrderBookManager manager("AAPL");

    manager.add_order(makeOrder("buy-1", Side::BUY, 100.0, 5));

    const auto best_bid = manager.get_orderbook("AAPL").bestBid();
    ASSERT_TRUE(best_bid.has_value());
    EXPECT_EQ(best_bid->_id, "buy-1");
    EXPECT_DOUBLE_EQ(best_bid->_price, 100.0);
    EXPECT_EQ(best_bid->_quantity, 5);
}

TEST(OrderBookManagerTest, AddOrderMatchesAgainstExistingOrdersInConfiguredBook) {
    OrderBookManager manager("AAPL");

    manager.add_order(makeOrder("sell-1", Side::SELL, 100.0, 5));
    manager.add_order(makeOrder("buy-1", Side::BUY, 101.0, 3));

    const auto best_ask = manager.get_orderbook("AAPL").bestAsk();
    ASSERT_TRUE(best_ask.has_value());
    EXPECT_EQ(best_ask->_id, "sell-1");
    EXPECT_EQ(best_ask->_quantity, 2);
    EXPECT_FALSE(manager.get_orderbook("AAPL").bestBid().has_value());
}

TEST(OrderBookManagerTest, GetOrderBookThrowsForUnknownSymbol) {
    OrderBookManager manager("AAPL");

    EXPECT_THROW(manager.get_orderbook("MSFT"), std::runtime_error);
}

TEST(OrderBookManagerTest, AddOrderForUnknownSymbolDoesNotCreateBook) {
    OrderBookManager manager("AAPL");

    manager.add_order(makeOrder("msft-buy-1", Side::BUY, 100.0, 5, "MSFT"));

    EXPECT_THROW(manager.get_orderbook("MSFT"), std::runtime_error);
}
} // namespace
