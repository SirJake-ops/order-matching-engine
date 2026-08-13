#include <thread>
#include <vector>

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
    OrderBookManager manager({"AAPL"});

    const OrderBook &book = manager.get_orderbook("AAPL");

    EXPECT_FALSE(book.bestBid().has_value());
    EXPECT_FALSE(book.bestAsk().has_value());
}

TEST(OrderBookManagerTest, ConstructorCreatesBooksForMultipleConfiguredSymbols) {
    OrderBookManager manager({"AAPL", "MSFT"});

    EXPECT_FALSE(manager.get_orderbook("AAPL").bestBid().has_value());
    EXPECT_FALSE(manager.get_orderbook("MSFT").bestBid().has_value());
}

TEST(OrderBookManagerTest, ConstructorThrowsForDuplicateSymbols) {
    EXPECT_THROW(OrderBookManager({"AAPL", "MSFT", "AAPL"}), std::runtime_error);
}

TEST(OrderBookManagerTest, AddOrderRoutesOrderToConfiguredSymbolBook) {
    OrderBookManager manager({"AAPL"});

    const auto trades = manager.add_order(makeOrder("buy-1", Side::BUY, 100.0, 5));

    EXPECT_TRUE(trades.empty());
    EXPECT_TRUE(manager.has_active_order("buy-1"));
    const auto best_bid = manager.get_orderbook("AAPL").bestBid();
    ASSERT_TRUE(best_bid.has_value());
    EXPECT_EQ(best_bid->_id, "buy-1");
    EXPECT_DOUBLE_EQ(best_bid->_price, 100.0);
    EXPECT_EQ(best_bid->_quantity, 5);
}

TEST(OrderBookManagerTest, AddOrderRoutesOrdersToTheirOwnSymbolBooks) {
    OrderBookManager manager({"AAPL", "MSFT"});

    manager.add_order(makeOrder("aapl-buy-1", Side::BUY, 100.0, 5, "AAPL"));
    manager.add_order(makeOrder("msft-buy-1", Side::BUY, 410.0, 7, "MSFT"));

    const auto aapl_best_bid = manager.get_orderbook("AAPL").bestBid();
    const auto msft_best_bid = manager.get_orderbook("MSFT").bestBid();
    ASSERT_TRUE(aapl_best_bid.has_value());
    ASSERT_TRUE(msft_best_bid.has_value());
    EXPECT_EQ(aapl_best_bid->_id, "aapl-buy-1");
    EXPECT_EQ(msft_best_bid->_id, "msft-buy-1");
    EXPECT_DOUBLE_EQ(aapl_best_bid->_price, 100.0);
    EXPECT_DOUBLE_EQ(msft_best_bid->_price, 410.0);
}

TEST(OrderBookManagerTest, AddOrderMatchesAgainstExistingOrdersInConfiguredBook) {
    OrderBookManager manager({"AAPL"});

    manager.add_order(makeOrder("sell-1", Side::SELL, 100.0, 5));
    const auto trades = manager.add_order(makeOrder("buy-1", Side::BUY, 101.0, 3));

    ASSERT_EQ(trades.size(), 1);
    EXPECT_EQ(trades.front()._buy_order_id, "buy-1");
    EXPECT_EQ(trades.front()._sell_order_id, "sell-1");
    EXPECT_DOUBLE_EQ(trades.front()._price, 100.0);
    EXPECT_EQ(trades.front()._quantity, 3);
    const auto best_ask = manager.get_orderbook("AAPL").bestAsk();
    ASSERT_TRUE(best_ask.has_value());
    EXPECT_EQ(best_ask->_id, "sell-1");
    EXPECT_EQ(best_ask->_quantity, 2);
    EXPECT_FALSE(manager.get_orderbook("AAPL").bestBid().has_value());
}

TEST(OrderBookManagerTest, FullyFilledIncomingOrderIsNotActive) {
    OrderBookManager manager({"AAPL"});

    manager.add_order(makeOrder("sell-1", Side::SELL, 100.0, 5));
    manager.add_order(makeOrder("buy-1", Side::BUY, 100.0, 5, "AAPL", 2));

    EXPECT_FALSE(manager.has_active_order("buy-1"));
    EXPECT_FALSE(manager.has_active_order("sell-1"));
}

TEST(OrderBookManagerTest, PartiallyFilledRestingOrderRemainsActive) {
    OrderBookManager manager({"AAPL"});

    manager.add_order(makeOrder("sell-1", Side::SELL, 100.0, 5));
    manager.add_order(makeOrder("buy-1", Side::BUY, 100.0, 3, "AAPL", 2));

    EXPECT_FALSE(manager.has_active_order("buy-1"));
    EXPECT_TRUE(manager.has_active_order("sell-1"));
}

TEST(OrderBookManagerTest, PartiallyFilledIncomingLimitOrderRemainsActive) {
    OrderBookManager manager({"AAPL"});

    manager.add_order(makeOrder("sell-1", Side::SELL, 100.0, 3));
    manager.add_order(makeOrder("buy-1", Side::BUY, 100.0, 5, "AAPL", 2));

    EXPECT_TRUE(manager.has_active_order("buy-1"));
    EXPECT_FALSE(manager.has_active_order("sell-1"));

    const auto best_bid = manager.get_orderbook("AAPL").bestBid();
    ASSERT_TRUE(best_bid.has_value());
    EXPECT_EQ(best_bid->_id, "buy-1");
    EXPECT_EQ(best_bid->_quantity, 2);
}

TEST(OrderBookManagerTest, MarketOrderDoesNotRemainActive) {
    OrderBookManager manager({"AAPL"});

    manager.add_order(makeOrder("sell-1", Side::SELL, 100.0, 2));
    manager.add_order(makeOrder("buy-market", Side::BUY, 0.0, 5, "AAPL", 2,
                                OrderType::MARKET));

    EXPECT_FALSE(manager.has_active_order("buy-market"));
    EXPECT_FALSE(manager.has_active_order("sell-1"));
    EXPECT_FALSE(manager.get_orderbook("AAPL").bestBid().has_value());
    EXPECT_FALSE(manager.get_orderbook("AAPL").bestAsk().has_value());
}

TEST(OrderBookManagerTest, RejectsDuplicateOrderIdAfterOriginalFullyFills) {
    OrderBookManager manager({"AAPL"});

    manager.add_order(makeOrder("sell-1", Side::SELL, 100.0, 1));
    manager.add_order(makeOrder("buy-1", Side::BUY, 100.0, 1, "AAPL", 2));

    EXPECT_FALSE(manager.has_active_order("buy-1"));
    EXPECT_THROW(manager.add_order(makeOrder("buy-1", Side::BUY, 99.0, 1, "AAPL", 3)),
                 std::runtime_error);
}

TEST(OrderBookManagerTest, RejectsInvalidQuantityAtManagerLevel) {
    OrderBookManager manager({"AAPL"});

    EXPECT_THROW(manager.add_order(makeOrder("buy-1", Side::BUY, 100.0, 0)),
                 std::runtime_error);
    EXPECT_FALSE(manager.has_active_order("buy-1"));
}

TEST(OrderBookManagerTest, RejectsInvalidLimitPriceAtManagerLevel) {
    OrderBookManager manager({"AAPL"});

    EXPECT_THROW(manager.add_order(makeOrder("buy-1", Side::BUY, 0.0, 1)),
                 std::runtime_error);
    EXPECT_FALSE(manager.has_active_order("buy-1"));
}

TEST(OrderBookManagerTest, GetOrderBookThrowsForUnknownSymbol) {
    OrderBookManager manager({"AAPL"});

    EXPECT_THROW(static_cast<void>(manager.get_orderbook("MSFT")), std::runtime_error);
}

TEST(OrderBookManagerTest, AddOrderForUnknownSymbolDoesNotCreateBook) {
    OrderBookManager manager({"AAPL"});

    EXPECT_THROW(manager.add_order(makeOrder("msft-buy-1", Side::BUY, 100.0, 5, "MSFT")),
                 std::runtime_error);

    EXPECT_THROW(static_cast<void>(manager.get_orderbook("MSFT")), std::runtime_error);
}

TEST(OrderBookManagerTest, ConcurrentOrdersAndSnapshotsRemainConsistent) {
    OrderBookManager manager({"AAPL"});

    auto add_orders = [&](const std::string &prefix, const double price) {
        for (int index = 0; index < 100; ++index) {
            manager.add_order(makeOrder(prefix + std::to_string(index), Side::BUY, price, 1));
        }
    };

    std::thread first_writer(add_orders, "first-", 99.0);
    std::thread second_writer(add_orders, "second-", 98.0);
    std::thread reader([&]() {
        for (int index = 0; index < 100; ++index) {
            static_cast<void>(manager.get_orderbook("AAPL").bid_depth(5));
        }
    });

    first_writer.join();
    second_writer.join();
    reader.join();

    EXPECT_EQ(manager.get_orderbook("AAPL").get_orders_buys().size(), 200);
}
} // namespace
