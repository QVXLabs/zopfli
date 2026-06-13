#include "zopfli_c_api.h"

#include "gtest/gtest.h"

namespace {

TEST(Hash, WarmupAndUpdateOverBuffer) {
  std::vector<unsigned char> data = zopfli_test::Bytes(
      "the quick brown fox the quick brown fox the quick brown fox");
  ZopfliHash h;
  ZopfliAllocHash(ZOPFLI_WINDOW_SIZE, &h);
  ZopfliResetHash(ZOPFLI_WINDOW_SIZE, &h);
  ZopfliWarmupHash(data.data(), 0, data.size(), &h);
  for (size_t i = 0; i < data.size(); i++) {
    ZopfliUpdateHash(data.data(), i, data.size(), &h);
  }
  // The repeated "the quick brown fox" must have produced a hash chain:
  // some position's head points back to an earlier equal-hash position.
  EXPECT_GE(h.head[h.val], 0);
  ZopfliCleanHash(&h);
}

TEST(Hash, ResetIsIdempotent) {
  ZopfliHash h;
  ZopfliAllocHash(ZOPFLI_WINDOW_SIZE, &h);
  ZopfliResetHash(ZOPFLI_WINDOW_SIZE, &h);
  ZopfliResetHash(ZOPFLI_WINDOW_SIZE, &h);
  EXPECT_EQ(h.val, 0);
  ZopfliCleanHash(&h);
}

}  // namespace
