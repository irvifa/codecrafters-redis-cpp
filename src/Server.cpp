#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <thread>
#include <vector>
#include <memory>
#include <mutex>

class RedisServer {
private:
    int server_fd;
    struct sockaddr_in server_addr;
    const int PORT = 6379;
    const int CONNECTION_BACKLOG = 5;
    const int BUFFER_SIZE = 1024;
    
    std::vector<std::thread> client_threads;
    std::mutex cout_mutex; // For thread-safe console output
    bool running;

    void setupServerSocket() {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            throw std::runtime_error("Failed to create server socket");
        }

        // Set SO_REUSEADDR option
        int reuse = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            throw std::runtime_error("setsockopt failed");
        }

        // Setup server address structure
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(PORT);
    }

    void bindSocket() {
        if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
            throw std::runtime_error("Failed to bind to port 6379");
        }
    }

    void startListening() {
        if (listen(server_fd, CONNECTION_BACKLOG) != 0) {
            throw std::runtime_error("listen failed");
        }
    }

    void logMessage(const std::string& message) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << message << std::endl;
    }

    void handleClient(int client_fd) {
        char buffer[BUFFER_SIZE];
        const char* response = "+PONG\r\n";
        
        while (running) {
            // Read from client
            ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
            
            // Handle client disconnect or error
            if (bytes_read <= 0) {
                if (bytes_read == 0) {
                    logMessage("Client disconnected");
                } else {
                    logMessage("Error reading from client");
                }
                break;
            }

            // Send PONG response
            if (send(client_fd, response, strlen(response), 0) < 0) {
                logMessage("Error sending response");
                break;
            }
        }

        close(client_fd);
    }

    void acceptClients() {
        while (running) {
            struct sockaddr_in client_addr;
            int client_addr_len = sizeof(client_addr);
            
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, (socklen_t*)&client_addr_len);
            
            if (client_fd < 0) {
                logMessage("Failed to accept client connection");
                continue;
            }

            logMessage("Client connected");
            
            // Create a new thread to handle this client
            client_threads.emplace_back([this, client_fd]() {
                handleClient(client_fd);
            });
        }
    }

public:
    RedisServer() : server_fd(-1), running(true) {
        setupServerSocket();
        bindSocket();
        startListening();
    }

    ~RedisServer() {
        stop();
        
        if (server_fd >= 0) {
            close(server_fd);
        }
    }

    void start() {
        logMessage("Server starting... Waiting for clients to connect...");
        acceptClients();
    }

    void stop() {
        running = false;
        
        // Wait for all client threads to finish
        for (auto& thread : client_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        client_threads.clear();
    }
};

int main(int argc, char** argv) {
    // Flush after every std::cout / std::cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    try {
        RedisServer server;
        server.start();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}