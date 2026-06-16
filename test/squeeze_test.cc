#include "zopfli_c_api.h"

#include "gtest/gtest.h"

namespace {

std::vector<unsigned char> RunData() {
  return zopfli_test::Bytes(
      "the quick brown fox jumps over the lazy dog. "
      "the quick brown fox jumps over the lazy dog. "
      "the quick brown fox jumps over the lazy dog.");
}

TEST(Squeeze, OptimalFixed) {
  std::vector<unsigned char> in = RunData();
  ZopfliOptions options;
  ZopfliInitOptions(&options);
  ZopfliContext ctx;
  ctx.options = options;
  ZopfliBlockState s;
  ZopfliInitBlockState(&ctx, 0, in.size(), 1, &s);
  ZopfliLZ77Store store;
  ZopfliInitLZ77Store(in.data(), &store);
  ZopfliLZ77OptimalFixed(&s, in.data(), 0, in.size(), &store);
  EXPECT_GT(store.size, 0u);
  // The parse must reconstruct exactly the input byte length.
  EXPECT_EQ(ZopfliLZ77GetByteRange(&store, 0, store.size), in.size());
  ZopfliCleanLZ77Store(ZopfliDefaultContext(), &store);
  ZopfliCleanBlockState(&s);
}

TEST(Squeeze, OptimalWithIterations) {
  std::vector<unsigned char> in = RunData();
  ZopfliOptions options;
  ZopfliInitOptions(&options);
  ZopfliContext ctx;
  ctx.options = options;
  ZopfliBlockState s;
  ZopfliInitBlockState(&ctx, 0, in.size(), 1, &s);
  ZopfliLZ77Store store;
  ZopfliInitLZ77Store(in.data(), &store);
  ZopfliLZ77Optimal(&s, in.data(), 0, in.size(), /*numiterations=*/3,
                    &store);
  EXPECT_EQ(ZopfliLZ77GetByteRange(&store, 0, store.size), in.size());
  ZopfliCleanLZ77Store(ZopfliDefaultContext(), &store);
  ZopfliCleanBlockState(&s);
}

}  // namespace
