#pragma once

#include "orderbook/OrderBook.h"
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace orderbook_manager {
  class OrderBookManager {
  public:
    explicit OrderBookManager(const std::vector<std::string> &symbols);

    std::vector<orderbook::Trade> add_order(const orderbook::Order &order);

    [[nodiscard]] const orderbook::OrderBook &get_orderbook(const std::string &symbol) const;

    [[nodiscard]] bool has_active_order(const std::string &order_id) const;

  private:
    void validate_order(const orderbook::Order &order) const;

    void record_new_order(const orderbook::Order &order);

    void apply_trade(const orderbook::Trade &trade);

    void apply_fill(const std::string &order_id, std::int64_t quantity);

    std::unordered_map<std::string, orderbook::OrderBook> _order_books;
    std::unordered_map<std::string, orderbook::OrderState> _active_orders;
    std::unordered_set<std::string> _seen_order_ids;
  };
} // namespace orderbook_manager
