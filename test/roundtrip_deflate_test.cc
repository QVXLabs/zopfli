#include "zopfli_c_api.h"

#include "gtest/gtest.h"

namespace {

#ifdef ZOPFLI_TEST_HAVE_ZLIB

// Raw DEFLATE round-trips (gzip/zlib are covered elsewhere; the raw format
// has no container framing to hide bit-level mistakes at the tail).
TEST(RoundTrip, DeflateRawSizes) {
  // Crosses the 65535 stored-chunk limit and the 1 MB master-block boundary.
  const size_t sizes[] = {0, 1, 300, 65535, 65536, 1000000, 1000001};
  for (size_t size : sizes) {
    SCOPED_TRACE(size);
    std::vector<unsigned char> random = zopfli_test::PseudoRandom(size);
    std::vector<unsigned char> repetitive(size);
    for (size_t i = 0; i < size; i++) {
      repetitive[i] = (unsigned char)("abcdefgh"[(i / 3) % 8]);
    }
    // Iterate by pointer: a braced list of the vectors themselves would
    // copy them into the initializer_list's backing array.
    for (const auto* inp : {&random, &repetitive}) {
      const std::vector<unsigned char>& in = *inp;
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

// Several master blocks in one stream (google/zopfli#182 reported a
// wrong-bytes deflate stream on large multi-block inputs; pin the behavior).
// Mixed content so the blocks aren't degenerate copies of each other.
TEST(RoundTrip, DeflateMultiMasterBlock) {
  const size_t size = 2500000;  // 3 x 1 MB master blocks
  std::vector<unsigned char> in = zopfli_test::PseudoRandom(size / 2);
  in.reserve(size);
  for (size_t i = in.size(); i < size; i++) {
    in.push_back((unsigned char)("multi master block "[i % 19] + i / 100000));
  }

  ZopfliOptions options;
  ZopfliInitOptions(&options);
  options.numiterations = 1;  // round-trip correctness, not ratio

  zopfli_test::Output out;
  ZopfliCompress(&options, ZOPFLI_FORMAT_DEFLATE,
                 in.data(), in.size(), out.out(), out.size_ptr());
  EXPECT_EQ(zopfli_test::Inflate(out.bytes(), -15), in);
}

#else
TEST(RoundTrip, DeflateRawSizes) { GTEST_SKIP() << "zlib not available"; }
TEST(RoundTrip, DeflateMultiMasterBlock) {
  GTEST_SKIP() << "zlib not available";
}
#endif

}  // namespace
