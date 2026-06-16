#include "zopfli_c_api.h"

#include <vector>

#include "gtest/gtest.h"

// Range coverage for the fixed-point optimal-parse cost model. The squeeze
// accumulator is a 32-bit integer with a per-block fractional shift; these
// tests span block sizes from a single byte (largest shift) up to the largest
// supported block, ZOPFLI_COST_MAX_BLOCK_SIZE (shift 0), to prove the shift
// never lets the accumulator overflow and that the parse stays correct.

namespace {

using zopfli_test::PseudoRandom;

// Compressible shape: a non-trivial pattern (period > ZOPFLI_MIN_MATCH) tiled
// to length n, giving many LZ77 matches and therefore large blocks.
std::vector<unsigned char> Repetitive(size_t n) {
  std::vector<unsigned char> pattern = PseudoRandom(251, 99);
  std::vector<unsigned char> v(n);
  for (size_t i = 0; i < n; i++) v[i] = pattern[i % pattern.size()];
  return v;
}

// 1. The shift keeps the worst-case accumulated cost below the sentinel for
//    every block size, is in range, and shrinks as blocks grow. This proves the
//    overflow bound directly, without depending on input data.
TEST(FixedPointRange, CostShiftInvariant) {
  // Includes 2^22, 2^23 and the maximum supported block (2^24) so the small
  // shifts (2, 1, 0) the contract allows are actually exercised.
  const size_t sizes[] = {1, 2, 256, 257, 300, 8192, 65536, 262144,
                          999999, 1000000, 1000001, 1500000, 2000000,
                          4194304, 8388608, ZOPFLI_COST_MAX_BLOCK_SIZE};
  int prev = 100;
  size_t i;
  for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
    int shift = ZopfliGetCostShift(sizes[i]);
    // Worst case is ~32 bits per position; scaled it must stay below 2^30.
    unsigned long long scaled = ((unsigned long long)32 * sizes[i]) << shift;
    EXPECT_GE(shift, 0);
    EXPECT_LE(shift, 16);
    EXPECT_LT(scaled, (1ULL << 30)) << "overflow risk at blocksize "
                                    << sizes[i];
    EXPECT_LE(shift, prev) << "shift not monotone at blocksize " << sizes[i];
    prev = shift;
  }
}

// Runs both optimal-parse entry points on a single block and checks the parse
// covers exactly the input. An overflow-corrupted cost would derail the dynamic
// program and break this; with assertions enabled ZopfliVerifyLenDist also
// validates every emitted match.
void CheckSqueezeCovers(const std::vector<unsigned char>& in, int iters) {
  ZopfliOptions options;
  ZopfliInitOptions(&options);
  ZopfliContext ctx;
  ctx.options = options;
  ZopfliBlockState s;
  ZopfliInitBlockState(&ctx, 0, in.size(), 1, &s);

  ZopfliLZ77Store store;
  ZopfliInitLZ77Store(in.data(), &store);
  ZopfliLZ77Optimal(&s, in.data(), 0, in.size(), iters, &store);
  EXPECT_EQ(ZopfliLZ77GetByteRange(&store, 0, store.size), in.size());
  EXPECT_GT(store.size, 0u);
  ZopfliCleanLZ77Store(ZopfliDefaultContext(), &store);

  ZopfliLZ77Store fixed_store;
  ZopfliInitLZ77Store(in.data(), &fixed_store);
  ZopfliLZ77OptimalFixed(&s, in.data(), 0, in.size(), &fixed_store);
  EXPECT_EQ(ZopfliLZ77GetByteRange(&fixed_store, 0, fixed_store.size),
            in.size());
  ZopfliCleanLZ77Store(ZopfliDefaultContext(), &fixed_store);

  ZopfliCleanBlockState(&s);
}

// 2. Drive the squeeze directly over a single block at every size, including
//    sizes past the master block so the smallest shift is exercised (direct
//    calls are not subject to master-block splitting).
TEST(FixedPointRange, SqueezeCoversInputAcrossSizes) {
  const size_t sizes[] = {1, 2, 3, 13, 300, 8 * 1024, 64 * 1024,
                          256 * 1024, 1000000, 1500000};
  size_t i;
  for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
    int iters = sizes[i] >= 256 * 1024 ? 1 : 3;
    CheckSqueezeCovers(PseudoRandom(sizes[i]), iters);
    CheckSqueezeCovers(Repetitive(sizes[i]), iters);
  }
}

#ifdef ZOPFLI_TEST_HAVE_ZLIB

std::vector<unsigned char> Compress(ZopfliFormat format,
                                    const std::vector<unsigned char>& in,
                                    int iters, int blocksplitting) {
  ZopfliOptions options;
  ZopfliInitOptions(&options);
  options.numiterations = iters;
  options.blocksplitting = blocksplitting;
  zopfli_test::Output out;
  ZopfliCompress(&options, format, in.data(), in.size(), out.out(),
                 out.size_ptr());
  return out.bytes();
}

// 3. Compress across the size range and through the master block boundary, with
//    block splitting off (whole input as one block per master block, the
//    smallest shift compression can reach) and on, then decompress and compare.
//    This is the decisive correctness check that fixed point never corrupts the
//    stream.
TEST(FixedPointRange, CompressRoundTrip) {
  const size_t sizes[] = {1, 300, 8 * 1024, 64 * 1024, 262144, 1000001};
  size_t i;
  int split;
  for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
    int iters = sizes[i] >= 256 * 1024 ? 1 : 5;
    std::vector<unsigned char> shapes[2];
    shapes[0] = PseudoRandom(sizes[i]);
    shapes[1] = Repetitive(sizes[i]);
    int shape;
    for (shape = 0; shape < 2; shape++) {
      const std::vector<unsigned char>& in = shapes[shape];
      for (split = 0; split < 2; split++) {
        std::vector<unsigned char> gz =
            Compress(ZOPFLI_FORMAT_GZIP, in, iters, split);
        EXPECT_EQ(zopfli_test::Inflate(gz, 31), in)
            << "gzip size " << sizes[i] << " shape " << shape << " split "
            << split;
        std::vector<unsigned char> zl =
            Compress(ZOPFLI_FORMAT_ZLIB, in, iters, split);
        EXPECT_EQ(zopfli_test::Inflate(zl, 15), in)
            << "zlib size " << sizes[i] << " shape " << shape << " split "
            << split;
      }
    }
  }
}

// 4. The fixed-point model must still drive real compression: a cost model
//    broken by overflow would collapse to near-all-literals and fail to shrink.
TEST(FixedPointRange, CompressShrinksRepetitive) {
  const size_t sizes[] = {64 * 1024, 262144, 1000001};
  size_t i;
  for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
    std::vector<unsigned char> in = Repetitive(sizes[i]);
    std::vector<unsigned char> out =
        Compress(ZOPFLI_FORMAT_GZIP, in, 1, 1);
    EXPECT_LT(out.size(), in.size() / 2) << "size " << sizes[i];
  }
}

#else  // ZOPFLI_TEST_HAVE_ZLIB

TEST(FixedPointRange, CompressRoundTrip) {
  GTEST_SKIP() << "zlib not available; round-trip check skipped";
}

#endif  // ZOPFLI_TEST_HAVE_ZLIB

}  // namespace
