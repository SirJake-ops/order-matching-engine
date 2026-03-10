//
// Created by jake on 2/24/26.
//

#ifndef TRADINGEXCHANGE_SERVER_H
#define TRADINGEXCHANGE_SERVER_H

#include <iostream>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::use_awaitable_t;
using tcp_acceptor = use_awaitable_t<>::as_default_on_t<tcp::acceptor>;
using tcp_socket = use_awaitable_t<>::as_default_on_t<tcp::socket>;


namespace this_coro = boost::asio::this_coro;

namespace server {
    class Server {
    public:
        explicit Server(const int &number_of_threads) {
            try {
                boost::asio::io_context io_context(number_of_threads);
                boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
                co_spawn(io_context, listener(), detached);
                io_context.run();
            } catch (std::exception &e) {
                std::cerr << "Server exception: " << e.what() << std::endl;
            }
        }

        static awaitable<void> echo(tcp_socket socket) {
            try {
                char buffer[1024];
                for (;;) {
                    const std::size_t n = co_await socket.async_read_some(boost::asio::buffer(buffer, sizeof(buffer)));
                    co_await socket.async_write_some(boost::asio::buffer(buffer, n));
                }
            } catch (std::exception &e) {
                std::cerr << "Server exception: " << e.what() << std::endl;
            }
        }

        [[noreturn]] static awaitable<void> listener();

    private:
    };
}

#endif //TRADINGEXCHANGE_SERVER_H
