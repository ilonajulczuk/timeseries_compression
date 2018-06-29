#include "compression.h"
#include "gtest/gtest.h"


TEST(CompressionTest, CountOfEncodedElements) {
  compression::Encoder encoder{};
  encoder.Append(2 * 60 * 60 + 5, 6.66);
  encoder.Append(2 * 60 * 60 + 6, 7.66);
  encoder.Append(2 * 60 * 60 + 7, 8.66);
  auto ts_data = encoder.Decode();
  EXPECT_EQ(3U, ts_data.size());
  EXPECT_EQ(2 * 60 * 60 + 5U, ts_data[0].first);
  EXPECT_FLOAT_EQ(6.66, ts_data[0].second);
  EXPECT_EQ(2 * 60 * 60 + 6U, ts_data[1].first);
  EXPECT_FLOAT_EQ(7.66, ts_data[1].second);
}

TEST(CompressionTest, TestEncodingDecodingDeltasOfDeltas) {
  // Test cases:
  // TODO...

}