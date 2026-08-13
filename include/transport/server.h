//
// Created by jake on 2/24/26.
//

#ifndef TRADINGEXCHANGE_SERVER_H
#define TRADINGEXCHANGE_SERVER_H

#include <deque>
#include <memory>
#include <mutex>
#include <set>
#include <stop_token>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>

#include "orderbook/OrderBookManager.h"
#include "transport/MarketDataStore.h"

namespace server {
    class Server {
    public:
        explicit Server(std::shared_ptr<MarketDataStore> market_data_store,
                        std::shared_ptr<orderbook_manager::OrderBookManager> order_book_manager,
                        unsigned short port = 8080);

        void run(const std::stop_token &stop_token = {});

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
        using WebsocketSession = std::shared_ptr<boost::beast::websocket::stream<tcp::socket> >;

        struct ClientSession : std::enable_shared_from_this<ClientSession> {
            ClientSession(WebsocketSession socket, Server &server);

            void start();
            void send(std::string message);
            void shutdown();

        private:
            friend class Server;

            void readNext();
            void handleRead(const boost::system::error_code &error_code);
            void queueMessage(std::string message);
            void writeNext();
            void handleWrite(const boost::system::error_code &error_code);
            void stop();

            WebsocketSession _socket;
            Server &_server;
            boost::asio::strand<boost::asio::io_context::executor_type> _strand;
            boost::beast::flat_buffer _read_buffer;
            std::deque<std::string> _pending_writes;
            bool _stopped{false};

            mutable std::mutex _subscription_mutex;
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

        [[nodiscard]] static std::string buildOrderBookResponse(const std::string &symbol,
                                                                const orderbook::OrderBook &order_book,
                                                                std::size_t levels);

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
        std::shared_ptr<orderbook_manager::OrderBookManager> _order_book_manager;

        mutable std::mutex _sessions_mutex;
        std::vector<ClientSessionPtr> _sessions;
        unsigned short _port;
    };
} // namespace server

#endif // TRADINGEXCHANGE_SERVER_H
