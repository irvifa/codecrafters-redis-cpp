#include "command_handler.hpp"
#include "rdb_reader.hpp"
#include <algorithm>
#include <stdexcept>
#include <iostream>

CommandHandler::CommandHandler(KeyValueStore& store, ConfigManager& cfg) 
    : kv_store(store), config_manager(cfg) {}

bool CommandHandler::isNumber(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}

std::string CommandHandler::handleCommand(const RESPParser::Command& cmd) {
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
    else if (cmd.name == "KEYS") {
        if (cmd.args.empty()) {
            throw std::runtime_error("KEYS command requires a pattern argument");
        }

        if (cmd.args[0] == "*") {
            try {
                std::string dbdir = config_manager.get("dir").value_or("");
                std::string dbfilename = config_manager.get("dbfilename").value_or("");
                std::string full_path = dbdir + "/" + dbfilename;

                RDBReader reader(full_path);
                auto keys = reader.readKeys();

                return RESPParser::createArray(keys);
            } catch (const std::exception& e) {
                // Log the error if needed
                return RESPParser::createArray({});
            }
        }

        return RESPParser::createArray({});
    }
    throw std::runtime_error("Unknown command");
}