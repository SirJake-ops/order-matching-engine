//
// Created by jake on 2/24/26.
//

#ifndef TRADINGEXCHANGE_EVENT_BUS_H
#define TRADINGEXCHANGE_EVENT_BUS_H

#include <functional>
#include <map>
#include <vector>
#include <string>
#include <typeindex>

namespace events {
    // TODO: Figure out if we are keeping these Event definitions in this header or not
    struct Event {
        virtual ~Event() = default;
        virtual std::type_index get_type_index() const = 0;
    };

    template <typename T>
    struct TypedEvent : public Event {
        virtual std::type_index get_type_index() const override {
            return typeid(T);
        }
    };
    // TODO: End of the Events

    class event_bus {
    public:
        using Callback = std::function<void(const std::string&)>;

        void subscribe(const std::string& topic, Callback callback) {
            _subscribers[topic].push_back(std::move(callback));
        }

        void publish(const std::string& topic, const std::string& message) {
            if (_subscribers.contains(topic)) {
                for (const auto& callback : _subscribers[topic]) {
                    callback(message);
                }
            }
        }

    private:
        std::map<std::string, std::vector<Callback>> _subscribers;
    };
}
#endif //TRADINGEXCHANGE_EVENT_BUS_H
