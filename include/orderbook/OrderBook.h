//
// Created by jake on 3/28/26.
//

#ifndef TRADINGEXCHANGE_ORDERBOOK_H
#define TRADINGEXCHANGE_ORDERBOOK_H
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
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
    std::int64_t _quantity;
    std::uint64_t _timestamp;
};

struct Trade {
    std::string _buy_order_id;
    std::string _sell_order_id;
    std::string _symbol;
    double _price;
    std::int64_t _quantity;
    std::uint64_t _timestamp;
};

class OrderBook {
  public:
    explicit OrderBook(std::string symbol) : _symbol(std::move(symbol)) {}
    ~OrderBook() = default;

    std::vector<Trade> addOrder(const Order &order);
    std::vector<Order> get_orders_buys() const;
    std::vector<Order> get_orders_sells() const;

    bool cancel_order(const std::string &order_id);

    [[nodiscard]] std::optional<Order> bestBid() const;

    [[nodiscard]] std::optional<Order> bestAsk() const;

    [[nodiscard]] std::vector<std::pair<double, int64_t>> bid_depth(std::size_t levels) const;
    [[nodiscard]] std::vector<std::pair<double, std::int64_t>>
    ask_depths(std::size_t levels) const;

  private:
    std::string _symbol;
    std::map<double, std::deque<Order>, std::greater<>> _buy_orders;
    std::map<double, std::deque<Order>, std::less<>> _sell_orders;
    std::unordered_map<std::string, std::vector<Trade>> _trade_history;
};
} // namespace orderbook

#endif // TRADINGEXCHANGE_ORDERBOOK_H
