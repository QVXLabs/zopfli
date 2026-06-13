#include "zopfli_c_api.h"

#include <algorithm>

#include "gtest/gtest.h"
namespace {

using zopfli_test::Output;
using zopfli_test::PseudoRandom;

std::vector<unsigned char> Compress(ZopfliFormat format,
                                    const std::vector<unsigned char>& in,
                                    int numiterations = 15,
                                    int blocksplitting = 1) {
  ZopfliOptions options;
  ZopfliInitOptions(&options);
  options.numiterations = numiterations;
  options.blocksplitting = blocksplitting;
  Output out;
  ZopfliCompress(&options, format, in.data(), in.size(), out.out(),
                 out.size_ptr());
  return out.bytes();
}

TEST(ZopfliPublic, GzipMagicAndShrinks) {
  std::vector<unsigned char> in(2000, 'a');
  std::vector<unsigned char> out = Compress(ZOPFLI_FORMAT_GZIP, in);
  ASSERT_GE(out.size(), 2u);
  EXPECT_EQ(out[0], 0x1f);
  EXPECT_EQ(out[1], 0x8b);
  EXPECT_LT(out.size(), in.size());
}

TEST(ZopfliPublic, ZlibHeaderIsValid) {
  std::vector<unsigned char> out =
      Compress(ZOPFLI_FORMAT_ZLIB, zopfli_test::Bytes("hello world"));
  ASSERT_GE(out.size(), 2u);
  // zlib: (CMF << 8 | FLG) is a multiple of 31.
  unsigned check = (static_cast<unsigned>(out[0]) << 8) | out[1];
  EXPECT_EQ(check % 31u, 0u);
}

TEST(ZopfliPublic, DeflateFormatNonEmpty) {
  std::vector<unsigned char> out =
      Compress(ZOPFLI_FORMAT_DEFLATE, zopfli_test::Bytes("deflate me"));
  EXPECT_FALSE(out.empty());
}

TEST(ZopfliPublic, EmptyInput) {
  std::vector<unsigned char> empty;
  // Should not crash and should still emit a valid container.
  EXPECT_FALSE(Compress(ZOPFLI_FORMAT_GZIP, empty).empty());
  EXPECT_FALSE(Compress(ZOPFLI_FORMAT_ZLIB, empty).empty());
}

// Drives block splitting, optimal parsing, and both option branches over
// a range of sizes and data shapes. This is the main coverage workhorse.
TEST(ZopfliPublic, Matrix) {
  std::vector<std::vector<unsigned char>> inputs;
  inputs.push_back(std::vector<unsigned char>(1, 'x'));        // 1 byte
  inputs.push_back(std::vector<unsigned char>(100, 'y'));      // tiny
  inputs.push_back(PseudoRandom(8 * 1024));                    // medium
  // Large, mixed: half repetitive then random, to force split points.
  std::vector<unsigned char> mixed(200 * 1024, 'q');
  std::vector<unsigned char> tail = PseudoRandom(100 * 1024, 7);
  std::copy(tail.begin(), tail.end(), mixed.begin() + 100 * 1024);
  inputs.push_back(mixed);

  const ZopfliFormat formats[] = {ZOPFLI_FORMAT_GZIP, ZOPFLI_FORMAT_ZLIB,
                                  ZOPFLI_FORMAT_DEFLATE};
  for (const auto& in : inputs) {
    for (ZopfliFormat fmt : formats) {
      // numiterations 0 exercises the greedy-only path; blocksplitting
      // 0 vs 1 exercises both block-layout branches.
      EXPECT_FALSE(Compress(fmt, in, 0, 0).empty());
      EXPECT_FALSE(Compress(fmt, in, 5, 1).empty());
    }
  }
}

}  // namespace
