//
// blocking_tcp_echo_client.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <boost/asio.hpp>
#include <system_error>

using boost::asio::ip::tcp;

std::string read_line(tcp::socket& socket) {
    boost::asio::streambuf reserve_buffer;
    boost::asio::read_until(socket, reserve_buffer, '\n');
    std::istream reserve_stream(&reserve_buffer);
    std::string reserve_string;
    std::getline(reserve_stream, reserve_string);
    return reserve_string;
}

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 3)
    {
      std::cerr << "Usage: blocking_tcp_echo_client <host> <port>\n";
      return 1;
    }

    boost::asio::io_service io_service;

    tcp::socket socket(io_service);
    tcp::resolver resolver(io_service);
    boost::asio::connect(socket, resolver.resolve({argv[1], argv[2]}));

    std::string message("request\n");
    boost::asio::write(socket, boost::asio::buffer(message));

    auto ready_message = read_line(socket);
    if(ready_message != "ready") {
      throw std::system_error(EBADMSG, std::system_category());
    }

    // Simulated work
    std::cout<<"work starting\n";
    sleep(5);

    // Let the queue know we're finished
    message = "finished\n";
    boost::asio::write(socket, boost::asio::buffer(message));

  }
  catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}