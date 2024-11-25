#ifndef REDIS_SERVER_HPP
#define REDIS_SERVER_HPP

#include "key_value_store.hpp"
#include "config_manager.hpp"
#include "command_handler.hpp"
#include <vector>
#include <thread>
#include <mutex>
#include <netinet/in.h>

class RedisServer {
private:
    int server_fd;
    struct sockaddr_in server_addr;
    const int PORT = 6379;
    const int CONNECTION_BACKLOG = 5;
    const int BUFFER_SIZE = 1024;
    
    std::vector<std::thread> client_threads;
    std::mutex cout_mutex;
    bool running;
    KeyValueStore kv_store;
    ConfigManager config_manager;
    CommandHandler command_handler;

    void setupServerSocket();
    void bindSocket();
    void startListening();
    void logMessage(const std::string& message);
    void handleClient(int client_fd);
    void acceptClients();

public:
    RedisServer(int argc, char** argv);
    ~RedisServer();
    void start();
    void stop();
};

#endif // REDIS_SERVER_HPP