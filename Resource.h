#pragma once

#include <string>
#include <boost/serialization/string.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/serialization/string.hpp>

namespace asio = boost::asio;
using asio::ip::tcp;

class Resource {
public:
    int loop_id;
    std::string host;
};
namespace boost {
    namespace serialization {

        template<class Archive>
        void serialize(Archive & ar, Resource & res, const unsigned int version) {
            ar & res.loop_id;
            ar & res.host;
        }
    } // namespace serialization
} // namespace boost

Resource read_resource(tcp::socket &socket);