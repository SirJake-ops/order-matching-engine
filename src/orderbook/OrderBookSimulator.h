//
// Created by Jacob Pagan on 9/16/2025.
//

#include <string>
#include <vector>
#include "../pricing/MarkPrice.h"

namespace market {
    class OrderBookSimulator {
    public:
        void onPriceUpdate(const MarkPrice& price) {
            _last_price = price;
        }

        [[nodiscard]] const MarkPrice& getLatestPrice() const { return _last_price; }

    private:
        MarkPrice _last_price{"", 0.0, 0.0, 0.0, 0};
    };
}