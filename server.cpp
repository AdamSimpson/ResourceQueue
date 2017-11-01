#include <iostream>
#include <thread>
#include <boost/asio.hpp>
#include <queue>
#include <mutex>

using boost::asio::ip::tcp;

// This is a mess
class JobQueue {
public:
  explicit JobQueue(int max_active): max_active{max_active},
                                    current_active{0}
  {}

  void enqueue(tcp::socket& socket) {
    mtx.lock();
    pending_queue.push(std::move(socket));
    mtx.unlock();
    tick();
  }

  // Inform the client they are cleared to run and wait for them to finish
  void run_job(tcp::socket& socket) {
    // Let the client know they can run
    char data[] = "R";
    boost::asio::write(socket, boost::asio::buffer(data, 1));

    // Listen for the client to finish
    boost::system::error_code error;
    size_t length = socket.read_some(boost::asio::buffer(data));
    current_active--;
    tick();
  }

private:
  std::mutex mtx;
  const int max_active;
  std::atomic<int> current_active;
  std::queue<tcp::socket> pending_queue;

  // Advance the queue
  void tick() {
    mtx.lock();
    if(!pending_queue.empty() && current_active < max_active) {
      auto socket(std::move(pending_queue.front()));
      pending_queue.pop();
      current_active++;
      mtx.unlock();
      run_job(socket);
    } else {
      mtx.unlock();
    }
  }

};

void session(tcp::socket socket, JobQueue& job_queue) {
  try {
      char data[1];

        boost::system::error_code error;
        size_t length = socket.read_some(boost::asio::buffer(data));
        if(data[0] != 'B') {
          std::cerr << "invalid value sent to server: " << data[0];
        }

        job_queue.enqueue(socket);
      }

  catch (std::exception& e) {
    std::cerr << "Exception in thread: " << e.what() << "\n";
  }
}

void server(boost::asio::io_service& io_service, unsigned short port, JobQueue& job_queue) {
  tcp::acceptor a(io_service, tcp::endpoint(tcp::v4(), port));
  for (;;)
  {
    tcp::socket socket(io_service);
    a.accept(socket);
    std::thread(session, std::move(socket), std::ref(job_queue)).detach();
  }
}

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: blocking_tcp_echo_server <port>\n";
      return 1;
    }

    JobQueue job_queue(4);
    boost::asio::io_service io_service;

    server(io_service, std::atoi(argv[1]), job_queue);
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
