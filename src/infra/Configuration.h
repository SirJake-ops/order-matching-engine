//
// Created by Jacob Pagan on 9/15/2025.
//

#ifndef TRADINGEXCHANGE_CONFIGURATION_H
#define TRADINGEXCHANGE_CONFIGURATION_H
#include <chrono>
#include <string>
#include <vector>


struct MarketConfig {
    std::string _exchange_name;
    std::vector<std::string> _holidays;
    double _make_fee;
    double _taker_fee;
};



#endif //TRADINGEXCHANGE_CONFIGURATION_H