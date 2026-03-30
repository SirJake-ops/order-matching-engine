//
// Created by jake on 3/28/26.
//

#ifndef TRADINGEXCHANGE_ORDERBOOK_H
#define TRADINGEXCHANGE_ORDERBOOK_H
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace orderbook {
    enum class Side {
        BUY,
        SELL,
    };

    enum class OrderType {
        LIMIT,
        MARKET,
    };

    struct Order {
        std::string _id;
        std::string _symbol;
        Side _side;
        OrderType _type;
        double _price;
        int _quantity;
        std::uint64_t _timestamp;
    };

    struct Trade {
        std::string _buy_order_id;
        std::string _sell_order_id;
        std::string _symbol;
        double _price;
        int _quantity;
        std::uint64_t _timestamp;
    };

    class OrderBook {
    public:
        OrderBook() = default;
        ~OrderBook() = default;

        std::vector<Trade> addOrder(const Order &order);

        bool cancelOrder(const std::string &order_id);

        std::optional<Order> bestBid() const;

        std::optional<Order> bestAsk() const;

    private:
        std::map<double, std::deque<Order>, std::greater<> > _buy_orders;
        std::map<double, std::deque<Order>, std::less<> > _sell_orders;
    };
}

#endif //TRADINGEXCHANGE_ORDERBOOK_H
