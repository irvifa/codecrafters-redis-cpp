#ifndef CONFIG_MANAGER_HPP
#define CONFIG_MANAGER_HPP

#include <string>
#include <unordered_map>
#include <mutex>
#include <optional>

class ConfigManager {
private:
    std::unordered_map<std::string, std::string> config;
    std::mutex mutex;

public:
    ConfigManager();
    void parseArgs(int argc, char** argv);
    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
};

#endif // CONFIG_MANAGER_HPP