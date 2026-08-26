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

Author: afalls@qvxlabs.com (Ardy123)
*/

#ifndef ZOPFLI_CONTEXT_H_
#define ZOPFLI_CONTEXT_H_

#include "util.h"    /* forward typedef of ZopfliContext */
#include "zopfli.h"  /* ZopfliOptions */

/*
Internal per-compression context. Holds a copy of the options (which carry the
custom allocator hook) so a single pointer threads configuration and allocation
through the code instead of many parameters. Built at each public entry point
from the caller's ZopfliOptions; util.h forward-typedefs it so ZopfliRealloc can
take it without an include cycle.
*/
struct ZopfliContext {
  ZopfliOptions options;
};

/* Builds the per-compression context from caller options, installing the
default allocator when zrealloc is NULL (the contract zopfli.h documents).
Every public entry point must use this rather than copying options bare. */
void ZopfliInitContext(const ZopfliOptions* options, ZopfliContext* ctx);

#endif  /* ZOPFLI_CONTEXT_H_ */
