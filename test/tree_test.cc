#include "zopfli_c_api.h"

#include <cmath>

#include "gtest/gtest.h"

namespace {

TEST(Tree, BitLengthsRespectMaxAndKraft) {
  size_t counts[6] = {0, 1, 2, 4, 8, 16};
  unsigned bitlengths[6] = {0};
  ZopfliCalculateBitLengths(counts, 6, 15, bitlengths);
  double kraft = 0.0;
  for (int i = 0; i < 6; i++) {
    if (counts[i] == 0) continue;
    EXPECT_GT(bitlengths[i], 0u);
    EXPECT_LE(bitlengths[i], 15u);
    kraft += std::ldexp(1.0, -static_cast<int>(bitlengths[i]));
  }
  EXPECT_LE(kraft, 1.0 + 1e-9);
}

TEST(Tree, LengthsToSymbolsAreCanonical) {
  // Two symbols of length 1: canonical codes are 0 and 1.
  const unsigned lengths[2] = {1, 1};
  unsigned symbols[2] = {0xffff, 0xffff};
  ZopfliLengthsToSymbols(lengths, 2, 15, symbols);
  EXPECT_EQ(symbols[0], 0u);
  EXPECT_EQ(symbols[1], 1u);
}

TEST(Tree, LengthsToSymbolsZeroLengthGetsNoCode) {
  const unsigned lengths[3] = {0, 1, 1};
  unsigned symbols[3] = {7, 7, 7};
  ZopfliLengthsToSymbols(lengths, 3, 15, symbols);
  EXPECT_EQ(symbols[0], 0u);  // zero-length symbol assigned 0
}

TEST(Tree, Entropy) {
  size_t counts[4] = {2, 2, 2, 2};
  double bitlengths[4] = {0};
  ZopfliCalculateEntropy(counts, 4, bitlengths);
  // Uniform distribution over 4 symbols => 2 bits each.
  for (int i = 0; i < 4; i++) EXPECT_NEAR(bitlengths[i], 2.0, 1e-9);
}

TEST(Tree, EntropyZeroCountClamped) {
  size_t counts[3] = {0, 0, 5};
  double bitlengths[3] = {0};
  ZopfliCalculateEntropy(counts, 3, bitlengths);
  for (int i = 0; i < 3; i++) EXPECT_GE(bitlengths[i], 0.0);
}

}  // namespace
