#include <cstdlib>
#include <cstring>
#include <iostream>
#include <boost/asio.hpp>
#include <system_error>
#include <boost/serialization/serialization.hpp>
#include "Resource.h"

namespace asio = boost::asio;
using asio::ip::tcp;

int main(int argc, char *argv[]) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: blocking_tcp_echo_client <host> <port>\n";
            return 1;
        }

        asio::io_service io_service;

        tcp::socket socket(io_service);
        tcp::resolver resolver(io_service);
        asio::connect(socket, resolver.resolve({argv[1], argv[2]}));

        std::string message("queue_request\n");
        asio::write(socket, asio::buffer(message));

        auto resource = read_resource(socket);
        std::cout << "acquired resource: " << resource.host << ", loop " << resource.loop_id << std::endl;

        // Simulated work
        std::cout << "work starting\n";
        sleep(5);

        // Let the queue know we're finished
        message = "finished\n";
        asio::write(socket, asio::buffer(message));

    }
    catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}