#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <iostream>
#include "../include/Resource.h"
#include "../include/Reservation.h"
#include "../include/ResourceQueue.h"
#include "../include/Connection.h"

namespace asio = boost::asio;
using asio::ip::tcp;

int main(int argc, char *argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: echo_server <port>\n";
            return 1;
        }

        asio::io_service io_service;

        ResourceQueue job_queue;
        Resource resource;
        resource.loop_id = 0;
        resource.host = std::string("localhost");
        job_queue.add_resource(resource);
        resource.loop_id = 1;
        job_queue.add_resource(resource);

        // Wait for connections
        asio::spawn(io_service,
                    [&](asio::yield_context yield) {
                        tcp::acceptor acceptor(io_service,
                                               tcp::endpoint(tcp::v4(), std::atoi(argv[1])));

                        for (;;) {
                            boost::system::error_code ec;
                            tcp::socket socket(io_service);
                            acceptor.async_accept(socket, yield[ec]);
                            if (!ec)
                                std::make_shared<Connection>(std::move(socket), job_queue)->begin();
                        }
                    });

        io_service.run();
    }
    catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}