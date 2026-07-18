#include "orderbook/OrderBookManager.h"
#include "orderbook/OrderBook.h"

#include <stdexcept>

orderbook_manager::OrderBookManager::OrderBookManager(const std::vector<std::string> &symbols) {
    for (const auto &symbol: symbols) {
        const auto [_, inserted] = _order_books.emplace(symbol, orderbook::OrderBook(symbol));

        if (!inserted) {
            throw std::runtime_error("Duplicate order book symbol " + symbol);
        }
    }
}

std::vector<orderbook::Trade>
orderbook_manager::OrderBookManager::add_order(const orderbook::Order &order) {
    validate_order(order);
    auto &book = _order_books.at(order._symbol);

    record_new_order(order);

    auto trades = book.addOrder(order);

    for (const auto &trade: trades) {
        apply_trade(trade);
    }

    if (order._type == orderbook::OrderType::MARKET) {
        _active_orders.erase(order._id);
    }

    return trades;
}

const orderbook::OrderBook &
orderbook_manager::OrderBookManager::get_orderbook(const std::string &symbol) const {
    const auto it = _order_books.find(symbol);
    if (it == _order_books.end()) {
        throw std::runtime_error("Order book for symbol " + symbol + " does not exist");
    }

    return it->second;
}

bool orderbook_manager::OrderBookManager::has_active_order(const std::string &order_id) const {
    return _active_orders.contains(order_id);
}

void orderbook_manager::OrderBookManager::validate_order(const orderbook::Order &order) const {
    if (!_order_books.contains(order._symbol)) {
        throw std::runtime_error("Order book for symbol " + order._symbol + " does not exist");
    }

    if (order._id.empty() || order._symbol.empty()) {
        throw std::runtime_error("Invalid order");
    }

    if (_seen_order_ids.contains(order._id)) {
        throw std::runtime_error("Duplicate order book id " + order._id);
    }

    if (order._quantity <= 0) {
        throw std::runtime_error("Invalid order");
    }

    if (order._type == orderbook::OrderType::LIMIT && order._price <= 0) {
        throw std::runtime_error("Invalid order");
    }
}

void orderbook_manager::OrderBookManager::record_new_order(const orderbook::Order &order) {
    _seen_order_ids.insert(order._id);

    _active_orders.emplace(order._id, orderbook::OrderState{
                               order,
                               orderbook::OrderStatus::NEW,
                               order._quantity,
                               order._quantity,
                               0
                           });
}

void orderbook_manager::OrderBookManager::apply_trade(const orderbook::Trade &trade) {
    apply_fill(trade._buy_order_id, trade._quantity);
    apply_fill(trade._sell_order_id, trade._quantity);
}

void orderbook_manager::OrderBookManager::apply_fill(const std::string &order_id, std::int64_t quantity) {
    const auto it = _active_orders.find(order_id);

    if (it == _active_orders.end()) {
        return;
    }

    auto &state = it->second;
    state.remaining_quantity -= quantity;
    state.filled_quantity += quantity;

    if (state.remaining_quantity <= 0) {
        state.status = orderbook::OrderStatus::FILLED;
        _active_orders.erase(it);
    } else {
        state.status = orderbook::OrderStatus::PARTIALLY_FILLED;
    }
}
