#ifndef COMMAND_HANDLER_HPP
#define COMMAND_HANDLER_HPP

#include "key_value_store.hpp"
#include "config_manager.hpp"
#include "resp_parser.hpp"
#include <string>

class CommandHandler {
private:
    KeyValueStore& kv_store;
    ConfigManager& config_manager;

    bool isNumber(const std::string& s);

public:
    CommandHandler(KeyValueStore& store, ConfigManager& cfg);
    std::string handleCommand(const RESPParser::Command& cmd);
};

#endif // COMMAND_HANDLER_HPP