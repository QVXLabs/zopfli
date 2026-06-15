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

#ifndef ZOPFLI_KATAJAINEN_H_
#define ZOPFLI_KATAJAINEN_H_

#include <string.h>

/*
Reusable scratch buffers for ZopfliLengthLimitedCodeLengths, so the hot
per-block-evaluation path does not malloc/free on every call. Owned by the
caller (one per thread / compression), making reuse thread-safe. The buffers are
void* to keep the internal node type private; they grow on demand. Zero-fill a
fresh instance with ZopfliInitKatajainenScratch (or `= {0}`).
*/
typedef struct ZopfliKatajainenScratch {
  void* leaves;
  size_t leaves_cap;  /* capacity in nodes */
  void* nodes;
  size_t nodes_cap;   /* capacity in nodes */
  void* lists;        /* array of Node*[2] */
  size_t lists_cap;   /* capacity in list entries */
} ZopfliKatajainenScratch;

void ZopfliInitKatajainenScratch(ZopfliKatajainenScratch* scratch);
void ZopfliCleanKatajainenScratch(ZopfliKatajainenScratch* scratch);

/*
Outputs minimum-redundancy length-limited code bitlengths for symbols with the
given counts. The bitlengths are limited by maxbits.

The output is tailored for DEFLATE: symbols that never occur, get a bit length
of 0, and if only a single symbol occurs at least once, its bitlength will be 1,
and not 0 as would theoretically be needed for a single symbol.

frequencies: The amount of occurrences of each symbol.
n: The amount of symbols.
maxbits: Maximum bit length, inclusive.
bitlengths: Output, the bitlengths for the symbol prefix codes.
return: 0 for OK, non-0 for error.
*/
int ZopfliLengthLimitedCodeLengths(
    const size_t* frequencies, int n, int maxbits, unsigned* bitlengths);

/*
As ZopfliLengthLimitedCodeLengths, but reuses caller-owned scratch buffers
instead of allocating per call. Thread-safe as long as each thread passes its
own scratch.
*/
int ZopfliLengthLimitedCodeLengthsScratch(
    ZopfliKatajainenScratch* scratch,
    const size_t* frequencies, int n, int maxbits, unsigned* bitlengths);

#endif  /* ZOPFLI_KATAJAINEN_H_ */
