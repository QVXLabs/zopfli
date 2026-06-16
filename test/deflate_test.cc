#include "zopfli_c_api.h"

#include <algorithm>

#include "gtest/gtest.h"
namespace {

using zopfli_test::Output;

std::vector<unsigned char> Deflate(int btype,
                                   const std::vector<unsigned char>& in) {
  ZopfliOptions options;
  ZopfliInitOptions(&options);
  Output out;
  unsigned char bp = 0;
  ZopfliDeflate(&options, btype, 1, in.data(), in.size(), &bp, out.out(),
                out.size_ptr());
  return out.bytes();
}

TEST(Deflate, AllBlockTypesProduceOutput) {
  std::vector<unsigned char> in(500, 'a');
  EXPECT_FALSE(Deflate(0, in).empty());  // uncompressed
  EXPECT_FALSE(Deflate(1, in).empty());  // fixed Huffman
  EXPECT_FALSE(Deflate(2, in).empty());  // dynamic Huffman
}

TEST(Deflate, BlockSizeAutoTypePicksSmallest) {
  std::vector<unsigned char> in(4000, 'z');
  ZopfliLZ77Store store;
  zopfli_test::GreedyStore(in, &store);

  uint32_t s0 = ZopfliCalculateBlockSize(&store, 0, store.size, 0);
  uint32_t s1 = ZopfliCalculateBlockSize(&store, 0, store.size, 1);
  uint32_t s2 = ZopfliCalculateBlockSize(&store, 0, store.size, 2);
  uint32_t best = ZopfliCalculateBlockSizeAutoType(&store, 0, store.size);

  EXPECT_GT(s0, 0u);
  EXPECT_GT(s1, 0u);
  EXPECT_GT(s2, 0u);
  uint32_t min3 = std::min(s0, std::min(s1, s2));
  EXPECT_EQ(best, min3);

  ZopfliCleanLZ77Store(ZopfliDefaultContext(), &store);
}

TEST(Deflate, UseExpensiveFixedHeuristic) {
  // Small blocks always qualify regardless of cost.
  EXPECT_TRUE(ZopfliUseExpensiveFixed(500, 1000000u, 1u));

  // Large block, fixed within 1.1x of dynamic -> worthwhile.
  EXPECT_TRUE(ZopfliUseExpensiveFixed(2000, 100u, 100u));
  // Large block, fixed far above 1.1x of dynamic -> not worthwhile.
  EXPECT_FALSE(ZopfliUseExpensiveFixed(2000, 1000u, 100u));

#if !(ZOPFLI_MASTER_BLOCK_SIZE != 0 && (ZOPFLI_MASTER_BLOCK_SIZE * 32 * 11 <= 0xFFFFFFFF))
  // Only reachable where the 64-bit path is compiled (master blocks disabled or
  // large): both costs are within the < 2^31 bit invariant, but fixedcost*10
  // and dyncost*11 exceed UINT32_MAX. Fixed (~4.29e9) is far above 1.1x dynamic
  // (1.1e9), so the answer is false; a 32-bit compare would wrap fixedcost*10
  // to 4 (<= dyncost*11) and flip it to true.
  const uint32_t fixedcost = 429496730u;  // *10 = UINT32_MAX + 4, wraps to 4
  const uint32_t dyncost = 100000000u;    // *11 = 1.1e9, no wrap
  EXPECT_FALSE(ZopfliUseExpensiveFixed(2000, fixedcost, dyncost));
#endif
}

}  // namespace
