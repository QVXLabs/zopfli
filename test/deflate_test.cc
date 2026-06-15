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

  ZopfliCleanLZ77Store(&store);
}

}  // namespace
