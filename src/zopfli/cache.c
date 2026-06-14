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

/* run_off sentinel: this position has no full sublen stored (pool overflow). */
#define LMC_NO_SUBLEN ((unsigned)-1)

void ZopfliInitCache(size_t blocksize, ZopfliLongestMatchCache* lmc) {
  size_t i;
  lmc->length = (unsigned short*)malloc(sizeof(unsigned short) * blocksize);
  lmc->dist = (unsigned short*)malloc(sizeof(unsigned short) * blocksize);
  lmc->run_off = (unsigned*)malloc(sizeof(unsigned) * blocksize);
  /* Same byte budget as the old fixed cache, used now as a shared run pool. */
  lmc->pool_cap = (size_t)ZOPFLI_CACHE_LENGTH * blocksize;
  lmc->pool_used = 0;
  lmc->all_complete = 1;
  lmc->pool = (unsigned char*)malloc(3 * lmc->pool_cap);
  if (lmc->pool == NULL || lmc->run_off == NULL) {
    fprintf(stderr,
        "Error: Out of memory. Tried allocating %lu bytes of memory.\n",
        (unsigned long)(3 * lmc->pool_cap));
    exit (EXIT_FAILURE);
  }

  /* length > 0 and dist 0 is invalid combination, which indicates on purpose
  that this cache value is not filled in yet. pool and run_off are intentionally
  left uninitialized: ZopfliMaxCachedSublen keys off length == 0 (no match
  cached), and run_off is only read for a real match, always written by
  ZopfliSublenToCache before being read back. */
  for (i = 0; i < blocksize; i++) lmc->length[i] = 1;
  for (i = 0; i < blocksize; i++) lmc->dist[i] = 0;
}

void ZopfliCleanCache(ZopfliLongestMatchCache* lmc) {
  free(lmc->length);
  free(lmc->dist);
  free(lmc->pool);
  free(lmc->run_off);
}

void ZopfliSublenToCache(const unsigned short* sublen,
                         size_t pos, size_t length,
                         ZopfliLongestMatchCache* lmc) {
  size_t i;
  size_t nruns = 0;
  unsigned char* run;

#if ZOPFLI_CACHE_LENGTH == 0
  return;
#endif

  if (length < 3) return;

  /* Count the distinct-distance runs this position needs. */
  for (i = 3; i <= length; i++) {
    if (i == length || sublen[i] != sublen[i + 1]) nruns++;
  }

  /* Overflow: keep within the pool budget. Mark incomplete and fall back to
  recomputation for this position (as an over-cap position did before). */
  if (lmc->pool_used + nruns > lmc->pool_cap) {
    lmc->all_complete = 0;
    lmc->run_off[pos] = LMC_NO_SUBLEN;
    return;
  }

  lmc->run_off[pos] = (unsigned)lmc->pool_used;
  run = &lmc->pool[lmc->pool_used * 3];
  for (i = 3; i <= length; i++) {
    if (i == length || sublen[i] != sublen[i + 1]) {
      run[0] = (unsigned char)(i - 3);
      run[1] = sublen[i] % 256;
      run[2] = (sublen[i] >> 8) % 256;
      run += 3;
    }
  }
  lmc->pool_used += nruns;
}

void ZopfliCacheToSublen(const ZopfliLongestMatchCache* lmc,
                         size_t pos, size_t length,
                         unsigned short* sublen) {
  size_t i;
  unsigned prevlength = 0;
  unsigned char* run;
#if ZOPFLI_CACHE_LENGTH == 0
  return;
#endif
  if (length < 3) return;
  run = &lmc->pool[(size_t)lmc->run_off[pos] * 3];
  for (;;) {
    unsigned runlen = run[0] + 3;
    unsigned dist = run[1] + 256 * run[2];
    unsigned hi = runlen < length ? runlen : (unsigned)length;
    for (i = prevlength; i <= hi; i++) {
      sublen[i] = dist;
    }
    if (runlen >= length) break;
    prevlength = runlen + 1;
    run += 3;
  }
}

/*
Returns the length up to which could be stored in the cache.
*/
unsigned ZopfliMaxCachedSublen(const ZopfliLongestMatchCache* lmc,
                               size_t pos, size_t length) {
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
