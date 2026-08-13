# Trading Exchange Engine Backlog

This backlog tracks the path from the current market-data simulator toward a true trading exchange engine.

## Current State

- Price generation exists through `PriceGenerator` and `MarketSimulator`.
- A real limit order book exists in `orderbook::OrderBook`, with limit and market order matching, FIFO behavior at a price level, cancellation, best bid/ask helpers, depth aggregation, and unit tests.
- The market-data service exposes HTTP price lookup and WebSocket price streaming.
- WebSocket clients now have per-session symbol subscription state for price updates.
- WebSocket I/O is serialized per session with an Asio strand and outbound queue; subscription state is mutex-protected.
- `MarketDataStore` stores the latest price snapshot per symbol behind a mutex.
- Early order-entry domain structs exist in `orderbook::OrderRequest`, `OrderResult`, and `OrderState`.
- `OrderBookManager` owns synchronized per-symbol books, rejects invalid submissions, tracks active and seen order ids, and returns snapshot copies for readers.
- The application still behaves mostly like a market-data simulator, not an order-driven exchange.

## Next Recommended Step

### EX-020: Implement `POST /api/orders`

Status: Not started

The existing market-data, WebSocket, event-bus, and order-manager components have been concurrency-hardened. The next feature step is the order-entry endpoint described under Phase 2.

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
- [ ] Add a domain type or enum for order status: accepted, rejected, partially filled, filled, canceled.
- [x] Add initial request and response models for order entry.
- [ ] Keep the existing `orderbook::Order` as the matching input, or define a clear adapter from API request to book order.

Notes:
- `OrderStatus` currently includes `NEW`, `PARTIALLY_FILLED`, `FILLED`, and `CANCELLED`.
- There is no `REJECTED` status yet, and `NEW` has not been reconciled with the backlog's `accepted` wording.
- `OrderRequest`, `OrderResult`, and `OrderState` exist in `include/orderbook/OrderBook.h`, but they are not yet wired into API handling or manager-level validation.

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

### EX-012: Add Trade and Execution Reports

Status: Not started

Acceptance criteria:
- Convert `orderbook::Trade` into client-facing execution reports.
- Emit execution reports for accepted, filled, partially filled, canceled, and rejected orders.
- Store recent trades for query and streaming.

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

Notes:
- Current source tests already cover manager-level validation, active-order tracking, duplicate ids after fills, market orders, and multi-symbol routing.
- Treat `client_order_id` as the order id for the first API slice unless a separate exchange-generated id is introduced.

### EX-021: Implement `DELETE /api/orders/{order_id}`

Status: Not started

Acceptance criteria:
- Cancel an active resting order.
- Return not found for unknown or already-final orders.
- Publish an order canceled event when cancellation succeeds.

### EX-022: Implement Order Query Endpoints

Status: Not started

Acceptance criteria:
- `GET /api/orders/{order_id}` returns order status.
- `GET /api/orders` returns recent or active orders.
- Query responses are sourced from order manager state.

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

- A fresh debug configure/build discovers the full CTest suite successfully.
- Run the normal debug preset and the ThreadSanitizer configuration before relying on concurrency-sensitive changes.

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

Status: Not started

Acceptance criteria:
- Separate transport concerns from matching logic.
- Keep order validation, order management, matching, event emission, and API serialization in distinct components.
- Document the ownership model for shared state.

## Later Work

- Authentication and client session identity.
- Persistence or event journaling.
- Replay and recovery.
- Risk checks and account balances.
- Latency and throughput benchmarking.
- Binary protocol or FIX-like gateway.
- Multi-threaded matching model with one book actor per symbol.
