#include "compression.h"
#include "gtest/gtest.h"

namespace compression {

TEST(CompressionTest, TestEndToEnd) {
  compression::Encoder encoder{};
  encoder.Append(2 * 60 * 60 + 5, 6.666);
  encoder.PrintBinData();

  encoder.Append(2 * 60 * 60 + 11, 66.66);
  encoder.PrintBinData();
  encoder.Append(2 * 60 * 60 + 13, 8.66);
  encoder.PrintBinData();
  encoder.Append(2 * 60 * 60 + 313, 8.66);
  encoder.PrintBinData();
  encoder.Append(2 * 60 * 60 + 613, 7.21);
  encoder.PrintBinData();
  encoder.Append(2 * 60 * 60 + 713, 8.66);
  encoder.PrintBinData();
  encoder.Append(2 * 60 * 60 + 816, 8.66);
  encoder.PrintBinData();
  encoder.Append(2 * 60 * 60 + 913, 8.66);

  auto ts_data = encoder.Decode();
  EXPECT_EQ(8U, ts_data.size());
  for (auto pair : ts_data) {
    std::cout << pair.first << " " << pair.second << "\n";
  }
  EXPECT_EQ(2 * 60 * 60 + 5U, ts_data[0].first);
  EXPECT_FLOAT_EQ(6.666, ts_data[0].second);
  EXPECT_EQ(2 * 60 * 60 + 11U, ts_data[1].first);
  EXPECT_FLOAT_EQ(66.66, ts_data[1].second);

  EXPECT_EQ(2 * 60 * 60 + 13U, ts_data[2].first);
  EXPECT_FLOAT_EQ(8.66, ts_data[2].second);

  EXPECT_EQ(2 * 60 * 60 + 313U, ts_data[3].first);

  EXPECT_FLOAT_EQ(8.66, ts_data[3].second);
  EXPECT_EQ(2 * 60 * 60 + 613U, ts_data[4].first);

  EXPECT_FLOAT_EQ(7.21, ts_data[4].second);
  EXPECT_EQ(2 * 60 * 60 + 713U, ts_data[5].first);
  EXPECT_EQ(2 * 60 * 60 + 816U, ts_data[6].first);
  EXPECT_EQ(2 * 60 * 60 + 913U, ts_data[7].first);
}

TEST(BitAppend, TestIfAppendingBitsWorksCorrectly) {
  int bit_offset = 3;
  std::uint8_t initial_byte = 0b11100000;
  std::uint64_t value = 0b100000011;
  int number_of_bits = 9;
  auto output_pair = compression::BitAppend(bit_offset, number_of_bits, value, initial_byte);
  EXPECT_EQ(4, output_pair.second);
  compression::PrintBin(output_pair.first);
  std::vector<std::uint8_t> expected_vec {0b11110000, 0b00110000};
  EXPECT_EQ(expected_vec, output_pair.first);
}

TEST(BitAppend, TestIfAppendingBitsZeroOffset) {
  int bit_offset = 0;
  std::uint8_t initial_byte = 0;
  std::uint64_t value = 0b100000011;

  int number_of_bits = 9;
  auto output_pair = compression::BitAppend(bit_offset, number_of_bits, value, initial_byte);
  EXPECT_EQ(1, output_pair.second);
  compression::PrintBin(output_pair.first);
  std::vector<std::uint8_t> expected_vec {0b10000001, 0b10000000};
  EXPECT_EQ(expected_vec, output_pair.first);
}

TEST(ValEncoding, LeadingZeroes) {
  ASSERT_EQ(57, compression::LeadingZeroBits(0b1111011));
  ASSERT_EQ(45, compression::LeadingZeroBits(0b1111010000100100000));
  ASSERT_EQ(46, compression::LeadingZeroBits(0b111001001000010000));
  ASSERT_EQ(64, compression::LeadingZeroBits(0));
  ASSERT_EQ(0, compression::LeadingZeroBits(0xFFFFFFFFFFFFFFFF));
 }

 TEST(ValEncoding, TrailingZeroes) {
  ASSERT_EQ(0, compression::TrailingZeroBits(0b1111011));
  ASSERT_EQ(5, compression::TrailingZeroBits(0b1111010000100100000));
  ASSERT_EQ(4, compression::TrailingZeroBits(0b111001001000010000));
  ASSERT_EQ(64, compression::TrailingZeroBits(0));
  ASSERT_EQ(0, compression::TrailingZeroBits(0xFFFFFFFFFFFFFFFF));
 }

 TEST(Decoding, ReadBits) {
  std::vector<std::uint8_t> data = {
    0b11101101,
    0b00101000,
    0b00001111,
    0b00011010,
    0b01011111,
    0b00111101,
    0b01110001,
    0b00111000,
  };
  auto bits = ReadBits(54, 0, 7,data);
  PrintBin(bits);
  ASSERT_EQ(0b100101000000011110001101001011111001111010111000100111U, bits);
 }

 TEST(ValEncoding, DoubleIntConvertion) {
  ValType val = 6.666;
  auto as_int = DoubleAsInt(val);
  ASSERT_EQ(val, DoubleFromInt(as_int));
 }

 TEST(BlockIterator, IterateOverBlock) {
  compression::Encoder encoder{};
  encoder.Append(2 * 60 * 60 + 5, 6.666);

  encoder.Append(2 * 60 * 60 + 11, 66.66);
  encoder.Append(2 * 60 * 60 + 13, 8.66);
  encoder.Append(2 * 60 * 60 + 313, 8.66);
  encoder.Append(2 * 60 * 60 + 613, 7.21);
  encoder.Append(2 * 60 * 60 + 713, 8.66);
  encoder.Append(2 * 60 * 60 + 816, 8.66);
  encoder.Append(2 * 60 * 60 + 913, 8.66);
  for (auto pair : *encoder.blocks_[0]) {
    std::cout << pair.first << ": v : " << pair.second << "\n";
  }

 }
}