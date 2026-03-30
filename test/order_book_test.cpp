#include <stdexcept>

#include "gtest/gtest.h"

#define private public
#include "orderbook/OrderBook.h"
#undef private

namespace {
    using orderbook::Order;
    using orderbook::OrderBook;
    using orderbook::OrderType;
    using orderbook::Side;

    Order makeOrder(
        const std::string &id,
        const Side side,
        const double price,
        const int quantity,
        const std::uint64_t timestamp = 1,
        const OrderType type = OrderType::LIMIT,
        const std::string &symbol = "AAPL"
    ) {
        return Order{id, symbol, side, type, price, quantity, timestamp};
    }

    TEST(OrderBookTest, BuyCrossesSingleAskCompletely) {
        OrderBook book;
        book.addOrder(makeOrder("sell-1", Side::SELL, 100.0, 10));

        const auto trades = book.addOrder(makeOrder("buy-1", Side::BUY, 101.0, 10, 2));

        ASSERT_EQ(trades.size(), 1);
        EXPECT_EQ(trades.front()._buy_order_id, "buy-1");
        EXPECT_EQ(trades.front()._sell_order_id, "sell-1");
        EXPECT_DOUBLE_EQ(trades.front()._price, 100.0);
        EXPECT_EQ(trades.front()._quantity, 10);
        EXPECT_FALSE(book.bestAsk().has_value());
        EXPECT_FALSE(book.bestBid().has_value());
    }

    TEST(OrderBookTest, BuyPartiallyFillsAskAndLeavesRemainder) {
        OrderBook book;
        book.addOrder(makeOrder("sell-1", Side::SELL, 100.0, 10));

        const auto trades = book.addOrder(makeOrder("buy-1", Side::BUY, 100.0, 4, 2));

        ASSERT_EQ(trades.size(), 1);
        EXPECT_EQ(trades.front()._quantity, 4);
        const auto best_ask = book.bestAsk();
        ASSERT_TRUE(best_ask.has_value());
        EXPECT_EQ(best_ask->_id, "sell-1");
        EXPECT_EQ(best_ask->_quantity, 6);
        EXPECT_FALSE(book.bestBid().has_value());
    }

    TEST(OrderBookTest, SellCrossesMultipleBidLevels) {
        OrderBook book;
        book.addOrder(makeOrder("buy-1", Side::BUY, 101.0, 3));
        book.addOrder(makeOrder("buy-2", Side::BUY, 100.0, 4));

        const auto trades = book.addOrder(makeOrder("sell-1", Side::SELL, 99.0, 5, 2));

        ASSERT_EQ(trades.size(), 2);
        EXPECT_EQ(trades[0]._buy_order_id, "buy-1");
        EXPECT_EQ(trades[0]._quantity, 3);
        EXPECT_DOUBLE_EQ(trades[0]._price, 101.0);
        EXPECT_EQ(trades[1]._buy_order_id, "buy-2");
        EXPECT_EQ(trades[1]._quantity, 2);
        EXPECT_DOUBLE_EQ(trades[1]._price, 100.0);

        const auto best_bid = book.bestBid();
        ASSERT_TRUE(best_bid.has_value());
        EXPECT_EQ(best_bid->_id, "buy-2");
        EXPECT_EQ(best_bid->_quantity, 2);
    }

    TEST(OrderBookTest, TwoOrdersAtSamePriceFillInFifoOrder) {
        OrderBook book;
        book.addOrder(makeOrder("sell-1", Side::SELL, 100.0, 2, 1));
        book.addOrder(makeOrder("sell-2", Side::SELL, 100.0, 2, 2));

        const auto trades = book.addOrder(makeOrder("buy-1", Side::BUY, 100.0, 3, 3));

        ASSERT_EQ(trades.size(), 2);
        EXPECT_EQ(trades[0]._sell_order_id, "sell-1");
        EXPECT_EQ(trades[0]._quantity, 2);
        EXPECT_EQ(trades[1]._sell_order_id, "sell-2");
        EXPECT_EQ(trades[1]._quantity, 1);

        const auto best_ask = book.bestAsk();
        ASSERT_TRUE(best_ask.has_value());
        EXPECT_EQ(best_ask->_id, "sell-2");
        EXPECT_EQ(best_ask->_quantity, 1);
    }

    TEST(OrderBookTest, CancelRemovesCorrectRestingOrder) {
        OrderBook book;
        book.addOrder(makeOrder("buy-1", Side::BUY, 100.0, 5));
        book.addOrder(makeOrder("buy-2", Side::BUY, 99.0, 5));

        EXPECT_TRUE(book.cancelOrder("buy-1"));

        const auto best_bid = book.bestBid();
        ASSERT_TRUE(best_bid.has_value());
        EXPECT_EQ(best_bid->_id, "buy-2");
        EXPECT_FALSE(book.cancelOrder("missing-order"));
    }

    TEST(OrderBookTest, NonCrossingOrderEntersBookCorrectly) {
        OrderBook book;
        book.addOrder(makeOrder("sell-1", Side::SELL, 102.0, 5));

        const auto trades = book.addOrder(makeOrder("buy-1", Side::BUY, 100.0, 5, 2));

        EXPECT_TRUE(trades.empty());
        const auto best_bid = book.bestBid();
        const auto best_ask = book.bestAsk();
        ASSERT_TRUE(best_bid.has_value());
        ASSERT_TRUE(best_ask.has_value());
        EXPECT_EQ(best_bid->_id, "buy-1");
        EXPECT_DOUBLE_EQ(best_bid->_price, 100.0);
        EXPECT_EQ(best_ask->_id, "sell-1");
        EXPECT_DOUBLE_EQ(best_ask->_price, 102.0);
    }

    TEST(OrderBookTest, BestBidAndBestAskUpdateCorrectly) {
        OrderBook book;
        book.addOrder(makeOrder("buy-1", Side::BUY, 99.0, 5));
        book.addOrder(makeOrder("buy-2", Side::BUY, 101.0, 5));
        book.addOrder(makeOrder("sell-1", Side::SELL, 103.0, 5));
        book.addOrder(makeOrder("sell-2", Side::SELL, 102.0, 5));

        ASSERT_TRUE(book.bestBid().has_value());
        ASSERT_TRUE(book.bestAsk().has_value());
        EXPECT_EQ(book.bestBid()->_id, "buy-2");
        EXPECT_EQ(book.bestAsk()->_id, "sell-2");

        book.cancelOrder("buy-2");
        book.cancelOrder("sell-2");

        ASSERT_TRUE(book.bestBid().has_value());
        ASSERT_TRUE(book.bestAsk().has_value());
        EXPECT_EQ(book.bestBid()->_id, "buy-1");
        EXPECT_EQ(book.bestAsk()->_id, "sell-1");
    }

    TEST(OrderBookTest, DuplicateOrderIdThrows) {
        OrderBook book;
        book.addOrder(makeOrder("dup-1", Side::BUY, 100.0, 1));

        EXPECT_THROW(book.addOrder(makeOrder("dup-1", Side::SELL, 99.0, 1)), std::runtime_error);
    }
}
