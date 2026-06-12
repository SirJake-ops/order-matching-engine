#pragma once

#include "orderbook/OrderBook.h"
#include <unordered_map>

namespace orderbook_manager {
class OrderBookManager {
  public:
    explicit OrderBookManager(const std::string &symbol)
        : _orderbooks({{symbol, orderbook::OrderBook(symbol)}}) {}

    std::vector<orderbook::Trade> add_order(const orderbook::Order &order);
    const orderbook::OrderBook &get_orderbook(const std::string &symbol);

  private:
    std::unordered_map<std::string, orderbook::OrderBook> _orderbooks;
};
} // namespace orderbook_manager
