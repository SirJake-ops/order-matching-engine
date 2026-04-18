//
// Created by Jacob Pagan on 9/16/2025.
//

#include <iostream>
#include <vector>
#include <string>
#include "pricing/PriceGenerator.h"
#include "events/event_bus.h"

namespace market {
    class MarketSimulator {
    public:
        MarketSimulator(events::event_bus& bus, std::vector<std::string> symbols);

        void update() const;

    private:
        events::event_bus& _bus;
        std::vector<std::string> _symbols;
    };
}