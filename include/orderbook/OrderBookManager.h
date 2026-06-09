
#include "orderbook/OrderBook.h"
#include <unordered_map>

namespace orderbook_manager {
class OrderBookManager {

    explicit OrderBookManager(const std::string &symbol)
        : _orderbooks({{symbol, orderbook::OrderBook(symbol)}}) {}

  private:
    std::unordered_map<std::string, orderbook::OrderBook> _orderbooks;
};
} // namespace orderbook_manager
