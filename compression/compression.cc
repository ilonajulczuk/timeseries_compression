
#include <vector>
#include <utility>
#include <iostream>
#include "compression.h"
#include <bitset>
#include <map>


namespace compression {

const int kMaxTimeLengthOfBlockSecs = 2 * 60 * 60;

std::map<int, unsigned int> kLenToSequenceTS = {
    {1, 0b0},
    {2, 0b10},
    {3, 0b110},
    {4, 0b1110},
    {4, 0b1111}
};

std::vector<std::pair<int, unsigned int>> kLenToSequenceVal = {
    {1, 0b0},
    {2, 0b10},
    {2, 0b11},
};

std::map<int, int> kSequenceToNumBits = {
    {0b0, 0},
    {0b10, 7},
    {0b110, 9},
    {0b1110, 12},
    {0b1111, 32}
};


std::uint64_t DoubleAsInt(ValType val) {
    return *reinterpret_cast<std::uint64_t*>(&val);
}

ValType DoubleFromInt(std::uint64_t int_encoded) {
    return *reinterpret_cast<ValType*>(&int_encoded);
}

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

void PrintBin(std::uint64_t data) {
    std::cout << "Values in binary:\n";
    std::cout << std::bitset<64>(data) << "\n";
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
    data_end_offset_ = 0;
}


std::uint8_t TailMask(int tail_size) {
    return 0xFF >> (8 - tail_size);
}


std::uint64_t ReadBits(int num_bits, unsigned int byte_offset, int bit_offset, std::vector<std::uint8_t>& data) {
    std::uint64_t encoded = 0;
    int num_bits_outstanding = num_bits;
    while(byte_offset < data.size() && num_bits_outstanding > 0) {
        int shift = 8 - num_bits_outstanding - bit_offset;
        if (shift > 0) { // no caryover!
            encoded += data[byte_offset] >> shift & TailMask(num_bits_outstanding);
            bit_offset += num_bits_outstanding;
            num_bits_outstanding = 0;
        } else {
            std::uint64_t dd = data[byte_offset] & TailMask(8 - bit_offset);
            encoded += dd << -shift;num_bits_outstanding -= 8 - bit_offset;
            bit_offset += (8 - bit_offset);
        }
        if (bit_offset >= 8) {
            bit_offset = bit_offset % 8;
            byte_offset++;
        }
    }
    // some sort of exception/error if requested to read more bits than available in the data stream?
    return encoded;
}

std::uint64_t EncodedDataBlock::ReadBits(int num_bits, unsigned int byte_offset, int bit_offset) {
    return compression::ReadBits(num_bits, byte_offset, bit_offset, data_);
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
    last_val_ = val;
    offset += (int)sizeof(ValType);

    int last_delta = delta;
    TSType last_timestamp = aligned_timestamp + delta;
    TSType timestamp = 0;
    unsigned int byte_offset = offset;
    int bit_offset = 0;
    while (byte_offset < data_.size()) {
        if ((byte_offset == data_.size() - 1) && bit_offset >= data_end_offset_) {
            break;
        }
        bool matched = false;
        for (auto p: kLenToSequenceTS) {
            auto len_of_sequence = p.first;
            auto sequence = p.second;
            auto number = ReadBits(len_of_sequence, byte_offset, bit_offset);
            if (number == sequence) {
                matched = true;
                bit_offset += len_of_sequence;
                if (bit_offset > 7) {
                    bit_offset -= 8;
                    byte_offset += 1;
                }

                int num_bits = kSequenceToNumBits[sequence];
                std::int64_t encoded_delta_of_delta = ReadBits(num_bits, byte_offset, bit_offset);
                bit_offset += num_bits;
                byte_offset += (bit_offset - (bit_offset % 8)) / 8;
                bit_offset %= 8;
                if ((encoded_delta_of_delta & (0b1 << (num_bits - 1))) > 0) {
                    encoded_delta_of_delta |= (0xFFFFFFFFFFFFFFFF << num_bits);
                }
                delta = last_delta + encoded_delta_of_delta;
                timestamp = last_timestamp + delta;
                last_delta = delta;
                last_timestamp = timestamp;
                break;
            }
        }
        if (!matched) {
            throw ParsingError("timestamp couldn't be decoded, sequence not matched");
        }
        matched = false;
        unsigned int number = 0;
        for (auto p: kLenToSequenceVal) {
            auto len_of_sequence = p.first;
            auto sequence = p.second;
            number = ReadBits(len_of_sequence, byte_offset, bit_offset);
            if (number == sequence) {
                bit_offset += len_of_sequence;
                if (bit_offset > 7) {
                    bit_offset -= 8;
                    byte_offset += 1;
                }
                matched = true;
                break;
            }
        }
        if (!matched) {
            throw ParsingError("value couldn't be decoded, sequence not matched");
        }
        switch (number) {
            case 0:
                val = last_val_;
                break;
            case 2: {
                auto xored_shifted = ReadBits(last_xor_meaningful_bits_, byte_offset, bit_offset);
                bit_offset += last_xor_meaningful_bits_;
                byte_offset += (bit_offset - (bit_offset % 8)) / 8;
                bit_offset %= 8;
                auto xored = xored_shifted << (64 - last_xor_meaningful_bits_ - last_xor_leading_zeros_);
                val = DoubleFromInt(DoubleAsInt(last_val_) ^ xored);
                break;
            }
            case 3: {
                int num_bits = 6;
                auto leading_zeros = ReadBits(num_bits, byte_offset, bit_offset);
                bit_offset += num_bits;
                byte_offset += (bit_offset - (bit_offset % 8)) / 8;
                bit_offset %= 8;

                num_bits = 6;
                auto meaningful_bits = ReadBits(num_bits, byte_offset, bit_offset);
                bit_offset += num_bits;
                byte_offset += (bit_offset - (bit_offset % 8)) / 8;
                bit_offset %= 8;

                num_bits = meaningful_bits;
                std::uint64_t xored_shifted = ReadBits(num_bits, byte_offset, bit_offset);
                bit_offset += num_bits;
                byte_offset += (bit_offset - (bit_offset % 8)) / 8;
                bit_offset %= 8;

                std::uint64_t xored = xored_shifted << (64 - meaningful_bits - leading_zeros);

                last_xor_leading_zeros_ = leading_zeros;
                last_xor_meaningful_bits_ = meaningful_bits;
                val = DoubleFromInt(DoubleAsInt(last_val_) ^ xored);
                break;
            }
            default:
                throw ParsingError("unknown sequence number while decoding the value " +
                 std::to_string(number));
        }
        last_val_ = val;
        unpacked.push_back({timestamp, val});
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
    auto output_pair = BitAppend(data_end_offset_, number_of_bits, output, initial_byte);
    data_.insert(data_.end(), output_pair.first.begin(), output_pair.first.end());
    data_end_offset_ = output_pair.second;
}

void EncodedDataBlock::AppendBits(int number_of_bits, std::uint64_t value) {
    std::uint8_t initial_byte = 0;
    if (data_end_offset_ > 0) {
        initial_byte = data_.back();
        data_.pop_back();
    }
    auto output_pair = BitAppend(data_end_offset_, number_of_bits, value, initial_byte);
    data_.insert(data_.end(), output_pair.first.begin(), output_pair.first.end());
    data_end_offset_ = output_pair.second;
}

std::uint64_t TrimToMeaningfulBits(std::uint64_t value, int leading_zeros, int trailing_zeros) {
    return value >> trailing_zeros;
}

void EncodedDataBlock::EncodeVal(ValType val) {
    std::uint64_t xored = DoubleAsInt(val) ^ DoubleAsInt(last_val_);
    if (xored == 0) {
        AppendBits(1, 0);
        return;
    }
    AppendBits(1, 1);

    int leading_zero_bits = LeadingZeroBits(xored);
    int trailing_zero_bits = TrailingZeroBits(xored);
    int meaningful_bits = 64 - leading_zero_bits - trailing_zero_bits;

    if (last_xor_leading_zeros_ != -1 && leading_zero_bits == last_xor_leading_zeros_ &&
        last_xor_meaningful_bits_ == meaningful_bits) {
        AppendBits(1, 0);
        AppendBits(meaningful_bits, TrimToMeaningfulBits(xored, leading_zero_bits, trailing_zero_bits));
    } else {
        AppendBits(1, 1);
        AppendBits(6, leading_zero_bits);
        AppendBits(6, meaningful_bits);
        AppendBits(meaningful_bits, TrimToMeaningfulBits(xored, leading_zero_bits, trailing_zero_bits));
    }
    last_val_ = val;
    last_xor_leading_zeros_ = leading_zero_bits;
    last_xor_meaningful_bits_ = meaningful_bits;
}


void EncodedDataBlock::Append(TSType timestamp, ValType val) {
    EncodeTS(timestamp);
    EncodeVal(val);
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

} // namespace compression
