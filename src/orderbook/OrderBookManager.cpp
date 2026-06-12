#include "orderbook/OrderBookManager.h"
#include "orderbook/OrderBook.h"

#include <algorithm>
#include <stdexcept>

std::vector<orderbook::Trade>
orderbook_manager::OrderBookManager::add_order(const orderbook::Order &order) {
    auto it = std::find_if(_orderbooks.begin(), _orderbooks.end(),
                           [&order](const auto &pair) { return pair.first == order._symbol; });
    if (it == _orderbooks.end()) {
        throw std::runtime_error("Order book for symbol " + order._symbol + " does not exist");
    }

    return it->second.addOrder(order);
}

const orderbook::OrderBook &
orderbook_manager::OrderBookManager::get_orderbook(const std::string &symbol) {
    auto it = _orderbooks.find(symbol);
    if (it == _orderbooks.end()) {
        throw std::runtime_error("Order book for symbol " + symbol + " does not exist");
    }

    return it->second;
}
