#ifndef RESP_PARSER_HPP
#define RESP_PARSER_HPP

#include <string>
#include <vector>
#include <sstream>

class RESPParser {
public:
    struct Command {
        std::string name;
        std::vector<std::string> args;
    };

    static Command parseCommand(const std::string& input);
    static std::string createBulkString(const std::string& str);
    static std::string createArray(const std::vector<std::string>& elements);
    static std::string createNullBulkString();
    static std::string createSimpleString(const std::string& str);

private:
    static void readBulkString(std::istringstream& iss, std::string& result);
};

#endif // RESP_PARSER_HPP