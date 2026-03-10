//
// Created by Jacob Pagan on 9/15/2025.
//

#include <memory>
#include <string>
#include <random>
#include "MarkPrice.h"
#include "PriceGenerator.h"

namespace market {
    MarkPrice PriceGenerator::generatePrice(const std::string &symbol) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<> distribution(100.0, 500.0);
        std::uniform_int_distribution<std::uint64_t> distributionInt(100, 10000);

        const double bid = distribution(gen);
        const double ask = bid + 0.05; // Tight spread for now
        const double last = bid;
        const std::uint64_t volume = distributionInt(gen);

        return MarkPrice(symbol, bid, ask, last, volume);
    }

    std::vector<MarkPrice> PriceGenerator::generatePrices(const std::vector<std::string> &symbols) {
        std::vector<MarkPrice> prices;
        for (const auto& symbol : symbols) {
            prices.push_back(generatePrice(symbol));
        }
        return prices;
    }
}
