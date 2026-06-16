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

#ifndef ZOPFLI_ZOPFLI_H_
#define ZOPFLI_ZOPFLI_H_

#include <stddef.h>
#include <stdint.h> /* for uint8_t */
#include <stdlib.h> /* for size_t */

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

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  /* ZOPFLI_ZOPFLI_H_ */
