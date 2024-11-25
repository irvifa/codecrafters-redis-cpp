#include "redis_server.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdexcept>

RedisServer::RedisServer(int argc, char** argv) : 
    server_fd(-1), 
    running(true),
    command_handler(kv_store, config_manager) {
    config_manager.parseArgs(argc, argv);
    setupServerSocket();
    bindSocket();
    startListening();
}

RedisServer::~RedisServer() {
    stop();
    if (server_fd >= 0) {
        close(server_fd);
    }
}

void RedisServer::setupServerSocket() {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        throw std::runtime_error("Failed to create server socket");
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        throw std::runtime_error("setsockopt failed");
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
}

void RedisServer::bindSocket() {
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        throw std::runtime_error("Failed to bind to port 6379");
    }
}

void RedisServer::startListening() {
    if (listen(server_fd, CONNECTION_BACKLOG) != 0) {
        throw std::runtime_error("listen failed");
    }
}

void RedisServer::logMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << message << std::endl;
}

void RedisServer::handleClient(int client_fd) {
    char buffer[BUFFER_SIZE];
    
    while (running) {
        ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                logMessage("Client disconnected");
            } else {
                logMessage("Error reading from client");
            }
            break;
        }

        buffer[bytes_read] = '\0';
        
        try {
            RESPParser::Command cmd = RESPParser::parseCommand(buffer);
            std::string response = command_handler.handleCommand(cmd);
            
            if (send(client_fd, response.c_str(), response.length(), 0) < 0) {
                logMessage("Error sending response");
                break;
            }
        } catch (const std::exception& e) {
            logMessage("Error processing command: " + std::string(e.what()));
            break;
        }
    }

    close(client_fd);
}

void RedisServer::acceptClients() {
    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        
        if (client_fd < 0) {
            logMessage("Failed to accept client connection");
            continue;
        }

        logMessage("Client connected");
        
        client_threads.emplace_back([this, client_fd]() {
            handleClient(client_fd);
        });
    }
}

void RedisServer::start() {
    logMessage("Server starting... Waiting for clients to connect...");
    acceptClients();
}

void RedisServer::stop() {
    running = false;
    for (auto& thread : client_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    client_threads.clear();
}