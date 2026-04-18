//
// Created by jake on 2/24/26.
//

#include "server.h"

awaitable<void> server::Server::listener() {
    const auto executor = co_await this_coro::executor;
    tcp::acceptor acceptor(executor, tcp::endpoint(tcp::v4(), 8080));
    for (;;) {
        auto socket = co_await acceptor.async_accept();
        co_spawn(executor, echo(std::move(socket)), detached);
    }
}
