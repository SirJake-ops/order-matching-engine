#include <limits>
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

    Order makeOrder(const std::string &id, const Side side, const double price, const std::int64_t quantity,
                    const std::uint64_t timestamp = 1, const OrderType type = OrderType::LIMIT,
                    const std::string &symbol = "AAPL") {
        return Order{id, symbol, side, type, price, quantity, timestamp};
    }

    TEST(OrderBookTest, BuyCrossesSingleAskCompletely) {
        OrderBook book("AAPL");
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
        OrderBook book("AAPL");
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
        OrderBook book("AAPL");
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
        OrderBook book("AAPL");
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
        OrderBook book("AAPL");
        book.addOrder(makeOrder("buy-1", Side::BUY, 100.0, 5));
        book.addOrder(makeOrder("buy-2", Side::BUY, 99.0, 5));

        EXPECT_TRUE(book.cancel_order("buy-1"));

        const auto best_bid = book.bestBid();
        ASSERT_TRUE(best_bid.has_value());
        EXPECT_EQ(best_bid->_id, "buy-2");
        EXPECT_FALSE(book.cancel_order("missing-order"));
    }

    TEST(OrderBookTest, NonCrossingOrderEntersBookCorrectly) {
        OrderBook book("AAPL");
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
        OrderBook book("AAPL");
        book.addOrder(makeOrder("buy-1", Side::BUY, 99.0, 5));
        book.addOrder(makeOrder("buy-2", Side::BUY, 101.0, 5));
        book.addOrder(makeOrder("sell-1", Side::SELL, 103.0, 5));
        book.addOrder(makeOrder("sell-2", Side::SELL, 102.0, 5));

        ASSERT_TRUE(book.bestBid().has_value());
        ASSERT_TRUE(book.bestAsk().has_value());
        EXPECT_EQ(book.bestBid()->_id, "buy-2");
        EXPECT_EQ(book.bestAsk()->_id, "sell-2");

        book.cancel_order("buy-2");
        book.cancel_order("sell-2");

        ASSERT_TRUE(book.bestBid().has_value());
        ASSERT_TRUE(book.bestAsk().has_value());
        EXPECT_EQ(book.bestBid()->_id, "buy-1");
        EXPECT_EQ(book.bestAsk()->_id, "sell-1");
    }

    TEST(OrderBookTest, DuplicateOrderIdThrows) {
        OrderBook book("AAPL");
        book.addOrder(makeOrder("dup-1", Side::BUY, 100.0, 1));

        EXPECT_THROW(book.addOrder(makeOrder("dup-1", Side::SELL, 99.0, 1)), std::runtime_error);
    }

    TEST(OrderBookTest, GetOrderReturnsCorrectOrder) {
        OrderBook book("AAPL");
        book.addOrder(makeOrder("buy-1", Side::BUY, 100.0, 1));
        book.addOrder(makeOrder("buy-2", Side::BUY, 105.0, 1));

        const auto buy_orders = book.get_orders_buys();
        ASSERT_TRUE(!buy_orders.empty());
        EXPECT_EQ(buy_orders[0]._id, "buy-2");
        EXPECT_EQ(buy_orders[0]._quantity, 1);

        const auto sell_orders_empty = book.get_orders_sells();
        ASSERT_TRUE(sell_orders_empty.empty());

        book.addOrder(makeOrder("sell-1", Side::SELL, 106.0, 1));
        const auto sell_orders = book.get_orders_sells();

        ASSERT_FALSE(sell_orders.empty());
        EXPECT_EQ(sell_orders[0]._id, "sell-1");
        EXPECT_EQ(sell_orders[0]._quantity, 1);
    }

    TEST(OrderBookTest, BidDepthUpdateCorrectly) {
        OrderBook book("AAPL");
        book.addOrder(makeOrder("buy-1", Side::BUY, 50.0, 1));
        book.addOrder(makeOrder("buy-2", Side::BUY, 51.0, 1));
        book.addOrder(makeOrder("buy-3", Side::BUY, 68.0, 1));

        ASSERT_TRUE(book.bestBid().has_value());
        EXPECT_EQ(book.bestBid()->_id, "buy-3");


        book.addOrder(makeOrder("sell-1", Side::SELL, 90.0, 1));
        book.addOrder(makeOrder("sell-2", Side::SELL, 91.0, 1));
        book.addOrder(makeOrder("sell-3", Side::SELL, 98.0, 1));

        const auto result_asks = book.ask_depths(2);
        EXPECT_EQ(result_asks.size(), 2);
        EXPECT_EQ(result_asks[0].first, 90.0);
        EXPECT_EQ(result_asks[0].second, 1);
        EXPECT_EQ(result_asks[1].first, 91.0);
        EXPECT_EQ(result_asks[1].second, 1);


        const auto result_bids = book.bid_depth(2);
        EXPECT_EQ(result_bids.size(), 2);
        EXPECT_EQ(result_bids[0].first, 68.0);
        EXPECT_EQ(result_bids[0].second, 1);
        EXPECT_EQ(result_bids[1].first, 51.0);
        EXPECT_EQ(result_bids[1].second, 1);
    }

    TEST(OrderBookTest, RejectsOrderForDifferentSymbol) {
        OrderBook book("AAPL");

        EXPECT_THROW(
            book.addOrder(makeOrder("msft-1", Side::BUY, 100.0, 1, 1, OrderType::LIMIT, "MSFT")),
            std::runtime_error
        );
    }

    TEST(OrderBookTest, DepthsDifferentQuantities) {
        OrderBook book("AAPL");

        book.addOrder(makeOrder("sell-1", Side::SELL, 90.0, 2));
        book.addOrder(makeOrder("sell-2", Side::SELL, 90.0, 3));

        const auto result_asks = book.ask_depths(1);

        ASSERT_EQ(result_asks.size(), 1);
        EXPECT_EQ(result_asks[0].first, 90.0);
        EXPECT_EQ(result_asks[0].second, 5);
    }

    TEST(OrderBookTest, RejectsPriceQuantityNegativeValues) {
        OrderBook book("AAPL");

        EXPECT_THROW(
            book.addOrder(makeOrder("sell-1", Side::SELL, -90.0, 2, 1)),
            std::runtime_error
        );

        EXPECT_THROW(
            book.addOrder(makeOrder("buy-1", Side::BUY, 100.0, -2, 1)),
            std::runtime_error
        );
    }

    TEST(OrderBookTest, RejectsEmptyIdEmptySymbolAndZeroQuantity) {
        OrderBook book("AAPL");

        EXPECT_THROW(
            book.addOrder(makeOrder("", Side::BUY, 100.0, 1)),
            std::runtime_error
        );

        EXPECT_THROW(
            book.addOrder(makeOrder("buy-1", Side::BUY, 100.0, 1, 1, OrderType::LIMIT, "")),
            std::runtime_error
        );

        EXPECT_THROW(
            book.addOrder(makeOrder("buy-2", Side::BUY, 100.0, 0)),
            std::runtime_error
        );
    }

    TEST(OrderBookTest, EmptyBookReturnsNoBestOrdersAndNoDepth) {
        OrderBook book("AAPL");

        EXPECT_FALSE(book.bestBid().has_value());
        EXPECT_FALSE(book.bestAsk().has_value());
        EXPECT_TRUE(book.get_orders_buys().empty());
        EXPECT_TRUE(book.get_orders_sells().empty());
        EXPECT_TRUE(book.bid_depth(2).empty());
        EXPECT_TRUE(book.ask_depths(2).empty());
    }

    TEST(OrderBookTest, DepthRejectsZeroLevels) {
        OrderBook book("AAPL");

        EXPECT_THROW(
            {
                const auto depth = book.bid_depth(0);
                static_cast<void>(depth);
            },
            std::runtime_error
        );
        EXPECT_THROW(
            {
                const auto depth = book.ask_depths(0);
                static_cast<void>(depth);
            },
            std::runtime_error
        );
    }

    TEST(OrderBookTest, MarketBuyConsumesBestAsksAndDoesNotRestRemainder) {
        OrderBook book("AAPL");
        book.addOrder(makeOrder("sell-1", Side::SELL, 100.0, 2));
        book.addOrder(makeOrder("sell-2", Side::SELL, 101.0, 3));

        const auto trades = book.addOrder(makeOrder("buy-market", Side::BUY, 0.0, 4, 2, OrderType::MARKET));

        ASSERT_EQ(trades.size(), 2);
        EXPECT_EQ(trades[0]._sell_order_id, "sell-1");
        EXPECT_EQ(trades[0]._quantity, 2);
        EXPECT_DOUBLE_EQ(trades[0]._price, 100.0);
        EXPECT_EQ(trades[1]._sell_order_id, "sell-2");
        EXPECT_EQ(trades[1]._quantity, 2);
        EXPECT_DOUBLE_EQ(trades[1]._price, 101.0);

        const auto best_ask = book.bestAsk();
        ASSERT_TRUE(best_ask.has_value());
        EXPECT_EQ(best_ask->_id, "sell-2");
        EXPECT_EQ(best_ask->_quantity, 1);
        EXPECT_FALSE(book.bestBid().has_value());
    }

    TEST(OrderBookTest, MarketSellConsumesBestBidsAndDoesNotRestRemainder) {
        OrderBook book("AAPL");
        book.addOrder(makeOrder("buy-1", Side::BUY, 101.0, 2));
        book.addOrder(makeOrder("buy-2", Side::BUY, 100.0, 3));

        const auto trades = book.addOrder(makeOrder("sell-market", Side::SELL, 0.0, 5, 2, OrderType::MARKET));

        ASSERT_EQ(trades.size(), 2);
        EXPECT_EQ(trades[0]._buy_order_id, "buy-1");
        EXPECT_EQ(trades[0]._quantity, 2);
        EXPECT_DOUBLE_EQ(trades[0]._price, 101.0);
        EXPECT_EQ(trades[1]._buy_order_id, "buy-2");
        EXPECT_EQ(trades[1]._quantity, 3);
        EXPECT_DOUBLE_EQ(trades[1]._price, 100.0);
        EXPECT_FALSE(book.bestBid().has_value());
        EXPECT_FALSE(book.bestAsk().has_value());
    }

    TEST(OrderBookTest, MarketOrderAgainstEmptyBookReturnsNoTradesAndDoesNotRest) {
        OrderBook book("AAPL");

        const auto trades = book.addOrder(makeOrder("buy-market", Side::BUY, 0.0, 10, 1, OrderType::MARKET));

        EXPECT_TRUE(trades.empty());
        EXPECT_TRUE(book.get_orders_buys().empty());
        EXPECT_TRUE(book.get_orders_sells().empty());
    }

    TEST(OrderBookTest, RejectsNonFinitePriceAndUnknownEnumValues) {
        OrderBook book("AAPL");

        EXPECT_THROW(book.addOrder(makeOrder("nan", Side::BUY,
                                             std::numeric_limits<double>::quiet_NaN(), 1)),
                     std::runtime_error);
        EXPECT_THROW(book.addOrder(makeOrder("side", static_cast<Side>(99), 100.0, 1)),
                     std::runtime_error);
        EXPECT_THROW(book.addOrder(makeOrder("type", Side::BUY, 100.0, 1, 1,
                                             static_cast<OrderType>(99))),
                     std::runtime_error);
    }
} // namespace
