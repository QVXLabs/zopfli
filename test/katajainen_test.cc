#include "zopfli_c_api.h"

#include <cmath>

#include "gtest/gtest.h"

namespace {

double Kraft(const unsigned* bitlengths, const size_t* freqs, int n) {
  double sum = 0.0;
  for (int i = 0; i < n; i++) {
    if (freqs[i] == 0) continue;
    sum += std::ldexp(1.0, -static_cast<int>(bitlengths[i]));
  }
  return sum;
}

TEST(Katajainen, ZeroSymbols) {
  unsigned bitlengths[1] = {0};
  EXPECT_EQ(ZopfliLengthLimitedCodeLengths(nullptr, 0, 15, bitlengths), 0);
}

TEST(Katajainen, OneSymbolGetsLengthOne) {
  size_t freqs[1] = {5};
  unsigned bitlengths[1] = {0};
  EXPECT_EQ(ZopfliLengthLimitedCodeLengths(freqs, 1, 15, bitlengths), 0);
  EXPECT_EQ(bitlengths[0], 1u);
}

TEST(Katajainen, TwoSymbols) {
  size_t freqs[2] = {1, 1};
  unsigned bitlengths[2] = {0};
  EXPECT_EQ(ZopfliLengthLimitedCodeLengths(freqs, 2, 15, bitlengths), 0);
  EXPECT_EQ(bitlengths[0], 1u);
  EXPECT_EQ(bitlengths[1], 1u);
}

TEST(Katajainen, ManySymbolsSatisfyKraft) {
  size_t freqs[8] = {1, 2, 3, 5, 8, 13, 21, 34};
  unsigned bitlengths[8] = {0};
  EXPECT_EQ(ZopfliLengthLimitedCodeLengths(freqs, 8, 15, bitlengths), 0);
  EXPECT_LE(Kraft(bitlengths, freqs, 8), 1.0 + 1e-9);
  for (int i = 0; i < 8; i++) EXPECT_LE(bitlengths[i], 15u);
}

TEST(Katajainen, MaxBitsTooSmallIsError) {
  // 5 symbols cannot be coded with a max length of 2 bits.
  size_t freqs[5] = {1, 1, 1, 1, 1};
  unsigned bitlengths[5] = {0};
  EXPECT_EQ(ZopfliLengthLimitedCodeLengths(freqs, 5, 2, bitlengths), 1);
}

TEST(Katajainen, ZeroFrequencySymbolsIgnored) {
  size_t freqs[5] = {0, 4, 0, 2, 1};
  unsigned bitlengths[5] = {0};
  EXPECT_EQ(ZopfliLengthLimitedCodeLengths(freqs, 5, 7, bitlengths), 0);
  EXPECT_EQ(bitlengths[0], 0u);
  EXPECT_EQ(bitlengths[2], 0u);
}

}  // namespace
