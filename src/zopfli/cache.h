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
The cache that speeds up ZopfliFindLongestMatch of lz77.c.
*/

#ifndef ZOPFLI_CACHE_H_
#define ZOPFLI_CACHE_H_

#include "util.h"

#ifdef ZOPFLI_LONGEST_MATCH_CACHE

/* run_off sentinel: this position has no full sublen stored (pool overflow). */
#define LMC_NO_SUBLEN ((unsigned)-1)

/*
Cache used by ZopfliFindLongestMatch to remember previously found length/dist
values. The sublen (best distance per shorter-than-best length) is stored as
variable-length 3-byte runs (length-3, dist-lo, dist-hi) in a shared pool;
run_off[pos] is a position's first run, ending at the run whose threshold equals
length[pos]. Storing the complete sublen lets the squeeze DP serve every
position from cache and skip rebuilding the hash after iteration 1.
all_complete clears if a position overflows the pool budget (pathological
input); those positions fall back to recomputation as over-cap ones did before.
*/
typedef struct ZopfliLongestMatchCache {
  uint16_t* length;
  uint16_t* dist;
  uint8_t* pool;  /* Shared run pool, 3 bytes per run. */
  unsigned* run_off;  /* Per pos: first run index in pool, or LMC_NO_SUBLEN. */
  size_t pool_used;  /* Next free run slot. */
  size_t pool_alloc;  /* Allocated pool slots; grows on demand up to pool_cap
      (typical blocks use ~1.5 runs/pos, far below the budget, so allocating
      the full budget up front would waste most of it). */
  size_t pool_cap;  /* Pool budget in runs; overflowing it disables caching
      for the position (all_complete = 0). */
  int all_complete;  /* 1 while every cached position has its full sublen. */
} ZopfliLongestMatchCache;

/* Initializes the ZopfliLongestMatchCache. */
void ZopfliInitCache(const ZopfliContext* ctx, size_t blocksize,
                     ZopfliLongestMatchCache* lmc);

/* Frees up the memory of the ZopfliLongestMatchCache. */
void ZopfliCleanCache(const ZopfliContext* ctx, ZopfliLongestMatchCache* lmc);

/* Stores sublen array in the cache. ctx is needed because the run pool grows
on demand. */
void ZopfliSublenToCache(const ZopfliContext* ctx,
                         const uint16_t* sublen,
                         size_t pos, size_t length,
                         ZopfliLongestMatchCache* lmc);

/* Extracts sublen array from the cache. */
void ZopfliCacheToSublen(const ZopfliLongestMatchCache* lmc,
                         size_t pos, size_t length,
                         uint16_t* sublen);

/* Returns the length up to which could be stored in the cache. Inline: it sits
on the squeeze DP's per-position fast-path trigger. */
ZOPFLI_INLINE unsigned ZopfliMaxCachedSublen(
    const ZopfliLongestMatchCache* lmc, size_t pos, size_t length) {
#if ZOPFLI_CACHE_LENGTH == 0
  return 0;
#endif
  /* length == 0 means no match is cached; LMC_NO_SUBLEN means the position
  overflowed the pool and has no full sublen. Otherwise the stored runs cover
  the whole match, so the max cached sublen is exactly length. */
  if (length == 0) return 0;
  if (lmc->run_off[pos] == LMC_NO_SUBLEN) return 0;
  return (unsigned)length;
}

#endif  /* ZOPFLI_LONGEST_MATCH_CACHE */

#endif  /* ZOPFLI_CACHE_H_ */
