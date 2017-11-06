#include "../include/Resource.h"
#include <boost/asio/read.hpp>
#include <boost/archive/text_iarchive.hpp>

// Read of a Resource
// Read header consisting of 4 byte size, in bytes, of serialized Resource
// followed by our serialized object
Resource read_resource(tcp::socket &socket) {
    // Read in 4 byte header
    const size_t header_size = 4;
    std::string header("", header_size);
    asio::read(socket, asio::buffer(&header[0], header_size));
    unsigned int resource_size = std::stoul(header);

    // Read in serialized Resource
    std::string serialized_resource("", resource_size);
    asio::read(socket, asio::buffer(&serialized_resource[0], resource_size));

    // de-serialize Resource
    Resource resource;
    std::istringstream archive_stream(serialized_resource);
    boost::archive::text_iarchive archive(archive_stream);
    archive >> resource;

    return resource;
}