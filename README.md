# TradingExchange

`TradingExchange` is a C++20 market data playground built around a simple simulation pipeline. It currently generates mock market updates, routes them through an event bus, and includes the early transport-layer work needed for HTTP and WebSocket delivery.

## Current Status

- Market price generation is implemented.
- The event bus and order book components exist.
- The transport layer is under active refactor for HTTP and WebSocket support.
- WebSocket backlog items are tracked in [pseudo-jira-websocket.md](/home/jake/cpp-projects/fluffy-parakeet/pseudo-jira-websocket.md).

## Project Layout

- `apps/market_data_service`: application entrypoint
- `include`: public headers
- `src/pricing`: market price models and generation
- `src/simulation`: market simulator
- `src/orderbook`: order book simulator
- `src/events`: event bus and related types
- `src/transport`: server and transport-layer code
- `test`: CMake-managed tests

## Build

Requirements:

- CMake 3.28+
- A C++20 compiler
- Boost

Example build:

```bash
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug
```

## Run

Build the project, then run:

```bash
./cmake-build-debug/market_data_service
```

The current application loop starts the market simulation and emits mock market updates continuously.

## Notes

- Coverage flags are enabled in the root `CMakeLists.txt`.
- The top-level application is still evolving; transport and websocket behavior are not fully complete yet.
