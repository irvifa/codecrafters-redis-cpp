#include "key_value_store.hpp"
#include "rdb_reader.hpp"
#include <fstream>

bool KeyValueStore::isExpired(const ValueWithExpiry& entry) const {
    if (!entry.expiry) {
        return false;
    }
    return std::chrono::steady_clock::now() > *entry.expiry;
}

void KeyValueStore::set(const std::string& key, const std::string& value, 
                       std::optional<std::chrono::milliseconds> expiry) {
    if (key.empty()) {
        throw std::invalid_argument("Key cannot be empty");
    }

    std::lock_guard<std::mutex> lock(mutex);
    
    ValueWithExpiry entry{value, std::nullopt};
    if (expiry) {
        if (expiry->count() < 0) {
            throw std::invalid_argument("Expiry time cannot be negative");
        }
        entry.expiry = std::chrono::steady_clock::now() + *expiry;
    }
    
    store[key] = std::move(entry);
}

std::optional<std::string> KeyValueStore::get(const std::string& key) {
    if (key.empty()) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(mutex);
    
    auto it = store.find(key);
    if (it == store.end()) {
        return std::nullopt;
    }
    
    if (isExpired(it->second)) {
        store.erase(it);
        return std::nullopt;
    }
    
    return it->second.value;
}

void KeyValueStore::loadFromRDB(const std::string& dir, const std::string& filename) {
    if (dir.empty() || filename.empty()) {
        throw std::invalid_argument("Directory and filename cannot be empty");
    }

    std::string filepath = dir;
    if (filepath.back() != '/' && filename.front() != '/') {
        filepath += '/';
    }
    filepath += filename;

    std::lock_guard<std::mutex> lock(mutex);

    try {
        // Use RDBReader to load keys
        RDBReader reader(filepath);
        auto keys = reader.readKeys();

        for (const auto& key : keys) {
            set(key, "", std::nullopt); // Insert keys with empty values
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load RDB: " + std::string(e.what()));
    }
}

// Add a cleanup method to remove expired entries
void KeyValueStore::cleanup() {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto it = store.begin(); it != store.end();) {
        if (isExpired(it->second)) {
            it = store.erase(it);
        } else {
            ++it;
        }
    }
}

// Add a method to remove a key explicitly
bool KeyValueStore::remove(const std::string& key) {
    if (key.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex);
    return store.erase(key) > 0;
}

std::vector<std::string> KeyValueStore::getKeys() const {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<std::string> keys;
    keys.reserve(store.size());
    
    for (const auto& [key, value] : store) {
        if (!isExpired(value)) {
            keys.push_back(key);
        }
    }
    return keys;
}
