#include <string>

#include <boost/asio/buffer.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>

#include "gtest/gtest.h"

#define private public
#include "transport/server.h"
#undef private

namespace {
using server::MarketDataStore;
using server::Server;

boost::beast::flat_buffer makeBuffer(const std::string &message) {
    boost::beast::flat_buffer buffer;
    boost::asio::buffer_copy(buffer.prepare(message.size()), boost::asio::buffer(message));
    buffer.commit(message.size());
    return buffer;
}

TEST(ServerHelperTest, HandleRequestReturnsPriceSnapshotForKnownSymbol) {
    auto store = std::make_shared<MarketDataStore>();
    const market::MarkPrice price("BTC/USD", 62500.10, 62500.50, 62500.20, 4200);
    store->updatePrice(price);

    Server server(store);
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
    Server server(store);
    Server::Request request{boost::beast::http::verb::get, "/api/market/prices/MSFT", 11};

    const auto response = server.handleRequest(request);

    EXPECT_EQ(response.result(), boost::beast::http::status::not_found);
    EXPECT_EQ(response.body(), R"({"error":"symbol_not_found"})");
}

TEST(ServerHelperTest, HandleRequestRejectsUnsupportedMethodAndUnknownRoute) {
    auto store = std::make_shared<MarketDataStore>();
    Server server(store);

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
    Server server(std::make_shared<MarketDataStore>());

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
    Server server(std::make_shared<MarketDataStore>());
    auto session =
        std::make_shared<boost::beast::websocket::stream<Server::tcp::socket>>(io_context);
    const auto client_session = std::make_shared<Server::ClientSession>(session);

    server.registerSession(client_session);
    EXPECT_EQ(server._sessions.size(), 1);

    server.unregisterSession(client_session);
    EXPECT_TRUE(server._sessions.empty());
}

TEST(ServerHelperTest, BroadcastWithNoSessionsDoesNothing) {
    Server server(std::make_shared<MarketDataStore>());
    const market::MarkPrice price("AAPL", 189.45, 189.50, 189.47, 900);

    EXPECT_NO_THROW(server.broadcastPriceUpdate(price));
    EXPECT_TRUE(server._sessions.empty());
}

TEST(ServerHelperTest, BroadcastRemovesSessionWhenWriteFails) {
    boost::asio::io_context io_context;
    Server server(std::make_shared<MarketDataStore>());
    auto session =
        std::make_shared<boost::beast::websocket::stream<Server::tcp::socket>>(io_context);
    const auto client_session = std::make_shared<Server::ClientSession>(session);
    const market::MarkPrice price("AAPL", 189.45, 189.50, 189.47, 900);

    server.registerSession(client_session);
    ASSERT_EQ(server._sessions.size(), 1);

    server.broadcastPriceUpdate(price);

    EXPECT_TRUE(server._sessions.empty());
}

TEST(ServerHelperTest, ShouldDeliverPricesReturnsTrueByDefault) {
    boost::asio::io_context io_context;
    auto websocket =
        std::make_shared<boost::beast::websocket::stream<Server::tcp::socket>>(io_context);

    const auto client_session = std::make_shared<Server::ClientSession>(websocket);

    const market::MarkPrice price("AAPL", 189.45, 189.50, 189.47, 900);
    EXPECT_TRUE(Server::should_deliver_prices(client_session, price));
}

TEST(ServerHelperTest, ShouldDeliverPricesFiltersExplicitSubscriptions) {
    boost::asio::io_context io_context;
    auto websocket =
        std::make_shared<boost::beast::websocket::stream<Server::tcp::socket>>(io_context);

    const auto client_session = std::make_shared<Server::ClientSession>(websocket);

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
        std::make_shared<boost::beast::websocket::stream<Server::tcp::socket>>(io_context);
    const auto client_session = std::make_shared<Server::ClientSession>(websocket);

    Server::subscribe_to_symbol(client_session, "AAPL");

    EXPECT_FALSE(client_session->receive_all_price_updates);
    EXPECT_TRUE(client_session->_subscribed_symbols.contains("AAPL"));
}

TEST(ServerHelperTest, UnsubscribeFromSymbolRemovesSymbolFromExplicitFiltering) {
    boost::asio::io_context io_context;
    auto websocket =
        std::make_shared<boost::beast::websocket::stream<Server::tcp::socket>>(io_context);
    const auto client_session = std::make_shared<Server::ClientSession>(websocket);
    const market::MarkPrice price("AAPL", 189.45, 189.50, 189.47, 900);

    Server::subscribe_to_symbol(client_session, "AAPL");
    ASSERT_TRUE(Server::should_deliver_prices(client_session, price));

    Server::unsubscribe_from_symbol(client_session, "AAPL");

    EXPECT_FALSE(client_session->receive_all_price_updates);
    EXPECT_FALSE(client_session->_subscribed_symbols.contains("AAPL"));
    EXPECT_FALSE(Server::should_deliver_prices(client_session, price));
}
} // namespace
