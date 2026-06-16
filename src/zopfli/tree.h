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

/*
Utilities for creating and using Huffman trees.
*/

#ifndef ZOPFLI_TREE_H_
#define ZOPFLI_TREE_H_

#include <stdint.h>
#include <string.h>

#include "katajainen.h"

/*
Calculates the bitlengths for the Huffman tree, based on the counts of each
symbol.
*/
void ZopfliCalculateBitLengths(const ZopfliContext* ctx, const size_t* count,
                               size_t n, int maxbits, unsigned *bitlengths);

/*
As ZopfliCalculateBitLengths, but reuses caller-owned scratch (thread-safe when
each thread passes its own).
*/
void ZopfliCalculateBitLengthsScratch(const ZopfliContext* ctx,
                                      ZopfliKatajainenScratch* scratch,
                                      const size_t* count, size_t n, int maxbits,
                                      unsigned *bitlengths);

/*
Converts a series of Huffman tree bitlengths, to the bit values of the symbols.
*/
void ZopfliLengthsToSymbols(const ZopfliContext* ctx, const unsigned* lengths,
                            size_t n, unsigned maxbits, unsigned* symbols);

/*
Calculates the entropy (ideal bit length) of each symbol from its count, as
fixed point with `frac` fractional bits (Q`frac`, frac <= 16): bitlengths[i] =
-log2(count[i] / sum) scaled by 2^frac. Integer-only and deterministic across
CPUs. These fractional costs drive the optimal parse; they cannot encode the
DEFLATE tree (that uses ZopfliCalculateBitLengths).
*/
void ZopfliCalculateEntropy(const size_t* count, size_t n,
                            uint32_t* bitlengths, int frac);

#endif  /* ZOPFLI_TREE_H_ */
