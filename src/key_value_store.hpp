#ifndef KEY_VALUE_STORE_HPP
#define KEY_VALUE_STORE_HPP

#include <string>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <chrono>

class KeyValueStore {
private:
    struct ValueWithExpiry {
        std::string value;
        std::optional<std::chrono::steady_clock::time_point> expiry;
    };
    
    std::unordered_map<std::string, ValueWithExpiry> store;
    std::mutex mutex;

    bool isExpired(const ValueWithExpiry& entry) const;

public:
    void set(const std::string& key, const std::string& value, 
             std::optional<std::chrono::milliseconds> expiry = std::nullopt);
    std::optional<std::string> get(const std::string& key);
};

#endif // KEY_VALUE_STORE_HPP