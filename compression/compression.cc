
#include <vector>
#include <utility>
#include <iostream>
#include "compression.h"


namespace compression {

const int kMaxTimeLengthOfBlockSecs = 2 * 60 * 60;

// Helper function to print bit values.
void PrintHex(std::vector<std::uint8_t> data) {
    std::cout << "Values in hex:\n";
    for (auto d: data) {
        std::cout << std::hex << (int)d << "\n";
    }
    std::cout << "\n";
}

TSType AlignTS(TSType timestamp) {
    // 2h blocks aligned to epoch.
    return timestamp - (timestamp % (2 * 60 * 60));
}

EncodedDataBlock::EncodedDataBlock(TSType timestamp, ValType val):
 last_ts_(timestamp), last_val_(val) {
    // Align timestamp to the epoch and figure out what the delta is.
    auto aligned_ts = AlignTS(timestamp);
    std::uint16_t delta = timestamp - aligned_ts;
    last_ts_delta_ = delta;
    last_ts_ = timestamp;
    start_ts_ = aligned_ts;
    // store the last delta.
    // Encode stuff correctly.

    std::uint8_t mask = 0xFF;
    for (int i = 0; i < (int)sizeof(TSType); i++) {
        data_.push_back((mask & (aligned_ts >> i * 8)));
    }

    // Paper says 14 bits, but, 2 bits of a difference and lack of
    // need for shifting everything sounds like an advantage to me.
    for (int i = 0; i < 2; i++) {
        data_.push_back((mask & (delta >> i * 8)));
    }

    const std::uint8_t *c = reinterpret_cast<std::uint8_t *>(&val);
    for (int i = 0; i < (int)sizeof(ValType); i++) {
        data_.push_back(c[i]);
    }
    // to be continued.
}

std::vector<std::pair<TSType, ValType>> EncodedDataBlock::Decode() {
    int ts_size = sizeof(TSType);
    TSType aligned_timestamp = 0;
    for (int i = 0; i < ts_size; i++) {
        aligned_timestamp |= (data_[i] << i * 8);
    }
    int delta_size = 2;
    std::uint16_t delta = 0;
    for (int i = ts_size; i < ts_size + delta_size; i++) {
        delta |= (data_[i] << i * 8);
    }
    int offset = ts_size + delta_size;
    std::uint8_t data[sizeof(ValType)];
    std::copy(data_.begin() + offset, data_.begin() + offset + (int)sizeof(ValType), data);
    ValType val = *reinterpret_cast<ValType*>(&data);
    return std::vector<std::pair<TSType, ValType>>{ {aligned_timestamp + delta, val}};
}

void Encoder::Append(TSType timestamp, ValType val) {
    if (!blocks_.empty()) {
        auto last_block = blocks_.back();
        if (last_block->WithinRange(timestamp)) {
            last_block->Append(timestamp, val);
            return;
        }
    }
    auto block = StartNewBlock(timestamp, val);
    blocks_.push_back(block);
}

std::vector<std::pair<TSType, ValType>> Encoder::Decode() {
    std::vector<std::pair<TSType, ValType>> all_ts;
    for(auto block : blocks_) {
        auto block_ts = block->Decode();
        all_ts.insert(all_ts.end(), block_ts.begin(), block_ts.end());
    }
    return all_ts;
}

bool EncodedDataBlock::WithinRange(TSType timestamp) {
    return timestamp - start_ts_ < kMaxTimeLengthOfBlockSecs;
}

void EncodedDataBlock::Append(TSType timestamp, ValType val) {

}

// Encoding aligned. And later shifting across bytes.


} // namespace compression
