#include <atomic>
#include <barrier>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "events/event_bus.h"

namespace {
    TEST(EventBusTest, ConcurrentSubscriptionAndPublishingAreSafe) {
        events::event_bus bus;
        std::atomic<int> callback_count{0};
        bus.subscribe("orders", [&](const std::string &) { ++callback_count; });

        std::barrier start_line(3);
        std::thread subscriber([&]() {
            start_line.arrive_and_wait();
            for (int index = 0; index < 100; ++index) {
                bus.subscribe("orders", [&](const std::string &) { ++callback_count; });
            }
        });
        std::thread publisher([&]() {
            start_line.arrive_and_wait();
            for (int index = 0; index < 100; ++index) {
                bus.publish("orders", "accepted");
            }
        });

        start_line.arrive_and_wait();
        subscriber.join();
        publisher.join();
        bus.publish("orders", "complete");

        EXPECT_GE(callback_count.load(), 201);
    }

    TEST(EventBusTest, CallbackCanSubscribeWithoutDeadlockingPublish) {
        events::event_bus bus;
        std::atomic<int> callback_count{0};

        bus.subscribe("orders", [&](const std::string &) {
            ++callback_count;
            bus.subscribe("orders", [&](const std::string &) { ++callback_count; });
        });

        bus.publish("orders", "first");
        bus.publish("orders", "second");

        EXPECT_EQ(callback_count.load(), 3);
    }
}
