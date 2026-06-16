#include "zopfli_c_api.h"

#include "gtest/gtest.h"

namespace {

TEST(Lz77, StoreAppendCopyByteRange) {
  std::vector<unsigned char> data(64, 'a');
  ZopfliLZ77Store a;
  ZopfliInitLZ77Store(data.data(), &a);
  ZopfliStoreLitLenDist(ZopfliDefaultContext(), 'a', 0, 0, &a);  // literal
  ZopfliStoreLitLenDist(ZopfliDefaultContext(), 5, 1, 1, &a);  // len/dist pair
  EXPECT_EQ(a.size, 2u);
  // 1 literal byte + a length-5 match = 6 bytes.
  EXPECT_EQ(ZopfliLZ77GetByteRange(&a, 0, a.size), 6u);

  ZopfliLZ77Store b;
  ZopfliInitLZ77Store(data.data(), &b);
  ZopfliAppendLZ77Store(ZopfliDefaultContext(), &a, &b);
  EXPECT_EQ(b.size, 2u);

  // ZopfliCopyLZ77Store cleans the destination first, so it must be
  // initialized before copying into it.
  ZopfliLZ77Store c;
  ZopfliInitLZ77Store(data.data(), &c);
  ZopfliCopyLZ77Store(ZopfliDefaultContext(), &a, &c);
  EXPECT_EQ(c.size, a.size);
  EXPECT_EQ(c.dists[1], a.dists[1]);

  ZopfliCleanLZ77Store(ZopfliDefaultContext(), &a);
  ZopfliCleanLZ77Store(ZopfliDefaultContext(), &b);
  ZopfliCleanLZ77Store(ZopfliDefaultContext(), &c);
}

TEST(Lz77, Histogram) {
  std::vector<unsigned char> in = zopfli_test::Bytes(
      "abcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabc");
  ZopfliLZ77Store store;
  zopfli_test::GreedyStore(in, &store);

  std::vector<size_t> ll(ZOPFLI_NUM_LL, 0);
  std::vector<size_t> d(ZOPFLI_NUM_D, 0);
  ZopfliLZ77GetHistogram(&store, 0, store.size, ll.data(), d.data());

  size_t ll_total = 0;
  for (size_t v : ll) ll_total += v;
  // End-of-block symbol (256) is not part of the store histogram.
  EXPECT_EQ(ll_total, store.size);

  ZopfliCleanLZ77Store(ZopfliDefaultContext(), &store);
}

TEST(Lz77, FindLongestMatchFindsRepeat) {
  // "xxxxabcdabcd..." — at the second "abcd" there is a length>=4 match.
  std::vector<unsigned char> in = zopfli_test::Bytes(
      "abcdefghabcdefghabcdefghabcdefghabcdefgh");
  ZopfliOptions options;
  ZopfliInitOptions(&options);
  ZopfliContext ctx;
  ctx.options = options;
  ZopfliBlockState s;
  ZopfliInitBlockState(&ctx, 0, in.size(), 1, &s);
  ZopfliHash h;
  ZopfliAllocHash(ZopfliDefaultContext(), ZOPFLI_WINDOW_SIZE, &h);
  ZopfliResetHash(ZOPFLI_WINDOW_SIZE, &h);
  ZopfliWarmupHash(in.data(), 0, in.size(), &h);

  const size_t match_pos = 8;  // start of the second "abcdefgh"
  for (size_t i = 0; i < match_pos; i++) {
    ZopfliUpdateHash(in.data(), i, in.size(), &h);
  }
  ZopfliUpdateHash(in.data(), match_pos, in.size(), &h);

  unsigned short distance = 0, length = 0;
  ZopfliFindLongestMatch(&s, &h, in.data(), match_pos, in.size(),
                         ZOPFLI_MAX_MATCH, nullptr, &distance, &length);
  EXPECT_GE(length, 8u);
  EXPECT_EQ(distance, 8u);

  ZopfliCleanHash(ZopfliDefaultContext(), &h);
  ZopfliCleanBlockState(&s);
}

}  // namespace
