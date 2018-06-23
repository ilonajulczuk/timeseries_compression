#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <utility>
#include <vector>
#include <cinttypes>

namespace compression {

using TSType = std::uint64_t;
using ValType = float;

extern const int kMaxTimeLengthOfBlockSecs;

// TODO: figure out how to handle failures, either exceptions or status codes?
class EncodedDataBlock {

public:

EncodedDataBlock(TSType timestamp, ValType val);

bool WithinRange(TSType timestamp);

void Append(TSType timestamp, ValType val);

private:

// start_ts_ is necessary to check if the next value fits within the block.
TSType start_ts_;

// Make life easier by caching values necessary for encoding next ts, val pair. 
TSType last_ts_;
int last_ts_delta_;
ValType last_val_;

// The actual encrypted data and its offset if the bits are not aligned perfectly from the end. 
int data_end_offset_;
// TODO: make it private
public:
std::vector<std::uint8_t> data_;

};


class Encoder {

public:

void Append(TSType timestamp, ValType val);

std::vector<std::pair<TSType, ValType>> Decode();
// Iterators to easily iterate over the data?

private:
EncodedDataBlock* StartNewBlock(TSType timestamp, ValType val) {
    return new EncodedDataBlock(timestamp, val);
}

// TODO: ownership of this.
std::vector<EncodedDataBlock*> blocks_;
};



}
#endif