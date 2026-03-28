//
// Created by jake on 2/24/26.
//

#ifndef TRADINGEXCHANGE_SERVER_H
#define TRADINGEXCHANGE_SERVER_H

#include <memory>
#include <string>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>

#include "transport/MarketDataStore.h"

namespace server {
    class Server {
    public:
        explicit Server(std::shared_ptr<MarketDataStore> market_data_store, unsigned short port = 8080);

        void run();

        void handleSession(boost::asio::ip::tcp::socket socket);

        void handleWebSocketSession(boost::beast::websocket::stream<boost::asio::ip::tcp::socket> &ws);

        void broadcastPriceUpdate(const market::MarkPrice &price);

    private:
        enum class WebSocketMessageType {
            Subscribe,
            Unsubscribe,
            Ping,
            Unknown
        };

        struct WebSocketMessage {
            WebSocketMessageType type{WebSocketMessageType::Unknown};
            std::string symbol;
        };

        using tcp = boost::asio::ip::tcp;
        using Request = boost::beast::http::request<boost::beast::http::string_body>;
        using Response = boost::beast::http::response<boost::beast::http::string_body>;
        using WebsocketSession = std::shared_ptr<boost::beast::websocket::stream<tcp::socket> >;

        [[nodiscard]] Response handleRequest(const Request &request) const;

        [[nodiscard]] std::string buildPriceResponse(const market::MarkPrice &price) const;

        [[nodiscard]] static std::string escapeJson(const std::string &value);

        [[nodiscard]] static std::string decodeUrlComponent(const std::string &value);

        [[nodiscard]] static bool isWebSocketUpgrade(const Request &request);

        [[nodiscard]] static std::string buildPriceUpdateMessage(const market::MarkPrice &price);

        void registerSession(const WebsocketSession &session);

        void unregisterSession(const WebsocketSession &session);

        WebSocketMessage parseWebSocketMessage(const boost::beast::flat_buffer &buffer);

        std::shared_ptr<MarketDataStore> _market_data_store;

        mutable std::mutex _sessions_mutex;
        std::vector<WebsocketSession> _sessions;
        unsigned short _port;
    };
}

#endif //TRADINGEXCHANGE_SERVER_H
