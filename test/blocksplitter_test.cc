#include "zopfli_c_api.h"

#include "gtest/gtest.h"

namespace {

TEST(BlockSplitter, Simple) {
  std::vector<unsigned char> in(1000, 'a');
  size_t* splitpoints = nullptr;
  size_t npoints = 0;
  ZopfliBlockSplitSimple(in.data(), 0, in.size(), 100, &splitpoints,
                         &npoints);
  // Boundaries at 0, 100, ..., 900 => 10 points.
  EXPECT_EQ(npoints, 10u);
  free(splitpoints);
}

TEST(BlockSplitter, SplitMixedInput) {
  // Two dissimilar halves so the cost model wants a split between them.
  std::vector<unsigned char> in;
  for (int i = 0; i < 20000; i++) in.push_back('a');
  std::vector<unsigned char> rnd = zopfli_test::PseudoRandom(20000, 3);
  in.insert(in.end(), rnd.begin(), rnd.end());

  ZopfliOptions options;
  ZopfliInitOptions(&options);
  size_t* splitpoints = nullptr;
  size_t npoints = 0;
  ZopfliBlockSplit(&options, in.data(), 0, in.size(), 0, &splitpoints,
                   &npoints);
  // Split points must be strictly increasing and within range.
  size_t prev = 0;
  for (size_t i = 0; i < npoints; i++) {
    EXPECT_GT(splitpoints[i], prev);
    EXPECT_LT(splitpoints[i], in.size());
    prev = splitpoints[i];
  }
  free(splitpoints);
}

TEST(BlockSplitter, SplitLZ77RespectsMaxBlocks) {
  std::vector<unsigned char> in;
  for (int i = 0; i < 30000; i++) in.push_back(static_cast<unsigned char>(i));
  ZopfliLZ77Store store;
  zopfli_test::GreedyStore(in, &store);

  ZopfliOptions options;
  ZopfliInitOptions(&options);
  const size_t maxblocks = 3;
  size_t* splitpoints = nullptr;
  size_t npoints = 0;
  ZopfliBlockSplitLZ77(&options, &store, maxblocks, &splitpoints, &npoints);
  // maxblocks blocks => at most maxblocks-1 interior split points.
  EXPECT_LE(npoints, maxblocks - 1);

  free(splitpoints);
  ZopfliCleanLZ77Store(&store);
}

}  // namespace
