#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

class RDBReader {
public:
    explicit RDBReader(const std::string& filepath);
    std::vector<std::string> readKeys();

private:
    std::ifstream file;
    bool validateHeader();
    uint64_t readLength();
    std::string readString();
    void skipValue();
};
