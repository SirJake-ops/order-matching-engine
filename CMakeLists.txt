cmake_minimum_required(VERSION 3.28)
project(TradingExchangeEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")

find_package(Boost REQUIRED)

set(PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)


add_library(pricing
        src/pricing/MarketPrice.cpp
)

target_include_directories(pricing
        PUBLIC
        ${PROJECT_INCLUDE_DIR}
)

add_library(simulation
        src/simulation/MarketSimulator.cpp
)

target_include_directories(simulation
        PUBLIC
        ${PROJECT_INCLUDE_DIR}
)

target_link_libraries(simulation
        PUBLIC
        pricing
        events
)

add_library(orderbook
        src/orderbook/OrderBookSimulator.cpp
)

target_include_directories(orderbook
        PUBLIC
        ${PROJECT_INCLUDE_DIR}
)

add_library(events
        src/events/event_bus.cpp
)

target_include_directories(events
        PUBLIC
        ${PROJECT_INCLUDE_DIR}
)

add_library(transport
        src/transport/MarketDataStore.cpp
        src/transport/server.cpp
)

target_include_directories(transport
        PUBLIC
        ${PROJECT_INCLUDE_DIR}
)

add_executable(market_data_service
        apps/market_data_service/main.cpp
)

target_include_directories(market_data_service
        PRIVATE
        ${PROJECT_INCLUDE_DIR}
)

target_link_libraries(market_data_service
        PRIVATE
        simulation
        orderbook
        transport
)


add_subdirectory(test)
