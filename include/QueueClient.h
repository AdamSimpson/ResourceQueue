#pragma once

#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include "resource.h"
#include <type_traits>

namespace asio = boost::asio;
using asio::ip::tcp;

Resource read_resource(tcp::socket &socket);

class QueueClient {
public:
    explicit QueueClient(std::string server_name, int server_port) : server_name(server_name),
                                                                     server_port(server_port) {}

    // Runs the specified function after waiting for resources
    // This only works if the queue server and client are on the same host
    template<typename R>
    auto queue_job(std::function<R()> func) {
        asio::io_service io_service;
        tcp::socket socket(io_service);
        tcp::resolver resolver(io_service);
        asio::connect(socket, resolver.resolve({server_name, std::to_string(server_port)}));

        // Initiate a request to enter the queue
        std::string message("queue_request\n");
        asio::write(socket, asio::buffer(message));

        auto resource = read_resource(socket);
        std::cout << "acquired resource: " << resource.host << ", loop " << resource.loop_id << std::endl;

        R result = func();

        // Let the queue know we're finished
        message = "finished\n";
        asio::write(socket, asio::buffer(message));
        return result;
    }

    // void return type template specilization
    void queue_job(std::function<void()> func) {
        asio::io_service io_service;
        tcp::socket socket(io_service);
        tcp::resolver resolver(io_service);
        asio::connect(socket, resolver.resolve({server_name, std::to_string(server_port)}));

        // Initiate a request to enter the queue
        std::string message("queue_request\n");
        asio::write(socket, asio::buffer(message));

        auto resource = read_resource(socket);
        std::cout << "acquired resource: " << resource.host << ", loop " << resource.loop_id << std::endl;

        func();

        // Let the queue know we're finished
        message = "finished\n";
        asio::write(socket, asio::buffer(message));
    }

private:
    const std::string server_name;
    const int server_port;
};