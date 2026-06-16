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

#ifndef ZOPFLI_DEFLATE_H_
#define ZOPFLI_DEFLATE_H_

/*
Functions to compress according to the DEFLATE specification, using the
"squeeze" LZ77 compression backend.
*/

#include <stdint.h>

#include "lz77.h"
#include "util.h"
#include "zopfli.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
Compresses according to the deflate specification and append the compressed
result to the output.
This function will usually output multiple deflate blocks. If final is 1, then
the final bit will be set on the last block.

options: global program options
btype: the deflate block type. Use 2 for best compression.
  -0: non compressed blocks (00)
  -1: blocks with fixed tree (01)
  -2: blocks with dynamic tree (10)
final: whether this is the last section of the input, sets the final bit to the
  last deflate block.
in: the input bytes
insize: number of input bytes
bp: bit pointer for the output array. This must initially be 0, and for
  consecutive calls must be reused (it can have values from 0-7). This is
  because deflate appends blocks as bit-based data, rather than on byte
  boundaries.
out: pointer to the dynamic output array to which the result is appended. Must
  be freed after use.
outsize: pointer to the dynamic output array size.
*/
void ZopfliDeflate(const ZopfliOptions* options, int btype, int final,
                   const uint8_t* in, size_t insize,
                   uint8_t* bp, uint8_t** out, size_t* outsize);

/*
Like ZopfliDeflate, but appends to a golden-ratio-growing ZopfliBuf instead of an
(out, outsize) pair. Internal: lets the gzip/zlib containers share one growable
buffer (and its capacity) with the deflate stream rather than crossing the
(out, outsize) boundary on every call.
*/
void ZopfliDeflateBuf(const ZopfliOptions* options, int btype, int final,
                      const uint8_t* in, size_t insize,
                      uint8_t* bp, ZopfliBuf* buf);

/*
Like ZopfliDeflate, but allows to specify start and end byte with instart and
inend. Only that part is compressed, but earlier bytes are still used for the
back window.
*/
void ZopfliDeflatePart(const ZopfliOptions* options, int btype, int final,
                       const uint8_t* in, size_t instart, size_t inend,
                       uint8_t* bp, uint8_t** out,
                       size_t* outsize);

/*
Calculates block size in bits.
litlens: lz77 lit/lengths
dists: ll77 distances
lstart: start of block
lend: end of block (not inclusive)
*/
uint32_t ZopfliCalculateBlockSize(const ZopfliLZ77Store* lz77,
                                  size_t lstart, size_t lend, int btype);

/*
As ZopfliCalculateBlockSize, but reuses caller-owned scratch (thread-safe when
each thread passes its own).
*/
uint32_t ZopfliCalculateBlockSizeScratch(const ZopfliContext* ctx,
                                         ZopfliKatajainenScratch* scratch,
                                         const ZopfliLZ77Store* lz77,
                                         size_t lstart, size_t lend, int btype);

/*
Calculates block size in bits, automatically using the best btype.
*/
uint32_t ZopfliCalculateBlockSizeAutoType(const ZopfliLZ77Store* lz77,
                                          size_t lstart, size_t lend);

/*
As ZopfliCalculateBlockSizeAutoType, but reuses caller-owned scratch.
*/
uint32_t ZopfliCalculateBlockSizeAutoTypeScratch(
    const ZopfliContext* ctx, ZopfliKatajainenScratch* scratch,
    const ZopfliLZ77Store* lz77, size_t lstart, size_t lend);

/*
Heuristic used by the auto-type block writer: whether to run the expensive
optimal-fixed-tree exploration for a block. Worth it for small blocks, or blocks
already pretty good with a fixed tree (fixedcost <= dyncost * 1.1, as x10 <=
x11). Uses a 32-bit compare when the master block size bounds costs so the x10
and x11 products fit in uint32_t, and widens to 64-bit otherwise. Inline
(header) so it is testable without an external symbol the optimizer/LTO could
inline away.
*/
ZOPFLI_INLINE int ZopfliUseExpensiveFixed(size_t blocksize, uint32_t fixedcost,
                                          uint32_t dyncost) {
#if ZOPFLI_MASTER_BLOCK_SIZE != 0 && \
    (ZOPFLI_MASTER_BLOCK_SIZE * 32 * 11 <= 0xFFFFFFFF)
  /* A block's cost is < 32 * blocksize bits and blocksize <= the master block
  size, so dyncost * 11 fits in uint32_t here: the 32-bit compare is exact. */
  return blocksize < 1000 || fixedcost * 10 <= dyncost * 11;
#else
  /* Master blocks disabled or large enough that the products (costs are < 2^31
  bits, so * 11 is ~2^35) could exceed uint32_t; widen to 64-bit. */
  return blocksize < 1000 ||
      (uint64_t)fixedcost * 10 <= (uint64_t)dyncost * 11;
#endif
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* ZOPFLI_DEFLATE_H_ */
