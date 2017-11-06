#include "Connection.h"
#include "iostream"
#include <boost/asio/write.hpp>
#include "ReservationRequest.h"
#include <boost/archive/text_oarchive.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>

namespace asio = boost::asio;
using asio::ip::tcp;

void Connection::go() {
    auto self(shared_from_this());
    asio::spawn(socket.get_io_service(),
                [this, self](asio::yield_context yield) {
                    try {

                        // Read initial request type from client
                        auto reserve_message = async_read_line(socket, yield);
                        if (reserve_message == "queue_request")
                            handle_queue_request(yield);
                        else if (reserve_message == "diagnostic_request")
                            handle_diagnostic_request(yield);
                        else
                            throw std::system_error(EPERM, std::system_category());
                    }
                    catch (std::exception &e) {
                        std::cout << "Exception: " << e.what() << std::endl;
                    }
                });
}

void Connection::async_write_resource(const Resource &resource, asio::yield_context yield) {
    // Serialize the data into a string
    std::ostringstream archive_stream;
    boost::archive::text_oarchive archive(archive_stream);
    archive << resource;
    auto serialized_resource = archive_stream.str();

    // Construct byte count header
    auto header = std::to_string(serialized_resource.size());
    header.resize(4);

    // Send header consisting of 4 byte size, in bytes, of archived Resource
    async_write(socket, asio::buffer(header), yield);
    async_write(socket, asio::buffer(serialized_resource), yield);
}

void Connection::handle_queue_request(asio::yield_context yield) {
    // Wait in the queue for a reservation to begin
    ReservationRequest reservation(socket.get_io_service(), queue);
    auto resource = reservation.async_wait(yield);

    async_write_resource(resource, yield);

    // Listen for the client to finish
    auto finished_message = async_read_line(socket, yield);
    if (finished_message != "finished") {
        throw std::system_error(EBADMSG, std::system_category());
    }
}

void Connection::handle_diagnostic_request(asio::yield_context yield) {
    std::cout << "queue stuff here...\n";
}

// Async read a line message into a string
std::string async_read_line(tcp::socket &socket, asio::yield_context &yield) {
    asio::streambuf reserve_buffer;
    asio::async_read_until(socket, reserve_buffer, '\n', yield);
    std::istream reserve_stream(&reserve_buffer);
    std::string reserve_string;
    std::getline(reserve_stream, reserve_string);
    return reserve_string;
}