cmake_minimum_required(VERSION 3.28)
project(TradingExchange)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")

find_package(Boost REQUIRED)

include_directories(include src)


add_library(pricing
        src/pricing/MarketPrice.cpp
)

add_library(simulation
        src/simulation/MarketSimulator.h
        src/simulation/MarketSimulator.cpp
)

target_link_libraries(simulation
        PUBLIC
        pricing
        events
)

add_library(orderbook
        src/orderbook/OrderBookSimulator.h
        src/orderbook/OrderBookSimulator.cpp
)

add_library(events
        src/events/event_bus.cpp
        src/events/event_types/event_types.h
)

add_library(transport
        src/transport/server.cpp
)


add_executable(market_data_service
        apps/market_data_service/main.cpp
)

target_link_directories(market_data_service PRIVATE Boost::filesystem)

target_link_libraries(market_data_service
        PRIVATE
        simulation
        orderbook
        transport
)


add_subdirectory(test)