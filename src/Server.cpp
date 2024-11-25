#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

class RedisServer {
private:
    int server_fd;
    int client_fd;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    const int PORT = 6379;
    const int CONNECTION_BACKLOG = 5;
    const int BUFFER_SIZE = 1024;

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

    void handleClient() {
        char buffer[BUFFER_SIZE];
        const char* response = "+PONG\r\n";
        
        while (true) {
            // Read from client
            ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
            
            // Handle client disconnect or error
            if (bytes_read <= 0) {
                if (bytes_read == 0) {
                    std::cout << "Client disconnected\n";
                } else {
                    std::cerr << "Error reading from client\n";
                }
                break;
            }

            // For this stage, we respond with PONG for any input
            // We'll add proper command parsing in later stages
            if (send(client_fd, response, strlen(response), 0) < 0) {
                std::cerr << "Error sending response\n";
                break;
            }
        }
    }

public:
    RedisServer() : server_fd(-1), client_fd(-1) {
        setupServerSocket();
        bindSocket();
        startListening();
    }

    ~RedisServer() {
        if (client_fd >= 0) {
            close(client_fd);
        }
        if (server_fd >= 0) {
            close(server_fd);
        }
    }

    void start() {
        std::cout << "Waiting for a client to connect...\n";
        
        int client_addr_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, (socklen_t*)&client_addr_len);
        
        if (client_fd < 0) {
            throw std::runtime_error("Failed to accept client connection");
        }
        
        std::cout << "Client connected\n";
        handleClient();
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