//
// Created by Jacob Pagan on 2/24/26.
//

#include "transport/server.h"

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>

#include <chrono>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <utility>
#include <algorithm>
#include <string_view>

namespace http = boost::beast::http;
using tcp = boost::asio::ip::tcp;

namespace server {
    Server::ClientSession::ClientSession(WebsocketSession socket, Server &server)
        : _socket(std::move(socket)),
          _server(server),
          _strand(boost::asio::make_strand(
              static_cast<boost::asio::io_context &>(_socket->get_executor().context()))) {
    }

    void Server::ClientSession::start() {
        const auto self = shared_from_this();
        boost::asio::dispatch(_strand, [self]() { self->readNext(); });
    }

    void Server::ClientSession::send(std::string message) {
        const auto self = shared_from_this();
        boost::asio::post(_strand, [self, message = std::move(message)]() mutable {
            self->queueMessage(std::move(message));
        });
    }

    void Server::ClientSession::shutdown() {
        const auto self = shared_from_this();
        boost::asio::post(_strand, [self]() { self->stop(); });
    }

    void Server::ClientSession::readNext() {
        if (_stopped) {
            return;
        }

        _read_buffer.consume(_read_buffer.size());
        const auto self = shared_from_this();
        _socket->async_read(
            _read_buffer,
            boost::asio::bind_executor(_strand, [self](const boost::system::error_code &error_code,
                                                       const std::size_t) {
                self->handleRead(error_code);
            }));
    }

    void Server::ClientSession::handleRead(const boost::system::error_code &error_code) {
        if (error_code) {
            stop();
            return;
        }

        const auto parsed_message = _server.parseWebSocketMessage(_read_buffer);
        switch (parsed_message.type) {
            case WebSocketMessageType::Subscribe:
                Server::subscribe_to_symbol(shared_from_this(), parsed_message.symbol);
                queueMessage(std::string(R"({"type":"subscribed","symbol":")")
                             + Server::escapeJson(parsed_message.symbol) + R"("})");
                break;
            case WebSocketMessageType::Unsubscribe:
                Server::unsubscribe_from_symbol(shared_from_this(), parsed_message.symbol);
                queueMessage(std::string(R"({"type":"unsubscribed","symbol":")")
                             + Server::escapeJson(parsed_message.symbol) + R"("})");
                break;
            case WebSocketMessageType::Ping:
                queueMessage(R"({"type":"pong"})");
                break;
            case WebSocketMessageType::Unknown:
                queueMessage(R"({"error":"unknown_message"})");
                break;
        }

        readNext();
    }

    void Server::ClientSession::queueMessage(std::string message) {
        if (_stopped) {
            return;
        }

        _pending_writes.push_back(std::move(message));
        if (_pending_writes.size() == 1) {
            writeNext();
        }
    }

    void Server::ClientSession::writeNext() {
        const auto self = shared_from_this();
        _socket->async_write(
            boost::asio::buffer(_pending_writes.front()),
            boost::asio::bind_executor(_strand, [self](const boost::system::error_code &error_code,
                                                       const std::size_t) {
                self->handleWrite(error_code);
            }));
    }

    void Server::ClientSession::handleWrite(const boost::system::error_code &error_code) {
        if (error_code) {
            stop();
            return;
        }

        _pending_writes.pop_front();
        if (!_pending_writes.empty()) {
            writeNext();
        }
    }

    void Server::ClientSession::stop() {
        if (_stopped) {
            return;
        }

        _stopped = true;

        boost::system::error_code ignored_error;
        auto &socket = boost::beast::get_lowest_layer(*_socket);
        static_cast<void>(socket.cancel(ignored_error));
        static_cast<void>(socket.close(ignored_error));
        _server.unregisterSession(shared_from_this());
    }

    Server::Server(std::shared_ptr<MarketDataStore> market_data_store,
                   std::shared_ptr<orderbook_manager::OrderBookManager> order_book_manager, const unsigned short port)
        : _market_data_store(std::move(market_data_store)),
          _order_book_manager(std::move(order_book_manager)),
          _port(port) {
    }

    void Server::run(const std::stop_token &stop_token) {
        try {
            boost::asio::io_context io_context(1);
            tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), _port));
            acceptor.non_blocking(true);
            auto work_guard = boost::asio::make_work_guard(io_context);
            std::jthread websocket_worker([&io_context]() { io_context.run(); });

            while (!stop_token.stop_requested()) {
                tcp::socket socket(io_context);
                boost::system::error_code error_code;
                static_cast<void>(acceptor.accept(socket, error_code));

                if (error_code == boost::asio::error::would_block
                    || error_code == boost::asio::error::try_again) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    continue;
                }
                if (error_code) {
                    std::cerr << "Request handling error: " << error_code.message() << std::endl;
                    continue;
                }

                try {
                    handleSession(std::move(socket));
                } catch (const std::exception &exception) {
                    std::cerr << "Request handling error: " << exception.what() << std::endl;
                }
            }

            std::vector<ClientSessionPtr> sessions;
            {
                std::lock_guard lock(_sessions_mutex);
                sessions = _sessions;
            }
            for (const auto &session: sessions) {
                session->shutdown();
            }

            work_guard.reset();
            websocket_worker.join();
        } catch (const std::exception &e) {
            std::cerr << "Server startup error: " << e.what() << std::endl;
        }
    }

    void Server::handleSession(boost::asio::ip::tcp::socket socket) {
        boost::beast::flat_buffer buffer;
        Request request;

        try {
            http::read(socket, buffer, request);
        } catch (const boost::system::system_error &exception) {
            const auto code = exception.code();
            if (code == boost::beast::http::error::end_of_stream || code == boost::asio::error::eof) {
                return;
            }

            throw;
        }
        if (isWebSocketUpgrade(request)) {
            const auto session =
                    std::make_shared<boost::beast::websocket::stream<tcp::socket> >(std::move(socket));
            session->accept(request);
            const auto client_session = std::make_shared<ClientSession>(session, *this);
            registerSession(client_session);
            client_session->start();
            return;
        }

        const auto response = handleRequest(request);
        http::write(socket, response);

        boost::system::error_code error_code;
        static_cast<void>(socket.shutdown(tcp::socket::shutdown_send, error_code));
    }

    void Server::broadcastPriceUpdate(const market::MarkPrice &price) {
        const auto message = buildPriceUpdateMessage(price);
        std::vector<ClientSessionPtr> sessions;
        {
            std::lock_guard lock(_sessions_mutex);
            sessions = _sessions;
        }
        for (const auto &session: sessions) {
            if (!should_deliver_prices(session, price)) {
                continue;
            }

            session->send(message);
        }
    }


    Server::Response Server::handleRequest(const Request &request) const {
        Response response;
        response.version(request.version());
        response.keep_alive(false);
        response.set(http::field::content_type, "application/json");

        if (request.method() == http::verb::get) {
            const std::string target = std::string(request.target());
            constexpr std::string_view symbol_path_prefix = "/api/market/prices/";
            if (target.starts_with(symbol_path_prefix)
                && target.size() > symbol_path_prefix.size()) {
                const std::string symbol = decodeUrlComponent(target.substr(symbol_path_prefix.size()));
                const auto price = _market_data_store->getPriceForSymbol(symbol);
                if (!price.has_value()) {
                    response.result(http::status::not_found);
                    response.body() = R"({"error":"symbol_not_found"})";
                    response.prepare_payload();
                    return response;
                }

                response.result(http::status::ok);
                response.body() = buildPriceResponse(price.value());
                response.prepare_payload();
                return response;
            }

            constexpr std::string_view orderbook_path_prefix = "/api/market/orderbook/";
            if (target.starts_with(orderbook_path_prefix) && target.size() > orderbook_path_prefix.size()) {
                const std::string symbol = decodeUrlComponent(target.substr(orderbook_path_prefix.size()));
                try {
                    const auto book = _order_book_manager->get_orderbook(symbol);
                    response.result(http::status::ok);
                    response.body() = buildOrderBookResponse(symbol, book, 5);
                    response.prepare_payload();
                    return response;
                } catch (const std::runtime_error &) {
                    response.result(http::status::not_found);
                    response.body() = R"({"error":"symbol_not_found"})";
                    response.prepare_payload();
                    return response;
                }
            }
            response.result(http::status::not_found);
            response.body() = R"({"error":"not_found"})";
            response.prepare_payload();
            return response;
        }

        response.result(http::status::method_not_allowed);
        response.body() = R"({"error":"method_not_allowed"})";
        response.prepare_payload();
        return response;
    }

    std::string Server::buildPriceResponse(const market::MarkPrice &price) const {
        std::ostringstream oss;

        oss << std::fixed << std::setprecision(2);
        oss << R"({"symbol":")" << escapeJson(price.getSymbol()) << R"(",)"
                << R"("bid":)" << price.getBid() << ","
                << R"("ask":)" << price.getAsk() << ","
                << R"("last":)" << price.getLast() << ","
                << R"("volume":)" << price.getVolume() << ","
                << R"("timestamp":)" << price.getTimestamp() << "}";

        return oss.str();
    }

    std::string Server::escapeJson(const std::string &value) {
        std::string escaped;
        escaped.reserve(value.size());

        for (const char character: value) {
            if (character == '"' || character == '\\') {
                escaped.push_back('\\');
            }

            escaped.push_back(character);
        }

        return escaped;
    }

    std::string Server::decodeUrlComponent(const std::string &value) {
        std::string decoded;
        decoded.reserve(value.size());

        for (std::size_t index = 0; index < value.size(); ++index) {
            if (value[index] == '%' && index + 2 < value.size()
                && std::isxdigit(static_cast<unsigned char>(value[index + 1]))
                && std::isxdigit(static_cast<unsigned char>(value[index + 2]))) {
                const std::string hex = value.substr(index + 1, 2);
                decoded.push_back(static_cast<char>(std::stoi(hex, nullptr, 16)));
                index += 2;
                continue;
            }

            decoded.push_back(value[index] == '+' ? ' ' : value[index]);
        }

        return decoded;
    }

    bool Server::isWebSocketUpgrade(const Request &request) {
        return boost::beast::websocket::is_upgrade(request);
    }

    std::string Server::buildPriceUpdateMessage(const market::MarkPrice &price) {
        std::ostringstream oss;

        oss << std::fixed << std::setprecision(2);
        oss << R"({"type":"price_update","data":{)"
                << R"("symbol":")" << escapeJson(price.getSymbol()) << R"(",)"
                << R"("bid":)" << price.getBid() << ","
                << R"("ask":)" << price.getAsk() << ","
                << R"("last":)" << price.getLast() << ","
                << R"("volume":)" << price.getVolume() << ","
                << R"("timestamp":)" << price.getTimestamp() << "}}";

        return oss.str();
    }

    std::string Server::buildOrderBookResponse(const std::string &symbol, const orderbook::OrderBook &order_book,
                                               const std::size_t levels) {
        std::ostringstream oss;
        const auto &bids = order_book.bid_depth(levels);
        const auto &asks = order_book.ask_depths(levels);

        oss << R"({"symbol":")" << escapeJson(symbol) << R"(",)"
                << R"("bids":[)";
        for (std::size_t i{}; i < bids.size(); ++i) {
            if (i > 0) oss << ",";
            oss << R"({"price":)" << bids[i].first
                    << R"(,"quantity":)" << bids[i].second << "}";
        }

        oss << R"(],"asks":[)";

        for (std::size_t i{}; i < asks.size(); ++i) {
            if (i > 0) oss << ",";
            oss << R"({"price":)" << asks[i].first
                    << R"(,"quantity":)" << asks[i].second << "}";
        }

        oss << "]}";
        return oss.str();
    }


    void Server::registerSession(const ClientSessionPtr &session) {
        std::lock_guard lock(_sessions_mutex);
        _sessions.push_back(session);
    }

    void Server::unregisterSession(const ClientSessionPtr &session) {
        std::lock_guard lock(_sessions_mutex);
        _sessions.erase(std::ranges::remove(_sessions, session).begin(), _sessions.end());
    }

    void Server::subscribe_to_symbol(const ClientSessionPtr &client_session,
                                     const std::string &symbol) {
        std::lock_guard lock(client_session->_subscription_mutex);
        client_session->receive_all_price_updates = false;
        client_session->_subscribed_symbols.insert(symbol);
    }

    void Server::unsubscribe_from_symbol(const ClientSessionPtr &client_session,
                                         const std::string &symbol) {
        std::lock_guard lock(client_session->_subscription_mutex);
        client_session->receive_all_price_updates = false;
        client_session->_subscribed_symbols.erase(symbol);
    }

    bool Server::should_deliver_prices(const ClientSessionPtr &client_session,
                                       const market::MarkPrice &price) {
        std::lock_guard lock(client_session->_subscription_mutex);
        if (client_session->receive_all_price_updates) {
            return true;
        }
        return client_session->_subscribed_symbols.find(price.getSymbol())
               != client_session->_subscribed_symbols.end();
    }

    Server::WebSocketMessage Server::parseWebSocketMessage(const boost::beast::flat_buffer &buffer) {
        const std::string message = boost::beast::buffers_to_string(buffer.data());

        if (message == "ping") {
            return WebSocketMessage{WebSocketMessageType::Ping};
        }

        constexpr std::string_view subscribed = "subscribe:";
        if (message.starts_with(subscribed) && message.size() > subscribed.size()) {
            return {WebSocketMessageType::Subscribe, message.substr(subscribed.size())};
        }

        constexpr std::string_view unsubscribed = "unsubscribe:";
        if (message.starts_with(unsubscribed) && message.size() > unsubscribed.size()) {
            return {WebSocketMessageType::Unsubscribe, message.substr(unsubscribed.size())};
        }

        return {WebSocketMessageType::Unknown};
    }
} // namespace server
