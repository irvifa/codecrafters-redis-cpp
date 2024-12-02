#include "rdb_reader.hpp"
#include <stdexcept>
#include <array>
#include <iostream>
#include <arpa/inet.h>

RDBReader::RDBReader(const std::string& filepath) : file(filepath, std::ios::binary) {
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open RDB file");
    }
    
    if (!validateHeader()) {
        throw std::runtime_error("Invalid RDB file format");
    }
}

bool RDBReader::validateHeader() {
    std::array<char, 5> magic;
    file.read(magic.data(), 5);
    
    // Check for "REDIS" magic string
    if (std::string(magic.data(), 5) != "REDIS") {
        return false;
    }

    // Skip version bytes
    file.seekg(4, std::ios::cur);

    return true;
}

uint64_t RDBReader::readLength() {
    uint8_t byte;
    file.read(reinterpret_cast<char*>(&byte), 1);
    
    switch (byte >> 6) {
        case 0b00: // 6-bit length
            return byte & 0x3F;
        
        case 0b01: // 14-bit length
        {
            uint8_t next;
            file.read(reinterpret_cast<char*>(&next), 1);
            return (((byte & 0x3F) << 8) | next);
        }
        
        case 0b10: // 32-bit length
        {
            uint32_t len;
            file.read(reinterpret_cast<char*>(&len), 4);
            return be32toh(len); // Convert from big-endian
        }
        
        case 0b11: // Special string encodings
            switch (byte) {
                case 0xC0: // 8-bit integer
                {
                    uint8_t val;
                    file.read(reinterpret_cast<char*>(&val), 1);
                    return val;
                }
                case 0xC1: // 16-bit integer (little-endian)
                {
                    uint16_t val;
                    file.read(reinterpret_cast<char*>(&val), 2);
                    return le16toh(val);
                }
                case 0xC2: // 32-bit integer (little-endian)
                {
                    uint32_t val;
                    file.read(reinterpret_cast<char*>(&val), 4);
                    return le32toh(val);
                }
                // Note: 0xC3 for LZF compression is not implemented
                default:
                    throw std::runtime_error("Unsupported string encoding");
            }
    }
    
    throw std::runtime_error("Invalid length encoding");
}

std::string RDBReader::readString() {
    uint8_t byte;
    file.read(reinterpret_cast<char*>(&byte), 1);
    
    // Handle special string encodings
    if ((byte >> 6) == 0b11) {
        switch (byte) {
            case 0xC0: // 8-bit integer
            {
                uint8_t val;
                file.read(reinterpret_cast<char*>(&val), 1);
                return std::to_string(val);
            }
            case 0xC1: // 16-bit integer (little-endian)
            {
                uint16_t val;
                file.read(reinterpret_cast<char*>(&val), 2);
                return std::to_string(le16toh(val));
            }
            case 0xC2: // 32-bit integer (little-endian)
            {
                uint32_t val;
                file.read(reinterpret_cast<char*>(&val), 4);
                return std::to_string(le32toh(val));
            }
            default:
                throw std::runtime_error("Unsupported string encoding");
        }
    }
    
    // Rewind one byte since we've already read the first byte
    file.seekg(-1, std::ios::cur);
    
    // Regular length-prefixed string
    uint64_t len = readLength();
    std::string str(len, '\0');
    file.read(&str[0], len);
    return str;
}

void RDBReader::skipValue() {
    // Skip the type byte
    uint8_t type;
    file.read(reinterpret_cast<char*>(&type), 1);
    
    // For string values, skip the content
    if (type == 0) { // String encoding
        uint64_t len = readLength();
        file.seekg(len, std::ios::cur);
    }
    // Add support for other types as needed
}

std::vector<std::string> RDBReader::readKeys() {
    std::vector<std::string> keys;
    
    // Reset to the start of the file after header
    file.seekg(9, std::ios::beg);
    
    while (file.peek() != EOF) {
        uint8_t flag;
        file.read(reinterpret_cast<char*>(&flag), 1);
        
        if (flag == 0xFE) {
            // Database selector, read the database index using size encoding
            readLength();
            continue;
        }
        
        if (flag == 0xFF) {
            // EOF marker
            break;
        }
        
        // Handle expiry
        std::optional<uint64_t> expiry;
        if (flag == 0xFD) {
            // Seconds-based expiry (4-byte little-endian)
            uint32_t exp_seconds;
            file.read(reinterpret_cast<char*>(&exp_seconds), 4);
            expiry = le32toh(exp_seconds);
            
            // Read next flag
            file.read(reinterpret_cast<char*>(&flag), 1);
        } 
        else if (flag == 0xFC) {
            // Milliseconds-based expiry (8-byte little-endian)
            uint64_t exp_millis;
            file.read(reinterpret_cast<char*>(&exp_millis), 8);
            expiry = le64toh(exp_millis);
            
            // Read next flag
            file.read(reinterpret_cast<char*>(&flag), 1);
        }
        
        // Ensure it's a string type
        if (flag == 0) {
            // Read the key
            std::string key = readString();
            keys.push_back(key);
            
            // Skip the value
            skipValue();
        }
    }
    
    return keys;
}
