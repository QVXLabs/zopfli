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

#ifndef ZOPFLI_ZOPFLI_H_
#define ZOPFLI_ZOPFLI_H_

#include <stddef.h>
#include <stdint.h> /* for uint8_t */
#include <stdlib.h> /* for size_t */

#include "version.h"  /* ZOPFLI_VERSION (generated) */

#ifdef __cplusplus
extern "C" {
#endif

/* Output format */
typedef enum {
  ZOPFLI_FORMAT_GZIP,
  ZOPFLI_FORMAT_ZLIB,
  ZOPFLI_FORMAT_DEFLATE
} ZopfliFormat;

/*
Options used throughout the program.
*/
typedef struct ZopfliOptions {
  /* Whether to print output */
  int verbose;

  /* Whether to print more detailed output */
  int verbose_more;

  /*
  Times to rerun the LZ77 optimization pass. 0 (default) = auto: a size-
  dependent count, larger for larger inputs. A value > 0 forces that fixed count.
  */
  int numiterations;

  /*
  If true, splits the data in multiple deflate blocks with optimal choice
  for the block boundaries. Block splitting gives better compression. Default:
  true (1).
  */
  int blocksplitting;

  /*
  No longer used, left for compatibility.
  */
  int blocksplittinglast;

  /*
  Maximum amount of blocks to split into (0 for unlimited, but this can give
  extreme results that hurt compression on some files). Default value: 15.
  */
  int blocksplittingmax;

  /*
  Custom allocator, realloc-style: size 0 frees ptr and returns NULL; ptr NULL
  allocates; returns NULL only on real failure. alloc_context is passed back
  unchanged as the first argument. ZopfliInitOptions sets this to an internal
  default allocator (plain realloc/free); a NULL zrealloc also falls back to it.
  All of zopfli's allocations route through this.
  */
  void* (*zrealloc)(void* alloc_context, void* ptr, size_t size);
  void* alloc_context;
} ZopfliOptions;

/* Initializes options with default values. */
void ZopfliInitOptions(ZopfliOptions* options);

/*
Compresses, according to the given output format, and appends the result to the
output.

options: global program options
output_type: the output format to use
out: pointer to the dynamic output array to which the result is appended. Must
  be freed after use
outsize: pointer to the dynamic output array size
*/
void ZopfliCompress(const ZopfliOptions* options, ZopfliFormat output_type,
                    const uint8_t* in, size_t insize,
                    uint8_t** out, size_t* outsize);

/*
Compresses according to the gzip specification and appends the compressed
result to the output.

options: global program options
out: pointer to the dynamic output array to which the result is appended. Must
  be freed after use.
outsize: pointer to the dynamic output array size.
*/
void ZopfliGzipCompress(const ZopfliOptions* options,
                        const uint8_t* in, size_t insize,
                        uint8_t** out, size_t* outsize);

/*
Compresses according to the zlib specification and appends the compressed
result to the output.

options: global program options
out: pointer to the dynamic output array to which the result is appended. Must
  be freed after use.
outsize: pointer to the dynamic output array size.
*/
void ZopfliZlibCompress(const ZopfliOptions* options,
                        const uint8_t* in, size_t insize,
                        uint8_t** out, size_t* outsize);

/*
Compresses according to the deflate specification and appends the compressed
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
Like ZopfliDeflate, but allows to specify start and end byte with instart and
inend. Only that part is compressed, but earlier bytes are still used for the
back window.
*/
void ZopfliDeflatePart(const ZopfliOptions* options, int btype, int final,
                       const uint8_t* in, size_t instart, size_t inend,
                       uint8_t* bp, uint8_t** out,
                       size_t* outsize);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* ZOPFLI_ZOPFLI_H_ */
