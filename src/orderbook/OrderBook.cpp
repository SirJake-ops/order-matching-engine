//
// Created by jake on 3/28/26.
//

#include "orderbook/OrderBook.h"

#include <algorithm>
#include <ranges>
#include <vector>

std::vector<orderbook::Trade> orderbook::OrderBook::addOrder(const orderbook::Order &order) {
    Order incoming_order = order;

    auto duplicate_exists = [&](const auto &book_side) {
        for (const auto &orders: book_side | std::views::values) {
            const auto it = std::ranges::find_if(orders, [&](const Order &existing_order) {
                return existing_order._id == incoming_order._id;
            });

            if (it != orders.end()) {
                return true;
            }
        }
        return false;
    };

    if (duplicate_exists(_buy_orders) || duplicate_exists(_sell_orders)) {
        throw std::runtime_error("Duplicate order id");
    }

    std::vector<Trade> trades;
    if (incoming_order._side == Side::BUY) {
        while (incoming_order._quantity > 0 && !_sell_orders.empty()) {
            auto best_ask_level = _sell_orders.begin();
            if (incoming_order._type == OrderType::LIMIT && best_ask_level->first > incoming_order._price) {
                break;
            }

            auto &resting_orders = best_ask_level->second;
            auto &resting_order = resting_orders.front();
            const int matched_quantity = std::min(incoming_order._quantity, resting_order._quantity);

            trades.push_back(Trade{
                incoming_order._id,
                resting_order._id,
                incoming_order._symbol,
                resting_order._price,
                matched_quantity,
                incoming_order._timestamp
            });

            incoming_order._quantity -= matched_quantity;
            resting_order._quantity -= matched_quantity;

            if (resting_order._quantity == 0) {
                resting_orders.pop_front();
            }
            if (resting_orders.empty()) {
                _sell_orders.erase(best_ask_level);
            }
        }

        if (incoming_order._quantity > 0 && incoming_order._type == OrderType::LIMIT) {
            _buy_orders[incoming_order._price].push_back(incoming_order);
        }
    } else {
        while (incoming_order._quantity > 0 && !_buy_orders.empty()) {
            auto best_bid_level = _buy_orders.begin();
            if (incoming_order._type == OrderType::LIMIT && best_bid_level->first < incoming_order._price) {
                break;
            }

            auto &resting_orders = best_bid_level->second;
            auto &resting_order = resting_orders.front();
            const int matched_quantity = std::min(incoming_order._quantity, resting_order._quantity);

            trades.push_back(Trade{
                resting_order._id,
                incoming_order._id,
                incoming_order._symbol,
                resting_order._price,
                matched_quantity,
                incoming_order._timestamp
            });

            incoming_order._quantity -= matched_quantity;
            resting_order._quantity -= matched_quantity;

            if (resting_order._quantity == 0) {
                resting_orders.pop_front();
            }
            if (resting_orders.empty()) {
                _buy_orders.erase(best_bid_level);
            }
        }

        if (incoming_order._quantity > 0 && incoming_order._type == OrderType::LIMIT) {
            _sell_orders[incoming_order._price].push_back(incoming_order);
        }
    }

    return trades;
}

std::optional<orderbook::Order> orderbook::OrderBook::bestBid() const {
    if (_buy_orders.empty()) {
        return std::nullopt;
    }

    const auto &orders_best_price = _buy_orders.begin()->second;
    if (orders_best_price.empty()) {
        return std::nullopt;
    }

    return orders_best_price.front();
}

std::optional<orderbook::Order> orderbook::OrderBook::bestAsk() const {
    if (_sell_orders.empty()) {
        return std::nullopt;
    }

    const auto &orders_best_price = _sell_orders.begin()->second;
    if (orders_best_price.empty()) {
        return std::nullopt;
    }
    return orders_best_price.front();
}

bool orderbook::OrderBook::cancelOrder(const std::string &order_id) {
    bool removed = false;
    auto remove_order = [&]<typename T>(T &book) {
        using BookT = std::decay_t<T>;
        std::erase_if(book, [&](typename BookT::value_type &price_level) {
            auto &orders = price_level.second;
            std::size_t before = orders.size();
            std::erase_if(orders, [&](const auto &o) { return o._id == order_id; });
            if (orders.size() != before) removed = true;
            return orders.empty();
        });
    };

    remove_order(_buy_orders);
    remove_order(_sell_orders);

    return removed;
}
