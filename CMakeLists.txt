cmake_minimum_required(VERSION 3.28)
project(TradingExchangeEngine)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

option(ENABLE_COVERAGE "Build with coverage instrumentation" OFF)

if(ENABLE_COVERAGE)
  add_compile_options(--coverage)
  add_link_options(--coverage)
endif()

find_program(GCOVR_EXECUTABLE gcovr)

if(NOT Boost_INCLUDE_DIR)
  foreach(BOOST_INCLUDE_ENV_VAR BOOST_INCLUDE_DIR BOOST_INCLUDEDIR)
    if(DEFINED ENV{${BOOST_INCLUDE_ENV_VAR}})
      set(Boost_INCLUDE_DIR "$ENV{${BOOST_INCLUDE_ENV_VAR}}" CACHE PATH
          "Boost include directory")
      break()
    endif()
  endforeach()
endif()

find_package(Boost REQUIRED)

if(TARGET Boost::headers)
  set(PROJECT_BOOST_HEADERS_TARGET Boost::headers)
elseif(TARGET Boost::boost)
  set(PROJECT_BOOST_HEADERS_TARGET Boost::boost)
else()
  add_library(project_boost_headers INTERFACE)
  target_include_directories(project_boost_headers INTERFACE ${Boost_INCLUDE_DIRS})
  set(PROJECT_BOOST_HEADERS_TARGET project_boost_headers)
endif()

set(PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(project_includes INTERFACE)
target_include_directories(project_includes INTERFACE ${PROJECT_INCLUDE_DIR})

add_library(pricing src/pricing/MarketPrice.cpp)
target_link_libraries(pricing PUBLIC project_includes)

add_library(events src/events/event_bus.cpp
                   include/events/event_types/event_types.h)
target_link_libraries(events PUBLIC project_includes ${PROJECT_BOOST_HEADERS_TARGET})

add_library(simulation src/simulation/MarketSimulator.cpp)
target_link_libraries(simulation PUBLIC project_includes pricing events)

add_library(
  orderbook src/orderbook/OrderBookManager.cpp src/orderbook/OrderBook.cpp
            src/orderbook/OrderBookSimulator.cpp)
target_link_libraries(orderbook PUBLIC project_includes pricing)

add_library(transport src/transport/MarketDataStore.cpp
                      src/transport/server.cpp)
target_link_libraries(transport PUBLIC project_includes pricing ${PROJECT_BOOST_HEADERS_TARGET})

add_executable(market_data_service apps/market_data_service/main.cpp)
target_include_directories(market_data_service PRIVATE ${PROJECT_INCLUDE_DIR})
target_link_libraries(market_data_service PRIVATE project_includes simulation orderbook
                                                  transport events)

add_subdirectory(test)

if(GCOVR_EXECUTABLE)
  add_custom_target(
    coverage
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    COMMAND
      ${GCOVR_EXECUTABLE} --root ${CMAKE_SOURCE_DIR} --object-directory
      ${CMAKE_BINARY_DIR} --filter ${CMAKE_SOURCE_DIR}/src --exclude
      ${CMAKE_SOURCE_DIR}/test/.* --exclude ${CMAKE_BINARY_DIR}/.* --exclude
      ${CMAKE_SOURCE_DIR}/CMakeFiles/.* --exclude ${CMAKE_SOURCE_DIR}/_deps/.*
      --exclude-directories ${CMAKE_SOURCE_DIR}/CMakeFiles --exclude-directories
      ${CMAKE_SOURCE_DIR}/_deps --html --html-details --output
      ${CMAKE_BINARY_DIR}/coverage.html ${CMAKE_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running tests and generating filtered coverage report"
    VERBATIM)
else()
  add_custom_target(
    coverage
    COMMAND
      ${CMAKE_COMMAND} -E echo
      "gcovr was not found. Install gcovr to generate filtered coverage reports."
    COMMAND ${CMAKE_COMMAND} -E false
    VERBATIM)
endif()
