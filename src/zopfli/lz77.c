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

#include "lz77.h"
#include "symbols.h"
#include "util.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void ZopfliInitLZ77Store(const uint8_t* data, ZopfliLZ77Store* store) {
  store->size = 0;
  store->cap = 0;
  store->litlens = 0;
  store->dists = 0;
  store->pos = 0;
  store->data = data;
  store->ll_counts = 0;
  store->d_counts = 0;
}

void ZopfliCleanLZ77Store(ZopfliLZ77Store* store) {
  ZopfliRealloc(store->litlens, 0);
  ZopfliRealloc(store->dists, 0);
  ZopfliRealloc(store->pos, 0);
  ZopfliRealloc(store->ll_counts, 0);
  ZopfliRealloc(store->d_counts, 0);
}

static size_t CeilDiv(size_t a, size_t b) {
  return (a + b - 1) / b;
}

/*
Grows the store's arrays to hold at least 'need' lz77 symbols, reusing the
existing allocation (geometric growth). The ll_counts/d_counts lengths are
deterministic functions of the symbol capacity, so one capacity covers all
five arrays.
*/
static void ZopfliReserveLZ77Store(ZopfliLZ77Store* store, size_t need) {
  size_t newcap, llc, dc;
  if (need <= store->cap) return;
  newcap = store->cap ? store->cap : 16;
  while (newcap < need) newcap = ZOPFLI_GROW_CAP(newcap);
  llc = ZOPFLI_NUM_LL * CeilDiv(newcap, ZOPFLI_NUM_LL);
  dc = ZOPFLI_NUM_D * CeilDiv(newcap, ZOPFLI_NUM_D);
  store->litlens = (uint16_t*)ZopfliRealloc(
      store->litlens, sizeof(*store->litlens) * newcap);
  store->dists = (uint16_t*)ZopfliRealloc(
      store->dists, sizeof(*store->dists) * newcap);
  store->pos = (size_t*)ZopfliRealloc(store->pos, sizeof(*store->pos) * newcap);
  store->ll_counts = (uint32_t*)ZopfliRealloc(
      store->ll_counts, sizeof(*store->ll_counts) * llc);
  store->d_counts = (uint32_t*)ZopfliRealloc(
      store->d_counts, sizeof(*store->d_counts) * dc);
  store->cap = newcap;
}

void ZopfliResetLZ77Store(ZopfliLZ77Store* store) {
  store->size = 0;
}

void ZopfliCopyLZ77Store(
    const ZopfliLZ77Store* source, ZopfliLZ77Store* dest) {
  size_t i;
  size_t llsize = ZOPFLI_NUM_LL * CeilDiv(source->size, ZOPFLI_NUM_LL);
  size_t dsize = ZOPFLI_NUM_D * CeilDiv(source->size, ZOPFLI_NUM_D);
  ZopfliCleanLZ77Store(dest);
  ZopfliInitLZ77Store(source->data, dest);
  dest->litlens =
      (uint16_t*)ZopfliRealloc(NULL, sizeof(*dest->litlens) * source->size);
  dest->dists =
      (uint16_t*)ZopfliRealloc(NULL, sizeof(*dest->dists) * source->size);
  dest->pos = (size_t*)ZopfliRealloc(NULL, sizeof(*dest->pos) * source->size);
  dest->ll_counts =
      (uint32_t*)ZopfliRealloc(NULL, sizeof(*dest->ll_counts) * llsize);
  dest->d_counts =
      (uint32_t*)ZopfliRealloc(NULL, sizeof(*dest->d_counts) * dsize);

  dest->size = source->size;
  dest->cap = source->size;
  for (i = 0; i < source->size; i++) {
    dest->litlens[i] = source->litlens[i];
    dest->dists[i] = source->dists[i];
    dest->pos[i] = source->pos[i];
  }
  for (i = 0; i < llsize; i++) {
    dest->ll_counts[i] = source->ll_counts[i];
  }
  for (i = 0; i < dsize; i++) {
    dest->d_counts[i] = source->d_counts[i];
  }
}

/*
Appends the length and distance to the LZ77 arrays of the ZopfliLZ77Store.
context must be a ZopfliLZ77Store*.
*/
void ZopfliStoreLitLenDist(uint16_t length, uint16_t dist,
                           size_t pos, ZopfliLZ77Store* store) {
  size_t i;
  size_t origsize = store->size;
  size_t llstart = ZOPFLI_NUM_LL * (origsize / ZOPFLI_NUM_LL);
  size_t dstart = ZOPFLI_NUM_D * (origsize / ZOPFLI_NUM_D);

  ZopfliReserveLZ77Store(store, origsize + 1);

  /* Everytime the index wraps around, a new cumulative histogram is made: we're
  keeping one histogram value per LZ77 symbol rather than a full histogram for
  each to save memory. The new chunk copies the previous chunk's totals (or
  zeros for the first chunk). */
  if (origsize % ZOPFLI_NUM_LL == 0) {
    for (i = 0; i < ZOPFLI_NUM_LL; i++) {
      store->ll_counts[origsize + i] =
          origsize == 0 ? 0 : store->ll_counts[origsize - ZOPFLI_NUM_LL + i];
    }
  }
  if (origsize % ZOPFLI_NUM_D == 0) {
    for (i = 0; i < ZOPFLI_NUM_D; i++) {
      store->d_counts[origsize + i] =
          origsize == 0 ? 0 : store->d_counts[origsize - ZOPFLI_NUM_D + i];
    }
  }

  store->litlens[origsize] = length;
  store->dists[origsize] = dist;
  store->pos[origsize] = pos;
  assert(length < 259);

  if (dist == 0) {
    store->ll_counts[llstart + length]++;
  } else {
    store->ll_counts[llstart + ZopfliGetLengthSymbol(length)]++;
    store->d_counts[dstart + ZopfliGetDistSymbol(dist)]++;
  }

  store->size = origsize + 1;
}

void ZopfliAppendLZ77Store(const ZopfliLZ77Store* store,
                           ZopfliLZ77Store* target) {
  size_t i;
  for (i = 0; i < store->size; i++) {
    ZopfliStoreLitLenDist(store->litlens[i], store->dists[i],
                          store->pos[i], target);
  }
}

size_t ZopfliLZ77GetByteRange(const ZopfliLZ77Store* lz77,
                              size_t lstart, size_t lend) {
  size_t l = lend - 1;
  if (lstart == lend) return 0;
  return lz77->pos[l] + ((lz77->dists[l] == 0) ?
      1 : lz77->litlens[l]) - lz77->pos[lstart];
}

/* Lit/len Huffman symbol at store position i, recomputed from litlens/dists
(the cached ll_symbol/d_symbol arrays were dropped to save memory). For a
literal (dist 0) the symbol is the byte value in litlens; otherwise it is the
length symbol. d_symbol is just ZopfliGetDistSymbol(dist), inlined at its (two,
dist != 0 guarded) use sites. */
static unsigned LitLenSymbol(const ZopfliLZ77Store* lz77, size_t i) {
  return lz77->dists[i] == 0
      ? lz77->litlens[i]
      : (unsigned)ZopfliGetLengthSymbol(lz77->litlens[i]);
}

static void ZopfliLZ77GetHistogramAt(const ZopfliLZ77Store* lz77, size_t lpos,
                                     size_t* ll_counts, size_t* d_counts) {
  /* The real histogram is created by using the histogram for this chunk, but
  all superfluous values of this chunk subtracted. */
  size_t llpos = ZOPFLI_NUM_LL * (lpos / ZOPFLI_NUM_LL);
  size_t dpos = ZOPFLI_NUM_D * (lpos / ZOPFLI_NUM_D);
  size_t i;
  for (i = 0; i < ZOPFLI_NUM_LL; i++) {
    ll_counts[i] = lz77->ll_counts[llpos + i];
  }
  for (i = lpos + 1; i < llpos + ZOPFLI_NUM_LL && i < lz77->size; i++) {
    ll_counts[LitLenSymbol(lz77, i)]--;
  }
  for (i = 0; i < ZOPFLI_NUM_D; i++) {
    d_counts[i] = lz77->d_counts[dpos + i];
  }
  for (i = lpos + 1; i < dpos + ZOPFLI_NUM_D && i < lz77->size; i++) {
    if (lz77->dists[i] != 0) d_counts[ZopfliGetDistSymbol(lz77->dists[i])]--;
  }
}

void ZopfliLZ77GetHistogram(const ZopfliLZ77Store* lz77,
                           size_t lstart, size_t lend,
                           size_t* ll_counts, size_t* d_counts) {
  size_t i;
  if (lstart + ZOPFLI_NUM_LL * 3 > lend) {
    memset(ll_counts, 0, sizeof(*ll_counts) * ZOPFLI_NUM_LL);
    memset(d_counts, 0, sizeof(*d_counts) * ZOPFLI_NUM_D);
    for (i = lstart; i < lend; i++) {
      ll_counts[LitLenSymbol(lz77, i)]++;
      if (lz77->dists[i] != 0) d_counts[ZopfliGetDistSymbol(lz77->dists[i])]++;
    }
  } else {
    /* Subtract the cumulative histograms at the end and the start to get the
    histogram for this range. */
    ZopfliLZ77GetHistogramAt(lz77, lend - 1, ll_counts, d_counts);
    if (lstart > 0) {
      size_t ll_counts2[ZOPFLI_NUM_LL];
      size_t d_counts2[ZOPFLI_NUM_D];
      ZopfliLZ77GetHistogramAt(lz77, lstart - 1, ll_counts2, d_counts2);

      for (i = 0; i < ZOPFLI_NUM_LL; i++) {
        ll_counts[i] -= ll_counts2[i];
      }
      for (i = 0; i < ZOPFLI_NUM_D; i++) {
        d_counts[i] -= d_counts2[i];
      }
    }
  }
}

void ZopfliInitBlockState(const ZopfliOptions* options,
                          size_t blockstart, size_t blockend, int add_lmc,
                          ZopfliBlockState* s) {
  s->options = options;
  s->blockstart = blockstart;
  s->blockend = blockend;
  ZopfliInitKatajainenScratch(&s->katascratch);
#ifdef ZOPFLI_LONGEST_MATCH_CACHE
  if (add_lmc) {
    s->lmc = (ZopfliLongestMatchCache*)ZopfliRealloc(
        NULL, sizeof(ZopfliLongestMatchCache));
    ZopfliInitCache(blockend - blockstart, s->lmc);
  } else {
    s->lmc = 0;
  }
#endif
}

void ZopfliCleanBlockState(ZopfliBlockState* s) {
  ZopfliCleanKatajainenScratch(&s->katascratch);
#ifdef ZOPFLI_LONGEST_MATCH_CACHE
  if (s->lmc) {
    ZopfliCleanCache(s->lmc);
    ZopfliRealloc(s->lmc, 0);
  }
#endif
}

/*
Gets a score of the length given the distance. Typically, the score of the
length is the length itself, but if the distance is very long, decrease the
score of the length a bit to make up for the fact that long distances use large
amounts of extra bits.

This is not an accurate score, it is a heuristic only for the greedy LZ77
implementation. More accurate cost models are employed later. Making this
heuristic more accurate may hurt rather than improve compression.

The two direct uses of this heuristic are:
-avoid using a length of 3 in combination with a long distance. This only has
 an effect if length == 3.
-make a slightly better choice between the two options of the lazy matching.

Indirectly, this affects:
-the block split points if the default of block splitting first is used, in a
 rather unpredictable way
-the first zopfli run, so it affects the chance of the first run being closer
 to the optimal output
*/
static int GetLengthScore(int length, int distance) {
  /*
  At 1024, the distance uses 9+ extra bits and this seems to be the sweet spot
  on tested files.
  */
  return distance > 1024 ? length - 1 : length;
}

void ZopfliVerifyLenDist(const uint8_t* data, size_t datasize, size_t pos,
                         uint16_t dist, uint16_t length) {

  /* TODO(lode): make this only run in a debug compile, it's for assert only. */
  size_t i;

  /* Read only by the assert below, which NDEBUG strips. */
  (void)datasize;

  assert(pos + length <= datasize);
  for (i = 0; i < length; i++) {
    if (data[pos - dist + i] != data[pos + i]) {
      assert(data[pos - dist + i] == data[pos + i]);
      break;
    }
  }
}

/*
Selects the GetMatch implementation:
  - x86 and 64-bit targets: memcmp over 16-byte chunks. memcmp with a constant
    size inlines to a wide compare (SSE on x86, ldp on aarch64) and needs no
    alignment.
  - Other (smaller) targets -- 32/16/8-bit non-x86, e.g. ARMv5/ARMv7, AVR: the
    aligned word + match-splice path below. There memcmp would emit a per-iter
    bcmp call and an unaligned word cast could fault.
ZOPFLI_FORCE_SPLICE forces the splice path on (for verifying it on x86). The
splice math is little-endian only.
*/
#if defined(ZOPFLI_FORCE_SPLICE)
# define ZOPFLI_GM_SPLICE 1
#elif !defined(__i386__) && !defined(__x86_64__) && !defined(_M_IX86) && \
      !defined(_M_X64) && !defined(ZOPFLI_NATIVE_64BIT) && \
      defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
# define ZOPFLI_GM_SPLICE 1
#endif

/*
Returns the first position >= scan where scan and match differ, looking no
further than end; the match length is the returned pointer minus scan. match is
the earlier copy being compared.

Compares wide chunks, then resolves the mismatching chunk one byte at a time.
The bounds guard keeps every wide read inside the buffer.
*/
static const uint8_t* GetMatch(const uint8_t* scan,
                                     const uint8_t* match,
                                     const uint8_t* end) {
#if defined(ZOPFLI_GM_SPLICE)
  typedef ZopfliNativeWord W;       /* splice word, two per iteration */
  const uintptr_t ws = sizeof(W);   /* native word */
  const uintptr_t step = 2 * ws;    /* bytes compared per iteration */
  uintptr_t off;

  /* Align scan to a native-word boundary, comparing the head byte by byte. */
  for (; ((uintptr_t)scan & (ws - 1)) != 0; ++scan, ++match) {
    if (scan == end || *scan != *match) return scan;
  }

  off = (uintptr_t)match & (ws - 1);
  if (off == 0) {
    /* match is aligned too: two aligned word compares per iteration. */
    for (; (uintptr_t)(end - scan) >= step; scan += step, match += step) {
      if (*(const W*)scan != *(const W*)match) break;
      if (*(const W*)(scan + ws) != *(const W*)(match + ws)) break;
    }
  } else if ((uintptr_t)(end - scan) >= step + ws) {
    /* match is misaligned by off: read aligned words from the match side and
    splice adjacent ones with the constant shift, carrying the high word as the
    accumulator so each iteration loads only the two new words. */
    const uint8_t* mp = match - off;
    const unsigned lo = (unsigned)(off * CHAR_BIT);
    const unsigned hi = (unsigned)(ws * CHAR_BIT) - lo;
    W acc = *(const W*)mp;
    for (; (uintptr_t)(end - scan) >= step + ws;
         mp += step, scan += step, match += step) {
      W n1 = *(const W*)(mp + ws);
      W n2 = *(const W*)(mp + step);
      if (*(const W*)scan != (W)((acc >> lo) | (n1 << hi))) break;
      if (*(const W*)(scan + ws) != (W)((n1 >> lo) | (n2 << hi))) break;
      acc = n2;
    }
  }
#else
  /* memcmp avoids unaligned word-cast faults; constant size still inlines. */
  for (; (size_t)(end - scan) >= 16 && memcmp(scan, match, 16) == 0;
       scan += 16, match += 16) {
  }
#endif

  /* The remaining bytes, and any mismatching chunk, one byte at a time. */
  for (; scan != end && *scan == *match; scan++, match++) {
  }

  return scan;
}

#ifdef ZOPFLI_LONGEST_MATCH_CACHE
/*
Gets distance, length and sublen values from the cache if possible.
Returns 1 if it got the values from the cache, 0 if not.
Updates the limit value to a smaller one if possible with more limited
information from the cache.
*/
static int TryGetFromLongestMatchCache(ZopfliBlockState* s,
    size_t pos, size_t* limit,
    uint16_t* sublen, uint16_t* distance, uint16_t* length) {
  /* The LMC cache starts at the beginning of the block rather than the
     beginning of the whole array. */
  size_t lmcpos = pos - s->blockstart;

  /* Length > 0 and dist 0 is invalid combination, which indicates on purpose
     that this cache value is not filled in yet. */
  uint8_t cache_available = s->lmc && (s->lmc->length[lmcpos] == 0 ||
      s->lmc->dist[lmcpos] != 0);
  uint8_t limit_ok_for_cache = cache_available &&
      (*limit == ZOPFLI_MAX_MATCH || s->lmc->length[lmcpos] <= *limit ||
      (sublen && ZopfliMaxCachedSublen(s->lmc,
          lmcpos, s->lmc->length[lmcpos]) >= *limit));

  if (s->lmc && limit_ok_for_cache && cache_available) {
    if (!sublen || s->lmc->length[lmcpos]
        <= ZopfliMaxCachedSublen(s->lmc, lmcpos, s->lmc->length[lmcpos])) {
      *length = s->lmc->length[lmcpos];
      if (*length > *limit) *length = (uint16_t)*limit;
      if (sublen) {
        ZopfliCacheToSublen(s->lmc, lmcpos, *length, sublen);
        *distance = sublen[*length];
        if (*limit == ZOPFLI_MAX_MATCH && *length >= ZOPFLI_MIN_MATCH) {
          assert(sublen[*length] == s->lmc->dist[lmcpos]);
        }
      } else {
        *distance = s->lmc->dist[lmcpos];
      }
      return 1;
    }
    /* Can't use much of the cache, since the "sublens" need to be calculated,
       but at  least we already know when to stop. */
    *limit = s->lmc->length[lmcpos];
  }

  return 0;
}

/*
Stores the found sublen, distance and length in the longest match cache, if
possible.
*/
static void StoreInLongestMatchCache(ZopfliBlockState* s,
    size_t pos, size_t limit, size_t size,
    const uint16_t* sublen,
    uint16_t distance, uint16_t length) {
  /* The LMC cache starts at the beginning of the block rather than the
     beginning of the whole array. */
  size_t lmcpos = pos - s->blockstart;

  /* Length > 0 and dist 0 is invalid combination, which indicates on purpose
     that this cache value is not filled in yet. */
  uint8_t cache_available = s->lmc && (s->lmc->length[lmcpos] == 0 ||
      s->lmc->dist[lmcpos] != 0);

  /* Cache full-limit matches, and also end-of-block positions where the limit
     was clamped to size - pos: there the stored match is the true maximum (it
     physically can't be longer), so the sublen is complete. Caching these lets
     the squeeze DP serve every position and skip the hash after iteration 1. */
  if (s->lmc && (limit == ZOPFLI_MAX_MATCH || pos + limit >= size)
      && sublen && !cache_available) {
    assert(s->lmc->length[lmcpos] == 1 && s->lmc->dist[lmcpos] == 0);
    s->lmc->dist[lmcpos] = length < ZOPFLI_MIN_MATCH ? 0 : distance;
    s->lmc->length[lmcpos] = length < ZOPFLI_MIN_MATCH ? 0 : length;
    assert(!(s->lmc->length[lmcpos] == 1 && s->lmc->dist[lmcpos] == 0));
    ZopfliSublenToCache(sublen, lmcpos, length, s->lmc);
  }
}
#endif

void ZopfliFindLongestMatch(ZopfliBlockState* s, const ZopfliHash* h,
    const uint8_t* array,
    size_t pos, size_t size, size_t limit,
    uint16_t* sublen, uint16_t* distance, uint16_t* length) {
  uint16_t hpos = pos & ZOPFLI_WINDOW_MASK, p, pp;
  uint16_t bestdist = 0;
  uint16_t bestlength = 1;
  const uint8_t* scan;
  const uint8_t* match;
  const uint8_t* arrayend;
#if ZOPFLI_MAX_CHAIN_HITS < ZOPFLI_WINDOW_SIZE
  int chain_counter = ZOPFLI_MAX_CHAIN_HITS;  /* For quitting early. */
#endif

  unsigned dist = 0;  /* Not uint16_t on purpose. */

  uint16_t* hhead = h->head;
  uint16_t* hprev = h->prev;
  uint16_t* hhashval = h->hashval;
  int hval = h->val;
  /* hhashval is read only by asserts (line ~511), which NDEBUG strips. */
  (void)hhashval;

#ifdef ZOPFLI_LONGEST_MATCH_CACHE
  if (TryGetFromLongestMatchCache(s, pos, &limit, sublen, distance, length)) {
    assert(pos + *length <= size);
    return;
  }
#endif

  assert(limit <= ZOPFLI_MAX_MATCH);
  assert(limit >= ZOPFLI_MIN_MATCH);
  assert(pos < size);

  if (size - pos < ZOPFLI_MIN_MATCH) {
    /* The rest of the code assumes there are at least ZOPFLI_MIN_MATCH bytes to
       try. */
    *length = 0;
    *distance = 0;
#ifdef ZOPFLI_LONGEST_MATCH_CACHE
    /* Cache the no-match so the squeeze DP can serve this position too. */
    StoreInLongestMatchCache(s, pos, limit, size, sublen, 0, 0);
#endif
    return;
  }

  if (pos + limit > size) {
    limit = size - pos;
  }
  arrayend = &array[pos] + limit;

  assert(hval < 65536);

  pp = hhead[hval];  /* During the whole loop, p == hprev[pp]. */
  p = hprev[pp];

  assert(pp == hpos);

  dist = p < pp ? pp - p : ((ZOPFLI_WINDOW_SIZE - p) + pp);

  /* Go through all distances. */
  while (dist < ZOPFLI_WINDOW_SIZE) {
    uint16_t currentlength = 0;

    assert(p < ZOPFLI_WINDOW_SIZE);
    assert(p == hprev[pp]);
    assert(hhashval[p] == hval);

    if (dist > 0) {
      assert(pos < size);
      assert(dist <= pos);
      scan = &array[pos];
      match = &array[pos - dist];

      /* Testing the byte at position bestlength first, goes slightly faster. */
      if (pos + bestlength >= size
          || *(scan + bestlength) == *(match + bestlength)) {

#ifdef ZOPFLI_HASH_SAME
        uint16_t same0 = h->same[pos & ZOPFLI_WINDOW_MASK];
        if (same0 > 2 && *scan == *match) {
          uint16_t same1 = h->same[(pos - dist) & ZOPFLI_WINDOW_MASK];
          uint16_t same = ZOPFLI_MIN(same0, same1);
          same = (uint16_t)ZOPFLI_MIN(same, limit);
          scan += same;
          match += same;
        }
#endif
        scan = GetMatch(scan, match, arrayend);
        currentlength = (uint16_t)(scan - &array[pos]);  /* found len. */
      }

      if (currentlength > bestlength) {
        if (sublen) {
          uint16_t j;
          for (j = bestlength + 1; j <= currentlength; j++) {
            sublen[j] = dist;
          }
        }
        bestdist = dist;
        bestlength = currentlength;
        if (currentlength >= limit) break;
      }
    }


#ifdef ZOPFLI_HASH_SAME_HASH
    /* Switch to the other hash once this will be more efficient. */
    if (hhead != h->head2 && bestlength >= h->same[hpos] &&
        h->val2 == h->hashval2[p]) {
      /* Now use the hash that encodes the length and first byte. */
      hhead = h->head2;
      hprev = h->prev2;
      hhashval = h->hashval2;
      hval = h->val2;
    }
#endif

    pp = p;
    p = hprev[p];
    if (p == pp) break;  /* Uninited prev value. */

    dist += p < pp ? pp - p : ((ZOPFLI_WINDOW_SIZE - p) + pp);

#if ZOPFLI_MAX_CHAIN_HITS < ZOPFLI_WINDOW_SIZE
    chain_counter--;
    if (chain_counter <= 0) break;
#endif
  }

#ifdef ZOPFLI_LONGEST_MATCH_CACHE
  StoreInLongestMatchCache(s, pos, limit, size, sublen, bestdist, bestlength);
#endif

  assert(bestlength <= limit);

  *distance = bestdist;
  *length = bestlength;
  assert(pos + *length <= size);
}

void ZopfliLZ77Greedy(ZopfliBlockState* s, const uint8_t* in,
                      size_t instart, size_t inend,
                      ZopfliLZ77Store* store, ZopfliHash* h) {
  size_t i = 0, j;
  uint16_t leng;
  uint16_t dist;
  int lengthscore;
  size_t windowstart = instart > ZOPFLI_WINDOW_SIZE
      ? instart - ZOPFLI_WINDOW_SIZE : 0;
  uint16_t dummysublen[259];

#ifdef ZOPFLI_LAZY_MATCHING
  /* Lazy matching. */
  unsigned prev_length = 0;
  unsigned prev_match = 0;
  int prevlengthscore;
  int match_available = 0;
#endif

  if (instart == inend) return;

  ZopfliResetHash(ZOPFLI_WINDOW_SIZE, h);
  ZopfliWarmupHash(in, windowstart, inend, h);
  for (i = windowstart; i < instart; i++) {
    ZopfliUpdateHash(in, i, inend, h);
  }

  for (i = instart; i < inend; i++) {
    ZopfliUpdateHash(in, i, inend, h);

    ZopfliFindLongestMatch(s, h, in, i, inend, ZOPFLI_MAX_MATCH, dummysublen,
                           &dist, &leng);
    lengthscore = GetLengthScore(leng, dist);

#ifdef ZOPFLI_LAZY_MATCHING
    /* Lazy matching. */
    prevlengthscore = GetLengthScore(prev_length, prev_match);
    if (match_available) {
      match_available = 0;
      if (lengthscore > prevlengthscore + 1) {
        ZopfliStoreLitLenDist(in[i - 1], 0, i - 1, store);
        if (lengthscore >= ZOPFLI_MIN_MATCH && leng < ZOPFLI_MAX_MATCH) {
          match_available = 1;
          prev_length = leng;
          prev_match = dist;
          continue;
        }
      } else {
        /* Add previous to output. */
        leng = prev_length;
        dist = prev_match;
        lengthscore = prevlengthscore;
        /* Add to output. */
        ZopfliVerifyLenDist(in, inend, i - 1, dist, leng);
        ZopfliStoreLitLenDist(leng, dist, i - 1, store);
        for (j = 2; j < leng; j++) {
          assert(i < inend);
          i++;
          ZopfliUpdateHash(in, i, inend, h);
        }
        continue;
      }
    }
    else if (lengthscore >= ZOPFLI_MIN_MATCH && leng < ZOPFLI_MAX_MATCH) {
      match_available = 1;
      prev_length = leng;
      prev_match = dist;
      continue;
    }
    /* End of lazy matching. */
#endif

    /* Add to output. */
    if (lengthscore >= ZOPFLI_MIN_MATCH) {
      ZopfliVerifyLenDist(in, inend, i, dist, leng);
      ZopfliStoreLitLenDist(leng, dist, i, store);
    } else {
      leng = 1;
      ZopfliStoreLitLenDist(in[i], 0, i, store);
    }
    for (j = 1; j < leng; j++) {
      assert(i < inend);
      i++;
      ZopfliUpdateHash(in, i, inend, h);
    }
  }
}
