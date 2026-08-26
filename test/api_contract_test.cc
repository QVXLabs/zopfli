#include "zopfli_c_api.h"

#include "gtest/gtest.h"

namespace {

// Negative numiterations must not lose data. It used to run zero squeeze
// iterations, emitting valid-looking blocks with empty bodies (assert-enabled
// builds aborted instead).
#ifdef ZOPFLI_TEST_HAVE_ZLIB
TEST(ApiContract, NegativeIterationsRoundTrips) {
  // Compressible content across a couple of blocks' worth of data.
  std::vector<unsigned char> in;
  for (int i = 0; i < 50000; i++) {
    in.push_back((unsigned char)("zopfli data "[i % 12] + i / 5000));
  }

  ZopfliOptions options;
  ZopfliInitOptions(&options);
  options.numiterations = -1;

  zopfli_test::Output gz;
  ZopfliCompress(&options, ZOPFLI_FORMAT_GZIP,
                 in.data(), in.size(), gz.out(), gz.size_ptr());
  EXPECT_EQ(zopfli_test::Inflate(gz.bytes(), 31), in);

  zopfli_test::Output zl;
  ZopfliCompress(&options, ZOPFLI_FORMAT_ZLIB,
                 in.data(), in.size(), zl.out(), zl.size_ptr());
  EXPECT_EQ(zopfli_test::Inflate(zl.bytes(), 15), in);
}
#else
TEST(ApiContract, NegativeIterationsRoundTrips) {
  GTEST_SKIP() << "zlib not available";
}
#endif

// ZopfliDeflatePart is a public entry point and must apply the same
// numiterations <= 0 -> auto fallback as the other entry points. It used to
// copy the options bare, run zero squeeze iterations, and emit valid-looking
// blocks with empty bodies: silent data loss with ZopfliInitOptions defaults.
#ifdef ZOPFLI_TEST_HAVE_ZLIB
TEST(ApiContract, DeflatePartDefaultIterationsRoundTrips) {
  std::vector<unsigned char> in;
  for (int i = 0; i < 30000; i++) {
    in.push_back((unsigned char)("deflate part "[i % 13] + i / 3000));
  }

  for (int numiterations : {0, -1}) {
    SCOPED_TRACE(numiterations);
    ZopfliOptions options;
    ZopfliInitOptions(&options);
    options.numiterations = numiterations;

    zopfli_test::Output out;
    unsigned char bp = 0;
    ZopfliDeflatePart(&options, 2, 1, in.data(), 0, in.size(), &bp,
                      out.out(), out.size_ptr());
    ASSERT_FALSE(out.empty());
    EXPECT_EQ(zopfli_test::Inflate(out.bytes(), -15), in);
  }
}
#else
TEST(ApiContract, DeflatePartDefaultIterationsRoundTrips) {
  GTEST_SKIP() << "zlib not available";
}
#endif

// The (out, outsize) pair is documented as append semantics: a non-empty
// caller buffer keeps its prefix and the compressed stream lands after it.
// Upstream's power-of-two append trick segfaulted on this (google/zopfli#13).
#ifdef ZOPFLI_TEST_HAVE_ZLIB
TEST(ApiContract, CompressAppendsToExistingBuffer) {
  const std::vector<unsigned char> in =
      zopfli_test::Bytes(std::string(4000, 'q') + "append contract");
  const std::string prefix = "existing bytes";

  ZopfliOptions options;
  ZopfliInitOptions(&options);

  unsigned char* out = (unsigned char*)malloc(prefix.size());
  memcpy(out, prefix.data(), prefix.size());
  size_t outsize = prefix.size();
  ZopfliCompress(&options, ZOPFLI_FORMAT_GZIP,
                 in.data(), in.size(), &out, &outsize);

  ASSERT_GT(outsize, prefix.size());
  EXPECT_EQ(std::string(out, out + prefix.size()), prefix);
  const std::vector<unsigned char> stream(out + prefix.size(), out + outsize);
  EXPECT_EQ(zopfli_test::Inflate(stream, 31), in);
  free(out);
}
#else
TEST(ApiContract, CompressAppendsToExistingBuffer) {
  GTEST_SKIP() << "zlib not available";
}
#endif

}  // namespace
