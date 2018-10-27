#include <vector>
#include <utility>
#include "compression.h"

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

TSType AlignTS(TSType timestamp) {
    // 2h blocks aligned to epoch.
    return timestamp - (timestamp % (2 * 60 * 60));
}

DataIterator::DataIterator():
 data_(nullptr), byte_offset_(0), bit_offset_(0), current_size_(0) {
 }

DataIterator::DataIterator(std::vector<std::uint8_t>* data):
 data_(data), byte_offset_(0), bit_offset_(0), current_size_(0) {
 }

DataIterator::DataIterator(std::vector<std::uint8_t>* data, int byte_offset, int bit_offset):
 data_(data), byte_offset_(byte_offset), bit_offset_(bit_offset), current_size_(0) {
 }

DataIterator& DataIterator::DataIterator::operator++() {
    if (byte_offset_ >= data_->size()) {
        throw ParsingError("trying to read outside of data, most likely corrupted format");
    }
    if (current_size_ == 0) {
        ReadPair();
    }
    UpdateOffsets();
    return *this;
}

DataIterator DataIterator::operator++(int) {
    DataIterator tmp = *this;
    if (byte_offset_ >= data_->size()) {
        throw ParsingError("trying to read outside of data, most likely corrupted format");
    }
    if (current_size_ == 0) {
        ReadPair();
    }
    UpdateOffsets();
    return tmp;
}

std::pair<TSType, ValType>& DataIterator::operator*() {
    if (byte_offset_ >= data_->size()) {
        throw ParsingError("trying to read outside of data, most likely corrupted format");
    }
    if (!current_size_) {
        ReadPair();
    }
    return current_pair_;
}

bool DataIterator::operator==(const DataIterator& rhs) {
    return (byte_offset_ == rhs.byte_offset_) && (bit_offset_ == rhs.bit_offset_);
}

bool DataIterator::operator!=(const DataIterator& rhs) {
    return !(*this == rhs);
}

void DataIterator::UpdateOffsets() {
    bit_offset_ += current_size_;
    byte_offset_ += (bit_offset_ - (bit_offset_ % 8)) / 8;
    bit_offset_ %= 8;
    current_size_ = 0;
}

void DataIterator::ReadPair() {
    if (byte_offset_ == 0 && bit_offset_ == 0) {
        TSType aligned_timestamp = ReadBits(64, 0, 0, *data_);
        std::uint16_t delta = ReadBits(16, 8, 0, *data_);
        ValType val = DoubleFromInt(ReadBits(64, 10, 0, *data_));

        TSType timestamp = aligned_timestamp + delta;
        last_timestamp_ = timestamp;
        last_val_ = val;
        last_val_ = val;
        last_delta_ = delta;

        current_size_ = 64 + 16 + 64;
        current_pair_ = {timestamp, val};
        return;
    }

    TSType timestamp = 0;
    ValType val = 0;
    int delta = 0;
    unsigned int byte_offset = byte_offset_;
    int bit_offset = bit_offset_;

    bool matched = false;
    for (auto p: kLenToSequenceTS) {
        auto len_of_sequence = p.first;
        auto sequence = p.second;
        auto number = ReadBits(len_of_sequence, byte_offset, bit_offset, *data_);
        if (number == sequence) {
            matched = true;
            bit_offset += len_of_sequence;
            if (bit_offset > 7) {
                bit_offset -= 8;
                byte_offset += 1;
            }

            int num_bits = kSequenceToNumBits[sequence];
            std::int64_t encoded_delta_of_delta = ReadBits(num_bits, byte_offset, bit_offset, *data_);
            bit_offset += num_bits;
            byte_offset += (bit_offset - (bit_offset % 8)) / 8;
            bit_offset %= 8;
            if ((encoded_delta_of_delta & (0b1 << (num_bits - 1))) > 0) {
                encoded_delta_of_delta |= (0xFFFFFFFFFFFFFFFF << num_bits);
            }
            delta = last_delta_ + encoded_delta_of_delta;
            timestamp = last_timestamp_ + delta;
            last_delta_ = delta;
            last_timestamp_ = timestamp;
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
        number = ReadBits(len_of_sequence, byte_offset, bit_offset, *data_);
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
            auto xored_shifted = ReadBits(last_xor_meaningful_bits_, byte_offset, bit_offset, *data_);
            bit_offset += last_xor_meaningful_bits_;
            byte_offset += (bit_offset - (bit_offset % 8)) / 8;
            bit_offset %= 8;
            auto xored = xored_shifted << (64 - last_xor_meaningful_bits_ - last_xor_leading_zeros_);
            val = DoubleFromInt(DoubleAsInt(last_val_) ^ xored);
            break;
        }
        case 3: {
            int num_bits = 6;
            auto leading_zeros = ReadBits(num_bits, byte_offset, bit_offset, *data_);
            bit_offset += num_bits;
            byte_offset += (bit_offset - (bit_offset % 8)) / 8;
            bit_offset %= 8;

            num_bits = 6;
            auto meaningful_bits = ReadBits(num_bits, byte_offset, bit_offset, *data_);
            bit_offset += num_bits;
            byte_offset += (bit_offset - (bit_offset % 8)) / 8;
            bit_offset %= 8;

            num_bits = meaningful_bits;
            std::uint64_t xored_shifted = ReadBits(num_bits, byte_offset, bit_offset, *data_);
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
    current_size_ = (byte_offset - byte_offset_) * 8 + bit_offset - bit_offset_;
    current_pair_ = {timestamp, val};
}


EncodedDataBlock::iterator EncodedDataBlock::begin() {
    return iterator(&data_);
}

EncodedDataBlock::iterator EncodedDataBlock::end() {
    int end_byte_offset = data_end_offset_ ? data_.size() -1: data_.size();
    return iterator(&data_, end_byte_offset, data_end_offset_);
}

EncodedDataBlock::EncodedDataBlock(TSType timestamp, ValType val):
    last_ts_(timestamp),
    last_val_(val),
    last_xor_leading_zeros_(-1), 
    last_xor_meaningful_bits_(-1),
    data_end_offset_(0) {
    // Align timestamp to the epoch and figure out what the delta is.
    auto aligned_ts = AlignTS(timestamp);
    std::uint16_t delta = timestamp - aligned_ts;
    last_ts_delta_ = delta;
    last_ts_ = timestamp;
    start_ts_ = aligned_ts;

    AppendBits(64, aligned_ts);
    AppendBits(16, delta);
    AppendBits(64, DoubleAsInt(val));
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
    std::vector<std::pair<TSType, ValType>> output;
    std::copy(begin(), end(), std::back_inserter(output));
    return output;
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
    AppendBits(number_of_bits, output);
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

EncoderIterator Encoder::begin() {
    return iterator(&blocks_);
}

EncoderIterator Encoder::end() {
    return iterator(&blocks_, true);
}

EncoderIterator::EncoderIterator(std::vector<EncodedDataBlock*>* blocks, bool end) :
blocks_(blocks) {
    if (end) {
        current_block_it_= (*blocks_)[blocks->size() - 1]->end();
        current_block_end_ = current_block_it_;
        pos_ = blocks_->size();
    } else {
        pos_ = 0;
        current_block_it_ = (*blocks_)[0]->begin();
        current_block_end_ = (*blocks_)[0]->end();
    }
}


EncoderIterator& EncoderIterator::EncoderIterator::operator++() {
    current_block_it_++;
    if (current_block_it_ == current_block_end_) {
        pos_++;
        if (pos_ < blocks_->size()) {
            current_block_it_ = (*blocks_)[pos_]->begin();
            current_block_end_ = (*blocks_)[pos_]->end();
        }
    }
    return *this;
}

EncoderIterator EncoderIterator::operator++(int) {
    EncoderIterator tmp = *this;
    current_block_it_++;
    if (current_block_it_ == current_block_end_) {
        pos_++;
        if (pos_ < blocks_->size()) {
            current_block_it_ = (*blocks_)[pos_]->begin();
            current_block_end_ = (*blocks_)[pos_]->end();
        }
    }
    return tmp;
}

std::pair<TSType, ValType>& EncoderIterator::operator*() {
    return *current_block_it_;
}

bool EncoderIterator::operator==(const EncoderIterator& rhs) {
    return (blocks_ == rhs.blocks_) && (current_block_it_ == rhs.current_block_it_) && (pos_ == rhs.pos_);
}

bool EncoderIterator::operator!=(const EncoderIterator& rhs) {
    return !(*this == rhs);
}

} // namespace compression
