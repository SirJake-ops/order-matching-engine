//
// Created by Jacob Pagan on 9/16/2025.
//

#ifndef TRADINGEXCHANGE_INSTRUMENT_H
#define TRADINGEXCHANGE_INSTRUMENT_H
#include <string>

struct Instrument {
    std::string _symbol;
    std::string _name;
    std::string _exchange;
    double _ticket_size;
    int _lot_size;
    std::string _currency;
    bool _is_active;
};

#endif //TRADINGEXCHANGE_INSTRUMENT_H