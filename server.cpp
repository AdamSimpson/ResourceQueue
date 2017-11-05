#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <iostream>
#include <memory>
#include <deque>
#include <cstring>
#include <system_error>
#include <string>
#include <functional>

using boost::asio::ip::tcp;

// Resource which is limited, access to resource is controlled by the ResourceQueue
struct Resource {
    int loop_id;
};

// Represent a reservation in the queue
class Reservation {
public:
    Reservation(boost::asio::io_service &io_service) : io_service(io_service),
                                                       ready_timer(io_service),
                                                       reservation_ready(false) {
    }

    // Create an infinite timer that will be cancelled by the queue when the job is ready
    Resource async_wait(boost::asio::yield_context yield) {
        // When entered into the queue the queue will tick and possibly expire call this->ready()
        // If the timer is expired async_wait will deadlock so we take care to only call it on a valid timer
        if (!reservation_ready) {
            ready_timer.expires_at(boost::posix_time::pos_infin);
            boost::system::error_code ec;
            ready_timer.async_wait(yield[ec]);
            if(ec != boost::asio::error::operation_aborted) {
                throw std::system_error(EBUSY, std::system_category());
            }
        }
        return resource;
    }

    // Callback used by ResourceQueue to cancel the timer which signals our reservation is ready
    void ready(Resource acquired_resource) {
        reservation_ready = true;
        resource = acquired_resource;
        ready_timer.cancel();
    }

private:
    bool reservation_ready;
    Resource resource;
    boost::asio::io_service &io_service;
    boost::asio::deadline_timer ready_timer;
};

// Execute queued callback functions as resources allow
class ResourceQueue {
public:
    explicit ResourceQueue(int max_active) : max_active{max_active} {}

    // Create a new queue reservation and return it to the requester
    void enter(Reservation *reservation) {
        pending_queue.push_back(reservation);
        tick();
    }

    void exit(Reservation *reservation) {
        auto active_position = std::find(active_queue.begin(), active_queue.end(), reservation);

        if (active_position != active_queue.end()) {
            active_queue.erase(active_position);
            tick();
        } else {
            auto pending_position = std::find(pending_queue.begin(), pending_queue.end(), reservation);
            if (pending_position != pending_queue.end())
                pending_queue.erase(active_position);
            else {
                std::cerr << "Invalid timer marked for removal from queue!\n";
            }
        }
    }

private:
    const int max_active;
    std::deque<Reservation *> pending_queue;
    std::deque<Reservation *> active_queue;

    // Advance the queue
    // Return true if a job was started
    void tick() {
        if (!pending_queue.empty() && active_queue.size() < max_active) {
            // Grab next pending reservation
            auto reservation = pending_queue.front();
            // Add the reservation to the active queue
            active_queue.push_back(reservation);
            // Remove the reservation from the pending queue
            pending_queue.pop_front();
            // Invoke the reservation callback to inform the request it's ready to run
            Resource resource; // Make up a resource for now
            reservation->ready(resource);
        }
    }
};

// Handle RAII access to the ResourceQueue
class ReservationRequest {
public:
    ReservationRequest(boost::asio::io_service &io_service, ResourceQueue &queue) : io_service(io_service),
                                                                                    queue(queue),
                                                                                    reservation(io_service) {
        queue.enter(&reservation);
    }

    ~ReservationRequest() {
        queue.exit(&reservation);
    }

    Resource async_wait(boost::asio::yield_context yield) {
        auto resource = reservation.async_wait(yield);
        return resource;
    }

private:
    boost::asio::io_service &io_service;
    Reservation reservation;
    ResourceQueue &queue;

};

// Async read a line message into a string
std::string async_read_line(tcp::socket &socket, boost::asio::yield_context &yield) {
    boost::asio::streambuf reserve_buffer;
    boost::asio::async_read_until(socket, reserve_buffer, '\n', yield);
    std::istream reserve_stream(&reserve_buffer);
    std::string reserve_string;
    std::getline(reserve_stream, reserve_string);
    return reserve_string;
}

class session : public std::enable_shared_from_this<session> {
public:
    explicit session(tcp::socket socket, ResourceQueue &queue) : socket(std::move(socket)),
                                                                 queue(queue) {}

    void go() {
        auto self(shared_from_this());
        boost::asio::spawn(socket.get_io_service(),
                           [this, self](boost::asio::yield_context yield) {
                               try {

                                   // Read initial request from client
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

private:
    void handle_queue_request(boost::asio::yield_context yield) {
        // Wait in the queue for a reservation to begin
        ReservationRequest reservation(socket.get_io_service(), queue);
        auto resource = reservation.async_wait(yield);

        // Let the client know they are ready to run
        std::string ready_message("ready\n");
        async_write(socket, boost::asio::buffer(ready_message), yield);

        // Listen for the client to finish
        auto finished_message = async_read_line(socket, yield);
        if (finished_message != "finished") {
            throw std::system_error(EBADMSG, std::system_category());
        }
    }

    void handle_diagnostic_request(boost::asio::yield_context yield) {
        std::cout << "queue stuff here...\n";
    }

    tcp::socket socket;
    ResourceQueue &queue;
};

int main(int argc, char *argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: echo_server <port>\n";
            return 1;
        }

        boost::asio::io_service io_service;

        ResourceQueue job_queue(1);

        boost::asio::spawn(io_service,
                           [&](boost::asio::yield_context yield) {
                               tcp::acceptor acceptor(io_service,
                                                      tcp::endpoint(tcp::v4(), std::atoi(argv[1])));

                               for (;;) {
                                   boost::system::error_code ec;
                                   tcp::socket socket(io_service);
                                   acceptor.async_accept(socket, yield[ec]);
                                   if (!ec) std::make_shared<session>(std::move(socket), job_queue)->go();
                               }
                           });

        io_service.run();
    }
    catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}