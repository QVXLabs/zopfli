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
Author: afalls@qvxlabs.com (Ardy123)
*/

#include "tree.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "katajainen.h"
#include "util.h"

void ZopfliLengthsToSymbols(const ZopfliContext* ctx, const unsigned* lengths,
                            size_t n, unsigned maxbits, unsigned* symbols) {
  size_t* bl_count =
      (size_t*)ZopfliRealloc(ctx, NULL, sizeof(size_t) * (maxbits + 1));
  size_t* next_code =
      (size_t*)ZopfliRealloc(ctx, NULL, sizeof(size_t) * (maxbits + 1));
  unsigned bits, i;
  unsigned code;

  for (i = 0; i < n; i++) {
    symbols[i] = 0;
  }

  /* 1) Count the number of codes for each code length. Let bl_count[N] be the
  number of codes of length N, N >= 1. */
  for (bits = 0; bits <= maxbits; bits++) {
    bl_count[bits] = 0;
  }
  for (i = 0; i < n; i++) {
    assert(lengths[i] <= maxbits);
    bl_count[lengths[i]]++;
  }
  /* 2) Find the numerical value of the smallest code for each code length. */
  code = 0;
  bl_count[0] = 0;
  for (bits = 1; bits <= maxbits; bits++) {
    code = (unsigned)((code + bl_count[bits-1]) << 1);
    next_code[bits] = code;
  }
  /* 3) Assign numerical values to all codes, using consecutive values for all
  codes of the same length with the base values determined at step 2. */
  for (i = 0;  i < n; i++) {
    unsigned len = lengths[i];
    if (len != 0) {
      symbols[i] = (unsigned)next_code[len];
      next_code[len]++;
    }
  }

  ZopfliRealloc(ctx, bl_count, 0);
  ZopfliRealloc(ctx, next_code, 0);
}

/*
log2(x) as a Q`frac` fixed-point value (frac <= 16, x >= 1). Integer-only and
deterministic. clz gives both the integer part (31 - clz) and the shift that
normalizes the mantissa to Q31 [2^31, 2^32). The fractional part is the classic
square-and-compare on that mantissa (its square needs 64 bits). One guard bit is
computed then rounded.
*/
static uint32_t IntLog2Fixed(uint32_t x, int frac) {
  int lz = ZopfliCLZ32(x);                /* x >= 1, so 0 <= lz <= 31. */
  uint64_t m = (uint64_t)x << lz;         /* Q31 mantissa, [2^31, 2^32). */
  uint32_t fracpart = 0;
  int b;
  /* frac <= 16 keeps fracpart's bits and the final << frac within range;
  guaranteed by the caller (the cost shift is 0..16). */
  assert(frac >= 0 && frac <= 16);
  for (b = 0; b <= frac; b++) {  /* frac + 1 bits (one guard bit). */
    m = (m * m) >> 31;          /* square, keep Q31; now in [2^31, 2^33). */
    fracpart <<= 1;
    if (m >> 32) { fracpart |= 1; m >>= 1; }  /* mantissa^2 >= 2 -> emit a bit. */
  }
  fracpart = (fracpart + 1) >> 1;  /* round the guard bit away. */
  return ((unsigned)(31 - lz) << frac) + fracpart;
}

void ZopfliCalculateEntropy(const size_t* count, size_t n,
                            uint32_t* bitlengths, int frac) {
  uint32_t sum = 0;
  unsigned i;
  uint32_t log2sum;
  for (i = 0; i < n; ++i) {
    sum += (uint32_t)count[i];
  }
  log2sum = IntLog2Fixed(sum == 0 ? (uint32_t)n : sum, frac);
  for (i = 0; i < n; ++i) {
    /* When the count is 0 but its cost is requested, the symbol will appear at
    least once, so cost it as count 1: log2sum - log2(1) = log2sum. log2 is
    monotonic so log2(count) <= log2sum, hence the result is exactly >= 0. */
    bitlengths[i] = count[i] == 0
        ? log2sum
        : log2sum - IntLog2Fixed((uint32_t)count[i], frac);
  }
}

void ZopfliCalculateBitLengthsScratch(const ZopfliContext* ctx,
                                      ZopfliKatajainenScratch* scratch,
                                      const size_t* count, size_t n, int maxbits,
                                      unsigned* bitlengths) {
  int error = ZopfliLengthLimitedCodeLengthsScratch(
      ctx, scratch, count, (int)n, maxbits, bitlengths);
  (void) error;
  assert(!error);
}

void ZopfliCalculateBitLengths(const ZopfliContext* ctx, const size_t* count,
                               size_t n, int maxbits, unsigned* bitlengths) {
  ZopfliKatajainenScratch scratch;
  ZopfliInitKatajainenScratch(&scratch);
  ZopfliCalculateBitLengthsScratch(ctx, &scratch, count, n, maxbits,
                                   bitlengths);
  ZopfliCleanKatajainenScratch(ctx, &scratch);
}
