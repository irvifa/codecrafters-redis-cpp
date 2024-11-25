#include "key_value_store.hpp"

bool KeyValueStore::isExpired(const ValueWithExpiry& entry) const {
    if (!entry.expiry) {
        return false;
    }
    return std::chrono::steady_clock::now() > *entry.expiry;
}

void KeyValueStore::set(const std::string& key, const std::string& value, 
                       std::optional<std::chrono::milliseconds> expiry) {
    std::lock_guard<std::mutex> lock(mutex);
    
    ValueWithExpiry entry{value, std::nullopt};
    if (expiry) {
        entry.expiry = std::chrono::steady_clock::now() + *expiry;
    }
    
    store[key] = entry;
}

std::optional<std::string> KeyValueStore::get(const std::string& key) {
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