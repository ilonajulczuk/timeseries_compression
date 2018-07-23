#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <utility>
#include <vector>
#include <cinttypes>
#include <stdexcept>

namespace compression {

class ParsingError : public std::logic_error {
public:
    ParsingError(const std::string& msg): std::logic_error(msg) {}
};

using TSType = std::uint64_t;
using ValType = double;

extern const int kMaxTimeLengthOfBlockSecs;

std::pair<std::vector<std::uint8_t>, int> BitAppend(
    int bit_offset, int number_of_bits, std::uint64_t value, std::uint8_t initial_byte);


void PrintBin(std::vector<std::uint8_t> data);
void PrintBin(std::uint64_t data);
std::uint64_t DoubleAsInt(ValType val);
std::uint64_t ReadBits(int num_bits, unsigned int byte_offset, int bit_offset, std::vector<std::uint8_t>& data);
ValType DoubleFromInt(std::uint64_t int_encoded);

// TODO: figure out how to handle failures, either exceptions or status codes?


int LeadingZeroBits(std::uint64_t val);
int TrailingZeroBits(std::uint64_t val);

class EncodedDataBlock {

public:

    EncodedDataBlock(TSType timestamp, ValType val);

    bool WithinRange(TSType timestamp);

    void Append(TSType timestamp, ValType val);

    std::vector<std::pair<TSType, ValType>> Decode();
    void PrintBinData() {
        PrintBin(data_);
    }

private:
    // start_ts_ is necessary to check if the next value fits within the block.
    TSType start_ts_;

    // Make life easier by caching values necessary for encoding next ts, val pair.
    TSType last_ts_;
    ValType last_val_;
    int last_ts_delta_;
    int last_xor_leading_zeros_;
    int last_xor_meaningful_bits_;

    // The actual encrypted data and its offset if the bits are not aligned perfectly from the end.
    int data_end_offset_;
    std::vector<std::uint8_t> data_;

    void AppendBits(int number_of_bits, std::uint64_t value);
    void EncodeTS(TSType timestamp);
    void EncodeVal(ValType val);
    std::uint64_t ReadBits(int num_bits, unsigned int byte_offset, int bit_offset);
};


class Encoder {

public:

    void Append(TSType timestamp, ValType val);

    std::vector<std::pair<TSType, ValType>> Decode();
    // Iterators to easily iterate over the data?

    void PrintBinData() {
        for (auto b: blocks_) {
            b->PrintBinData();
        }
    }
private:
    EncodedDataBlock* StartNewBlock(TSType timestamp, ValType val) {
        return new EncodedDataBlock(timestamp, val);
    }

    // TODO: ownership of this.
    std::vector<EncodedDataBlock*> blocks_;
};



}
#endif