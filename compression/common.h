#ifndef COMPRESSION_COMMON_H
#define COMPRESSION_COMMON_H

#include <cinttypes>
#include <stdexcept>

namespace compression {

class ParsingError : public std::logic_error {
public:
    ParsingError(const std::string& msg): std::logic_error(msg) {}
};

using TSType = std::uint64_t;
using ValType = double;

}
#endif