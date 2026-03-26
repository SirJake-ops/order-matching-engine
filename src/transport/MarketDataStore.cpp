#include "transport/MarketDataStore.h"

namespace server {
    void MarketDataStore::updatePrice(const market::MarkPrice &price) {
        std::lock_guard lock(_mutex);
        _prices.insert_or_assign(price.getSymbol(), price);
    }

    std::vector<market::MarkPrice> MarketDataStore::getAllPrices() const {
        std::lock_guard lock(_mutex);

        std::vector<market::MarkPrice> prices;
        prices.reserve(_prices.size());
        for (const auto &[symbol, price]: _prices) {
            prices.push_back(price);
        }

        return prices;
    }

    std::optional<market::MarkPrice> MarketDataStore::getPriceForSymbol(const std::string &symbol) const {
        std::lock_guard lock(_mutex);
        const auto iterator = _prices.find(symbol);
        if (iterator == _prices.end()) {
            return std::nullopt;
        }

        return iterator->second;
    }
}
