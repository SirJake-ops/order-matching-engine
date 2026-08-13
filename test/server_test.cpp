#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/buffer.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

#include "gtest/gtest.h"

#define private public
#include "transport/server.h"
#undef private

namespace {
    using server::MarketDataStore;
    using server::Server;
    using orderbook::Order;
    using orderbook::OrderType;
    using orderbook::Side;
    using tcp = boost::asio::ip::tcp;
    namespace websocket = boost::beast::websocket;

    boost::beast::flat_buffer makeBuffer(const std::string &message) {
        boost::beast::flat_buffer buffer;
        boost::asio::buffer_copy(buffer.prepare(message.size()), boost::asio::buffer(message));
        buffer.commit(message.size());
        return buffer;
    }

    std::shared_ptr<orderbook_manager::OrderBookManager> makeOrderBookManager() {
        return std::make_shared<orderbook_manager::OrderBookManager>(
            std::vector<std::string>{"AAPL", "MSFT", "BTC/USD"});
    }

    Order makeOrder(const std::string &id, const Side side, const double price,
                    const std::int64_t quantity, const std::string &symbol = "AAPL",
                    const std::uint64_t timestamp = 1,
                    const OrderType type = OrderType::LIMIT) {
        return Order{id, symbol, side, type, price, quantity, timestamp};
    }

    class WebSocketIntegrationHarness {
    public:
        using Client = websocket::stream<boost::beast::tcp_stream>;

    private:
        boost::asio::io_context server_io;
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> server_work_guard;
        tcp::acceptor acceptor;
        std::shared_ptr<MarketDataStore> store;
        Server server;
        unsigned short port;
        boost::asio::io_context client_io;
        std::deque<std::unique_ptr<Client> > clients;
        std::vector<std::thread> server_threads;
        std::thread server_io_thread;

    public:
        WebSocketIntegrationHarness()
            : server_work_guard(boost::asio::make_work_guard(server_io)),
              acceptor(server_io, tcp::endpoint(tcp::v4(), 0)),
              store(std::make_shared<MarketDataStore>()),
              server(store, makeOrderBookManager()), port(acceptor.local_endpoint().port()) {
            server_io_thread = std::thread([this]() { server_io.run(); });
        }

        ~WebSocketIntegrationHarness() {
            for (const auto &client: clients) {
                if (client->is_open()) {
                    boost::system::error_code error_code;
                    client->close(websocket::close_code::normal, error_code);
                }
            }

            for (auto &thread: server_threads) {
                if (thread.joinable()) {
                    thread.join();
                }
            }

            server_work_guard.reset();
            server_io.stop();
            if (server_io_thread.joinable()) {
                server_io_thread.join();
            }
        }

        Client &connectClient() {
            server_threads.emplace_back([this]() {
                tcp::socket socket(server_io);
                acceptor.accept(socket);
                server.handleSession(std::move(socket));
            });

            auto client = std::make_unique<Client>(client_io);
            tcp::resolver resolver(client_io);
            const auto results = resolver.resolve("127.0.0.1", std::to_string(port));
            boost::beast::get_lowest_layer(*client).connect(results);
            client->handshake("127.0.0.1", "/ws");
            clients.push_back(std::move(client));
            waitForSessionCount(clients.size());
            return *clients.back();
        }

        std::string readMessage(Client &client) {
            boost::beast::flat_buffer buffer;
            boost::beast::get_lowest_layer(client).expires_after(std::chrono::seconds(1));
            client.read(buffer);
            boost::beast::get_lowest_layer(client).expires_never();
            return boost::beast::buffers_to_string(buffer.data());
        }

        bool hasPendingBytes(Client &client) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            boost::system::error_code error_code;
            const auto bytes_available = boost::beast::get_lowest_layer(client).socket().available(error_code);
            return !error_code && bytes_available > 0;
        }

        void writeMessage(Client &client, const std::string &message) {
            boost::beast::get_lowest_layer(client).expires_after(std::chrono::seconds(1));
            client.write(boost::asio::buffer(message));
            boost::beast::get_lowest_layer(client).expires_never();
        }

        Server &getServer() { return server; }

    private:
        void waitForSessionCount(const std::size_t expected_count) {
            for (int attempt = 0; attempt < 50; ++attempt) {
                {
                    std::lock_guard lock(server._sessions_mutex);
                    if (server._sessions.size() == expected_count) {
                        return;
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    };

    TEST(ServerHelperTest, HandleRequestReturnsPriceSnapshotForKnownSymbol) {
        auto store = std::make_shared<MarketDataStore>();
        const market::MarkPrice price("BTC/USD", 62500.10, 62500.50, 62500.20, 4200);
        store->updatePrice(price);

        Server server(store, makeOrderBookManager());
        Server::Request request{boost::beast::http::verb::get, "/api/market/prices/BTC%2FUSD", 11};

        const auto response = server.handleRequest(request);

        EXPECT_EQ(response.result(), boost::beast::http::status::ok);
        EXPECT_EQ(response[boost::beast::http::field::content_type], "application/json");
        EXPECT_NE(response.body().find("\"symbol\":\"BTC/USD\""), std::string::npos);
        EXPECT_NE(response.body().find("\"bid\":"), std::string::npos);
        EXPECT_NE(response.body().find("\"timestamp\":"), std::string::npos);
    }

    TEST(ServerHelperTest, HandleRequestReturnsNotFoundForUnknownSymbol) {
        auto store = std::make_shared<MarketDataStore>();
        Server server(store, makeOrderBookManager());
        Server::Request request{boost::beast::http::verb::get, "/api/market/prices/MSFT", 11};

        const auto response = server.handleRequest(request);

        EXPECT_EQ(response.result(), boost::beast::http::status::not_found);
        EXPECT_EQ(response.body(), R"({"error":"symbol_not_found"})");
    }

    TEST(ServerHelperTest, HandleRequestRejectsUnsupportedMethodAndUnknownRoute) {
        auto store = std::make_shared<MarketDataStore>();
        Server server(store, makeOrderBookManager());

        Server::Request post_request{boost::beast::http::verb::post, "/api/market/prices/AAPL", 11};
        const auto post_response = server.handleRequest(post_request);
        EXPECT_EQ(post_response.result(), boost::beast::http::status::method_not_allowed);
        EXPECT_EQ(post_response.body(), R"({"error":"method_not_allowed"})");

        Server::Request unknown_route{boost::beast::http::verb::get, "/api/market/unknown", 11};
        const auto unknown_response = server.handleRequest(unknown_route);
        EXPECT_EQ(unknown_response.result(), boost::beast::http::status::not_found);
        EXPECT_EQ(unknown_response.body(), R"({"error":"not_found"})");
    }

    TEST(ServerHelperTest, ParseWebSocketMessageRecognizesPingAndSubscriptionCommands) {
        Server server(std::make_shared<MarketDataStore>(), makeOrderBookManager());

        auto ping_buffer = makeBuffer("ping");
        const auto ping_message = server.parseWebSocketMessage(ping_buffer);
        EXPECT_EQ(ping_message.type, Server::WebSocketMessageType::Ping);
        EXPECT_TRUE(ping_message.symbol.empty());

        auto subscribe_buffer = makeBuffer("subscribe:ETH/USD");
        const auto subscribe_message = server.parseWebSocketMessage(subscribe_buffer);
        EXPECT_EQ(subscribe_message.type, Server::WebSocketMessageType::Subscribe);
        EXPECT_EQ(subscribe_message.symbol, "ETH/USD");

        auto unsubscribe_buffer = makeBuffer("unsubscribe:ETH/USD");
        const auto unsubscribe_message = server.parseWebSocketMessage(unsubscribe_buffer);
        EXPECT_EQ(unsubscribe_message.type, Server::WebSocketMessageType::Unsubscribe);
        EXPECT_EQ(unsubscribe_message.symbol, "ETH/USD");

        auto unknown_buffer = makeBuffer("hello");
        const auto unknown_message = server.parseWebSocketMessage(unknown_buffer);
        EXPECT_EQ(unknown_message.type, Server::WebSocketMessageType::Unknown);
    }

    TEST(ServerHelperTest, DetectsWebSocketUpgradeRequests) {
        Server::Request normal_request{boost::beast::http::verb::get, "/api/market/prices/AAPL", 11};
        EXPECT_FALSE(Server::isWebSocketUpgrade(normal_request));

        Server::Request upgrade_request{boost::beast::http::verb::get, "/ws", 11};
        upgrade_request.set(boost::beast::http::field::connection, "upgrade");
        upgrade_request.set(boost::beast::http::field::upgrade, "websocket");
        upgrade_request.set(boost::beast::http::field::sec_websocket_version, "13");
        upgrade_request.set(boost::beast::http::field::sec_websocket_key, "dGhlIHNhbXBsZSBub25jZQ==");
        EXPECT_TRUE(Server::isWebSocketUpgrade(upgrade_request));
    }

    TEST(ServerHelperTest, ShouldReturnOrderBookForSymbol) {
        auto store = std::make_shared<MarketDataStore>();
        Server server(store, makeOrderBookManager());
        Server::Request request{boost::beast::http::verb::get, "/api/market/orderbook/AAPL", 11};

        const auto response = server.handleRequest(request);

        EXPECT_EQ(response.result(), boost::beast::http::status::ok);
        EXPECT_EQ(response.body(), R"({"symbol":"AAPL","bids":[],"asks":[]})");
    }

    TEST(ServerHelperTest, HandleRequestReturnsNotFoundForUnknownOrderBookSymbol) {
        auto store = std::make_shared<MarketDataStore>();
        Server server(store, makeOrderBookManager());
        Server::Request request{boost::beast::http::verb::get, "/api/market/orderbook/NVDA", 11};

        const auto response = server.handleRequest(request);

        EXPECT_EQ(response.result(), boost::beast::http::status::not_found);
        EXPECT_EQ(response.body(), R"({"error":"symbol_not_found"})");
    }

    TEST(ServerHelperTest, HandleRequestReturnsOrderBookDepthForSymbol) {
        auto store = std::make_shared<MarketDataStore>();
        auto order_book_manager = makeOrderBookManager();
        order_book_manager->add_order(makeOrder("buy-1", Side::BUY, 100.0, 3));
        order_book_manager->add_order(makeOrder("buy-2", Side::BUY, 99.0, 2));
        order_book_manager->add_order(makeOrder("sell-1", Side::SELL, 101.0, 4));
        Server server(store, order_book_manager);
        Server::Request request{boost::beast::http::verb::get, "/api/market/orderbook/AAPL", 11};

        const auto response = server.handleRequest(request);

        EXPECT_EQ(response.result(), boost::beast::http::status::ok);
        EXPECT_EQ(response.body(),
                  R"({"symbol":"AAPL","bids":[{"price":100,"quantity":3},{"price":99,"quantity":2}],"asks":[{"price":101,"quantity":4}]})");
    }

    TEST(ServerHelperTest, BuildPriceUpdateMessageUsesCurrentWebSocketEnvelope) {
        const market::MarkPrice price("AAPL", 189.45, 189.50, 189.47, 900);

        const auto message = Server::buildPriceUpdateMessage(price);

        EXPECT_NE(message.find(R"("type":"price_update")"), std::string::npos);
        EXPECT_NE(message.find(R"("symbol":"AAPL")"), std::string::npos);
        EXPECT_NE(message.find(R"("bid":189.45)"), std::string::npos);
        EXPECT_NE(message.find(R"("ask":189.50)"), std::string::npos);
        EXPECT_NE(message.find(R"("last":189.47)"), std::string::npos);
        EXPECT_NE(message.find(R"("volume":900)"), std::string::npos);
        EXPECT_NE(message.find(R"("timestamp":)"), std::string::npos);
    }

    TEST(ServerHelperTest, UrlHelpersDecodeEncodedSymbolsAndEscapeJsonQuotes) {
        EXPECT_EQ(Server::decodeUrlComponent("BTC%2FUSD+SPOT"), "BTC/USD SPOT");
        EXPECT_EQ(Server::escapeJson("A\"B\\C"), "A\\\"B\\\\C");
    }

    TEST(ServerHelperTest, RegisterAndUnregisterSessionManageSessionCollection) {
        boost::asio::io_context io_context;
        Server server(std::make_shared<MarketDataStore>(), makeOrderBookManager());
        auto session =
                std::make_shared<boost::beast::websocket::stream<Server::tcp::socket> >(io_context);
        const auto client_session = std::make_shared<Server::ClientSession>(session, server);

        server.registerSession(client_session);
        EXPECT_EQ(server._sessions.size(), 1);

        server.unregisterSession(client_session);
        EXPECT_TRUE(server._sessions.empty());
    }

    TEST(ServerHelperTest, BroadcastWithNoSessionsDoesNothing) {
        Server server(std::make_shared<MarketDataStore>(), makeOrderBookManager());
        const market::MarkPrice price("AAPL", 189.45, 189.50, 189.47, 900);

        EXPECT_NO_THROW(server.broadcastPriceUpdate(price));
        EXPECT_TRUE(server._sessions.empty());
    }

    TEST(ServerHelperTest, BroadcastRemovesSessionWhenWriteFails) {
        boost::asio::io_context io_context;
        Server server(std::make_shared<MarketDataStore>(), makeOrderBookManager());
        auto session =
                std::make_shared<boost::beast::websocket::stream<Server::tcp::socket> >(io_context);
        const auto client_session = std::make_shared<Server::ClientSession>(session, server);
        const market::MarkPrice price("AAPL", 189.45, 189.50, 189.47, 900);

        server.registerSession(client_session);
        ASSERT_EQ(server._sessions.size(), 1);

        server.broadcastPriceUpdate(price);
        io_context.poll();

        EXPECT_TRUE(server._sessions.empty());
    }

    TEST(ServerHelperTest, ShouldDeliverPricesReturnsTrueByDefault) {
        boost::asio::io_context io_context;
        auto websocket =
                std::make_shared<boost::beast::websocket::stream<Server::tcp::socket> >(io_context);

        Server server(std::make_shared<MarketDataStore>(), makeOrderBookManager());
        const auto client_session = std::make_shared<Server::ClientSession>(websocket, server);

        const market::MarkPrice price("AAPL", 189.45, 189.50, 189.47, 900);
        EXPECT_TRUE(Server::should_deliver_prices(client_session, price));
    }

    TEST(ServerHelperTest, ShouldDeliverPricesFiltersExplicitSubscriptions) {
        boost::asio::io_context io_context;
        auto websocket =
                std::make_shared<boost::beast::websocket::stream<Server::tcp::socket> >(io_context);

        Server server(std::make_shared<MarketDataStore>(), makeOrderBookManager());
        const auto client_session = std::make_shared<Server::ClientSession>(websocket, server);

        client_session->receive_all_price_updates = false;
        client_session->_subscribed_symbols.insert("AAPL");

        const market::MarkPrice price("AAPL", 189.45, 189.50, 189.47, 900);
        const market::MarkPrice price2("MSFT", 189.45, 189.50, 189.47, 900);

        EXPECT_TRUE(Server::should_deliver_prices(client_session, price));
        EXPECT_FALSE(Server::should_deliver_prices(client_session, price2));
    }

    TEST(ServerHelperTest, SubscribeToSymbolSwitchesClientToExplicitSymbolFiltering) {
        boost::asio::io_context io_context;
        auto websocket =
                std::make_shared<boost::beast::websocket::stream<Server::tcp::socket> >(io_context);
        Server server(std::make_shared<MarketDataStore>(), makeOrderBookManager());
        const auto client_session = std::make_shared<Server::ClientSession>(websocket, server);

        Server::subscribe_to_symbol(client_session, "AAPL");

        EXPECT_FALSE(client_session->receive_all_price_updates);
        EXPECT_TRUE(client_session->_subscribed_symbols.contains("AAPL"));
    }

    TEST(ServerHelperTest, UnsubscribeFromSymbolRemovesSymbolFromExplicitFiltering) {
        boost::asio::io_context io_context;
        auto websocket =
                std::make_shared<boost::beast::websocket::stream<Server::tcp::socket> >(io_context);
        Server server(std::make_shared<MarketDataStore>(), makeOrderBookManager());
        const auto client_session = std::make_shared<Server::ClientSession>(websocket, server);
        const market::MarkPrice price("AAPL", 189.45, 189.50, 189.47, 900);

        Server::subscribe_to_symbol(client_session, "AAPL");
        ASSERT_TRUE(Server::should_deliver_prices(client_session, price));

        Server::unsubscribe_from_symbol(client_session, "AAPL");

        EXPECT_FALSE(client_session->receive_all_price_updates);
        EXPECT_FALSE(client_session->_subscribed_symbols.contains("AAPL"));
        EXPECT_FALSE(Server::should_deliver_prices(client_session, price));
    }

    TEST(ServerWebSocketIntegrationTest, ClientCanConnectAndReceiveCommandResponses) {
        WebSocketIntegrationHarness harness;
        auto &client = harness.connectClient();

        harness.writeMessage(client, "subscribe:AAPL");
        EXPECT_EQ(harness.readMessage(client), R"({"type":"subscribed","symbol":"AAPL"})");

        harness.writeMessage(client, "unsubscribe:AAPL");
        EXPECT_EQ(harness.readMessage(client), R"({"type":"unsubscribed","symbol":"AAPL"})");

        harness.writeMessage(client, "unknown-command");
        EXPECT_EQ(harness.readMessage(client), R"({"error":"unknown_message"})");
    }

    TEST(ServerWebSocketIntegrationTest, DefaultClientReceivesPriceUpdates) {
        WebSocketIntegrationHarness harness;
        auto &client = harness.connectClient();

        harness.getServer().broadcastPriceUpdate(market::MarkPrice("AAPL", 189.45, 189.50, 189.47, 900));

        const auto message = harness.readMessage(client);
        EXPECT_NE(message.find(R"("type":"price_update")"), std::string::npos);
        EXPECT_NE(message.find(R"("symbol":"AAPL")"), std::string::npos);
    }

    TEST(ServerWebSocketIntegrationTest, SubscribedClientReceivesMatchingPriceUpdates) {
        WebSocketIntegrationHarness harness;
        auto &client = harness.connectClient();

        harness.writeMessage(client, "subscribe:AAPL");
        ASSERT_EQ(harness.readMessage(client), R"({"type":"subscribed","symbol":"AAPL"})");

        harness.getServer().broadcastPriceUpdate(market::MarkPrice("AAPL", 189.45, 189.50, 189.47, 900));

        const auto message = harness.readMessage(client);
        EXPECT_NE(message.find(R"("type":"price_update")"), std::string::npos);
        EXPECT_NE(message.find(R"("symbol":"AAPL")"), std::string::npos);
    }

    TEST(ServerWebSocketIntegrationTest, SubscribedClientDoesNotReceiveNonMatchingPriceUpdates) {
        WebSocketIntegrationHarness harness;
        auto &client = harness.connectClient();

        harness.writeMessage(client, "subscribe:AAPL");
        ASSERT_EQ(harness.readMessage(client), R"({"type":"subscribed","symbol":"AAPL"})");

        harness.getServer().broadcastPriceUpdate(market::MarkPrice("MSFT", 410.10, 410.20, 410.15, 1200));

        EXPECT_FALSE(harness.hasPendingBytes(client));
    }

    TEST(ServerWebSocketIntegrationTest, UnsubscribedClientDoesNotReceiveFilteredPriceUpdates) {
        WebSocketIntegrationHarness harness;
        auto &client = harness.connectClient();

        harness.writeMessage(client, "subscribe:AAPL");
        ASSERT_EQ(harness.readMessage(client), R"({"type":"subscribed","symbol":"AAPL"})");
        harness.writeMessage(client, "unsubscribe:AAPL");
        ASSERT_EQ(harness.readMessage(client), R"({"type":"unsubscribed","symbol":"AAPL"})");

        harness.getServer().broadcastPriceUpdate(market::MarkPrice("AAPL", 189.45, 189.50, 189.47, 900));

        EXPECT_FALSE(harness.hasPendingBytes(client));
    }

    TEST(ServerWebSocketIntegrationTest, SubscriptionChangesAndBroadcastsCanRunConcurrently) {
        WebSocketIntegrationHarness harness;
        auto &client = harness.connectClient();
        std::atomic<bool> start_broadcasting{false};

        std::thread broadcaster([&]() {
            while (!start_broadcasting.load()) {
                std::this_thread::yield();
            }
            for (int update = 0; update < 100; ++update) {
                harness.getServer().broadcastPriceUpdate(
                    market::MarkPrice(update % 2 == 0 ? "AAPL" : "MSFT", 100.0, 100.1, 100.0, 10));
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });

        start_broadcasting = true;
        for (int command = 0; command < 20; ++command) {
            const bool subscribing = command % 2 == 0;
            harness.writeMessage(client, subscribing ? "subscribe:AAPL" : "unsubscribe:AAPL");
            const std::string expected_type = subscribing ? R"("type":"subscribed")"
                                                          : R"("type":"unsubscribed")";

            for (;;) {
                const auto message = harness.readMessage(client);
                if (message.find(expected_type) != std::string::npos) {
                    break;
                }
            }
        }

        broadcaster.join();
    }

    TEST(ServerLifecycleTest, StopTokenEndsServerRunLoop) {
        Server server(std::make_shared<MarketDataStore>(), makeOrderBookManager(), 0);
        std::jthread server_thread([&](const std::stop_token stop_token) { server.run(stop_token); });

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        server_thread.request_stop();
    }
} // namespace
