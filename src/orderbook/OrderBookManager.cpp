#include "orderbook/OrderBookManager.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

void orderbook_manager::OrderBookManager::add_order(const orderbook::Order &order) {
    try {
        auto it = std::find_if(_orderbooks.begin(), _orderbooks.end(),
                               [&order](const auto &pair) { return pair.first == order._symbol; });
        if (it == _orderbooks.end()) {
            throw std::runtime_error("Order book for symbol " + order._symbol + " does not exist");
        }

        _orderbooks.emplace(order._symbol, orderbook::OrderBook(order._symbol));
        _orderbooks.at(order._symbol).addOrder(order);
    } catch (const std::exception &e) {
        std::cerr << "Error adding order: " << e.what() << std::endl;
    }
}

const orderbook::OrderBook &
orderbook_manager::OrderBookManager::get_orderbook(const std::string &symbol) {
    auto it = _orderbooks.find(symbol);
    if (it == _orderbooks.end()) {
        throw std::runtime_error("Order book for symbol " + symbol + " does not exist");
    }

    return it->second;
}
