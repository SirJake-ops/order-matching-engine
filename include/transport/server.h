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
        void run() const;

    private:
        using tcp = boost::asio::ip::tcp;
        using Request = boost::beast::http::request<boost::beast::http::string_body>;
        using Response = boost::beast::http::response<boost::beast::http::string_body>;

        [[nodiscard]] Response handleRequest(const Request &request) const;
        [[nodiscard]] std::string buildPriceResponse(const market::MarkPrice &price) const;
        [[nodiscard]] static std::string escapeJson(const std::string &value);
        [[nodiscard]] static std::string decodeUrlComponent(const std::string &value);

        std::shared_ptr<MarketDataStore> _market_data_store;
        unsigned short _port;
    };
}

#endif //TRADINGEXCHANGE_SERVER_H
