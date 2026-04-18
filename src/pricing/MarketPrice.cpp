//
// Created by Jacob Pagan on 9/15/2025.
//

#include "pricing/MarkPrice.h"
#include "pricing/PriceGenerator.h"
#include <random>
#include <string>
#include <unordered_map>

namespace market {
MarkPrice PriceGenerator::generatePrice(const std::string &symbol) {
    static std::unordered_map<std::string, double> prevPrices;
    static std::normal_distribution<double> norm(0.0, 1.0);

    static std::random_device rd;
    static std::mt19937 gen(rd());

    if (!prevPrices.contains(symbol)) {
        std::uniform_real_distribution<> distribution(100.0, 500.0);
        prevPrices[symbol] = distribution(gen);
    }

    constexpr double mu = 0.05;
    constexpr double sigma = 0.02;
    constexpr double dt = 1.0 / 252.0;

    double z = norm(gen);

    double bid =
        prevPrices[symbol] * std::exp((mu - 0.5 * sigma * sigma) * dt + sigma * std::sqrt(dt) * z);
    prevPrices[symbol] = bid;
    const double ask = bid + (bid * 0.0005);
    const double last = bid;
    const std::uint64_t volume = std::uniform_int_distribution<std::uint64_t>(100, 10000)(gen);

    return MarkPrice(symbol, bid, ask, last, volume);
}

std::vector<MarkPrice> PriceGenerator::generatePrices(const std::vector<std::string> &symbols) {
    std::vector<MarkPrice> prices;
    prices.reserve(symbols.size());
    for (const auto &symbol : symbols) {
        prices.push_back(generatePrice(symbol));
    }
    return prices;
}
} // namespace market
