#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <iostream>
#include <memory>
#include <queue>

using boost::asio::ip::tcp;

class JobQueue {
public:
  explicit JobQueue(int max_active) : max_active{max_active},
                                      current_active{0} {}

  void enter(const std::shared_ptr<boost::asio::deadline_timer> &timer) {
    pending_queue.push(timer);
    tick();
  }

  // Exit is only to be called on active jobs
  void exit() {
    current_active--;
    tick();
  }

private:
  const int max_active;
  int current_active;
  std::queue<std::shared_ptr<boost::asio::deadline_timer> > pending_queue;

  // Advance the queue
  void tick() {
    if (!pending_queue.empty() && current_active < max_active) {
      auto timer = pending_queue.front();
      pending_queue.pop();
      current_active++;
      // Expire the timer so the coroutine can proceed
      timer->expires_from_now(boost::posix_time::seconds(0));
    }
  }
};

// Handle access to the queue
class Reservation {
public:
  Reservation(boost::asio::io_service &io_service, JobQueue& queue) : io_service(io_service),
                                                             queue_timer(std::make_shared<boost::asio::deadline_timer>(
                                                                 io_service)),
                                                             queue(queue) {}
  ~Reservation() {
    queue.exit();
  }
  // Create an infinite timer that will be cancelled by the queue when the job is ready
  void async_wait(boost::asio::yield_context yield) {
                           queue_timer->expires_at(boost::posix_time::pos_infin);
                           queue.enter(queue_timer);
                           queue_timer->async_wait(yield);
  }

private:
  boost::asio::io_service &io_service;
  std::shared_ptr<boost::asio::deadline_timer> queue_timer;
  JobQueue& queue;
};

class session : public std::enable_shared_from_this<session> {
public:
  explicit session(tcp::socket socket, JobQueue &job_queue) : socket_(std::move(socket)),
                                                              job_queue(job_queue) {}

  void go() {
    auto self(shared_from_this());
    boost::asio::spawn(socket_.get_io_service(),
                       [this, self](boost::asio::yield_context yield) {
                         try {
                           // Read initial request from client
                           char data[1];
                           std::size_t n = socket_.async_read_some(boost::asio::buffer(data), yield);
                           if (data[0] != 'B') {
                             std::cerr << "invalid value sent to server: " << data;
                             // throw something or another
                           }
                           // Wait in the queue for an available build slot
                           Reservation reservation(socket_.get_io_service(), job_queue);
                           reservation.async_wait(yield);

                           // Let the client know they are ready to run
                           data[0] = 'R';
                           socket_.async_write_some(boost::asio::buffer(data, 1), yield);

                           // Listen for the client to finish
                           boost::system::error_code error;
                           size_t length = socket_.async_read_some(boost::asio::buffer(data), yield);
                           if (data[0] != 'C') {
                             std::cerr << "unexpected something or another\n";
                           }

                         }
                         catch (std::exception &e) {
                           std::cout<<"throwin stuff: " << e.what() << std::endl;
                           //socket_.close();
                         }
                       });
  }

private:
  tcp::socket socket_;
  JobQueue &job_queue;
};

int main(int argc, char *argv[]) {
  try {
    if (argc != 2) {
      std::cerr << "Usage: echo_server <port>\n";
      return 1;
    }

    boost::asio::io_service io_service;

    JobQueue job_queue(1);

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
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}