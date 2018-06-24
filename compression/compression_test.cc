#include "compression.h"
#include "gtest/gtest.h"


TEST(CompressionTest, CountOfEncodedElements) {
  compression::Encoder encoder{};
  encoder.Append(2 * 60 * 60 + 5, 6.66);
  encoder.Append(6, 7.66);
  encoder.Append(7, 8.66);
  auto ts_data = encoder.Decode();
  EXPECT_EQ(1U, ts_data.size());
  EXPECT_EQ(2 * 60 * 60 + 5U, ts_data[0].first);
  EXPECT_FLOAT_EQ(6.66, ts_data[0].second);
}