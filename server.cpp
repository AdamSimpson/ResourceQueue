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

using boost::asio::ip::tcp;

class JobQueue {
public:
    explicit JobQueue(int max_active) : max_active{max_active},
                                        current_active{0} {}

    // Return true if job started immediately
    bool enter(const std::shared_ptr<boost::asio::deadline_timer> &timer) {
        pending_queue.push_back(timer);
        return tick();
    }

    void exit(const std::shared_ptr<boost::asio::deadline_timer> &timer) {
        auto active_position = std::find(active_queue.begin(), active_queue.end(), timer);

        if (active_position != active_queue.end()) {
            active_queue.erase(active_position);
            tick();
        } else {
            auto pending_position = std::find(pending_queue.begin(), pending_queue.end(), timer);
            if (pending_position != pending_queue.end())
                pending_queue.erase(active_position);
            else {
                std::cerr << "Invalid timer marked for removal from queue!\n";
            }
        }
    }

private:
    const int max_active;
    int current_active;
    std::deque<std::shared_ptr<boost::asio::deadline_timer> > pending_queue;
    std::deque<std::shared_ptr<boost::asio::deadline_timer> > active_queue;

    // Advance the queue
    // Return true if a job was started
    bool tick() {
        if (!pending_queue.empty() && active_queue.size() < max_active) {
            // Grab next pending timer
            auto &timer = pending_queue.front();
            // Add the timer to the active queue
            active_queue.push_back(timer);
            // Remove the timer from the pending queue
            pending_queue.pop_front();
            // Cancel the timer so the co-routine that is waiting on it can proceed
            timer->cancel();
            return true;
        } else {
            return false;
        }
    }
};

// Handle access to the queue
class Reservation {
public:
    Reservation(boost::asio::io_service &io_service, JobQueue &queue) : io_service(io_service),
                                                                        queue_timer(
                                                                                std::make_shared<boost::asio::deadline_timer>(
                                                                                        io_service)),
                                                                        queue(queue) {}

    ~Reservation() {
        queue.exit(queue_timer);
    }

    // Create an infinite timer that will be cancelled by the queue when the job is ready
    void async_wait(boost::asio::yield_context yield) {
        // When entered into the queue the queue will tick and possibly expire the timer
        // if the timer is expired async_wait will deadlock so we take care to only call it on a valid timer
        bool running = queue.enter(queue_timer);
        if (!running) {
            queue_timer->expires_at(boost::posix_time::pos_infin);
            queue_timer->async_wait(yield);
        }
    }

private:
    boost::asio::io_service &io_service;
    std::shared_ptr<boost::asio::deadline_timer> queue_timer;
    JobQueue &queue;
};

// Async read a line message into a string
std::string async_read_line(tcp::socket& socket, boost::asio::yield_context& yield) {
    boost::asio::streambuf reserve_buffer;
    boost::asio::async_read_until(socket, reserve_buffer, '\n', yield);
    std::istream reserve_stream(&reserve_buffer);
    std::string reserve_string;
    std::getline(reserve_stream, reserve_string);
    return reserve_string;
}

class session : public std::enable_shared_from_this<session> {
public:
    explicit session(tcp::socket socket, JobQueue &job_queue) : socket(std::move(socket)),
                                                                job_queue(job_queue) {}
    void go() {
        auto self(shared_from_this());
        boost::asio::spawn(socket.get_io_service(),
                           [this, self](boost::asio::yield_context yield) {
                               try {

                                   // Read initial request from client
                                   auto reserve_message = async_read_line(socket, yield);
                                   if (reserve_message != "request") {
                                       throw std::system_error(EBADMSG, std::system_category());
                                   }

                                   // Wait in the queue for a build slot to open up
                                   Reservation reservation(socket.get_io_service(), job_queue);
                                   reservation.async_wait(yield);

                                   // Let the client know they are ready to run
                                   std::string ready_message("ready\n");
                                   async_write(socket, boost::asio::buffer(ready_message), yield);

                                   // Listen for the client to finish
                                   auto finished_message = async_read_line(socket, yield);
                                   if (finished_message != "finished") {
                                       throw std::system_error(EBADMSG, std::system_category());
                                   }
                               }
                               catch (std::exception &e) {
                                   std::cout << "Exception: " << e.what() << std::endl;
                               }
                           });
    }

private:
    tcp::socket socket;
    JobQueue &job_queue;
};

int main(int argc, char *argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: echo_server <port>\n";
            return 1;
        }

        boost::asio::io_service io_service;

        JobQueue job_queue(2);

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