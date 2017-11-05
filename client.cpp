#include <cstdlib>
#include <cstring>
#include <iostream>
#include <boost/asio.hpp>
#include <system_error>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/serialization.hpp>

using boost::asio::ip::tcp;

struct Resource {
    int loop_id;
    std::string host;
};
namespace boost {
    namespace serialization {

        template<class Archive>
        void serialize(Archive &ar, Resource &res, const unsigned int version) {
            ar & res.loop_id;
            ar & res.host;
        }
    } // namespace serialization
} // namespace boost

std::string read_line(tcp::socket &socket) {
    boost::asio::streambuf reserve_buffer;
    boost::asio::read_until(socket, reserve_buffer, '\n');
    std::istream reserve_stream(&reserve_buffer);
    std::string reserve_string;
    std::getline(reserve_stream, reserve_string);
    return reserve_string;
}

// Read of a Resource
// Read header consisting of 4 byte size, in bytes, of serialized Resource
// followed by our serialized object
Resource read_resource(tcp::socket &socket) {
    // Read in 4 byte header
    const size_t header_size = 4;
    std::string header("", header_size);
    boost::asio::read(socket, boost::asio::buffer(&header[0], header_size));
    uint32_t resource_size = std::stoi(header);

    // Read in serialized Resource
    std::string serialized_resource("", resource_size);
    boost::asio::read(socket, boost::asio::buffer(&serialized_resource[0], resource_size));

    // de-serialize Resource
    Resource resource;
    std::istringstream archive_stream(serialized_resource);
    boost::archive::text_iarchive archive(archive_stream);
    archive >> resource;

    return resource;
}

int main(int argc, char *argv[]) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: blocking_tcp_echo_client <host> <port>\n";
            return 1;
        }

        boost::asio::io_service io_service;

        tcp::socket socket(io_service);
        tcp::resolver resolver(io_service);
        boost::asio::connect(socket, resolver.resolve({argv[1], argv[2]}));

        std::string message("queue_request\n");
        boost::asio::write(socket, boost::asio::buffer(message));

        auto resource = read_resource(socket);
        std::cout << "acquired resource: " << resource.host << ", loop " << resource.loop_id << std::endl;

        // Simulated work
        std::cout << "work starting\n";
        sleep(5);

        // Let the queue know we're finished
        message = "finished\n";
        boost::asio::write(socket, boost::asio::buffer(message));

    }
    catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}