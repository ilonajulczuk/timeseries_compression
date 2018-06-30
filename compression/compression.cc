
#include <vector>
#include <utility>
#include <iostream>
#include "compression.h"
#include <bitset>


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

void PrintBin(std::vector<std::uint8_t> data) {
    std::cout << "Values in binary:\n";
    for (auto d: data) {
        std::cout << std::bitset<8>(d) << "\n";
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
        data_.push_back((mask & (aligned_ts >> ((int)sizeof(TSType) - i - 1) * 8)));
    }

    // Paper says 14 bits, but, 2 bits of a difference and lack of
    // need for shifting everything sounds like an advantage to me.
    for (int i = 0; i < 2; i++) {
        data_.push_back((mask & (delta >> (2 - i - 1) * 8)));
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
        aligned_timestamp |= (data_[i] << (ts_size - 1 - i) * 8);
    }
    int delta_size = 2;
    std::uint16_t delta = 0;
    for (int i = ts_size; i < ts_size + delta_size; i++) {
        delta |= (data_[i] << (2 - 1 - i) * 8);
    }
    int offset = ts_size + delta_size;
    std::uint8_t data[sizeof(ValType)];
    std::copy(data_.begin() + offset, data_.begin() + offset + (int)sizeof(ValType), data);
    ValType val = *reinterpret_cast<ValType*>(&data);
    std::vector<std::pair<TSType, ValType>> unpacked{{aligned_timestamp + delta, val}};
    return unpacked;
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

std::pair<std::vector<std::uint8_t>, int> BitAppend(int bit_offset, int number_of_bits, std::uint64_t value, std::uint8_t initial_byte) {
    std::vector<std::uint8_t> output;
    std::uint8_t byte = initial_byte;
    while (number_of_bits > 0) {
        int offset = number_of_bits - 8 + bit_offset;
        if (offset > 0) {
            byte |= (value >> offset) & 0xFF;
        } else {
            byte |= (value << -offset) & 0xFF;
        }
        output.push_back(byte);
        number_of_bits -= (8 - bit_offset);
        byte = 0;
        bit_offset = 0;
    }
    bit_offset = (8 + number_of_bits) % 8;
    return {output, bit_offset};
}

void EncodedDataBlock::Append(TSType timestamp, ValType val) {
    int delta = timestamp - last_ts_;
    int delta_of_delta = delta - last_ts_delta_;
    last_ts_delta_ = delta;
    last_ts_ = timestamp;
    int number_of_bits;
    std::cout << "Delta of delta " << delta_of_delta << "\n";
    std::uint32_t mask;
    std::uint64_t output = 0;
    if (delta_of_delta == 0) {
        auto encoding = 0b0;
        number_of_bits = 1;
    } else if (delta_of_delta >= -63 && delta_of_delta <= 64) {
        auto encoding = 0b10;
        mask = 0b1111111;
        output = encoding << 7 | (delta_of_delta & mask);
        number_of_bits = 9;
    } else if (delta_of_delta >= -255 && delta_of_delta <= 256) {
        auto encoding = 0b110;
        mask = 0b111111111;
        output = encoding << 9 | (delta_of_delta & mask);
        number_of_bits = 12;
    } else if (delta_of_delta >= -2047 && delta_of_delta <= 2048) {
        auto encoding = 0b1110;
        mask = 0b111111111111;
        output = encoding << 12 | (delta_of_delta & mask);
        number_of_bits = 16;
    } else {
        auto encoding = 0b1111;
        mask = 0xFFFFFFFF;
        output = encoding << 32 | (delta_of_delta & mask);
        number_of_bits = 32 + 4;
    }
    std::uint8_t initial_byte = 0;
    if (data_end_offset_ > 0) {
        initial_byte = data_.back();
        data_.pop_back();
    }
    std::cout << "Val to be appended " << std::bitset<64>(output) << "\n";
    auto output_pair = BitAppend(data_end_offset_, number_of_bits, output, initial_byte);
    data_.insert(data_.end(), output_pair.first.begin(), output_pair.first.end());
    data_end_offset_ = output_pair.second;
}

// Encoding aligned. And later shifting across bytes.


} // namespace compression
