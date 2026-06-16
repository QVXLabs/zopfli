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

#include "squeeze.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "deflate.h"
#include "symbols.h"
#include "tree.h"
#include "util.h"

/*
Cost stored for a byte position that has not been reached yet. Larger than any
real accumulated cost (the shift in ZopfliGetCostShift keeps real costs below
2^29) and below the type's maximum, so it never participates in arithmetic.
*/
#define ZOPFLI_COST_SENTINEL ((ZopfliCost)1 << 30)

typedef struct SymbolStats {
  /* The literal and length symbols. */
  size_t litlens[ZOPFLI_NUM_LL];
  /* The 32 unique dist symbols, not the 32768 possible dists. */
  size_t dists[ZOPFLI_NUM_D];

  /* Entropy bit-length of each symbol, Q(shift) fixed point (the per-block cost
  shift), i.e. ideal bits scaled by 2^shift. Feeds the cost model directly. */
  uint32_t ll_symbols[ZOPFLI_NUM_LL];
  uint32_t d_symbols[ZOPFLI_NUM_D];
} SymbolStats;

typedef struct RanState {
  unsigned int m_w, m_z;
} RanState;

/*
Fixed-point cost model for the optimal parse. Costs are bit lengths scaled by
2^shift. The tables are precomputed once per forward pass so the hot inner loop
is a couple of integer lookups and an add instead of a function call. Length and
distance contributions are kept separate because a match's distance is only
known per candidate length.
*/
typedef struct CostCache {
  /* Cost of a literal byte, indexed by the byte value. */
  ZopfliCost lit_cost[256];
  /* Cost of a match's length, indexed by the length (3..ZOPFLI_MAX_MATCH). */
  ZopfliCost ll_cost[ZOPFLI_MAX_MATCH + 1];
  /* Cost of a match's distance, indexed by its distance symbol (0..29). */
  ZopfliCost d_cost[ZOPFLI_NUM_D];
  /* Smallest cost any match can have; lets the inner loop skip work. */
  ZopfliCost mincost;
} CostCache;

/* Sets everything to 0. */
static void InitStats(SymbolStats* stats) {
  memset(stats->litlens, 0, ZOPFLI_NUM_LL * sizeof(stats->litlens[0]));
  memset(stats->dists, 0, ZOPFLI_NUM_D * sizeof(stats->dists[0]));

  memset(stats->ll_symbols, 0, ZOPFLI_NUM_LL * sizeof(stats->ll_symbols[0]));
  memset(stats->d_symbols, 0, ZOPFLI_NUM_D * sizeof(stats->d_symbols[0]));
}

static void CopyStats(SymbolStats* source, SymbolStats* dest) {
  memcpy(dest->litlens, source->litlens,
         ZOPFLI_NUM_LL * sizeof(dest->litlens[0]));
  memcpy(dest->dists, source->dists, ZOPFLI_NUM_D * sizeof(dest->dists[0]));

  memcpy(dest->ll_symbols, source->ll_symbols,
         ZOPFLI_NUM_LL * sizeof(dest->ll_symbols[0]));
  memcpy(dest->d_symbols, source->d_symbols,
         ZOPFLI_NUM_D * sizeof(dest->d_symbols[0]));
}

/* result = stats1 + stats2/2 (the b/2 truncates identically to the old
(size_t)(a*1.0 + b*0.5), so this is the same blend, integer-only). */
static void AddStatFreqsHalf(const SymbolStats* stats1,
                             const SymbolStats* stats2, SymbolStats* result) {
  size_t i;
  for (i = 0; i < ZOPFLI_NUM_LL; i++) {
    result->litlens[i] = stats1->litlens[i] + stats2->litlens[i] / 2;
  }
  for (i = 0; i < ZOPFLI_NUM_D; i++) {
    result->dists[i] = stats1->dists[i] + stats2->dists[i] / 2;
  }
  result->litlens[256] = 1;  /* End symbol. */
}

static void InitRanState(RanState* state) {
  state->m_w = 1;
  state->m_z = 2;
}

/* Get random number: "Multiply-With-Carry" generator of G. Marsaglia */
static unsigned int Ran(RanState* state) {
  state->m_z = 36969 * (state->m_z & 65535) + (state->m_z >> 16);
  state->m_w = 18000 * (state->m_w & 65535) + (state->m_w >> 16);
  return (state->m_z << 16) + state->m_w;  /* 32-bit result. */
}

static void RandomizeFreqs(RanState* state, size_t* freqs, int n) {
  int i;
  for (i = 0; i < n; i++) {
    if ((Ran(state) >> 4) % 3 == 0) freqs[i] = freqs[Ran(state) % n];
  }
}

static void RandomizeStatFreqs(RanState* state, SymbolStats* stats) {
  RandomizeFreqs(state, stats->litlens, ZOPFLI_NUM_LL);
  RandomizeFreqs(state, stats->dists, ZOPFLI_NUM_D);
  stats->litlens[256] = 1;  /* End symbol. */
}

static void ClearStatFreqs(SymbolStats* stats) {
  size_t i;
  for (i = 0; i < ZOPFLI_NUM_LL; i++) stats->litlens[i] = 0;
  for (i = 0; i < ZOPFLI_NUM_D; i++) stats->dists[i] = 0;
}

int ZopfliGetCostShift(size_t blocksize) {
  /* A single position costs at most ~32 bits, so a whole block costs less than
  32 * blocksize bits. Pick the largest shift keeping the scaled total <= 2^29,
  which leaves headroom below the 2^30 sentinel and the type maximum. Smaller
  blocks get more fractional bits. The floor is 0 (not 3): big blocks simply get
  fewer fractional bits rather than overflowing. This covers blocksize up to
  2^24; in normal use blocks are far smaller (bounded by the master block size,
  ZOPFLI_MASTER_BLOCK_SIZE), so the floor is never reached. */
  int shift = 16;
  /* 64-bit so the bound is computed identically on 32- and 64-bit platforms
  (size_t is 32-bit on ILP32, where 32 * blocksize could otherwise wrap). */
  while (shift > 0 && (uint64_t)32 * blocksize > ((uint64_t)1 << (29 - shift))) {
    shift--;
  }
  /* Beyond 2^24 even shift 0 cannot hold the bound; the int cost DP would
  overflow. Only reachable with master blocks disabled and one huge block. */
  assert(((uint64_t)32 * blocksize << shift) <= ((uint64_t)1 << 29));
  return shift;
}

/* Fills in mincost: the cheapest cost any valid match can have. Only the 30
real distance symbols are considered, matching the deflate spec. */
static void SetMinMatchCost(CostCache* c) {
  int i;
  ZopfliCost minll = c->ll_cost[ZOPFLI_MIN_MATCH];
  ZopfliCost mind = c->d_cost[0];
  for (i = ZOPFLI_MIN_MATCH + 1; i <= ZOPFLI_MAX_MATCH; i++) {
    minll = ZOPFLI_MIN(minll, c->ll_cost[i]);
  }
  for (i = 1; i < 30; i++) {
    mind = ZOPFLI_MIN(mind, c->d_cost[i]);
  }
  c->mincost = minll + mind;
}

/* Builds the cache from symbol statistics (the per-iteration cost model). The
entropy in stats is already Q(shift) fixed point, so it is the scaled cost
directly; only the literal extra bits (length/dist) need the << shift. */
static void BuildStatCostCache(const SymbolStats* stats, int shift,
                               CostCache* c) {
  int i;
  for (i = 0; i < 256; i++) {
    c->lit_cost[i] = (ZopfliCost)stats->ll_symbols[i];
  }
  for (i = ZOPFLI_MIN_MATCH; i <= ZOPFLI_MAX_MATCH; i++) {
    c->ll_cost[i] = (ZopfliCost)stats->ll_symbols[ZopfliGetLengthSymbol(i)]
        + ((ZopfliCost)ZopfliGetLengthExtraBits(i) << shift);
  }
  for (i = 0; i < 30; i++) {
    c->d_cost[i] = (ZopfliCost)stats->d_symbols[i]
        + ((ZopfliCost)ZopfliGetDistSymbolExtraBits(i) << shift);
  }
  SetMinMatchCost(c);
}

/* Builds the cache that exactly matches the deflate fixed tree. The costs are
whole bit counts, so the shift is an exact multiply with no rounding. */
static void BuildFixedCostCache(int shift, CostCache* c) {
  int i;
  for (i = 0; i < 256; i++) {
    c->lit_cost[i] = (ZopfliCost)(i <= 143 ? 8 : 9) << shift;
  }
  for (i = ZOPFLI_MIN_MATCH; i <= ZOPFLI_MAX_MATCH; i++) {
    /* 7 or 8 bits for the length symbol, plus its extra bits. */
    int base = (ZopfliGetLengthSymbol(i) <= 279 ? 7 : 8)
        + ZopfliGetLengthExtraBits(i);
    c->ll_cost[i] = (ZopfliCost)base << shift;
  }
  for (i = 0; i < 30; i++) {
    /* Every dist symbol has length 5, plus its extra bits. */
    int base = 5 + ZopfliGetDistSymbolExtraBits(i);
    c->d_cost[i] = (ZopfliCost)base << shift;
  }
  SetMinMatchCost(c);
}

/*
Applies the optimal-parse cost update for a run of match lengths [klo, khi] that
all share one distance, at block index j. Shared by the cached-run fast path and
the freshly found-match path, so there is a single cost-update implementation.
*/
static void UpdateCostForRange(ZopfliCost* costs, uint16_t* length_array,
    uint16_t* dist_array, size_t j, size_t klo, size_t khi,
    uint16_t dist, ZopfliCost dcost, const CostCache* cache,
    ZopfliCost costsj, ZopfliCost mincostaddcostj) {
  size_t k;
  for (k = klo; k <= khi; k++) {
    ZopfliCost newCost;
    if (costs[j + k] <= mincostaddcostj) continue;
    newCost = cache->ll_cost[k] + dcost + costsj;
    assert(newCost >= 0);
    if (newCost < costs[j + k]) {
      assert(k <= ZOPFLI_MAX_MATCH);
      costs[j + k] = newCost;
      length_array[j + k] = (uint16_t)k;
      dist_array[j + k] = dist;
    }
  }
}

/*
Performs the forward pass for "squeeze". Gets the most optimal length to reach
every byte from a previous byte, using cost calculations.
s: the ZopfliBlockState
in: the input data array
instart: where to start
inend: where to stop (not inclusive)
cache: precomputed fixed-point costs of each lit/len/dist symbol.
length_array: output array of size (inend - instart) which will receive the best
    length to reach this byte from a previous byte.
returns the cost that was, according to the cost model, needed to get to the end.
*/
static ZopfliCost GetBestLengths(ZopfliBlockState *s,
                                    const uint8_t* in,
                                    size_t instart, size_t inend,
                                    const CostCache* cache,
                                    uint16_t* length_array,
                                    uint16_t* dist_array,
                                    ZopfliHash* h, ZopfliCost* costs,
                                    int build_hash) {
  /* Best cost to get here so far. */
  size_t blocksize = inend - instart;
  size_t i = 0, k, kend;
  uint16_t leng;
  uint16_t dist;
  uint16_t sublen[259];
  size_t windowstart = instart > ZOPFLI_WINDOW_SIZE
      ? instart - ZOPFLI_WINDOW_SIZE : 0;
  ZopfliCost result;
  ZopfliCost mincost = cache->mincost;
  ZopfliCost mincostaddcostj;

  if (instart == inend) return 0;

  /* The hash is a pure function of the input, so once the longest-match cache
  holds every position's full sublen (all_complete), later iterations serve
  every position from cache and never read the hash. build_hash is then 0 and
  the whole rebuild is skipped. */
  if (build_hash) {
    ZopfliResetHash(ZOPFLI_WINDOW_SIZE, h);
    ZopfliWarmupHash(in, windowstart, inend, h);
    for (i = windowstart; i < instart; i++) {
      ZopfliUpdateHash(in, i, inend, h);
    }
  }

  for (i = 1; i < blocksize + 1; i++) costs[i] = ZOPFLI_COST_SENTINEL;
  costs[0] = 0;  /* Because it's the start. */
  length_array[0] = 0;
  dist_array[0] = 0;

  for (i = instart; i < inend; i++) {
    size_t j = i - instart;  /* Index in the costs array and length_array. */
    if (build_hash) ZopfliUpdateHash(in, i, inend, h);

#ifdef ZOPFLI_SHORTCUT_LONG_REPETITIONS
    /* If we're in a long repetition of the same character and have more than
    ZOPFLI_MAX_MATCH characters before and after our position. Skipped when the
    hash isn't built; the cache fast path produces identical costs for these. */
    if (build_hash
        && h->same[i & ZOPFLI_WINDOW_MASK] > ZOPFLI_MAX_MATCH * 2
        && i > instart + ZOPFLI_MAX_MATCH + 1
        && i + ZOPFLI_MAX_MATCH * 2 + 1 < inend
        && h->same[(i - ZOPFLI_MAX_MATCH) & ZOPFLI_WINDOW_MASK]
            > ZOPFLI_MAX_MATCH) {
      /* Cost of a ZOPFLI_MAX_MATCH match at distance 1 (dist symbol 0). */
      ZopfliCost symbolcost = cache->ll_cost[ZOPFLI_MAX_MATCH]
          + cache->d_cost[0];
      /* The skipped positions are never stored in the cache, so this block is
      not fully cacheable: keep building the hash on every iteration. */
      if (s->lmc) s->lmc->all_complete = 0;
      /* Set the length to reach each one to ZOPFLI_MAX_MATCH, and the cost to
      the cost corresponding to that length. Doing this, we skip
      ZOPFLI_MAX_MATCH values to avoid calling ZopfliFindLongestMatch. */
      for (k = 0; k < ZOPFLI_MAX_MATCH; k++) {
        costs[j + ZOPFLI_MAX_MATCH] = costs[j] + symbolcost;
        length_array[j + ZOPFLI_MAX_MATCH] = ZOPFLI_MAX_MATCH;
        dist_array[j + ZOPFLI_MAX_MATCH] = 1;  /* Dist-1 repetition. */
        i++;
        j++;
        ZopfliUpdateHash(in, i, inend, h);
      }
    }
#endif

    /* Literal. */
    if (i + 1 <= inend) {
      ZopfliCost newCost = cache->lit_cost[in[i]] + costs[j];
      assert(newCost >= 0);
      if (newCost < costs[j + 1]) {
        costs[j + 1] = newCost;
        length_array[j + 1] = 1;
        dist_array[j + 1] = 0;  /* Literal. */
      }
    }
    mincostaddcostj = mincost + costs[j];

#ifdef ZOPFLI_LONGEST_MATCH_CACHE
    /* Fast path: when this position's match is fully cached, run the cost DP
    straight from the cache's runs instead of materializing all sublen entries.
    The trigger is exactly TryGetFromLongestMatchCache's full-sublen hit (limit
    ZOPFLI_MAX_MATCH, sublen present), so the runs equal what ZopfliFindLongest-
    Match would have produced; anything else falls through to the call below. */
    if (s->lmc) {
      size_t lmcpos = i - s->blockstart;
      unsigned cachedlen = s->lmc->length[lmcpos];
      /* cache_available: length 0, or a stored dist. Only then is the cache slot
      written (sublen is otherwise uninitialized). maxsub is computed once and
      reused for the trigger and the run extraction. */
      int avail = (cachedlen == 0 || s->lmc->dist[lmcpos] != 0);
      unsigned maxsub = avail ?
          ZopfliMaxCachedSublen(s->lmc, lmcpos, cachedlen) : 0;
      if (avail && cachedlen <= maxsub) {
        /* Walk this position's runs straight from the shared pool. The last
        run's threshold is cachedlen, so the klo > kend test always terminates
        before reading past it. */
        if (cachedlen >= ZOPFLI_MIN_MATCH) {
          const uint8_t* run =
              &s->lmc->pool[(size_t)s->lmc->run_off[lmcpos] * 3];
          size_t klo;
          kend = ZOPFLI_MIN((size_t)cachedlen, inend - i);
          for (klo = 3; klo <= kend; run += 3) {
            size_t runlen = (size_t)run[0] + 3;
            uint16_t rundist = (uint16_t)(run[1] + 256 * run[2]);
            size_t khi = ZOPFLI_MIN(runlen, kend);
            if (klo <= khi) {
              ZopfliCost dcost = cache->d_cost[ZopfliGetDistSymbol(rundist)];
              UpdateCostForRange(costs, length_array, dist_array, j, klo, khi,
                                 rundist, dcost, cache, costs[j],
                                 mincostaddcostj);
            }
            klo = runlen + 1;
          }
        }
        continue;
      }
    }
#endif

    /* Reaching the slow path means this position wasn't fully cached, so the
    hash must have been built this pass. */
    assert(build_hash);
    ZopfliFindLongestMatch(s, h, in, i, inend, ZOPFLI_MAX_MATCH, sublen,
                           &dist, &leng);

    /* Lengths. sublen[k] is piecewise-constant, so group equal-distance runs
    and apply each run's cost once. */
    kend = ZOPFLI_MIN(leng, inend - i);
    k = 3;
    while (k <= kend) {
      uint16_t rundist = sublen[k];
      size_t khi = k;
      ZopfliCost dcost;
      while (khi + 1 <= kend && sublen[khi + 1] == rundist) khi++;
      dcost = cache->d_cost[ZopfliGetDistSymbol(rundist)];
      UpdateCostForRange(costs, length_array, dist_array, j, k, khi,
                         rundist, dcost, cache, costs[j], mincostaddcostj);
      k = khi + 1;
    }
  }

  assert(costs[blocksize] >= 0);
  result = costs[blocksize];

  return result;
}

/*
Calculates the optimal path of lz77 lengths to use from the calculated
length_array. The length_array must contain the optimal length to reach that
byte. The path will be filled with the lengths to use, so its data size will be
the number of lz77 symbols.
*/
static void TraceBackwards(size_t size, const uint16_t* length_array,
                           uint16_t** path, size_t* pathsize,
                           size_t* pathcap) {
  size_t index = size;
  *pathsize = 0;  /* Reuse the buffer (kept allocated) across iterations. */
  if (size == 0) return;
  for (;;) {
    if (*pathsize == *pathcap) {
      *pathcap = *pathcap ? ZOPFLI_GROW_CAP(*pathcap) : 16;
      *path = (uint16_t*)ZopfliRealloc(*path, *pathcap * sizeof(**path));
    }
    (*path)[(*pathsize)++] = length_array[index];
    assert(length_array[index] <= index);
    assert(length_array[index] <= ZOPFLI_MAX_MATCH);
    assert(length_array[index] != 0);
    index -= length_array[index];
    if (index == 0) break;
  }

  /* Mirror result. */
  for (index = 0; index < *pathsize / 2; index++) {
    uint16_t temp = (*path)[index];
    (*path)[index] = (*path)[*pathsize - index - 1];
    (*path)[*pathsize - index - 1] = temp;
  }
}

/*
Outputs the lz77 symbols for the chosen path. Distances were already computed
during GetBestLengths (dist_array, parallel to length_array), so unlike the
forward pass, this needs no hash and no match recalculation.
*/
static void FollowPath(const uint8_t* in, size_t instart, size_t inend,
                       const uint16_t* path, size_t pathsize,
                       const uint16_t* dist_array,
                       ZopfliLZ77Store* store) {
  size_t i, pos = instart;
  size_t cur = 0;  /* Position within the block, i.e. pos - instart. */

  if (instart == inend) return;

  for (i = 0; i < pathsize; i++) {
    uint16_t length = path[i];
    assert(pos < inend);
    if (length >= ZOPFLI_MIN_MATCH) {
      uint16_t dist = dist_array[cur + length];
      ZopfliVerifyLenDist(in, inend, pos, dist, length);
      ZopfliStoreLitLenDist(length, dist, pos, store);
    } else {
      length = 1;
      ZopfliStoreLitLenDist(in[pos], 0, pos, store);
    }
    assert(pos + length <= inend);
    cur += length;
    pos += length;
  }
}

/* Calculates the entropy of the statistics at the block's cost shift (Q(shift)
fixed point), so the result feeds the cost model directly. */
static void CalculateStatistics(SymbolStats* stats, int shift) {
  ZopfliCalculateEntropy(stats->litlens, ZOPFLI_NUM_LL, stats->ll_symbols, shift);
  ZopfliCalculateEntropy(stats->dists, ZOPFLI_NUM_D, stats->d_symbols, shift);
}

/* Appends the symbol statistics from the store. */
static void GetStatistics(const ZopfliLZ77Store* store, SymbolStats* stats,
                          int shift) {
  size_t i;
  for (i = 0; i < store->size; i++) {
    if (store->dists[i] == 0) {
      stats->litlens[store->litlens[i]]++;
    } else {
      stats->litlens[ZopfliGetLengthSymbol(store->litlens[i])]++;
      stats->dists[ZopfliGetDistSymbol(store->dists[i])]++;
    }
  }
  stats->litlens[256] = 1;  /* End symbol. */

  CalculateStatistics(stats, shift);
}

/*
Does a single run for ZopfliLZ77Optimal. For good compression, repeated runs
with updated statistics should be performed.
s: the block state
in: the input data array
instart: where to start
inend: where to stop (not inclusive)
path: pointer to dynamically allocated memory to store the path
pathsize: pointer to the size of the dynamic path array
length_array: array of size (inend - instart) used to store lengths
cache: precomputed fixed-point cost model for this squeeze run
store: place to output the LZ77 data
returns the cost that was, according to the cost model, needed to get to the end.
    This is not the actual cost.
*/
static ZopfliCost LZ77OptimalRun(ZopfliBlockState* s,
    const uint8_t* in, size_t instart, size_t inend,
    uint16_t** path, size_t* pathsize, size_t* pathcap,
    uint16_t* length_array, uint16_t* dist_array,
    const CostCache* cache, ZopfliLZ77Store* store,
    ZopfliHash* h, ZopfliCost* costs, int build_hash) {
  ZopfliCost cost = GetBestLengths(s, in, instart, inend, cache,
                length_array, dist_array, h, costs, build_hash);
  TraceBackwards(inend - instart, length_array, path, pathsize, pathcap);
  FollowPath(in, instart, inend, *path, *pathsize, dist_array, store);
  assert(cost < ZOPFLI_COST_SENTINEL);
  return cost;
}

void ZopfliLZ77Optimal(ZopfliBlockState *s,
                       const uint8_t* in, size_t instart, size_t inend,
                       int numiterations,
                       ZopfliLZ77Store* store) {
  /* Dist to get to here with smallest cost. */
  size_t blocksize = inend - instart;
  uint16_t* length_array =
      (uint16_t*)ZopfliRealloc(NULL, sizeof(uint16_t) * (blocksize + 1));
  uint16_t* dist_array =
      (uint16_t*)ZopfliRealloc(NULL, sizeof(uint16_t) * (blocksize + 1));
  uint16_t* path = 0;
  size_t pathsize = 0;
  size_t pathcap = 0;
  ZopfliLZ77Store currentstore;
  ZopfliHash hash;
  ZopfliHash* h = &hash;
  SymbolStats stats, beststats, laststats;
  int i;
  ZopfliCost* costs =
      (ZopfliCost*)ZopfliRealloc(NULL, sizeof(*costs) * (blocksize + 1));
  int shift = ZopfliGetCostShift(blocksize);
  CostCache cache;
  uint32_t cost;
  uint32_t bestcost = ZOPFLI_LARGE_COST;
  uint32_t lastcost = 0;
  /* Try randomizing the costs a bit once the size stabilizes. */
  RanState ran_state;
  int lastrandomstep = -1;
  int build_hash;

  InitRanState(&ran_state);
  InitStats(&stats);
  ZopfliInitLZ77Store(in, &currentstore);
  ZopfliAllocHash(ZOPFLI_WINDOW_SIZE, h);

  /* Do regular deflate, then loop multiple shortest path runs, each time using
  the statistics of the previous run. */

  /* Initial run. */
  ZopfliLZ77Greedy(s, in, instart, inend, &currentstore, h);
  GetStatistics(&currentstore, &stats, shift);

  /* Repeat statistics with each time the cost model from the previous stat
  run. Iteration 0 fills the cache gaps the greedy pass left, so it needs the
  hash; later iterations skip it once the cache holds every position. */
  build_hash = 1;
  for (i = 0; i < numiterations; i++) {
    ZopfliResetLZ77Store(&currentstore);
    BuildStatCostCache(&stats, shift, &cache);
    LZ77OptimalRun(s, in, instart, inend, &path, &pathsize, &pathcap,
                   length_array, dist_array, &cache, &currentstore, h, costs,
                   build_hash);
    if (i == 0) build_hash = !(s->lmc && s->lmc->all_complete);
    cost = ZopfliCalculateBlockSizeScratch(&s->katascratch, &currentstore, 0,
                                           currentstore.size, 2);
    if (s->options->verbose_more || (s->options->verbose && cost < bestcost)) {
      fprintf(stderr, "Iteration %d: %d bit\n", i, (int) cost);
    }
    if (cost < bestcost) {
      /* Copy to the output store. */
      ZopfliCopyLZ77Store(&currentstore, store);
      CopyStats(&stats, &beststats);
      bestcost = cost;
    }
    CopyStats(&stats, &laststats);
    ClearStatFreqs(&stats);
    GetStatistics(&currentstore, &stats, shift);
    if (lastrandomstep != -1) {
      /* This makes it converge slower but better. Do it only once the
      randomness kicks in so that if the user does few iterations, it gives a
      better result sooner. */
      AddStatFreqsHalf(&stats, &laststats, &stats);
      CalculateStatistics(&stats, shift);
    }
    if (i > 5 && cost == lastcost) {
      CopyStats(&beststats, &stats);
      RandomizeStatFreqs(&ran_state, &stats);
      CalculateStatistics(&stats, shift);
      lastrandomstep = i;
    }
    lastcost = cost;
  }

  ZopfliRealloc(length_array, 0);
  ZopfliRealloc(dist_array, 0);
  ZopfliRealloc(path, 0);
  ZopfliRealloc(costs, 0);
  ZopfliCleanLZ77Store(&currentstore);
  ZopfliCleanHash(h);
}

void ZopfliLZ77OptimalFixed(ZopfliBlockState *s,
                            const uint8_t* in,
                            size_t instart, size_t inend,
                            ZopfliLZ77Store* store)
{
  /* Dist to get to here with smallest cost. */
  size_t blocksize = inend - instart;
  uint16_t* length_array =
      (uint16_t*)ZopfliRealloc(NULL, sizeof(uint16_t) * (blocksize + 1));
  uint16_t* dist_array =
      (uint16_t*)ZopfliRealloc(NULL, sizeof(uint16_t) * (blocksize + 1));
  uint16_t* path = 0;
  size_t pathsize = 0;
  size_t pathcap = 0;
  ZopfliHash hash;
  ZopfliHash* h = &hash;
  ZopfliCost* costs =
      (ZopfliCost*)ZopfliRealloc(NULL, sizeof(*costs) * (blocksize + 1));
  CostCache cache;

  ZopfliAllocHash(ZOPFLI_WINDOW_SIZE, h);

  s->blockstart = instart;
  s->blockend = inend;

  /* Shortest path for fixed tree This one should give the shortest possible
  result for fixed tree, no repeated runs are needed since the tree is known. */
  BuildFixedCostCache(ZopfliGetCostShift(blocksize), &cache);
  LZ77OptimalRun(s, in, instart, inend, &path, &pathsize, &pathcap,
                 length_array, dist_array, &cache, store, h, costs, 1);

  ZopfliRealloc(length_array, 0);
  ZopfliRealloc(dist_array, 0);
  ZopfliRealloc(path, 0);
  ZopfliRealloc(costs, 0);
  ZopfliCleanHash(h);
}
