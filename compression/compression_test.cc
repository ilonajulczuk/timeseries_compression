#include "compression.h"
#include "gtest/gtest.h"


TEST(CompressionTest, CountOfEncodedElements) {
  compression::Encoder encoder{};
  encoder.Append(2 * 60 * 60 + 5, 6.66);
  encoder.PrintBinData();
  encoder.Append(2 * 60 * 60 + 11, 7.66);
  encoder.PrintBinData();
  encoder.Append(2 * 60 * 60 + 13, 8.66);
  encoder.PrintBinData();
  encoder.Append(2 * 60 * 60 + 313, 8.66);
  encoder.PrintBinData();
  encoder.Append(2 * 60 * 60 + 613, 8.66);
  encoder.PrintBinData();
  encoder.Append(2 * 60 * 60 + 713, 8.66);
  encoder.PrintBinData();
  encoder.Append(2 * 60 * 60 + 816, 8.66);
  encoder.PrintBinData();
  encoder.Append(2 * 60 * 60 + 913, 8.66);
  auto ts_data = encoder.Decode();
  EXPECT_EQ(8U, ts_data.size());
  EXPECT_EQ(2 * 60 * 60 + 5U, ts_data[0].first);
  //EXPECT_FLOAT_EQ(6.66, ts_data[0].second);
  EXPECT_EQ(2 * 60 * 60 + 11U, ts_data[1].first);
  //EXPECT_FLOAT_EQ(7.66, ts_data[1].second);
  EXPECT_EQ(2 * 60 * 60 + 13U, ts_data[2].first);
  EXPECT_EQ(2 * 60 * 60 + 313U, ts_data[3].first);
  EXPECT_EQ(2 * 60 * 60 + 613U, ts_data[4].first);
  EXPECT_EQ(2 * 60 * 60 + 713U, ts_data[5].first);
  EXPECT_EQ(2 * 60 * 60 + 816U, ts_data[6].first);
  EXPECT_EQ(2 * 60 * 60 + 913U, ts_data[7].first);
  encoder.PrintBinData();
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
