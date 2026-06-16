#include "zopfli_c_api.h"

#include <cmath>

#include "gtest/gtest.h"

namespace {

TEST(Tree, BitLengthsRespectMaxAndKraft) {
  size_t counts[6] = {0, 1, 2, 4, 8, 16};
  unsigned bitlengths[6] = {0};
  ZopfliCalculateBitLengths(ZopfliDefaultContext(), counts, 6, 15, bitlengths);
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
  ZopfliLengthsToSymbols(ZopfliDefaultContext(), lengths, 2, 15, symbols);
  EXPECT_EQ(symbols[0], 0u);
  EXPECT_EQ(symbols[1], 1u);
}

TEST(Tree, LengthsToSymbolsZeroLengthGetsNoCode) {
  const unsigned lengths[3] = {0, 1, 1};
  unsigned symbols[3] = {7, 7, 7};
  ZopfliLengthsToSymbols(ZopfliDefaultContext(), lengths, 3, 15, symbols);
  EXPECT_EQ(symbols[0], 0u);  // zero-length symbol assigned 0
}

TEST(Tree, Entropy) {
  size_t counts[4] = {2, 2, 2, 2};
  uint32_t bitlengths[4] = {0};
  ZopfliCalculateEntropy(counts, 4, bitlengths, 16);
  // Uniform over 4 symbols => 2 bits each; Q16 fixed point = 2 << 16 (exact:
  // log2(8) - log2(2) = 3 - 1, both powers of two).
  for (int i = 0; i < 4; i++) EXPECT_EQ(bitlengths[i], 2u << 16);
}

TEST(Tree, EntropyZeroCount) {
  size_t counts[3] = {0, 0, 5};
  uint32_t bitlengths[3] = {0};
  ZopfliCalculateEntropy(counts, 3, bitlengths, 16);
  // The only present symbol costs 0 bits; absent symbols are costed as count 1,
  // i.e. log2(sum) > 0. Integer result is exactly >= 0 (no float clamp needed).
  EXPECT_EQ(bitlengths[2], 0u);
  EXPECT_GT(bitlengths[0], 0u);
  EXPECT_GT(bitlengths[1], 0u);
}

}  // namespace
