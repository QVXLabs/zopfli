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

void ZopfliInitCache(size_t blocksize, ZopfliLongestMatchCache* lmc) {
  size_t i;
  lmc->length = (unsigned short*)malloc(sizeof(unsigned short) * blocksize);
  lmc->dist = (unsigned short*)malloc(sizeof(unsigned short) * blocksize);
  /* Rather large amount of memory. */
  lmc->sublen = (unsigned char*)malloc(ZOPFLI_CACHE_LENGTH * 3 * blocksize);
  if(lmc->sublen == NULL) {
    fprintf(stderr,
        "Error: Out of memory. Tried allocating %lu bytes of memory.\n",
        (unsigned long)ZOPFLI_CACHE_LENGTH * 3 * blocksize);
    exit (EXIT_FAILURE);
  }

  /* length > 0 and dist 0 is invalid combination, which indicates on purpose
  that this cache value is not filled in yet. sublen is intentionally left
  uninitialized: ZopfliMaxCachedSublen keys off length == 0 (no match cached),
  and the entries read for a real match are always written by ZopfliSublenToCache
  before they are read back. */
  for (i = 0; i < blocksize; i++) lmc->length[i] = 1;
  for (i = 0; i < blocksize; i++) lmc->dist[i] = 0;
}

void ZopfliCleanCache(ZopfliLongestMatchCache* lmc) {
  free(lmc->length);
  free(lmc->dist);
  free(lmc->sublen);
}

void ZopfliSublenToCache(const unsigned short* sublen,
                         size_t pos, size_t length,
                         ZopfliLongestMatchCache* lmc) {
  size_t i;
  size_t j = 0;
  unsigned bestlength = 0;
  unsigned char* cache;

#if ZOPFLI_CACHE_LENGTH == 0
  return;
#endif

  cache = &lmc->sublen[ZOPFLI_CACHE_LENGTH * pos * 3];
  if (length < 3) return;
  for (i = 3; i <= length; i++) {
    if (i == length || sublen[i] != sublen[i + 1]) {
      cache[j * 3] = i - 3;
      cache[j * 3 + 1] = sublen[i] % 256;
      cache[j * 3 + 2] = (sublen[i] >> 8) % 256;
      bestlength = i;
      j++;
      if (j >= ZOPFLI_CACHE_LENGTH) break;
    }
  }
  if (j < ZOPFLI_CACHE_LENGTH) {
    assert(bestlength == length);
    cache[(ZOPFLI_CACHE_LENGTH - 1) * 3] = bestlength - 3;
  } else {
    assert(bestlength <= length);
  }
  assert(bestlength == ZopfliMaxCachedSublen(lmc, pos, length));
}

void ZopfliCacheToSublen(const ZopfliLongestMatchCache* lmc,
                         size_t pos, size_t length,
                         unsigned short* sublen) {
  size_t i, j;
  unsigned maxlength = ZopfliMaxCachedSublen(lmc, pos, length);
  unsigned prevlength = 0;
  unsigned char* cache;
#if ZOPFLI_CACHE_LENGTH == 0
  return;
#endif
  if (length < 3) return;
  cache = &lmc->sublen[ZOPFLI_CACHE_LENGTH * pos * 3];
  for (j = 0; j < ZOPFLI_CACHE_LENGTH; j++) {
    unsigned length = cache[j * 3] + 3;
    unsigned dist = cache[j * 3 + 1] + 256 * cache[j * 3 + 2];
    for (i = prevlength; i <= length; i++) {
      sublen[i] = dist;
    }
    if (length == maxlength) break;
    prevlength = length + 1;
  }
}

int ZopfliCacheSublenRuns(const ZopfliLongestMatchCache* lmc,
                          size_t pos, unsigned maxlen,
                          unsigned short* run_maxlen,
                          unsigned short* run_dist) {
  unsigned char* cache;
  size_t j;
  int n = 0;
#if ZOPFLI_CACHE_LENGTH == 0
  return 0;
#endif
  if (maxlen < 3) return 0;
  cache = &lmc->sublen[ZOPFLI_CACHE_LENGTH * pos * 3];
  for (j = 0; j < ZOPFLI_CACHE_LENGTH; j++) {
    unsigned len = cache[j * 3] + 3;
    unsigned dist = cache[j * 3 + 1] + 256 * cache[j * 3 + 2];
    run_maxlen[n] = (unsigned short)len;
    run_dist[n] = (unsigned short)dist;
    n++;
    if (len == maxlen) break;
  }
  return n;
}

/*
Returns the length up to which could be stored in the cache.
*/
unsigned ZopfliMaxCachedSublen(const ZopfliLongestMatchCache* lmc,
                               size_t pos, size_t length) {
  unsigned char* cache;
#if ZOPFLI_CACHE_LENGTH == 0
  return 0;
#endif
  /* length == 0 means no match is cached at this position, hence no sublen.
  This replaces a sublen[1]==0 && sublen[2]==0 test so the sublen buffer no
  longer needs zero-initializing (callers pass the cached length). */
  if (length == 0) return 0;
  cache = &lmc->sublen[ZOPFLI_CACHE_LENGTH * pos * 3];
  return cache[(ZOPFLI_CACHE_LENGTH - 1) * 3] + 3;
}

#endif  /* ZOPFLI_LONGEST_MATCH_CACHE */
