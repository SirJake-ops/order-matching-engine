//
// Created by Jacob Pagan on 9/15/2025.
//

#ifndef TRADINGEXCHANGE_PRICEGENERATOR_H
#define TRADINGEXCHANGE_PRICEGENERATOR_H

#include "pricing/MarkPrice.h"
#include <string>
#include <vector>

namespace market {
    class PriceGenerator {
    public:
        static MarkPrice generatePrice(const std::string &symbol);
        static std::vector<MarkPrice> generatePrices(const std::vector<std::string> &symbols);
    };
}

#endif //TRADINGEXCHANGE_PRICEGENERATOR_H
