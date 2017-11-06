#include <cstdlib>
#include <cstring>
#include <iostream>
#include "QueueClient.h"

int main(int argc, char *argv[]) {
    try {
        QueueClient queue(std::string("localhost"), 12345);

        queue.queue_job([]() {
            std::cout << "did some work" << std::endl;
        });
    }
    catch (std::exception &e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}