//
// Created by jake on 2/24/26.
//

#include "transport/server.h"

#include <cctype>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace http = boost::beast::http;
using tcp = boost::asio::ip::tcp;

namespace server {
    Server::Server(std::shared_ptr<MarketDataStore> market_data_store, const unsigned short port)
        : _market_data_store(std::move(market_data_store)), _port(port) {
    }

    void Server::run() const {
        try {
            boost::asio::io_context io_context(1);
            tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), _port));

            for (;;) {
                tcp::socket socket(io_context);
                acceptor.accept(socket);

                boost::beast::flat_buffer buffer;
                Request request;
                http::read(socket, buffer, request);

                const auto response = handleRequest(request);
                http::write(socket, response);

                boost::system::error_code error_code;
                socket.shutdown(tcp::socket::shutdown_send, error_code);
            }
        } catch (const std::exception &exception) {
            std::cerr << "Server exception: " << exception.what() << std::endl;
        }
    }

    Server::Response Server::handleRequest(const Request &request) const {
        Response response;
        response.version(request.version());
        response.keep_alive(false);
        response.set(http::field::content_type, "application/json");

        if (request.method() != http::verb::get) {
            response.result(http::status::method_not_allowed);
            response.body() = R"({"error":"method_not_allowed"})";
            response.prepare_payload();
            return response;
        }

        const std::string target = std::string(request.target());
        constexpr std::string_view symbol_path_prefix = "/api/market/prices/";
        if (target.rfind(symbol_path_prefix.data(), 0) == 0 && target.size() > symbol_path_prefix.size()) {
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

        response.result(http::status::not_found);
        response.body() = R"({"error":"not_found"})";
        response.prepare_payload();
        return response;
    }

    std::string Server::buildPriceResponse(const market::MarkPrice &price) const {
        return "{\"symbol\":\"" + escapeJson(price.getSymbol()) +
               "\",\"bid\":" + std::to_string(price.getBid()) +
               ",\"ask\":" + std::to_string(price.getAsk()) +
               ",\"last\":" + std::to_string(price.getLast()) +
               ",\"volume\":" + std::to_string(price.getVolume()) +
               ",\"timestamp\":" + std::to_string(price.getTimestamp()) +
               "}";
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
            if (value[index] == '%' && index + 2 < value.size() &&
                std::isxdigit(static_cast<unsigned char>(value[index + 1])) &&
                std::isxdigit(static_cast<unsigned char>(value[index + 2]))) {
                const std::string hex = value.substr(index + 1, 2);
                decoded.push_back(static_cast<char>(std::stoi(hex, nullptr, 16)));
                index += 2;
                continue;
            }

            decoded.push_back(value[index] == '+' ? ' ' : value[index]);
        }

        return decoded;
    }
}
