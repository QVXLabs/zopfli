#include "zopfli_c_api.h"

#include "gtest/gtest.h"

namespace {

TEST(Cache, EmptyHasNoCachedSublen) {
  ZopfliLongestMatchCache lmc;
  ZopfliInitCache(ZopfliDefaultContext(), 16, &lmc);
  EXPECT_EQ(ZopfliMaxCachedSublen(&lmc, 0, 0), 0u);
  ZopfliCleanCache(ZopfliDefaultContext(), &lmc);
}

TEST(Cache, SublenRoundTrip) {
  const size_t blocksize = 8;
  ZopfliLongestMatchCache lmc;
  ZopfliInitCache(ZopfliDefaultContext(), blocksize, &lmc);

  // Build a sublen where each length maps to a distinct distance.
  const unsigned short length = 40;
  std::vector<unsigned short> sublen(259, 0);
  for (unsigned short i = 3; i <= length; i++) sublen[i] = i * 2;

  const size_t pos = 2;
  ZopfliSublenToCache(ZopfliDefaultContext(), sublen.data(), pos, length,
                      &lmc);

  unsigned maxcached = ZopfliMaxCachedSublen(&lmc, pos, length);
  EXPECT_GE(maxcached, 3u);

  std::vector<unsigned short> got(259, 0);
  ZopfliCacheToSublen(&lmc, pos, length, got.data());
  // Distances up to the max cached length must be reproduced exactly.
  for (unsigned short i = 3; i <= maxcached; i++) {
    EXPECT_EQ(got[i], sublen[i]) << "at length " << i;
  }

  ZopfliCleanCache(ZopfliDefaultContext(), &lmc);
}

}  // namespace
