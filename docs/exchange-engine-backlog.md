# Trading Exchange Engine Backlog

This backlog tracks the path from the current market-data simulator toward a C++ exchange engine with deterministic replay and reproducible performance experiments.

Last reviewed: 2026-11-09, against the source, tests, build configuration, and local service verification. Status describes implemented behavior; the milestone sequence below is proposed work.

For this planning portion, assistance is limited to Markdown documentation: backlog updates, design discussion, API contracts, and test plans. Jake will write the implementation. Proposed work below is not authorization to change code.

## Project Direction

Agreed direction: combine a C++ systems-engineering project with an engine for benchmarking, replay, and performance experiments.

The target is a local, order-driven exchange simulator whose matching behavior can be explained, tested, replayed, and measured. The engineering story should cover order lifecycle correctness, ownership of shared state, reproducibility, recovery, and measured tradeoffs.

Proposed priorities:

1. Complete a usable order flow through the existing service.
2. Define lifecycle and numeric rules so outcomes are unambiguous.
3. Make ordered command streams replayable and verify identical results.
4. Measure matching, management, and transport separately; optimize from evidence.

A UI, real-money trading, external brokerage integration, and distributed deployment are outside the current target. A small command-line client or workload driver can demonstrate the engine before a UI is considered.

## Current State

- Price generation exists through `PriceGenerator` and `MarketSimulator`.
- A real limit order book exists in `orderbook::OrderBook`, with limit and market order matching, FIFO behavior at a price level, cancellation, best bid/ask helpers, depth aggregation, and unit tests.
- The market-data service exposes HTTP price lookup and WebSocket price streaming.
- WebSocket clients now have per-session symbol subscription state for price updates.
- WebSocket I/O is serialized per session with an Asio strand and outbound queue; subscription state is mutex-protected.
- `MarketDataStore` stores the latest price snapshot per symbol behind a mutex.
- Early order-entry domain structs exist in `orderbook::OrderRequest`, `OrderResult`, and `OrderState`.
- `OrderBookManager` owns synchronized per-symbol books, rejects invalid submissions, tracks active and seen order ids, and returns snapshot copies for readers.
- Orders can currently enter through the C++ manager API and tests only. HTTP accepts GET requests; submission, cancellation, and order query endpoints do not exist.
- Simulated prices are independent of matching. `main` creates empty books and never submits orders, so live book depth stays empty during the current simulation.
- Cancellation exists inside `OrderBook`, but is not exposed by `OrderBookManager`. Filled orders and market orders leave the active map; completed order history is not retained.
- Prices currently use `double`; quantities use signed 64-bit integers. Instrument tick/lot rules are not defined.
- State is in memory. There is no command journal, replay/recovery path, or benchmark harness.
- The application is a working market-data simulator with a tested matching core. Connecting order entry is the next functional milestone.

### Verified Local Baseline — 2026-11-09

- CLion's Debug profile configures successfully using Ninja, GCC 16.2.1, C++20, and Boost 1.92 in `build/clion-debug`.
- A fresh build completed; all **68 CTest cases passed**, including loopback WebSocket integration tests.
- `scripts/smoke-test.py` passed against that executable: price lookup, depth lookup, encoded symbols, HTTP 404, and SIGTERM shutdown.
- The service was started as the transient user unit `trading-exchange-engine.service`; live AAPL price and order-book requests returned successfully on port 8080. This is a local run, not a boot-time deployment.
- Stale in-source `CMakeCache.txt` and `CMakeFiles/` were moved to `build/legacy-in-source-20261109/`. The IDE profile uses the existing `_deps/googletest-src` checkout.
- CI and release automation are committed in `.github/workflows/ci.yml`. Hosted run `42757306965` for `3dc19cf` passed GCC Debug, Clang Debug, ASan/UBSan, ThreadSanitizer, and Coverage. GCC Release stalled during APT index refresh and was canceled around its job limit; the quality gate failed and release publication was skipped. See the [CI incident notes](ci-cd.md#observed-setup-timeout--2026-11-09-local--2026-11-10-utc).

Local commands for the configured IDE build:

```bash
cmake --build build/clion-debug --parallel 4
ctest --test-dir build/clion-debug --output-on-failure
journalctl --user -u trading-exchange-engine -f
systemctl --user stop trading-exchange-engine
```

The smoke test starts its own server and requires port 8080 to be free. Stop the running service before using it. Shared CLI presets remain in `CMakePresets.json`; this machine's existing `build/debug` cache still contains old machine paths and should not be reused without a fresh configure.

## Proposed Milestones

| Milestone | Observable result | Main backlog items | Status |
| --- | --- | --- | --- |
| M0: Working baseline | Build, test, and run the market-data service locally | EX-001, EX-002, EX-011A, EX-060, EX-065 | Locally verified; hosted GCC Release setup blocked |
| M1: First external trade | Submit a resting order and a crossing order over HTTP; receive the trade and observe remaining depth | EX-010, EX-020, first slice of EX-061 | Not started; supporting domain types already exist |
| M2: Complete order lifecycle | Query accepted, partial, filled, and canceled orders; cancel safely; define prices and quantities | EX-011, EX-012, EX-013, EX-021, EX-022, EX-051 | Partially implemented internally; APIs not started |
| M3: Reproducible engine | Apply a sequenced command stream, publish book/trade events, and replay to identical outcomes | EX-030, EX-031, EX-040, EX-062, EX-070, EX-072 | Not started |
| M4: Measured performance | Reproduce baseline throughput and latency; explain a measured improvement and its tradeoffs | EX-071, EX-073 | Not started |

EX-050 (configuration) and EX-063 (transport reliability) support these milestones. Capture an initial matching baseline with EX-071 before optimization; a full replay/recovery implementation is not a prerequisite for an early measurement.

### Decisions to Make Next

- For the first deterministic model, use one configured instrument with integer price ticks and whole-unit quantities, or support several instruments with distinct tick/lot rules immediately? Proposed starting scope: one instrument, while preserving existing multi-symbol routing.
- Define market-order remainder handling, order-id scope, API status/error mapping, and retention of final states before implementing EX-020. Do not infer that an inactive order was fully filled.
- Choose the first benchmark workload and record a baseline before setting numerical latency/throughput goals. Proposed start: matching-only limit orders with resting, crossing, and cancellation mixes.
- Decide the recovery guarantee for the journal before implementing EX-072: in-memory replay experiments and crash-durable acknowledgment require different guarantees.

## Next Recommended Step

Start with EX-010's minimum API contract and EX-020, with HTTP order-flow tests from EX-061. Existing concurrency regression coverage is a baseline; resource limits and stalled-client behavior remain separate work under EX-063.

First demonstration: submit a sell limit order for 10 units at 100, then a buy limit order for 4 units at 100. The API should report a trade for 4 units at 100, and the existing depth endpoint should show 6 units remaining on the ask side. Add malformed-input and rejection cases in the same slice.

This slice does not require completing persistence, a general event framework, or a benchmark suite first. No feature implementation has started as part of this backlog review.

## Recently Completed / Mostly Complete

### EX-011A: Stabilize the Early Order Book Manager

Status: Complete

Acceptance criteria:
- [x] Add unit tests for construction and routing orders to an existing symbol book.
- [x] Add tests for current unknown-symbol lookup and submission behavior.
- [x] Decide whether unknown order symbols should be rejected or dynamically created.
- [x] Fix `add_order` so it does not silently swallow errors or recreate existing books unexpectedly.
- [x] Return trades from submitted orders instead of discarding them.
- [x] Support construction with multiple configured symbols.
- [x] Expose clear lookup behavior for missing symbols.

Notes:
- Unknown symbols are currently rejected by throwing `std::runtime_error`.
- `test/order_book_manager_test.cpp` covers configured-symbol construction, multi-symbol construction, duplicate-symbol rejection, order routing, empty trade returns for resting orders, matching through the managed book, returned trade payloads, unknown-symbol lookup, and unknown-symbol submission errors.
- `OrderBookManager` currently accepts a list of configured symbols.
- `add_order` validates that a book exists, propagates errors to callers, and returns the trades produced by the underlying book.
- `OrderBookManager` now tracks active orders and seen order ids, updates active state as trades fill orders, and removes fully filled or market orders from the active set.
- Manager operations are serialized, and `get_orderbook` returns a snapshot so internal book references cannot escape synchronization.

### EX-002: Harden Existing Concurrent State

Status: Complete

Acceptance criteria:
- [x] Serialize WebSocket reads, writes, and close handling per session.
- [x] Protect per-session subscription state from concurrent broadcast and command handling.
- [x] Make event-bus subscription and publication thread-safe without invoking callbacks under its mutex.
- [x] Protect `OrderBookManager` books, active states, and seen order ids.
- [x] Protect the shared price-generator state.
- [x] Add concurrent regression coverage and a ThreadSanitizer build verification path.

Notes:
- WebSocket sessions own an outbound queue and use an Asio strand; one slow client no longer holds a process-wide write mutex.
- The current manager uses a single correctness-first mutex. Per-symbol execution can be introduced later if profiling justifies it.
- Server shutdown is stop-token driven, and request threads are no longer detached.

### EX-001: Finish WebSocket Price Subscription Behavior

Status: Complete

Goal: make the current Pub/Sub slice complete before moving into matching-engine work.

Acceptance criteria:
- [x] `subscribe:<symbol>` updates the client subscription state.
- [x] `unsubscribe:<symbol>` updates the client subscription state.
- [x] Subscribe and unsubscribe commands send acknowledgement messages.
- [x] Unknown WebSocket messages return `{"error":"unknown_message"}`.
- [x] `broadcastPriceUpdate` only writes matching updates to each client.
- [x] Tests cover default delivery, explicit symbol filtering, subscribe helper behavior, and unsubscribe helper behavior.
- [x] Add WebSocket integration tests for actual client subscribe/unsubscribe traffic.

Notes:
- Current default is `receive_all_price_updates = true`.
- Once a client subscribes to a symbol, it switches to explicit symbol filtering.
- `ServerWebSocketIntegrationTest` covers loopback WebSocket connection, subscribe/unsubscribe acknowledgements, unknown-message errors, default price delivery, matching subscribed delivery, and filtered non-delivery.

## Phase 1: Exchange Domain Model

### EX-010: Define Order IDs, Client IDs, and Order Statuses

Status: In progress

Acceptance criteria:
- [x] Add initial lifecycle statuses for new, partially filled, filled, and canceled orders.
- [ ] Define the API mapping for accepted/rejected requests and order lifecycle states, including market orders with unfilled remainders.
- [x] Add initial request and response models for order entry.
- [ ] Keep the existing `orderbook::Order` as the matching input, or define a clear adapter from API request to book order.
- [ ] Define order-id uniqueness scope and timestamp/command-sequence ownership.

Notes:
- `OrderStatus` currently includes `NEW`, `PARTIALLY_FILLED`, `FILLED`, and `CANCELLED`.
- There is no `REJECTED` status yet, and `NEW` has not been reconciled with the API's accepted wording. Decide whether rejection is a submission result or a retained order status.
- `OrderRequest` and `OrderResult` exist in `include/orderbook/OrderBook.h` but are unused by API handling. The manager validates `Order` directly and uses `OrderState` internally.

### EX-011: Add an Order Manager

Status: Mostly complete

Acceptance criteria:
- [x] Create a component that owns one `orderbook::OrderBook` per symbol.
- [x] Route incoming orders to the correct book.
- [x] Return trades produced by the book.
- [x] Track active orders by order id.
- [x] Reject duplicate ids, unknown symbols, invalid quantity, and invalid price.
- [ ] Wire the order manager into the order-entry API.

Notes:
- `include/orderbook/OrderBookManager.h` and `src/orderbook/OrderBookManager.cpp` now exist and are included in the `orderbook` CMake target.
- The manager owns one book per configured symbol and rejects duplicate configured symbols.
- Unknown symbols, duplicate order ids, invalid quantity, and invalid limit price are rejected at the manager level before routing to an order book.
- `OrderBookManager` tracks active orders by order id, keeps a seen-order-id set so ids cannot be reused after fills, and updates active state as trades are applied.
- `OrderBookManager` is wired into the transport layer for read-only depth snapshots, but not into order entry.
- `has_active_order` is the only order-state lookup. Cancellation, final-state retention, and queryable execution history still need manager support for EX-021/EX-022.

### EX-012: Add Trade and Execution Reports

Status: Not started

Acceptance criteria:
- Convert `orderbook::Trade` into client-facing execution reports.
- Emit execution reports for accepted, filled, partially filled, canceled, and rejected orders.
- Store recent trades for query and streaming.

### EX-013: Define Exact Price and Quantity Rules

Status: Not started

Acceptance criteria:
- [ ] Define integer price ticks and instrument tick sizes; replace floating-point matching keys with an exact representation.
- [ ] Define whole-unit or lot-based quantities and reject values outside the chosen instrument rules.
- [ ] Specify API conversion and serialization without silently rounding invalid prices.
- [ ] Cover price boundaries, quantity/depth overflow, and exact trade-price/depth results.

Notes:
- Coordinate with EX-051. The current `double` representation is adequate for demonstrating the existing slice, but exact price rules are part of the intended deterministic engine.

## Phase 2: Order Entry API

### EX-020: Implement `POST /api/orders`

Status: Not started

Acceptance criteria:
- Accept JSON order input with symbol, side, type, price, quantity, and client order id.
- Validate request shape and business rules.
- Submit valid orders to the order manager.
- Return accepted/rejected response with order id and status.
- Return generated trades or execution reports when matches occur.

Test checklist:
- [ ] Valid resting limit order returns accepted/new status.
- [ ] Crossing limit order returns generated trades in the response.
- [ ] Market order submits without requiring a positive price.
- [ ] Unknown symbol returns rejected or not-found behavior.
- [ ] Duplicate client order id is rejected.
- [ ] Invalid quantity is rejected.
- [ ] Invalid limit price is rejected.
- [ ] Malformed JSON is rejected.
- [ ] Unsupported side/type strings are rejected.
- [ ] Partial and zero-fill market orders return the explicitly defined remainder outcome.
- [ ] HTTP submissions update the depth returned by the existing order-book endpoint.
- [ ] Missing fields, wrong JSON types, out-of-range values, and non-finite prices produce a stable error response without mutating the book.

Notes:
- Current source tests already cover manager-level validation, active-order tracking, duplicate ids after fills, market orders, and multi-symbol routing.
- Treat `client_order_id` as the order id for the first API slice unless a separate exchange-generated id is introduced.

### EX-021: Implement `DELETE /api/orders/{order_id}`

Status: Not started

Acceptance criteria:
- Cancel an active resting order.
- Expose cancellation through the synchronized manager and update its active state atomically with book changes.
- Return not found for unknown or already-final orders.
- Publish an order canceled event when cancellation succeeds.

### EX-022: Implement Order Query Endpoints

Status: Not started

Acceptance criteria:
- `GET /api/orders/{order_id}` returns order status.
- `GET /api/orders` returns recent or active orders.
- Query responses are sourced from order manager state.
- Retain final order states under a defined retention policy; distinguish completed orders from unknown ids.

## Phase 3: Market Data From the Book

### EX-030: Publish Order Book Updates

Status: Not started

Acceptance criteria:
- Emit order book updates after accepted, matched, or canceled orders.
- Include symbol, best bid, best ask, and configurable depth levels.
- Stream updates over WebSocket as `orderbook_update`.

### EX-031: Publish Trade Updates

Status: Not started

Acceptance criteria:
- Emit trade messages when orders match.
- Include symbol, price, quantity, timestamp, buy order id, and sell order id.
- Stream updates over WebSocket as `trade`.

### EX-032: Add REST Market Data Endpoints

Status: Partially complete

Acceptance criteria:
- [ ] `GET /api/market/prices` returns all latest prices.
- [x] `GET /api/market/orderbook/{symbol}` returns current depth.
- [ ] `GET /api/market/trades` returns recent trades.

Notes:
- `GET /api/market/prices/{symbol}` exists and returns a single latest price snapshot.
- `GET /api/market/orderbook/{symbol}` exists and returns bid/ask depth from `OrderBookManager`.
- `MarketDataStore::getAllPrices()` exists, but there is no REST endpoint for all prices yet.

## Phase 4: Event Model

### EX-040: Replace String Events With Typed Events

Status: Not started

Acceptance criteria:
- Define typed events for price updates, order book updates, order lifecycle events, and trades.
- Stop publishing free-form strings from `MarketSimulator`.
- Ensure transport, storage, and simulation components consume the same event types.

### EX-041: Make the Event Bus Thread-Safe

Status: Complete

Acceptance criteria:
- [x] Protect subscriber registration and publishing from concurrent access issues.
- [x] Keep callbacks synchronous, using a subscriber snapshot so callbacks run without the bus mutex held.
- [x] Add tests for concurrent subscription/publication and reentrant subscription.

## Phase 5: Configuration and Static Data

### EX-050: Load Runtime Configuration

Status: Not started

Acceptance criteria:
- Move symbols, port, update interval, and depth levels out of `main`.
- Support a defined config source, such as JSON file or environment variables.
- Fail fast on invalid configuration.

### EX-051: Integrate Static Instrument Data

Status: Not started

Acceptance criteria:
- Define supported instruments and metadata.
- Validate order symbols against static data.
- Use instrument rules for price precision and quantity precision.

## Phase 6: Reliability and Quality

### Local Test Discovery Note

- The 2026-11-09 local Debug verification discovered and passed 68 tests; this is a dated baseline, not a fixed required count.
- Use the configured `build/clion-debug` directory on this machine or a fresh preset build directory. Old in-source and migrated build caches caused IDE configuration problems.
- Loopback integration tests require local socket access. The initial sandboxed run failed on socket permissions; the run with socket access passed all tests.
- Run ThreadSanitizer for concurrency-sensitive changes. Benchmark optimized builds separately from sanitizer builds.

### EX-060: Add WebSocket Integration Tests

Status: Complete

Acceptance criteria:
- [x] Test WebSocket connection establishment.
- [x] Test subscribe and unsubscribe acknowledgements.
- [x] Test that subscribed clients receive matching price updates.
- [x] Test that unsubscribed clients do not receive filtered updates.

### EX-061: Add End-to-End Order Flow Tests

Status: Not started

Acceptance criteria:
- Submit crossing orders and verify trades.
- Submit non-crossing orders and verify book depth.
- Cancel resting orders and verify depth changes.
- Verify trade and book update messages are emitted.

### EX-062: Define Engine Boundaries

Status: Partially complete

Acceptance criteria:
- [x] Keep the matching book and manager separate from the transport classes.
- [ ] Define a transport-independent command/result boundary usable by HTTP, replay, and benchmark drivers.
- [ ] Keep API parsing/serialization outside matching and lifecycle logic.
- [ ] Document state ownership, command ordering, and where events are emitted relative to mutation and acknowledgment.

### EX-063: Bound Transport Resource Usage and Propagate Failures

Status: Not started

Acceptance criteria:
- [ ] Define request/body limits and read/write/upgrade deadlines.
- [ ] Ensure a stalled HTTP client cannot block all subsequent accepts or prevent shutdown.
- [ ] Bound per-session WebSocket output queues and define the slow-consumer policy.
- [ ] Propagate bind/startup failure so the process fails clearly instead of continuing price generation without a listener.
- [ ] Cover stalled clients, slow consumers, occupied ports, and shutdown with active connections.

Notes:
- `handleSession` currently performs synchronous HTTP reads/writes and the WebSocket handshake in the accept loop.
- WebSocket writes are serialized correctly, but `_pending_writes` has no configured bound. The server logs startup exceptions without signaling failure to `main`.

### EX-065: Add Build, Test, and Release Automation

Status: Implemented; hosted GCC Release setup blocked

Acceptance criteria:
- [x] Commit Debug/Release, Clang, sanitizer, and coverage configurations.
- [x] Add a real-service HTTP smoke test and a CI quality gate.
- [x] Add version-tag release packaging with a checksum and runtime dependency notes.
- [x] Verify the local Debug build, CTest suite, and service smoke test.
- [x] Inspect hosted run `42757306965` for current commit `3dc19cf`; identify the Release setup stall.
- [ ] Obtain a fully passing hosted run, including GCC Release and the service smoke test.
- [ ] Verify release packaging/publication on an intentionally selected version tag.

Notes:
- See [CI/CD documentation](ci-cd.md) and `.github/workflows/ci.yml`. Most hosted checks passed, but the required gate remains failed; no release publication is verified.

### EX-066: Make CI Package Setup Fail Clearly and Recover From Transient Failures

Status: Diagnosed; workflow changes not started

Acceptance criteria:
- [ ] Separate APT index refresh from dependency installation in job logs.
- [ ] Bound APT retries, HTTP/HTTPS connection/data waits, and total setup time.
- [ ] Fail clearly when required package indexes cannot be refreshed; define fallback behavior if mirror failures recur.
- [ ] Apply the setup policy consistently to matrix, coverage, and publication jobs.
- [ ] Verify a complete hosted Release build/test/smoke run after retry or remediation.

Notes:
- In the reviewed run, GCC Release never reached Boost installation. The same setup script completed on the Debug VM in about 11 seconds. The evidence supports a package-mirror connectivity stall, with the exact network cause unconfirmed.

## Phase 7: Determinism, Replay, and Performance

### EX-070: Add Deterministic Command Streams and Invariant Tests

Status: Not started

Acceptance criteria:
- [ ] Define ordered submit/cancel commands and deterministic id/time inputs, independent of HTTP and wall-clock scheduling.
- [ ] Generate repeatable workloads from a recorded seed and configuration.
- [ ] Apply the same command stream to fresh engines and compare trades, lifecycle outcomes, and final depth.
- [ ] Check invariants such as FIFO execution, positive resting quantities, fill conservation, and cancellation consistency across generated sequences.
- [ ] Preserve a failing sequence as a small reproducible regression fixture.

### EX-071: Establish Reproducible Performance Baselines

Status: Not started

Acceptance criteria:
- [ ] Measure matching-only work, synchronized manager operations, and end-to-end transport as separate paths.
- [ ] Define seeded workloads including order counts, active book depth, crossing/resting mix, cancellation rate, and symbol distribution.
- [ ] Record throughput and latency distributions (including p50/p95/p99), warm-up, repeated runs, and measurement boundaries.
- [ ] Record revision, compiler/build flags, dependency versions, hardware, workload seed, and concurrency settings with results.
- [ ] Use optimized builds without sanitizer instrumentation; verify workload correctness separately.
- [ ] Store a reproducible command and baseline report before making optimization claims.

Notes:
- Initially measure the current manager mutex and book-copy behavior. Choose architecture changes only after a profile identifies a relevant cost.
- Initial baselines are measurements, not an arbitrary CI performance gate. Shared-runner timing should not be treated as a workstation latency guarantee.

### EX-072: Add a Command Journal and Replay/Recovery

Status: Not started

Acceptance criteria:
- [ ] Specify journal schema/version, command sequence numbers, deterministic inputs, and acknowledgment/durability semantics.
- [ ] Replay the recorded command stream through the same engine command boundary used by live requests.
- [ ] Compare recovered orders, trades, and depth with uninterrupted execution.
- [ ] Define handling for rejected commands, duplicate submissions, a truncated final record, and incompatible journal versions.
- [ ] Document which guarantees require durable writes; benchmark their cost separately from in-memory matching.

Notes:
- Start with a single ordered command stream and file-backed journal. Snapshots and multi-symbol partitioning can follow after a correct recovery path exists.

### EX-073: Make and Explain a Measured Optimization

Status: Not started

Acceptance criteria:
- [ ] Select a bottleneck from EX-071 profiling and state a measurable hypothesis.
- [ ] Compare before/after revisions using the same recorded workloads and environment.
- [ ] Preserve lifecycle, matching, replay, and concurrency correctness.
- [ ] Report latency/throughput results alongside memory use and any fairness or complexity tradeoffs.
- [ ] Document whether the evidence supports keeping the change.

## Later Work

- Authentication and client session identity.
- Journal snapshots and faster recovery after EX-072.
- Risk checks and account balances.
- Binary protocol or FIX-like gateway.
- Multi-threaded matching with one owner per symbol if benchmark evidence supports it.
- Optional UI or strategy bots once the command and market-data contracts are usable.
