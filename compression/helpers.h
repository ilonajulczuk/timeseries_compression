#ifndef COMPRESSION_HELPERS_H
#define COMPRESSION_HELPERS_H

#include <utility>
#include <vector>
#include <cinttypes>
#include "common.h"

namespace compression {

std::pair<std::vector<std::uint8_t>, int> BitAppend(
    int bit_offset, int number_of_bits, std::uint64_t value, std::uint8_t initial_byte);


void PrintBin(std::vector<std::uint8_t> data);
void PrintBin(std::uint64_t data);
std::uint64_t DoubleAsInt(ValType val);


int LeadingZeroBits(std::uint64_t val);
int TrailingZeroBits(std::uint64_t val);

}
#endif