/*
Copyright 2013 Google Inc. All Rights Reserved.

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

#include "zopfli.h"
#include "util.h"

#include <stdio.h>

#include "context.h"
#include "deflate.h"


/* Calculates the adler32 checksum of the data */
static unsigned adler32(const uint8_t* data, size_t size)
{
  static const unsigned sums_overflow = 5550;
  unsigned s1 = 1;
  unsigned s2 = 0;

  while (size > 0) {
    size_t amount = ZOPFLI_MIN(size, sums_overflow);
    size -= amount;
    while (amount > 0) {
      s1 += (*data++);
      s2 += s1;
      amount--;
    }
    s1 %= 65521;
    s2 %= 65521;
  }

  return (s2 << 16) | s1;
}

void ZopfliZlibCompress(const ZopfliOptions* options,
                        const uint8_t* in, size_t insize,
                        uint8_t** out, size_t* outsize) {
  uint8_t bitpointer = 0;
  unsigned checksum = adler32(in, insize);
  unsigned cmf = 120;  /* CM 8, CINFO 7. See zlib spec.*/
  unsigned flevel = 3;
  unsigned fdict = 0;
  unsigned cmfflg = 256 * cmf + fdict * 32 + flevel * 64;
  unsigned fcheck = 31 - cmfflg % 31;
  ZopfliContext ctx;
  ZopfliBuf buf;
  cmfflg += fcheck;

  ZopfliInitContext(options, &ctx);
  buf.data = *out;
  buf.size = *outsize;
  buf.cap = *outsize;

  ZopfliBufPush(&ctx, &buf, (uint8_t)((cmfflg >> 8) & 0xff));
  ZopfliBufPush(&ctx, &buf, (uint8_t)(cmfflg & 0xff));

  ZopfliDeflateBuf(options, 2 /* dynamic block */, 1 /* final */,
                   in, insize, &bitpointer, &buf);

  ZopfliBufPush(&ctx, &buf, (uint8_t)((checksum >> 24) & 0xff));
  ZopfliBufPush(&ctx, &buf, (uint8_t)((checksum >> 16) & 0xff));
  ZopfliBufPush(&ctx, &buf, (uint8_t)((checksum >> 8) & 0xff));
  ZopfliBufPush(&ctx, &buf, (uint8_t)(checksum & 0xff));

  *out = buf.data;
  *outsize = buf.size;

  if (options->verbose) {
    /* Percent removed with 2 decimals, integer-only (basis points). */
    long bp = insize ? (long)(((long long)insize - (long long)*outsize) * 10000
                              / (long long)insize) : 0;
    long abp = bp < 0 ? -bp : bp;  /* keep the sign for small negatives */
    fprintf(stderr,
            "Original Size: %zu, Zlib: %zu, Compression: %s%ld.%02ld%% Removed\n",
            insize, *outsize, bp < 0 ? "-" : "", abp / 100, abp % 100);
  }
}
