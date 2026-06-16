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
*/

#include "hash.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define HASH_SHIFT 5
#define HASH_MASK 32767

/* Empty/uninitialized 16-bit hash slot. Real positions and hash values are
<= HASH_MASK (32767), so 0xFFFF can never collide with a valid entry. */
#define ZOPFLI_HASH_EMPTY ((uint16_t)-1)

void ZopfliAllocHash(const ZopfliContext* ctx, size_t window_size,
                     ZopfliHash* h) {
  /* head/head2 are indexed by the masked hash value, so only HASH_MASK + 1
  buckets are ever used (not 65536). */
  h->head =
      (uint16_t*)ZopfliRealloc(ctx, NULL, sizeof(*h->head) * (HASH_MASK + 1));
  h->prev = (uint16_t*)ZopfliRealloc(ctx, NULL, sizeof(*h->prev) * window_size);
  h->hashval =
      (uint16_t*)ZopfliRealloc(ctx, NULL, sizeof(*h->hashval) * window_size);

#ifdef ZOPFLI_HASH_SAME
  h->same = (uint16_t*)ZopfliRealloc(ctx, NULL, sizeof(*h->same) * window_size);
#endif

#ifdef ZOPFLI_HASH_SAME_HASH
  h->head2 =
      (uint16_t*)ZopfliRealloc(ctx, NULL, sizeof(*h->head2) * (HASH_MASK + 1));
  h->prev2 =
      (uint16_t*)ZopfliRealloc(ctx, NULL, sizeof(*h->prev2) * window_size);
  h->hashval2 =
      (uint16_t*)ZopfliRealloc(ctx, NULL, sizeof(*h->hashval2) * window_size);
#endif
}

void ZopfliResetHash(size_t window_size, ZopfliHash* h) {
  size_t i;

  h->val = 0;
  for (i = 0; i < HASH_MASK + 1; i++) {
    h->head[i] = ZOPFLI_HASH_EMPTY;  /* no head so far. */
  }
  for (i = 0; i < window_size; i++) {
    /* If prev[j] == j, then prev[j] is uninitialized. */
    h->prev[i] = (uint16_t)i;
    h->hashval[i] = ZOPFLI_HASH_EMPTY;
  }

#ifdef ZOPFLI_HASH_SAME
  for (i = 0; i < window_size; i++) {
    h->same[i] = 0;
  }
#endif

#ifdef ZOPFLI_HASH_SAME_HASH
  h->val2 = 0;
  for (i = 0; i < HASH_MASK + 1; i++) {
    h->head2[i] = ZOPFLI_HASH_EMPTY;
  }
  for (i = 0; i < window_size; i++) {
    h->prev2[i] = (uint16_t)i;
    h->hashval2[i] = ZOPFLI_HASH_EMPTY;
  }
#endif
}

void ZopfliCleanHash(const ZopfliContext* ctx, ZopfliHash* h) {
  ZopfliRealloc(ctx, h->head, 0);
  ZopfliRealloc(ctx, h->prev, 0);
  ZopfliRealloc(ctx, h->hashval, 0);

#ifdef ZOPFLI_HASH_SAME_HASH
  ZopfliRealloc(ctx, h->head2, 0);
  ZopfliRealloc(ctx, h->prev2, 0);
  ZopfliRealloc(ctx, h->hashval2, 0);
#endif

#ifdef ZOPFLI_HASH_SAME
  ZopfliRealloc(ctx, h->same, 0);
#endif
}

/*
Update the sliding hash value with the given byte. All calls to this function
must be made on consecutive input characters. Since the hash value exists out
of multiple input bytes, a few warmups with this function are needed initially.
*/
static void UpdateHashValue(ZopfliHash* h, uint8_t c) {
  h->val = (((h->val) << HASH_SHIFT) ^ (c)) & HASH_MASK;
}

void ZopfliUpdateHash(const uint8_t* array, size_t pos, size_t end,
                ZopfliHash* h) {
  uint16_t hpos = pos & ZOPFLI_WINDOW_MASK;
#ifdef ZOPFLI_HASH_SAME
  size_t amount = 0;
#endif

  UpdateHashValue(h, pos + ZOPFLI_MIN_MATCH <= end ?
      array[pos + ZOPFLI_MIN_MATCH - 1] : 0);
  h->hashval[hpos] = h->val;
  if (h->head[h->val] != ZOPFLI_HASH_EMPTY &&
      h->hashval[h->head[h->val]] == h->val) {
    h->prev[hpos] = h->head[h->val];
  }
  else h->prev[hpos] = hpos;
  h->head[h->val] = hpos;

#ifdef ZOPFLI_HASH_SAME
  /* Update "same". */
  if (h->same[(pos - 1) & ZOPFLI_WINDOW_MASK] > 1) {
    amount = h->same[(pos - 1) & ZOPFLI_WINDOW_MASK] - 1;
  }
  while (pos + amount + 1 < end &&
      array[pos] == array[pos + amount + 1] && amount < (uint16_t)(-1)) {
    amount++;
  }
  h->same[hpos] = (uint16_t)amount;
#endif

#ifdef ZOPFLI_HASH_SAME_HASH
  h->val2 = ((h->same[hpos] - ZOPFLI_MIN_MATCH) & 255) ^ h->val;
  h->hashval2[hpos] = h->val2;
  if (h->head2[h->val2] != ZOPFLI_HASH_EMPTY &&
      h->hashval2[h->head2[h->val2]] == h->val2) {
    h->prev2[hpos] = h->head2[h->val2];
  }
  else h->prev2[hpos] = hpos;
  h->head2[h->val2] = hpos;
#endif
}

void ZopfliWarmupHash(const uint8_t* array, size_t pos, size_t end,
                ZopfliHash* h) {
  UpdateHashValue(h, array[pos + 0]);
  if (pos + 1 < end) UpdateHashValue(h, array[pos + 1]);
}
