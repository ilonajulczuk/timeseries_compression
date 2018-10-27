#include <bitset>
#include <iostream>
#include "helpers.h"

namespace compression {

std::uint64_t DoubleAsInt(ValType val) {
    return *reinterpret_cast<std::uint64_t*>(&val);
}

ValType DoubleFromInt(std::uint64_t int_encoded) {
    return *reinterpret_cast<ValType*>(&int_encoded);
}

void PrintHex(std::vector<std::uint8_t> data) {
    std::cout << "Values in hex:\n";
    for (auto d: data) {
        std::cout << std::hex << (int)d << "\n";
    }
    std::cout << "\n";
}

void PrintBin(std::vector<std::uint8_t> data) {
    std::cout << "Values in binary:\n";
    for (auto d: data) {
        std::cout << std::bitset<8>(d) << "\n";
    }
    std::cout << "\n";
}

void PrintBin(std::uint64_t data) {
    std::cout << "Values in binary:\n";
    std::cout << std::bitset<64>(data) << "\n";
}

int LeadingZeroBits(std::uint64_t val) {
    std::bitset<64> bitset_val(val);
    int count = 0;
    for (int i = 0; i < 64; i++) {
        if (bitset_val[63 - i] == 0 ) {
            count++;
        } else {
            break;
        }
    }
    return count;
}

int TrailingZeroBits(std::uint64_t val) {
    std::bitset<64> bitset_val(val);
    int count = 0;
    for (int i = 0; i < 64; i++) {
        if (bitset_val[i] == 0 ) {
            count++;
        } else {
            break;
        }
    }
    return count;
}


std::uint64_t TrimToMeaningfulBits(std::uint64_t value, int leading_zeros, int trailing_zeros) {
    return value >> trailing_zeros;
}

}