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
#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <optional>
#include <chrono>

// Configuration Manager
class ConfigManager {
private:
    std::unordered_map<std::string, std::string> config;
    std::mutex mutex;

public:
    ConfigManager() {
        // Set default values
        config["dir"] = "./";
        config["dbfilename"] = "dump.rdb";
    }

    void parseArgs(int argc, char** argv) {
        for (int i = 1; i < argc; i += 2) {
            if (i + 1 >= argc) break;
            
            std::string arg = argv[i];
            std::string value = argv[i + 1];
            
            if (arg == "--dir") {
                set("dir", value);
            } else if (arg == "--dbfilename") {
                set("dbfilename", value);
            }
        }
    }

    void set(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex);
        config[key] = value;
    }

    std::optional<std::string> get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = config.find(key);
        if (it != config.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

// RESP Protocol Parser
class RESPParser {
public:
    struct Command {
        std::string name;
        std::vector<std::string> args;
    };

    static Command parseCommand(const std::string& input) {
        Command cmd;
        std::istringstream iss(input);
        std::string line;

        std::getline(iss, line, '\n');
        if (line[0] != '*') {
            throw std::runtime_error("Invalid RESP array");
        }

        int arrayLen = std::stoi(line.substr(1));
        if (arrayLen < 1) {
            throw std::runtime_error("Empty command");
        }

        readBulkString(iss, cmd.name);
        std::transform(cmd.name.begin(), cmd.name.end(), cmd.name.begin(), ::toupper);

        for (int i = 1; i < arrayLen; i++) {
            std::string arg;
            readBulkString(iss, arg);
            cmd.args.push_back(arg);
        }

        return cmd;
    }

    static std::string createBulkString(const std::string& str) {
        return "$" + std::to_string(str.length()) + "\r\n" + str + "\r\n";
    }

    static std::string createArray(const std::vector<std::string>& elements) {
        std::string result = "*" + std::to_string(elements.size()) + "\r\n";
        for (const auto& element : elements) {
            result += createBulkString(element);
        }
        return result;
    }

    static std::string createNullBulkString() {
        return "$-1\r\n";
    }

    static std::string createSimpleString(const std::string& str) {
        return "+" + str + "\r\n";
    }

private:
    static void readBulkString(std::istringstream& iss, std::string& result) {
        std::string line;
        
        std::getline(iss, line, '\n');
        if (line[0] != '$') {
            throw std::runtime_error("Invalid bulk string");
        }
        
        int strLen = std::stoi(line.substr(1));
        if (strLen < 0) {
            throw std::runtime_error("Null bulk string");
        }

        char* buffer = new char[strLen + 1];
        iss.read(buffer, strLen);
        buffer[strLen] = '\0';
        result = std::string(buffer);
        delete[] buffer;

        iss.ignore(2);
    }
};

// Key-Value Store
class KeyValueStore {
private:
    struct ValueWithExpiry {
        std::string value;
        std::optional<std::chrono::steady_clock::time_point> expiry;
    };
    
    std::unordered_map<std::string, ValueWithExpiry> store;
    std::mutex mutex;

    bool isExpired(const ValueWithExpiry& entry) const {
        if (!entry.expiry) {
            return false;
        }
        return std::chrono::steady_clock::now() > *entry.expiry;
    }

public:
    void set(const std::string& key, const std::string& value, 
             std::optional<std::chrono::milliseconds> expiry = std::nullopt) {
        std::lock_guard<std::mutex> lock(mutex);
        
        ValueWithExpiry entry{value, std::nullopt};
        if (expiry) {
            entry.expiry = std::chrono::steady_clock::now() + *expiry;
        }
        
        store[key] = entry;
    }

    std::optional<std::string> get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = store.find(key);
        if (it != store.end()) {
            if (!isExpired(it->second)) {
                return it->second.value;
            }
            // Clean up expired key
            store.erase(it);
        }
        return std::nullopt;
    }
};

// Command Handler
class CommandHandler {
private:
    KeyValueStore& kv_store;
    ConfigManager& config_manager;

    bool isNumber(const std::string& s) {
        return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
    }


public:
    CommandHandler(KeyValueStore& store, ConfigManager& cfg) 
        : kv_store(store), config_manager(cfg) {}

    std::string handleCommand(const RESPParser::Command& cmd) {
        if (cmd.name == "PING") {
            return RESPParser::createSimpleString("PONG");
        } 
        else if (cmd.name == "ECHO") {
            if (cmd.args.empty()) {
                throw std::runtime_error("ECHO command requires an argument");
            }
            return RESPParser::createBulkString(cmd.args[0]);
        }
        else if (cmd.name == "CONFIG") {
            if (cmd.args.size() < 2) {
                throw std::runtime_error("CONFIG command requires subcommand and parameter");
            }
            
            std::string subcommand = cmd.args[0];
            std::transform(subcommand.begin(), subcommand.end(), subcommand.begin(), ::toupper);
            
            if (subcommand == "GET") {
                std::string param = cmd.args[1];
                auto value = config_manager.get(param);
                if (!value) {
                    throw std::runtime_error("Unknown config parameter");
                }
                return RESPParser::createArray({param, *value});
            }
            throw std::runtime_error("Unknown CONFIG subcommand");
        }
        else if (cmd.name == "SET") {
            if (cmd.args.size() < 2) {
                throw std::runtime_error("SET command requires key and value arguments");
            }
            
            std::optional<std::chrono::milliseconds> expiry;
            if (cmd.args.size() >= 4) {
                std::string px_arg = cmd.args[2];
                std::transform(px_arg.begin(), px_arg.end(), px_arg.begin(), ::toupper);
                
                if (px_arg == "PX" && isNumber(cmd.args[3])) {
                    expiry = std::chrono::milliseconds(std::stoll(cmd.args[3]));
                }
            }
            
            kv_store.set(cmd.args[0], cmd.args[1], expiry);
            return RESPParser::createSimpleString("OK");
        }
        else if (cmd.name == "GET") {
            if (cmd.args.empty()) {
                throw std::runtime_error("GET command requires a key argument");
            }
            auto value = kv_store.get(cmd.args[0]);
            if (value) {
                return RESPParser::createBulkString(*value);
            }
            return RESPParser::createNullBulkString();
        }
        throw std::runtime_error("Unknown command");
    }
};

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

    void setupServerSocket() {
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
            
            client_threads.emplace_back([this, client_fd]() {
                handleClient(client_fd);
            });
        }
    }

public:
    RedisServer(int argc, char** argv) : 
        server_fd(-1), 
        running(true),
        command_handler(kv_store, config_manager) {
        config_manager.parseArgs(argc, argv);
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
        for (auto& thread : client_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        client_threads.clear();
    }
};

int main(int argc, char** argv) {
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