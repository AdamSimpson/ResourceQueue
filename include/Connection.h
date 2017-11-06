#pragma once

#include "ResourceQueue.h"

class Connection : public std::enable_shared_from_this<Connection> {
public:
    explicit Connection(tcp::socket socket, ResourceQueue &queue) : socket(std::move(socket)),
                                                                    queue(queue) {}

    void begin();

private:
    tcp::socket socket;
    ResourceQueue &queue;

    // Async write of a Resource
    // Send header consisting of 4 byte size, in bytes, of archived Resource
    // followed by our serialized object
    void async_write_resource(const Resource &resource, asio::yield_context yield);

    // Handle a request to enter the queue to acquire a resource
    void handle_queue_request(asio::yield_context yield);

    // Send queue diagnostic information
    void handle_diagnostic_request(asio::yield_context yield);
};

std::string async_read_line(tcp::socket &socket, asio::yield_context &yield);
