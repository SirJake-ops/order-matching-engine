#include "orderbook/OrderBookManager.h"
#include "orderbook/OrderBook.h"

#include <algorithm>
#include <stdexcept>

std::vector<orderbook::Trade>
orderbook_manager::OrderBookManager::add_order(const orderbook::Order &order) {
    auto it = std::ranges::find_if(_orderbooks,
                                   [&order](const auto &pair) { return pair.first == order._symbol; });
    if (it == _orderbooks.end()) {
        throw std::runtime_error("Order book for symbol " + order._symbol + " does not exist");
    }

    return it->second.addOrder(order);
}

const orderbook::OrderBook &
orderbook_manager::OrderBookManager::get_orderbook(const std::string &symbol) const {
    const auto it = _orderbooks.find(symbol);
    if (it == _orderbooks.end()) {
        throw std::runtime_error("Order book for symbol " + symbol + " does not exist");
    }

    return it->second;
}
