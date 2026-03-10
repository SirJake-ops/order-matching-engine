//
// Created by Jacob Pagan on 9/15/2025.
//

#ifndef TRADINGEXCHANGE_MARKPRICE_H
#define TRADINGEXCHANGE_MARKPRICE_H
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>
#include <utility>

namespace market {
    class MarkPrice {
    public:
        explicit MarkPrice(std::string symbol, const double &price, const double &ask, const double &last,
                           const uint64_t volume) : _symbol(std::move(symbol)), _bid(price), _ask(ask), _last(last),
                                                    _volume(volume) {
            _timestamp = std::time(nullptr);
        }

        MarkPrice(const MarkPrice &other) = default;
        MarkPrice& operator=(const MarkPrice &other) = default;

        MarkPrice(MarkPrice &&other) noexcept = default;
        MarkPrice& operator=(MarkPrice &&other) noexcept = default;


        [[nodiscard]] std::string getSymbol() const { return _symbol; }
        [[nodiscard]] double getBid() const { return _bid; }
        [[nodiscard]] double getAsk() const { return _ask; }
        [[nodiscard]] double getLast() const { return _last; }
        [[nodiscard]] uint64_t getVolume() const { return _volume; }
        [[nodiscard]] std::time_t getTimestamp() const { return _timestamp; }

    private:
        std::string _symbol;
        double _bid;
        double _ask;
        double _last;
        uint64_t _volume;
        std::time_t _timestamp;
    };
}

#endif //TRADINGEXCHANGE_MARKPRICE_H
