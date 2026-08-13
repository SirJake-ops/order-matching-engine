cmake_minimum_required(VERSION 3.28)
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
enable_testing()

add_executable(
        TradingExchangeEngineTest
        test_main.cpp
        event_bus_test.cpp
        market_data_store_test.cpp
        market_simulator_test.cpp
        order_book_manager_test.cpp
        order_book_test.cpp
        server_test.cpp
)

target_link_libraries(TradingExchangeEngineTest
    PRIVATE
    project_includes
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    simulation
    orderbook
    pricing
    events
    transport
)

include(GoogleTest)
gtest_discover_tests(TradingExchangeEngineTest)
