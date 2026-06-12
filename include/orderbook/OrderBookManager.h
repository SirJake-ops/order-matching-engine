#pragma once

#include "orderbook/OrderBook.h"
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace orderbook_manager {
class OrderBookManager {
  public:
    explicit OrderBookManager(const std::vector<std::string> &symbols) {
        for (const auto &symbol : symbols) {

            const auto [_, inserted] = _orderbooks.emplace(symbol, orderbook::OrderBook(symbol));

            if (!inserted) {
                throw std::runtime_error("Duplicate order book symbol " + symbol);
            }
        }
    }

    std::vector<orderbook::Trade> add_order(const orderbook::Order &order);
    [[nodiscard]] const orderbook::OrderBook &get_orderbook(const std::string &symbol) const;

  private:
    std::unordered_map<std::string, orderbook::OrderBook> _orderbooks;
};
} // namespace orderbook_manager
