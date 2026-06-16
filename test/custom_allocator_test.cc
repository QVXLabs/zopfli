#include "zopfli_c_api.h"

#include <cstdlib>
#include <cstring>

#include "gtest/gtest.h"

namespace {

// Tracks live allocations routed through the custom zrealloc hook. The hook
// follows zopfli's allocator contract: size 0 frees (returns NULL), ptr NULL
// allocates, otherwise resizes (one logical allocation, live count unchanged).
struct Tracker {
  size_t live = 0;         // outstanding allocations
  size_t alloc_calls = 0;  // total non-free calls
};

void* TrackingRealloc(void* alloc_context, void* ptr, size_t size) {
  Tracker* t = static_cast<Tracker*>(alloc_context);
  if (size == 0) {
    if (ptr) t->live--;
    free(ptr);
    return NULL;
  }
  if (!ptr) t->live++;
  t->alloc_calls++;
  return realloc(ptr, size);
}

std::vector<unsigned char> CompressDefault(ZopfliFormat fmt,
                                           const std::vector<unsigned char>& in) {
  ZopfliOptions options;
  ZopfliInitOptions(&options);
  unsigned char* out = nullptr;
  size_t outsize = 0;
  ZopfliCompress(&options, fmt, in.data(), in.size(), &out, &outsize);
  std::vector<unsigned char> bytes(out, out + outsize);
  free(out);
  return bytes;
}

// Compresses with the tracking allocator, checks the output matches the
// default-allocator output byte for byte, and that every allocation was routed
// through the hook and freed (no leak) once the output is released.
void CheckFormat(ZopfliFormat fmt, const std::vector<unsigned char>& in) {
  std::vector<unsigned char> expected = CompressDefault(fmt, in);

  Tracker tracker;
  ZopfliOptions options;
  ZopfliInitOptions(&options);
  options.zrealloc = TrackingRealloc;
  options.alloc_context = &tracker;

  unsigned char* out = nullptr;
  size_t outsize = 0;
  ZopfliCompress(&options, fmt, in.data(), in.size(), &out, &outsize);

  ASSERT_GT(tracker.alloc_calls, 0u);  // hook actually used
  std::vector<unsigned char> got(out, out + outsize);
  EXPECT_EQ(got, expected);  // byte-identical to the default allocator

  // The output buffer is the only thing still live; free it through the hook.
  TrackingRealloc(&tracker, out, 0);
  EXPECT_EQ(tracker.live, 0u);  // every allocation routed and freed
}

TEST(CustomAllocator, GzipByteIdenticalNoLeak) {
  CheckFormat(ZOPFLI_FORMAT_GZIP, zopfli_test::Bytes(
      std::string(20000, 'a') + "the quick brown fox jumps over the lazy dog"));
}

TEST(CustomAllocator, ZlibByteIdenticalNoLeak) {
  CheckFormat(ZOPFLI_FORMAT_ZLIB, zopfli_test::PseudoRandom(20000));
}

TEST(CustomAllocator, DeflateByteIdenticalNoLeak) {
  CheckFormat(ZOPFLI_FORMAT_DEFLATE, zopfli_test::Bytes(
      std::string(5000, 'x') + std::string(5000, 'y')));
}

}  // namespace
