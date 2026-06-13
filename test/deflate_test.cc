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

  double s0 = ZopfliCalculateBlockSize(&store, 0, store.size, 0);
  double s1 = ZopfliCalculateBlockSize(&store, 0, store.size, 1);
  double s2 = ZopfliCalculateBlockSize(&store, 0, store.size, 2);
  double best = ZopfliCalculateBlockSizeAutoType(&store, 0, store.size);

  EXPECT_GT(s0, 0.0);
  EXPECT_GT(s1, 0.0);
  EXPECT_GT(s2, 0.0);
  double min3 = std::min(s0, std::min(s1, s2));
  EXPECT_NEAR(best, min3, 1e-6);

  ZopfliCleanLZ77Store(&store);
}

}  // namespace
