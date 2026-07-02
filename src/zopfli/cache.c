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

#include "cache.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef ZOPFLI_LONGEST_MATCH_CACHE

void ZopfliInitCache(const ZopfliContext* ctx, size_t blocksize,
                     ZopfliLongestMatchCache* lmc) {
  size_t i;
  lmc->length =
      (uint16_t*)ZopfliRealloc(ctx, NULL, sizeof(uint16_t) * blocksize);
  lmc->dist = (uint16_t*)ZopfliRealloc(ctx, NULL, sizeof(uint16_t) * blocksize);
  lmc->run_off =
      (unsigned*)ZopfliRealloc(ctx, NULL, sizeof(unsigned) * blocksize);
  /* Same byte budget as the old fixed cache, used now as a shared run pool. */
  lmc->pool_cap = (size_t)ZOPFLI_CACHE_LENGTH * blocksize;
  lmc->pool_used = 0;
  lmc->all_complete = 1;
  lmc->pool = (uint8_t*)ZopfliRealloc(ctx, NULL, 3 * lmc->pool_cap);
  /* For an empty block (blocksize 0) the arrays above are zero-size and stay
  NULL; nothing below reads them. Real allocation failures abort inside
  ZopfliRealloc, so no out-of-memory check is needed here. */

  /* length > 0 and dist 0 is invalid combination, which indicates on purpose
  that this cache value is not filled in yet. pool and run_off are intentionally
  left uninitialized: ZopfliMaxCachedSublen keys off length == 0 (no match
  cached), and run_off is only read for a real match, always written by
  ZopfliSublenToCache before being read back. */
  for (i = 0; i < blocksize; i++) {
    lmc->length[i] = 1;
    lmc->dist[i] = 0;
  }
}

void ZopfliCleanCache(const ZopfliContext* ctx, ZopfliLongestMatchCache* lmc) {
  ZopfliRealloc(ctx, lmc->length, 0);
  ZopfliRealloc(ctx, lmc->dist, 0);
  ZopfliRealloc(ctx, lmc->pool, 0);
  ZopfliRealloc(ctx, lmc->run_off, 0);
}

void ZopfliSublenToCache(const uint16_t* sublen,
                         size_t pos, size_t length,
                         ZopfliLongestMatchCache* lmc) {
  size_t i;
  size_t nruns = 0;
  size_t avail;
  uint8_t* run;

#if ZOPFLI_CACHE_LENGTH == 0
  return;
#endif

  if (length < 3) return;

  /* Emit runs in a single sublen pass, straight into the pool's free space.
  On overflow nothing is committed (pool_used is unchanged and free-slot
  contents are never read), same as the position never fitting. */
  avail = lmc->pool_cap - lmc->pool_used;
  run = &lmc->pool[lmc->pool_used * 3];
  for (i = 3; i <= length; i++) {
    if (i == length || sublen[i] != sublen[i + 1]) {
      if (nruns == avail) {
        /* Overflow: keep within the pool budget. Mark incomplete and fall back
        to recomputation for this position (as an over-cap position did
        before). */
        lmc->all_complete = 0;
        lmc->run_off[pos] = LMC_NO_SUBLEN;
        return;
      }
      run[0] = (uint8_t)(i - 3);
      run[1] = sublen[i] & 0xff;
      run[2] = (sublen[i] >> 8) & 0xff;
      run += 3;
      nruns++;
    }
  }
  lmc->run_off[pos] = (unsigned)lmc->pool_used;
  lmc->pool_used += nruns;
}

void ZopfliCacheToSublen(const ZopfliLongestMatchCache* lmc,
                         size_t pos, size_t length,
                         uint16_t* sublen) {
  size_t i;
  unsigned prevlength = 0;
  uint8_t* run;
#if ZOPFLI_CACHE_LENGTH == 0
  return;
#endif
  if (length < 3) return;
  run = &lmc->pool[(size_t)lmc->run_off[pos] * 3];
  for (;;) {
    unsigned runlen = run[0] + 3;
    unsigned dist = run[1] + 256 * run[2];
    unsigned hi = ZOPFLI_MIN(runlen, (unsigned)length);
    for (i = prevlength; i <= hi; i++) {
      sublen[i] = dist;
    }
    if (runlen >= length) break;
    prevlength = runlen + 1;
    run += 3;
  }
}

#endif  /* ZOPFLI_LONGEST_MATCH_CACHE */
