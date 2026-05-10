cmake_minimum_required(VERSION 3.28)
project(TradingExchangeEngine)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")

find_program(GCOVR_EXECUTABLE gcovr)

find_package(Boost REQUIRED)

set(PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(pricing src/pricing/MarketPrice.cpp)

target_include_directories(pricing PUBLIC ${PROJECT_INCLUDE_DIR})

add_library(simulation src/simulation/MarketSimulator.cpp)

target_include_directories(simulation PUBLIC ${PROJECT_INCLUDE_DIR})

target_link_libraries(simulation PUBLIC pricing events)

add_library(orderbook
        src/orderbook/OrderBook.cpp
        src/orderbook/OrderBookSimulator.cpp)

target_include_directories(orderbook PUBLIC ${PROJECT_INCLUDE_DIR})

add_library(events src/events/event_bus.cpp)

target_include_directories(events PUBLIC ${PROJECT_INCLUDE_DIR})

add_library(transport src/transport/MarketDataStore.cpp
                      src/transport/server.cpp)

target_include_directories(transport PUBLIC ${PROJECT_INCLUDE_DIR})

add_executable(market_data_service apps/market_data_service/main.cpp)

target_include_directories(market_data_service PRIVATE ${PROJECT_INCLUDE_DIR})

target_link_libraries(market_data_service PRIVATE simulation orderbook
                                                  transport)

add_subdirectory(test)

if (GCOVR_EXECUTABLE)
    add_custom_target(coverage
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            COMMAND ${GCOVR_EXECUTABLE}
                    --root ${CMAKE_SOURCE_DIR}
                    --object-directory ${CMAKE_BINARY_DIR}
                    --filter ${CMAKE_SOURCE_DIR}/src
                    --exclude ${CMAKE_SOURCE_DIR}/test/.*
                    --exclude ${CMAKE_BINARY_DIR}/.*
                    --exclude ${CMAKE_SOURCE_DIR}/CMakeFiles/.*
                    --exclude ${CMAKE_SOURCE_DIR}/_deps/.*
                    --exclude-directories ${CMAKE_SOURCE_DIR}/CMakeFiles
                    --exclude-directories ${CMAKE_SOURCE_DIR}/_deps
                    --html --html-details
                    --output ${CMAKE_BINARY_DIR}/coverage.html
                    ${CMAKE_BINARY_DIR}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running tests and generating filtered coverage report"
            VERBATIM)
else ()
    add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -E echo "gcovr was not found. Install gcovr to generate filtered coverage reports."
            COMMAND ${CMAKE_COMMAND} -E false
            VERBATIM)
endif ()
