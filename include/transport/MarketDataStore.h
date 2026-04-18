#ifndef TRADINGEXCHANGE_MARKETDATASTORE_H
#define TRADINGEXCHANGE_MARKETDATASTORE_H

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "pricing/MarkPrice.h"

namespace server {
    class MarketDataStore {
    public:
        void updatePrice(const market::MarkPrice &price);
        [[nodiscard]] std::vector<market::MarkPrice> getAllPrices() const;
        [[nodiscard]] std::optional<market::MarkPrice> getPriceForSymbol(const std::string &symbol) const;

    private:
        mutable std::mutex _mutex;
        std::map<std::string, market::MarkPrice> _prices;
    };
}

#endif //TRADINGEXCHANGE_MARKETDATASTORE_H
