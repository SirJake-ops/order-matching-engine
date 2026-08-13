//
// Created by jake on 2/24/26.
//

#include "events/event_bus.h"

namespace events {
    void event_bus::subscribe(const std::string &topic, Callback callback) {
        std::lock_guard lock(_subscribers_mutex);
        _subscribers[topic].push_back(std::move(callback));
    }

    void event_bus::publish(const std::string &topic, const std::string &message) const {
        std::vector<Callback> callbacks;
        {
            std::lock_guard lock(_subscribers_mutex);
            const auto subscribers = _subscribers.find(topic);
            if (subscribers == _subscribers.end()) {
                return;
            }
            callbacks = subscribers->second;
        }

        for (const auto &callback: callbacks) {
            callback(message);
        }
    }
}
