//
// Created by jake on 2/24/26.
//

#ifndef TRADINGEXCHANGE_EVENT_BUS_H
#define TRADINGEXCHANGE_EVENT_BUS_H

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace events {
    class event_bus {
    public:
        using Callback = std::function<void(const std::string &)>;

        void subscribe(const std::string &topic, Callback callback);

        void publish(const std::string &topic, const std::string &message) const;

    private:
        mutable std::mutex _subscribers_mutex;
        std::map<std::string, std::vector<Callback> > _subscribers;
    };
}
#endif //TRADINGEXCHANGE_EVENT_BUS_H
