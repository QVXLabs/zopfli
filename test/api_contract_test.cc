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

}  // namespace
