# Trading Exchange Engine

`trading-exchange-engine` is a C++20 market data and order book service built around a simple simulation pipeline. It currently generates mock market updates, routes them through an event bus, maintains order book components, and includes the early transport-layer work needed for HTTP and WebSocket delivery.

## Current Status

- Market price generation is implemented.
- The event bus and order book components exist.
- The transport layer is under active refactor for HTTP and WebSocket support.
- Backlog items are tracked in [docs/exchange-engine-backlog.md](docs/exchange-engine-backlog.md).

## Project Layout

- `apps/market_data_service`: application entrypoint
- `include`: public headers
- `src/pricing`: market price models and generation
- `src/simulation`: market simulator
- `src/orderbook`: limit order book and multi-symbol order manager
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
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

For concurrency-sensitive changes, run the ThreadSanitizer preset as well:

```bash
cmake --preset tsan
cmake --build --preset tsan
ctest --preset tsan
```

## CI and Releases

GitHub Actions builds and tests pull requests and pushes to `master`. The pipeline
checks GCC Debug/Release, Clang Debug, AddressSanitizer/UndefinedBehaviorSanitizer,
ThreadSanitizer, source coverage, and the running service's HTTP endpoints.

Push a version tag such as `v1.0.0` to run all checks and publish a Linux binary
and SHA-256 checksum as a GitHub Release. No hosting account or deployment secrets
are needed. Jobs run only in public repositories on standard Ubuntu runners;
there are no Actions cache or artifact uploads.

See [CI/CD documentation](docs/ci-cd.md) for local commands, release instructions,
cost details, and runner compatibility notes.

## Run

Build the project, then run:

```bash
./build/debug/market_data_service
```

## Run for CLion

Open the repository root, not an existing build directory. CLion should detect
`CMakePresets.json` and offer the `debug` and `release` profiles automatically.

If CLion still shows old targets or old run configurations, remove the project
from the recent list, delete `.idea/`, and reopen the repo so CLion rebuilds
its workspace from the current CMake project instead of stale metadata.
