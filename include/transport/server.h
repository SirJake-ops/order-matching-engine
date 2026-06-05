//
// Created by jake on 2/24/26.
//

#ifndef TRADINGEXCHANGE_SERVER_H
#define TRADINGEXCHANGE_SERVER_H

#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

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

    void broadcastPriceUpdate(const market::MarkPrice &price);

  private:
    enum class WebSocketMessageType { Subscribe, Unsubscribe, Ping, Unknown };

    struct WebSocketMessage {
        WebSocketMessageType type{WebSocketMessageType::Unknown};
        std::string symbol;
    };

    using tcp = boost::asio::ip::tcp;
    using Request = boost::beast::http::request<boost::beast::http::string_body>;
    using Response = boost::beast::http::response<boost::beast::http::string_body>;
    using WebsocketSession = std::shared_ptr<boost::beast::websocket::stream<tcp::socket>>;

    struct ClientSession {
        explicit ClientSession(WebsocketSession socket) : _socket(std::move(socket)) {}

        WebsocketSession _socket;
        std::set<std::string> _subscribed_symbols;
        bool receive_all_price_updates{true};
    };

    using ClientSessionPtr = std::shared_ptr<ClientSession>;

    [[nodiscard]] Response handleRequest(const Request &request) const;

    [[nodiscard]] std::string buildPriceResponse(const market::MarkPrice &price) const;

    [[nodiscard]] static std::string escapeJson(const std::string &value);

    [[nodiscard]] static std::string decodeUrlComponent(const std::string &value);

    [[nodiscard]] static bool isWebSocketUpgrade(const Request &request);

    [[nodiscard]] static std::string buildPriceUpdateMessage(const market::MarkPrice &price);

    void handleWebSocketSession(const ClientSessionPtr &client_session);
    void registerSession(const ClientSessionPtr &client_session);

    void unregisterSession(const ClientSessionPtr &client_session);

    static void subscribe_to_symbol(const ClientSessionPtr &client_session,
                                    const std::string &symbol);

    static void unsubscribe_from_symbol(const ClientSessionPtr &client_session,
                                        const std::string &symbol);

    [[nodiscard]] static bool should_deliver_prices(const ClientSessionPtr &client_session,
                                                    const market::MarkPrice &price);

    WebSocketMessage parseWebSocketMessage(const boost::beast::flat_buffer &buffer);

    std::shared_ptr<MarketDataStore> _market_data_store;

    mutable std::mutex _sessions_mutex;
    mutable std::mutex _web_socket_write_mutex;
    std::vector<ClientSessionPtr> _sessions;
    unsigned short _port;
};
} // namespace server

#endif // TRADINGEXCHANGE_SERVER_H
