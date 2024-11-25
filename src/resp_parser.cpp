#include "resp_parser.hpp"
#include <algorithm>
#include <stdexcept>

RESPParser::Command RESPParser::parseCommand(const std::string& input) {
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

std::string RESPParser::createBulkString(const std::string& str) {
    return "$" + std::to_string(str.length()) + "\r\n" + str + "\r\n";
}

std::string RESPParser::createArray(const std::vector<std::string>& elements) {
    std::string result = "*" + std::to_string(elements.size()) + "\r\n";
    for (const auto& element : elements) {
        result += createBulkString(element);
    }
    return result;
}

std::string RESPParser::createNullBulkString() {
    return "$-1\r\n";
}

std::string RESPParser::createSimpleString(const std::string& str) {
    return "+" + str + "\r\n";
}

void RESPParser::readBulkString(std::istringstream& iss, std::string& result) {
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