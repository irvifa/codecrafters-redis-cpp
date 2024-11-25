#include "config_manager.hpp"

ConfigManager::ConfigManager() {
    // Set default values
    config["dir"] = "./";
    config["dbfilename"] = "dump.rdb";
}

void ConfigManager::parseArgs(int argc, char** argv) {
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

void ConfigManager::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex);
    config[key] = value;
}

std::optional<std::string> ConfigManager::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = config.find(key);
    if (it != config.end()) {
        return it->second;
    }
    return std::nullopt;
}