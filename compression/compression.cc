
#include <vector>
#include <utility>
#include <iostream>
#include "compression.h"
#include <bitset>
#include <map>


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
 last_ts_(timestamp), last_val_(val), last_xor_leading_zeros_(-1), last_xor_meaningful_bits_(-1)  {
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
}


std::uint8_t TailMask(int tail_size) {
    return 0xFF >> (8 - tail_size);
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
    offset += (int)sizeof(ValType);

    int last_delta = delta;
    TSType last_timestamp = aligned_timestamp + delta;
    // I should be alternating now between timestamps and values.
    // Values are not encoded yet, so I will assume zeros.
    unsigned int byte_offset = offset;
    int bit_offset = 0;

    std::map<int, int> len_to_sequence = {
        {1, 0b0},
        {2, 0b10},
        {3, 0b110},
        {4, 0b1110},
        {4, 0b1111}
    };

    std::map<int, int> sequence_to_num_bits = {
        {0b0, 0},
        {0b10, 7},
        {0b110, 9},
        {0b1110, 12},
        {0b1111, 32}
    };

    while (byte_offset < data_.size()) {
        std::cout << "Byte offset: " << byte_offset << ", bit offset: " << bit_offset << "\n";
        std::cout << "Current byte: " << std::bitset<8>(data_[byte_offset]) << "\n";

        if ((byte_offset == data_.size() - 1) && bit_offset >= data_end_offset_) {
            break;
        }

        for (auto p: len_to_sequence) {
            auto len_of_sequence = p.first;
            auto sequence = p.second;
            int shift = 8 - len_of_sequence - bit_offset;
            int number = 0;
            if (shift > 0) { // no caryover!
                std::cout << "No carryover\n";
                number = data_[byte_offset] >> shift & TailMask(len_of_sequence);
            } else {
                std::cout << "Carryover\n";
                if (byte_offset + 1 >= data_.size()) {
                    break;
                }
                number = (data_[byte_offset] & TailMask(len_of_sequence + shift)) << -shift;
                int carry_shift = 8 - len_of_sequence;
                int carry_over_number = (data_[byte_offset + 1] >> carry_shift) & TailMask(-shift);
                number += carry_over_number;
            }
            if (number == sequence) {
                std::cout << "Matched the sequence: " << sequence << "\n";
                bit_offset += len_of_sequence;
                if (bit_offset > 7) {
                    bit_offset -= 8;
                    byte_offset += 1;
                }
                std::int64_t encoded_delta_of_delta = 0;
                int num_bits = sequence_to_num_bits[sequence];
                int num_bits_outstanding = num_bits;
                while(byte_offset < data_.size() && num_bits_outstanding > 0) {
                    shift = 8 - num_bits_outstanding - bit_offset;
                    if (shift > 0) { // no caryover!
                        encoded_delta_of_delta += data_[byte_offset] >> shift & TailMask(num_bits_outstanding);
                        bit_offset += num_bits_outstanding;
                        num_bits_outstanding = 0;
                    } else {
                        encoded_delta_of_delta += (data_[byte_offset] & TailMask(8 - bit_offset)) << -shift;
                        num_bits_outstanding -= 8 - bit_offset;
                        bit_offset += (8 - bit_offset);
                    }
                    if (bit_offset >= 8) {
                        bit_offset = bit_offset % 8;
                        byte_offset++;
                    }
                }
                std::cout << "Encoded dd: " << std::bitset<64>(encoded_delta_of_delta) << "\n";
                if ((encoded_delta_of_delta & (0b1 << (num_bits - 1))) > 0) {
                    std::cout << "Negatifying the val" << "\n";
                    encoded_delta_of_delta |= (0xFFFFFFFFFFFFFFFF << num_bits);
                }
                delta = last_delta + encoded_delta_of_delta;
                TSType timestamp = last_timestamp + delta;

                std::cout << "Encoded delta of delta is: " << encoded_delta_of_delta << "\n";
                std::cout << "Delta is: " << delta << "\n";
                unpacked.push_back({timestamp, 0});
                last_delta = delta;
                last_timestamp = timestamp;
                break;
            } else {
                std::cout << "Sequence not matched, number: " << number << ", sequence " << p.second << "\n";
            }
        }
    }

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
        int shift = number_of_bits - 8 + bit_offset;
        if (shift > 0) {
            byte |= (value >> shift) & 0xFF;
        } else {
            byte |= (value << -shift) & 0xFF;
        }
        output.push_back(byte);
        number_of_bits -= (8 - bit_offset);
        byte = 0;
        bit_offset = 0;
    }
    bit_offset = (8 + number_of_bits) % 8;
    return {output, bit_offset};
}

void EncodedDataBlock::EncodeTS(TSType timestamp) {
    int delta = timestamp - last_ts_;
    int delta_of_delta = delta - last_ts_delta_;
    last_ts_delta_ = delta;
    last_ts_ = timestamp;
    int number_of_bits;
    std::cout << "Delta of delta " << delta_of_delta << "\n";
    std::uint32_t mask;
    std::uint64_t output = 0;
    std::uint64_t encoding = 0;
    if (delta_of_delta == 0) {
        number_of_bits = 1;
    } else if (delta_of_delta >= -63 && delta_of_delta <= 64) {
        encoding = 0b10;
        mask = 0b1111111;
        output = encoding << 7 | (delta_of_delta & mask);
        number_of_bits = 9;
    } else if (delta_of_delta >= -255 && delta_of_delta <= 256) {
        encoding = 0b110;
        mask = 0b111111111;
        output = encoding << 9 | (delta_of_delta & mask);
        number_of_bits = 12;
    } else if (delta_of_delta >= -2047 && delta_of_delta <= 2048) {
        encoding = 0b1110;
        mask = 0b111111111111;
        output = encoding << 12 | (delta_of_delta & mask);
        number_of_bits = 16;
    } else {
        encoding = 0b1111;
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

void EncodedDataBlock::EncodeVal(ValType val) {
    std::uint64_t xored = (std::uint64_t)val ^ (std::uint64_t)last_val_;
    if (xored == 0) {
        // store 0 and return

    }

    // store 1
    int leading_zero_bits = LeadingZeroBits(xored);
    int trailing_zero_bits = TrailingZeroBits(xored);
    int meaningful_bits = 64 - leading_zero_bits - trailing_zero_bits;
    
    if (last_xor_leading_zeros_ != -1) {
        if (leading_zero_bits == last_xor_leading_zeros_ &&
         last_xor_meaningful_bits_ == meaningful_bits) {
             // use same scheme as before.
             // 0 for encoding
         }
    }
    // 1 for encoding and more fun
}

void EncodedDataBlock::Append(TSType timestamp, ValType val) {
    EncodeTS(timestamp);
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
// What do I need for the value encoding.
// Count leading zeroes.
// Count trailing zeroes.
} // namespace compression
