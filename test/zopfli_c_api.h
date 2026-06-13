// Shared test contract: pulls in zopfli's C headers with C linkage and
// provides small C++ helpers. The internal headers (lz77.h, tree.h, ...)
// lack their own extern "C" guards, so they must be included here inside
// one extern "C" block to link against the C-compiled library.
#ifndef ZOPFLI_TEST_ZOPFLI_C_API_H_
#define ZOPFLI_TEST_ZOPFLI_C_API_H_

extern "C" {
#include "zopfli.h"
#include "util.h"
#include "deflate.h"
#include "lz77.h"
#include "squeeze.h"
#include "blocksplitter.h"
#include "tree.h"
#include "katajainen.h"
#include "hash.h"
#include "cache.h"
}

#include <cstdlib>
#include <string>
#include <vector>

namespace zopfli_test {

// Owns a malloc'd output buffer from a zopfli compress call and frees it.
class Output {
 public:
  Output() = default;
  ~Output() { free(data_); }
  Output(const Output&) = delete;
  Output& operator=(const Output&) = delete;

  unsigned char** out() { return &data_; }
  size_t* size_ptr() { return &size_; }
  size_t size() const { return size_; }
  unsigned char operator[](size_t i) const { return data_[i]; }
  bool empty() const { return size_ == 0; }
  std::vector<unsigned char> bytes() const {
    return std::vector<unsigned char>(data_, data_ + size_);
  }

 private:
  unsigned char* data_ = nullptr;
  size_t size_ = 0;
};

inline std::vector<unsigned char> Bytes(const std::string& s) {
  return std::vector<unsigned char>(s.begin(), s.end());
}

// Builds a greedy LZ77 parse of `in` into `store`. The store borrows
// `in.data()`, so `in` must outlive it; caller calls ZopfliCleanLZ77Store.
inline void GreedyStore(const std::vector<unsigned char>& in,
                        ZopfliLZ77Store* store) {
  ZopfliOptions options;
  ZopfliInitOptions(&options);
  ZopfliBlockState s;
  ZopfliInitBlockState(&options, 0, in.size(), 1, &s);
  ZopfliHash h;
  ZopfliAllocHash(ZOPFLI_WINDOW_SIZE, &h);
  ZopfliInitLZ77Store(in.data(), store);
  ZopfliLZ77Greedy(&s, in.data(), 0, in.size(), store, &h);
  ZopfliCleanHash(&h);
  ZopfliCleanBlockState(&s);
}

// Deterministic pseudo-random bytes for incompressible-input tests.
inline std::vector<unsigned char> PseudoRandom(size_t n, unsigned seed = 1) {
  std::vector<unsigned char> v(n);
  unsigned state = seed;
  for (size_t i = 0; i < n; i++) {
    state = state * 1103515245u + 12345u;
    v[i] = static_cast<unsigned char>(state >> 16);
  }
  return v;
}

}  // namespace zopfli_test

#endif  // ZOPFLI_TEST_ZOPFLI_C_API_H_
