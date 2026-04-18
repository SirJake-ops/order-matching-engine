cmake_minimum_required(VERSION 3.28)
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
enable_testing()

file(GLOB PARENT_SOURCES ../src/*.cpp)
file(GLOB PARENT_HEADERS ../include/*.h)

add_executable(
        TradingExchangeTest
        main_test.cpp
        ${PARENT_SOURCES}
)

target_include_directories(TradingExchangeTest PRIVATE ../include)

target_link_libraries(TradingExchangeTest
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
)

include(GoogleTest)
gtest_discover_tests(TradingExchangeTest)
