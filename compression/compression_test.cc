#include "compression.h"
#include "gtest/gtest.h"


TEST(CompressionTest, CountOfEncodedElements) {
  compression::Encoder encoder{};
  encoder.Append(5, 6.66);
  encoder.Append(6, 7.66);
  encoder.Append(7, 8.66);
  auto ts_data = encoder.Decode();
  EXPECT_EQ(3U, ts_data.size());
}