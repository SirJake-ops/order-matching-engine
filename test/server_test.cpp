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

    TEST(ServerHelperTest, ParseWebSocketMessageRecognizesPingAndSubscriptionCommands) {
        Server server(std::make_shared<MarketDataStore>());

        boost::beast::flat_buffer ping_buffer;
        const std::string ping = "ping";
        boost::asio::buffer_copy(ping_buffer.prepare(ping.size()), boost::asio::buffer(ping));
        ping_buffer.commit(ping.size());

        const auto ping_message = server.parseWebSocketMessage(ping_buffer);
        EXPECT_EQ(ping_message.type, Server::WebSocketMessageType::Ping);
        EXPECT_TRUE(ping_message.symbol.empty());

        boost::beast::flat_buffer subscribe_buffer;
        const std::string subscribe = "subscribe:ETH/USD";
        boost::asio::buffer_copy(subscribe_buffer.prepare(subscribe.size()), boost::asio::buffer(subscribe));
        subscribe_buffer.commit(subscribe.size());

        const auto subscribe_message = server.parseWebSocketMessage(subscribe_buffer);
        EXPECT_EQ(subscribe_message.type, Server::WebSocketMessageType::Subscribe);
        EXPECT_EQ(subscribe_message.symbol, "ETH/USD");

        boost::beast::flat_buffer unknown_buffer;
        const std::string unknown = "hello";
        boost::asio::buffer_copy(unknown_buffer.prepare(unknown.size()), boost::asio::buffer(unknown));
        unknown_buffer.commit(unknown.size());

        const auto unknown_message = server.parseWebSocketMessage(unknown_buffer);
        EXPECT_EQ(unknown_message.type, Server::WebSocketMessageType::Unknown);
    }

    TEST(ServerHelperTest, BuildPriceUpdateMessageUsesCurrentWebSocketEnvelope) {
        const market::MarkPrice price("AAPL", 189.45, 189.50, 189.47, 900);

        const auto message = Server::buildPriceUpdateMessage(price);

        EXPECT_EQ(message, "{\"type\":\"price_update\",\"price\":189.450000}");
    }

    TEST(ServerHelperTest, UrlHelpersDecodeEncodedSymbolsAndEscapeJsonQuotes) {
        EXPECT_EQ(Server::decodeUrlComponent("BTC%2FUSD+SPOT"), "BTC/USD SPOT");
        EXPECT_EQ(Server::escapeJson("A\"B\\C"), "A\\\"B\\\\C");
    }

    TEST(ServerHelperTest, RegisterAndUnregisterSessionManageSessionCollection) {
        boost::asio::io_context io_context;
        Server server(std::make_shared<MarketDataStore>());
        auto session = std::make_shared<boost::beast::websocket::stream<Server::tcp::socket>>(io_context);

        server.registerSession(session);
        EXPECT_EQ(server._sessions.size(), 1);

        server.unregisterSession(session);
        EXPECT_TRUE(server._sessions.empty());
    }
}
