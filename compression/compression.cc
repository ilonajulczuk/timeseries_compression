
#include <vector>
#include <utility>
#include "compression.h"


namespace compression {

const int kMaxTimeLengthOfBlockSecs = 2 * 60 * 60;

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

    return std::vector<std::pair<TSType, ValType>>();
}

bool EncodedDataBlock::WithinRange(TSType timestamp) {
    return timestamp - start_ts_ < kMaxTimeLengthOfBlockSecs;
}

void EncodedDataBlock::Append(TSType timestamp, ValType val) {

}

EncodedDataBlock::EncodedDataBlock(TSType timestamp, ValType val):
 last_ts_(timestamp), last_val_(val) {
    // Align timestamp to the epoch and figure out what the delta is.
    // store the last delta.
    // Encode stuff correctly.
    // Set up offset end of encoded data.
}

}
