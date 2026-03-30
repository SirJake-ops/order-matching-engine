# TradingExchange

`TradingExchange` is a C++20 market data service built around a simple simulation pipeline. It generates mock market updates, routes them through an event bus, and exposes them over HTTP and WebSocket.

This project is intended to run alongside the sibling Java application in `../probable-fiesta`. `TradingExchange` is the market-data provider, and `probable-fiesta` is the trading API that consumes its HTTP and WebSocket feeds.

## Current Status

- Market price generation is implemented.
- The event bus and order book components exist.
- The transport layer serves market prices over HTTP and WebSocket.
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

The service listens on `8080` and continuously emits mock market updates.

Important integration:

- HTTP snapshots: `GET /api/market/prices/{symbol}`
- WebSocket feed: `ws://localhost:8080/ws`
- The Java API in `../probable-fiesta` expects this service to be running when it creates market orders.

Recommended local startup order:

1. Start `TradingExchange` on `8080`
2. Start `probable-fiesta` on `8081`
3. Verify cross-app connectivity with:

```bash
curl -i http://localhost:8080/api/market/prices/BTC%2FUSD
curl -i http://localhost:8081/api/v1/auth/me
```

## Notes

- Coverage flags are enabled in the root `CMakeLists.txt`.
- This repo is the market-data side of a two-application local setup with `../probable-fiesta`.
