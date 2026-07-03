#include "zopfli_c_api.h"

#include "gtest/gtest.h"

namespace {

#ifdef ZOPFLI_TEST_HAVE_ZLIB

// Raw DEFLATE (window_bits -15) round-trips. gzip and zlib are covered by
// FixedPointRange.CompressRoundTrip; the raw format shares the deflate body
// but has no container framing to hide bit-level mistakes at the tail.
TEST(RoundTrip, DeflateRawSizes) {
  // Cross the stored-block chunk limit (65535) and the 1 MB master-block
  // boundary; include the empty input.
  const size_t sizes[] = {0, 1, 300, 65535, 65536, 1000000, 1000001};
  for (size_t size : sizes) {
    SCOPED_TRACE(size);
    std::vector<unsigned char> random = zopfli_test::PseudoRandom(size);
    std::vector<unsigned char> repetitive(size);
    for (size_t i = 0; i < size; i++) {
      repetitive[i] = (unsigned char)("abcdefgh"[(i / 3) % 8]);
    }
    for (const auto& in : {random, repetitive}) {
      ZopfliOptions options;
      ZopfliInitOptions(&options);
      options.numiterations = 3;

      zopfli_test::Output out;
      ZopfliCompress(&options, ZOPFLI_FORMAT_DEFLATE,
                     in.data(), in.size(), out.out(), out.size_ptr());
      EXPECT_EQ(zopfli_test::Inflate(out.bytes(), -15), in);
    }
  }
}

#else
TEST(RoundTrip, DeflateRawSizes) { GTEST_SKIP() << "zlib not available"; }
#endif

}  // namespace
