/*
Copyright 2011 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Author: lode.vandevenne@gmail.com (Lode Vandevenne)
Author: jyrki.alakuijala@gmail.com (Jyrki Alakuijala)
Author: afalls@qvxlabs.com (Ardavon Falls)
*/

/*
Several utilities, including: #defines to try different compression results,
basic deflate specification values and generic program options.
*/

#ifndef ZOPFLI_UTIL_H_
#define ZOPFLI_UTIL_H_

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* Inline qualifier for header helpers; MSVC's C spells it `__inline`. */
#ifdef _MSC_VER
#define ZOPFLI_INLINE static __inline
#else
#define ZOPFLI_INLINE static inline
#endif

/* Count-leading-zeros support: clang/GCC have __builtin_clz; MSVC's C compiler
uses the _BitScanReverse intrinsic (anything else falls back to a loop). */
#if defined(__has_builtin)
# if __has_builtin(__builtin_clz)
#  define ZOPFLI_HAS_BUILTIN_CLZ
# endif
#elif defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__ >= 304)
# define ZOPFLI_HAS_BUILTIN_CLZ
#endif
#if !defined(ZOPFLI_HAS_BUILTIN_CLZ) && defined(_MSC_VER)
# include <intrin.h>
#endif

/* Native machine word width, used to size GetMatch's word-at-a-time scan.
ZOPFLI_NATIVE_64BIT when size_t is 64-bit, ZOPFLI_NATIVE_32BIT when unsigned int
is 32-bit, ZOPFLI_NATIVE_16BIT when it is 16-bit; smaller targets define none. */
#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFu
# define ZOPFLI_NATIVE_64BIT 1
#elif UINT_MAX == 0xFFFFFFFFu
# define ZOPFLI_NATIVE_32BIT 1
#elif UINT_MAX == 0xFFFFu
# define ZOPFLI_NATIVE_16BIT 1
#endif

/* Minimum and maximum length that can be encoded in deflate. */
#define ZOPFLI_MAX_MATCH 258
#define ZOPFLI_MIN_MATCH 3

/* Number of distinct literal/length and distance symbols in DEFLATE */
#define ZOPFLI_NUM_LL 288
#define ZOPFLI_NUM_D 32

/*
The window size for deflate. Must be a power of two. This should be 32768, the
maximum possible by the deflate spec. Anything less hurts compression more than
speed.
*/
#define ZOPFLI_WINDOW_SIZE 32768

/*
The window mask used to wrap indices into the window. This is why the
window size must be a power of two.
*/
#define ZOPFLI_WINDOW_MASK (ZOPFLI_WINDOW_SIZE - 1)

/*
A block structure of huge, non-smart, blocks to divide the input into, to allow
operating on huge files without exceeding memory, such as the 1GB wiki9 corpus.
The whole compression algorithm, including the smarter block splitting, will
be executed independently on each huge block.
Dividing into huge blocks hurts compression, but not much relative to the size.
Set it to 0 to disable master blocks.
*/
#define ZOPFLI_MASTER_BLOCK_SIZE 1000000

/*
Largest part (in bytes) the optimal-parse cost model can represent. ZopfliCost
is 32-bit and a block costs < 32 * blocksize bits, so 32 * blocksize must stay
<= 2^29 (see ZopfliGetCostShift), capping a safe block at 2^24 bytes. The
master-block loop clamps part sizes to this even when ZOPFLI_MASTER_BLOCK_SIZE
is 0 (disabled) or larger, so the cost DP can never overflow.
*/
#define ZOPFLI_COST_MAX_BLOCK_SIZE ((uint32_t)1 << 24)

/*
Sentinel larger than any real block-size/cost (in bits), for initializing a
running minimum. Cost values are uint32_t (a block is < 2^31 bits), so UINT32_MAX
exceeds any real value; same value on every platform. Only ever compared, never
added, so it cannot overflow.
*/
#define ZOPFLI_LARGE_COST UINT32_MAX

/*
Integer type for the squeeze optimal-parse cost accumulator. Kept 32-bit so the
hot per-byte costs[] array and its add/compare stay single-word, which matters
on 32-bit processors. `int` is 32-bit on every ILP32 and LP64 target of
interest, so no width detection is needed. Costs are stored in fixed point with
a per-block shift (see squeeze.c); the shift is chosen so the worst-case
accumulated cost cannot overflow this type.
*/
typedef int ZopfliCost;

/*
For longest match cache. max 256. Uses huge amounts of memory but makes it
faster. Uses this many times three bytes per single byte of the input data.
This is so because longest match finding has to find the exact distance
that belongs to each length for the best lz77 strategy.
Good values: e.g. 5, 8.
*/
#define ZOPFLI_CACHE_LENGTH 8

/*
limit the max hash chain hits for this hash value. This has an effect only
on files where the hash value is the same very often. On these files, this
gives worse compression (the value should ideally be 32768, which is the
ZOPFLI_WINDOW_SIZE, while zlib uses 4096 even for best level), but makes it
faster on some specific files.
Good value: e.g. 8192.
*/
#define ZOPFLI_MAX_CHAIN_HITS 8192

/*
Whether to use the longest match cache for ZopfliFindLongestMatch. This cache
consumes a lot of memory but speeds it up. No effect on compression size.
*/
#define ZOPFLI_LONGEST_MATCH_CACHE

/*
Enable to remember amount of successive identical bytes in the hash chain for
finding longest match
required for ZOPFLI_HASH_SAME_HASH and ZOPFLI_SHORTCUT_LONG_REPETITIONS
This has no effect on the compression result, and enabling it increases speed.
*/
#define ZOPFLI_HASH_SAME

/*
Switch to a faster hash based on the info from ZOPFLI_HASH_SAME once the
best length so far is long enough. This is way faster for files with lots of
identical bytes, on which the compressor is otherwise too slow. Regular files
are unaffected or maybe a tiny bit slower.
This has no effect on the compression result, only on speed.
*/
#define ZOPFLI_HASH_SAME_HASH

/*
Enable this, to avoid slowness for files which are a repetition of the same
character more than a multiple of ZOPFLI_MAX_MATCH times. This should not affect
the compression result.
*/
#define ZOPFLI_SHORTCUT_LONG_REPETITIONS

/*
Whether to use lazy matching in the greedy LZ77 implementation. This gives a
better result of ZopfliLZ77Greedy, but the effect this has on the optimal LZ77
varies from file to file.
*/
#define ZOPFLI_LAZY_MATCHING

/* Integer min and absolute difference. Function-like macros so they work for
any integer type used in the hot path (int/size_t/unsigned/unsigned short)
without truncation. Args are evaluated twice: pass side-effect-free operands. */
#define ZOPFLI_MIN(a, b) ((a) < (b) ? (a) : (b))
#define ZOPFLI_ABS_DIFF(x, y) ((x) > (y) ? (x) - (y) : (y) - (x))

/*
Appends value to dynamically allocated memory, doubling its allocation size
whenever needed.

value: the value to append, type T
data: pointer to the dynamic array to append to, type T**
size: pointer to the size of the array to append to, type size_t*. This is the
size that you consider the array to be, not the internal allocation size.
Precondition: allocated size of data is at least a power of two greater than or
equal than *size.
*/
#ifdef __cplusplus /* C++ cannot assign void* from malloc to *data */
#define ZOPFLI_APPEND_DATA(/* T */ value, /* T** */ data, /* size_t* */ size) {\
  if (!((*size) & ((*size) - 1))) {\
    /*double alloc size if it's a power of two*/\
    void** data_void = reinterpret_cast<void**>(data);\
    *data_void = (*size) == 0 ? malloc(sizeof(**data))\
                              : realloc((*data), (*size) * 2 * sizeof(**data));\
  }\
  (*data)[(*size)] = (value);\
  (*size)++;\
}
#else /* C gives problems with strict-aliasing rules for (void**) cast */
#define ZOPFLI_APPEND_DATA(/* T */ value, /* T** */ data, /* size_t* */ size) {\
  if (!((*size) & ((*size) - 1))) {\
    /*double alloc size if it's a power of two*/\
    (*data) = (*size) == 0 ? malloc(sizeof(**data))\
                           : realloc((*data), (*size) * 2 * sizeof(**data));\
  }\
  (*data)[(*size)] = (value);\
  (*size)++;\
}
#endif

/* Number of leading zero bits in a 32-bit value; x must be nonzero. */
ZOPFLI_INLINE int ZopfliCLZ32(uint32_t x) {
#if defined(ZOPFLI_HAS_BUILTIN_CLZ)
  return __builtin_clz(x);
#elif defined(_MSC_VER)
  unsigned long idx;
  _BitScanReverse(&idx, x);
  return 31 - (int)idx;
#else
  int n;
  for (n = 0; !(x & 0x80000000u); ++n) x <<= 1;
  return n;
#endif
}

#endif  /* ZOPFLI_UTIL_H_ */
