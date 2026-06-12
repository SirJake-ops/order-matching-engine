# Trading Exchange Engine Backlog

This backlog tracks the path from the current market-data simulator toward a true trading exchange engine.

## Current State

- Price generation exists through `PriceGenerator` and `MarketSimulator`.
- A real limit order book exists in `orderbook::OrderBook`, with limit and market order matching, FIFO behavior at a price level, cancellation, best bid/ask helpers, depth aggregation, and unit tests.
- The market-data service exposes HTTP price lookup and WebSocket price streaming.
- WebSocket clients now have per-session symbol subscription state for price updates.
- `MarketDataStore` stores the latest price snapshot per symbol behind a mutex.
- Early order-entry domain structs exist in `orderbook::OrderRequest`, `OrderResult`, and `OrderState`.
- Early `OrderBookManager` scaffolding exists with initial unit tests, but it is not yet a complete order manager and still needs behavior fixes before it should be treated as exchange infrastructure.
- The application still behaves mostly like a market-data simulator, not an order-driven exchange.

## Next Recommended Step

### EX-011A: Stabilize the Early Order Book Manager

Status: In progress

Goal: turn the current `OrderBookManager` scaffold into a tested, reliable component before adding HTTP order entry.

Acceptance criteria:
- [x] Add unit tests for construction and routing orders to an existing symbol book.
- [x] Add tests for current unknown-symbol lookup and submission behavior.
- [ ] Decide whether unknown order symbols should be rejected or dynamically created.
- [ ] Fix `add_order` so it does not silently swallow errors or recreate existing books unexpectedly.
- [ ] Return trades from submitted orders instead of discarding them.
- [ ] Support construction with multiple configured symbols.
- [ ] Expose clear lookup behavior for missing symbols.

Notes:
- `test/order_book_manager_test.cpp` covers configured-symbol construction, order routing, matching through the managed book, unknown-symbol lookup, and the current behavior that unknown-symbol submission does not create a book.
- `OrderBookManager` currently accepts one symbol in its constructor.
- `add_order` validates that a book exists, then calls `emplace` for the same symbol and adds the order; because `emplace` does not replace an existing key, this happens to route to the existing book, but the intent is unclear.
- Exceptions are currently caught and logged to `stderr`, which prevents callers from reliably handling rejection paths.

## Recently Completed / Mostly Complete

### EX-001: Finish WebSocket Price Subscription Behavior

Status: Mostly complete

Goal: make the current Pub/Sub slice complete before moving into matching-engine work.

Acceptance criteria:
- [x] `subscribe:<symbol>` updates the client subscription state.
- [x] `unsubscribe:<symbol>` updates the client subscription state.
- [x] Subscribe and unsubscribe commands send acknowledgement messages.
- [x] Unknown WebSocket messages return `{"error":"unknown_message"}`.
- [x] `broadcastPriceUpdate` only writes matching updates to each client.
- [x] Tests cover default delivery, explicit symbol filtering, subscribe helper behavior, and unsubscribe helper behavior.
- [ ] Add WebSocket integration tests for actual client subscribe/unsubscribe traffic.

Notes:
- Current default is `receive_all_price_updates = true`.
- Once a client subscribes to a symbol, it switches to explicit symbol filtering.

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

Status: Started

Acceptance criteria:
- [ ] Create a component that owns one `orderbook::OrderBook` per symbol.
- [ ] Route incoming orders to the correct book.
- [ ] Return trades produced by the book.
- [ ] Track active orders by order id.
- [ ] Reject duplicate ids, unknown symbols, invalid quantity, and invalid price.

Notes:
- `include/orderbook/OrderBookManager.h` and `src/orderbook/OrderBookManager.cpp` now exist and are included in the `orderbook` CMake target.
- The current implementation is scaffold-level only: initial tests exist, but there is no active-order tracking, no multi-symbol initialization, and no returned trade results.

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
- [ ] `GET /api/market/orderbook/{symbol}` returns current depth.
- [ ] `GET /api/market/trades` returns recent trades.

Notes:
- `GET /api/market/prices/{symbol}` exists and returns a single latest price snapshot.
- `MarketDataStore::getAllPrices()` exists, but there is no REST endpoint for all prices yet.

## Phase 4: Event Model

### EX-040: Replace String Events With Typed Events

Status: Not started

Acceptance criteria:
- Define typed events for price updates, order book updates, order lifecycle events, and trades.
- Stop publishing free-form strings from `MarketSimulator`.
- Ensure transport, storage, and simulation components consume the same event types.

### EX-041: Make the Event Bus Thread-Safe

Status: Not started

Acceptance criteria:
- Protect subscriber registration and publishing from concurrent access issues.
- Decide whether callbacks run synchronously or through a queue.
- Add tests for multiple subscribers and basic concurrent publish behavior.

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

### EX-060: Add WebSocket Integration Tests

Status: Not started

Acceptance criteria:
- Test WebSocket connection establishment.
- Test subscribe and unsubscribe acknowledgements.
- Test that subscribed clients receive matching price updates.
- Test that unsubscribed clients do not receive filtered updates.

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
