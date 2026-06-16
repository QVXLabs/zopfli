#include "zopfli_c_api.h"

#include "gtest/gtest.h"

namespace {

TEST(Hash, WarmupAndUpdateOverBuffer) {
  std::vector<unsigned char> data = zopfli_test::Bytes(
      "the quick brown fox the quick brown fox the quick brown fox");
  ZopfliHash h;
  ZopfliAllocHash(ZopfliDefaultContext(), ZOPFLI_WINDOW_SIZE, &h);
  ZopfliResetHash(ZOPFLI_WINDOW_SIZE, &h);
  ZopfliWarmupHash(data.data(), 0, data.size(), &h);
  for (size_t i = 0; i < data.size(); i++) {
    ZopfliUpdateHash(data.data(), i, data.size(), &h);
  }
  // The repeated substring should create at least one "prev" link to an earlier
  // position with the same hash value.
  bool has_chain = false;
  for (size_t pos = 0; pos < data.size(); ++pos) {
    const unsigned short hpos = static_cast<unsigned short>(pos & ZOPFLI_WINDOW_MASK);
    if (h.prev[hpos] != hpos) { has_chain = true; break; }
  }
  EXPECT_TRUE(has_chain);
  ZopfliCleanHash(ZopfliDefaultContext(), &h);
}

TEST(Hash, ResetIsIdempotent) {
  ZopfliHash h;
  ZopfliAllocHash(ZopfliDefaultContext(), ZOPFLI_WINDOW_SIZE, &h);
  ZopfliResetHash(ZOPFLI_WINDOW_SIZE, &h);
  ZopfliResetHash(ZOPFLI_WINDOW_SIZE, &h);
  EXPECT_EQ(h.val, 0);
  ZopfliCleanHash(ZopfliDefaultContext(), &h);
}

}  // namespace
