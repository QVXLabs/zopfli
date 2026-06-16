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

#include "util.h"

#include "context.h"
#include "zopfli.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* Reports an allocation failure and aborts; callers never see a failed
ZopfliRealloc and need no out-of-memory checks of their own. Returns void* (it
never returns, since exit() is noreturn) so it slots into the ternary below. */
static void* ZopfliOutOfMemory(size_t size) {
  fprintf(stderr, "Error: out of memory allocating %zu bytes\n", size);
  exit(EXIT_FAILURE);
  return NULL;  /* unreachable (exit is noreturn); satisfies MSVC's checker */
}

/* Default allocator: in the realloc-style contract every zrealloc hook follows.
size 0 frees ptr and returns NULL (portable, unlike realloc(ptr, 0)); ptr NULL
allocates. OOM handling lives in ZopfliRealloc, not here. */
void* ZopfliDefaultRealloc(void* alloc_context, void* ptr, size_t size) {
  (void)alloc_context;
  return size == 0 ? (free(ptr), NULL) : realloc(ptr, size);
}

/* Context backed by the default allocator, for allocations outside any
caller-supplied options (CLI helpers, option-less size calculators, tests), so
every allocation can pass a non-NULL context. */
const ZopfliContext* ZopfliDefaultContext(void) {
  static const ZopfliContext kDefaultContext = {
    { 0, 0, 0, 1, 0, 15, ZopfliDefaultRealloc, NULL }
  };
  return &kDefaultContext;
}

void* ZopfliRealloc(const ZopfliContext* ctx, void* ptr, size_t size) {
  /* ctx is never NULL; its options always name an allocator hook. */
  void* result;
  assert(ctx && ctx->options.zrealloc);
  result = ctx->options.zrealloc(ctx->options.alloc_context, ptr, size);
  return size != 0 && !result ? ZopfliOutOfMemory(size) : result;
}

void ZopfliBufPush(const ZopfliContext* ctx, ZopfliBuf* b, uint8_t value) {
  if (b->size == b->cap) {
    b->cap = b->cap ? ZOPFLI_GROW_CAP(b->cap) : 16;
    b->data = (uint8_t*)ZopfliRealloc(ctx, b->data, b->cap);
  }
  b->data[b->size++] = value;
}

void ZopfliInitOptions(ZopfliOptions* options) {
  /* Zero everything (numiterations 0 = auto; alloc_context NULL), then set the
  non-zero defaults and point zrealloc at the internal default allocator. */
  memset(options, 0, sizeof(*options));
  options->blocksplitting = 1;
  options->blocksplittingmax = 15;
  options->zrealloc = ZopfliDefaultRealloc;
}
