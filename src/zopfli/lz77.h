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
Functions for basic LZ77 compression and utilities for the "squeeze" LZ77
compression.
*/

#ifndef ZOPFLI_LZ77_H_
#define ZOPFLI_LZ77_H_

#include <stdint.h>
#include <stdlib.h>

#include "cache.h"
#include "hash.h"
#include "katajainen.h"
#include "zopfli.h"

/*
Stores lit/length and dist pairs for LZ77.
Parameter litlens: Contains the literal symbols or length values.
Parameter dists: Contains the distances. A value is 0 to indicate that there is
no dist and the corresponding litlens value is a literal instead of a length.
Parameter size: The size of both the litlens and dists arrays.
The memory can best be managed by using ZopfliInitLZ77Store to initialize it,
ZopfliCleanLZ77Store to destroy it, and ZopfliStoreLitLenDist to append values.

*/
typedef struct ZopfliLZ77Store {
  uint16_t* litlens;  /* Lit or len. */
  uint16_t* dists;  /* If 0: indicates literal in corresponding litlens,
      if > 0: length in corresponding litlens, this is the distance. */
  size_t size;
  size_t cap;  /* Allocated capacity in lz77 symbols, for reuse across runs. */

  const uint8_t* data;  /* original data */
  size_t* pos;  /* position in data where this LZ77 command begins */

  /* Cumulative histograms wrapping around per chunk. Each chunk has the amount
  of distinct symbols as length, so using 1 value per LZ77 symbol, we have a
  precise histogram at every N symbols, and the rest can be calculated by
  looping through the actual symbols of this chunk. */
  uint32_t* ll_counts;
  uint32_t* d_counts;
} ZopfliLZ77Store;

/*
Some state information for compressing a block.
This is currently a bit under-used (with mainly only the longest match cache),
but is kept for easy future expansion.
*/
typedef struct ZopfliBlockState {
  const ZopfliContext* ctx;

#ifdef ZOPFLI_LONGEST_MATCH_CACHE
  /* Cache for length/distance pairs found so far. */
  ZopfliLongestMatchCache* lmc;
#endif

  /* The start (inclusive) and end (not inclusive) of the current block. */
  size_t blockstart;
  size_t blockend;

  /* Reused scratch for length-limited Huffman code construction, so the hot
  block-size evaluations don't malloc/free per call. Per block state, so
  thread-safe. */
  ZopfliKatajainenScratch katascratch;

  /* Optimal-parse working buffers for the squeeze pass. Allocated once per
  ZopfliLZ77Optimal[Fixed] call and reused across its iterations; carried here
  so the DP helpers reach them via the block state instead of long parameter
  lists. costs/length_array/dist_array are sized to the block (blocksize + 1);
  path is the traced result, grown on demand. */
  ZopfliCost* costs;
  uint16_t* length_array;
  uint16_t* dist_array;
  uint16_t* path;
  size_t pathsize;
  size_t pathcap;
} ZopfliBlockState;

void ZopfliInitLZ77Store(const uint8_t* data, ZopfliLZ77Store* store);
void ZopfliCleanLZ77Store(const ZopfliContext* ctx, ZopfliLZ77Store* store);
/* Empties the store (size 0) but keeps its allocation, so it can be refilled
without reallocating. */
void ZopfliResetLZ77Store(ZopfliLZ77Store* store);
void ZopfliCopyLZ77Store(const ZopfliContext* ctx,
                         const ZopfliLZ77Store* source, ZopfliLZ77Store* dest);
void ZopfliStoreLitLenDist(const ZopfliContext* ctx, uint16_t length,
                           uint16_t dist, size_t pos, ZopfliLZ77Store* store);
void ZopfliAppendLZ77Store(const ZopfliContext* ctx,
                           const ZopfliLZ77Store* store,
                           ZopfliLZ77Store* target);
/* Gets the amount of raw bytes that this range of LZ77 symbols spans. */
size_t ZopfliLZ77GetByteRange(const ZopfliLZ77Store* lz77,
                              size_t lstart, size_t lend);
/* Gets the histogram of lit/len and dist symbols in the given range, using the
cumulative histograms, so faster than adding one by one for large range. Does
not add the one end symbol of value 256. */
void ZopfliLZ77GetHistogram(const ZopfliLZ77Store* lz77,
                            size_t lstart, size_t lend,
                            size_t* ll_counts, size_t* d_counts);

void ZopfliInitBlockState(const ZopfliContext* ctx,
                          size_t blockstart, size_t blockend, int add_lmc,
                          ZopfliBlockState* s);
void ZopfliCleanBlockState(ZopfliBlockState* s);

/*
Finds the longest match (length and corresponding distance) for LZ77
compression.
Even when not using "sublen", it can be more efficient to provide an array,
because only then the caching is used.
array: the data
pos: position in the data to find the match for
size: size of the data
limit: limit length to maximum this value (default should be 258). This allows
    finding a shorter dist for that length (= less extra bits). Must be
    in the range [ZOPFLI_MIN_MATCH, ZOPFLI_MAX_MATCH].
sublen: output array of 259 elements, or null. Has, for each length, the
    smallest distance required to reach this length. Only 256 of its 259 values
    are used, the first 3 are ignored (the shortest length is 3. It is purely
    for convenience that the array is made 3 longer).
*/
void ZopfliFindLongestMatch(
    ZopfliBlockState *s, const ZopfliHash* h, const uint8_t* array,
    size_t pos, size_t size, size_t limit,
    uint16_t* sublen, uint16_t* distance, uint16_t* length);

/*
Verifies if length and dist are indeed valid, only used for assertion.
*/
void ZopfliVerifyLenDist(const uint8_t* data, size_t datasize, size_t pos,
                         uint16_t dist, uint16_t length);

/*
Does LZ77 using an algorithm similar to gzip, with lazy matching, rather than
with the slow but better "squeeze" implementation.
The result is placed in the ZopfliLZ77Store.
If instart is larger than 0, it uses values before instart as starting
dictionary.
*/
void ZopfliLZ77Greedy(ZopfliBlockState* s, const uint8_t* in,
                      size_t instart, size_t inend,
                      ZopfliLZ77Store* store, ZopfliHash* h);

#endif  /* ZOPFLI_LZ77_H_ */
