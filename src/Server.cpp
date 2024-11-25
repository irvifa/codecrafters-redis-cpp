#include "redis_server.hpp"
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    // Enable line buffering for stdout and stderr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    try {
        RedisServer server(argc, argv);
        server.start();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}