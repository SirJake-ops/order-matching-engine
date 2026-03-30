//
// Created by Jacob Pagan on 9/16/2025.
//

#include <string>

#include "OrderBook.h"
#include "pricing/MarkPrice.h"

namespace market {
    class OrderBookSimulator {
    public:
        void onPriceUpdate(const MarkPrice &price) {
            _last_price = price;
        }

        [[nodiscard]] const MarkPrice &getLatestPrice() const { return _last_price; }

        void addOrder(const orderbook::Order &order) const;

        bool cancelOrder(const std::string &order_id) const;

        orderbook::OrderBook getBook(const std::string &symbol) const;

    private:
        MarkPrice _last_price{"", 0.0, 0.0, 0.0, 0};
        std::map<std::string, orderbook::Order> _orders;
    };
}
