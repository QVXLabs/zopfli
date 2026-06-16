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
Bounded package merge algorithm, based on the paper
"A Fast and Space-Economical Algorithm for Length-Limited Coding
Jyrki Katajainen, Alistair Moffat, Andrew Turpin".
*/

#include "katajainen.h"
#include "util.h"
#include <assert.h>
#include <stdlib.h>
#include <limits.h>

typedef struct Node Node;

/*
Nodes forming chains. Also used to represent leaves.
*/
struct Node {
  size_t weight;  /* Total weight (symbol count) of this chain. */
  Node* tail;  /* Previous node(s) of this chain, or 0 if none. */
  int count;  /* Leaf symbol index, or number of leaves before this chain. */
};

/*
Memory pool for nodes.
*/
typedef struct NodePool {
  Node* next;  /* Pointer to a free node in the pool. */
} NodePool;

/*
Initializes a chain node with the given values and marks it as in use.
*/
static void InitNode(size_t weight, int count, Node* tail, Node* node) {
  node->weight = weight;
  node->count = count;
  node->tail = tail;
}

/*
Performs a Boundary Package-Merge step. Puts a new chain in the given list. The
new chain is, depending on the weights, a leaf or a combination of two chains
from the previous list.
lists: The lists of chains.
maxbits: Number of lists.
leaves: The leaves, one per symbol.
numsymbols: Number of leaves.
pool: the node memory pool.
index: The index of the list in which a new chain or leaf is required.
*/
static void BoundaryPM(Node* (*lists)[2], Node* leaves, int numsymbols,
                       NodePool* pool, int index) {
  Node* newchain;
  Node* oldchain;
  int lastcount = lists[index][1]->count;  /* Count of last chain of list. */

  if (index == 0 && lastcount >= numsymbols) return;

  newchain = pool->next++;
  oldchain = lists[index][1];

  /* These are set up before the recursive calls below, so that there is a list
  pointing to the new node, to let the garbage collection know it's in use. */
  lists[index][0] = oldchain;
  lists[index][1] = newchain;

  if (index == 0) {
    /* New leaf node in list 0. */
    InitNode(leaves[lastcount].weight, lastcount + 1, 0, newchain);
  } else {
    size_t sum = lists[index - 1][0]->weight + lists[index - 1][1]->weight;
    if (lastcount < numsymbols && sum > leaves[lastcount].weight) {
      /* New leaf inserted in list, so count is incremented. */
      InitNode(leaves[lastcount].weight, lastcount + 1, oldchain->tail,
          newchain);
    } else {
      InitNode(sum, lastcount, lists[index - 1][1], newchain);
      /* Two lookahead chains of previous list used up, create new ones. */
      BoundaryPM(lists, leaves, numsymbols, pool, index - 1);
      BoundaryPM(lists, leaves, numsymbols, pool, index - 1);
    }
  }
}

static void BoundaryPMFinal(Node* (*lists)[2],
    Node* leaves, int numsymbols, NodePool* pool, int index) {
  int lastcount = lists[index][1]->count;  /* Count of last chain of list. */

  size_t sum = lists[index - 1][0]->weight + lists[index - 1][1]->weight;

  if (lastcount < numsymbols && sum > leaves[lastcount].weight) {
    Node* newchain = pool->next;
    Node* oldchain = lists[index][1]->tail;

    lists[index][1] = newchain;
    newchain->count = lastcount + 1;
    newchain->tail = oldchain;
  } else {
    lists[index][1]->tail = lists[index - 1][1];
  }
}

/*
Initializes each list with as lookahead chains the two leaves with lowest
weights.
*/
static void InitLists(
    NodePool* pool, const Node* leaves, int maxbits, Node* (*lists)[2]) {
  int i;
  Node* node0 = pool->next++;
  Node* node1 = pool->next++;
  InitNode(leaves[0].weight, 1, 0, node0);
  InitNode(leaves[1].weight, 2, 0, node1);
  for (i = 0; i < maxbits; i++) {
    lists[i][0] = node0;
    lists[i][1] = node1;
  }
}

/*
Converts result of boundary package-merge to the bitlengths. The result in the
last chain of the last list contains the amount of active leaves in each list.
chain: Chain to extract the bit length from (last chain from last list).
*/
static void ExtractBitLengths(Node* chain, Node* leaves, unsigned* bitlengths) {
  int counts[16] = {0};
  unsigned end = 16;
  unsigned ptr = 15;
  unsigned value = 1;
  Node* node;
  int val;

  for (node = chain; node; node = node->tail) {
    counts[--end] = node->count;
  }

  val = counts[15];
  while (ptr >= end) {
    for (; val > counts[ptr - 1]; val--) {
      bitlengths[leaves[val - 1].count] = value;
    }
    ptr--;
    value++;
  }
}

/*
Sorts leaves ascending by weight. The symbol index is packed into the low bits
of weight, so all keys are distinct and the order is unique. Inlined shell sort
(in-place, no recursion) avoids qsort's per-comparison indirect call.
*/
static void SortLeaves(Node* leaves, int num) {
  /* Knuth gaps ((3^k - 1) / 2). Capped at 121: the start gap is < num and num
     <= 288 (ZOPFLI_NUM_LL), so larger gaps never apply. Still correct if not. */
  static const uint8_t kGaps[] = { 1, 4, 13, 40, 121 };
  int g = (int)(sizeof(kGaps) / sizeof(kGaps[0])) - 1;
  /* Start at the largest gap smaller than num. */
  for (; g > 0 && kGaps[g] >= num; g--) { }
  for (; g >= 0; g--) {
    int i, gap = kGaps[g];
    for (i = gap; i < num; i++) {
      Node tmp = leaves[i];
      int j;
      for (j = i; j >= gap && leaves[j - gap].weight > tmp.weight; j -= gap) {
        leaves[j] = leaves[j - gap];
      }
      leaves[j] = tmp;
    }
  }
}

void ZopfliInitKatajainenScratch(ZopfliKatajainenScratch* scratch) {
  scratch->leaves = 0;
  scratch->leaves_cap = 0;
  scratch->nodes = 0;
  scratch->nodes_cap = 0;
  scratch->lists = 0;
  scratch->lists_cap = 0;
}

void ZopfliCleanKatajainenScratch(const ZopfliContext* ctx,
                                  ZopfliKatajainenScratch* scratch) {
  ZopfliRealloc(ctx, scratch->leaves, 0);
  ZopfliRealloc(ctx, scratch->nodes, 0);
  ZopfliRealloc(ctx, scratch->lists, 0);
}

static Node* EnsureNodes(const ZopfliContext* ctx, void** buf, size_t* cap,
                         size_t need) {
  if (need > *cap) {
    *buf = ZopfliRealloc(ctx, *buf, need * sizeof(Node));
    *cap = need;
  }
  return (Node*)*buf;
}

int ZopfliLengthLimitedCodeLengthsScratch(
    const ZopfliContext* ctx, ZopfliKatajainenScratch* scratch,
    const size_t* frequencies, int n, int maxbits, unsigned* bitlengths) {
  NodePool pool;
  int i;
  int numsymbols = 0;  /* Amount of symbols with frequency > 0. */
  int numBoundaryPMRuns;
  Node* nodes;

  /* Array of lists of chains. Each list requires only two lookahead chains at
  a time, so each list is a array of two Node*'s. */
  Node* (*lists)[2];

  /* One leaf per symbol. Only numsymbols leaves will be used. */
  Node* leaves =
      EnsureNodes(ctx, &scratch->leaves, &scratch->leaves_cap, (size_t)n);

  /* Initialize all bitlengths at 0. */
  for (i = 0; i < n; i++) {
    bitlengths[i] = 0;
  }

  /* Count used symbols and place them in the leaves. */
  for (i = 0; i < n; i++) {
    if (frequencies[i]) {
      leaves[numsymbols].weight = frequencies[i];
      leaves[numsymbols].count = i;  /* Index of symbol this leaf represents. */
      numsymbols++;
    }
  }

  /* Check special cases and error conditions. Scratch is caller-owned, so these
  just return. */
  if ((1 << maxbits) < numsymbols) {
    return 1;  /* Error, too few maxbits to represent symbols. */
  }
  if (numsymbols == 0) {
    return 0;  /* No symbols at all. OK. */
  }
  if (numsymbols == 1) {
    bitlengths[leaves[0].count] = 1;
    return 0;  /* Only one symbol, give it bitlength 1, not 0. OK. */
  }
  if (numsymbols == 2) {
    bitlengths[leaves[0].count]++;
    bitlengths[leaves[1].count]++;
    return 0;
  }

  /* Sort the leaves from lightest to heaviest. Add count into the same
  variable for stable sorting. */
  for (i = 0; i < numsymbols; i++) {
    if (leaves[i].weight >=
        ((size_t)1 << (sizeof(leaves[0].weight) * CHAR_BIT - 9))) {
      return 1;  /* Error, we need 9 bits for the count. */
    }
    leaves[i].weight = (leaves[i].weight << 9) | leaves[i].count;
  }
  SortLeaves(leaves, numsymbols);
  for (i = 0; i < numsymbols; i++) {
    leaves[i].weight >>= 9;
  }

  if (numsymbols - 1 < maxbits) {
    maxbits = numsymbols - 1;
  }

  /* Initialize node memory pool. */
  nodes = EnsureNodes(ctx, &scratch->nodes, &scratch->nodes_cap,
                      (size_t)maxbits * 2 * (size_t)numsymbols);
  pool.next = nodes;

  if ((size_t)maxbits > scratch->lists_cap) {
    ZopfliRealloc(ctx, scratch->lists, 0);
    scratch->lists =
        ZopfliRealloc(ctx, NULL, (size_t)maxbits * 2 * sizeof(Node*));
    scratch->lists_cap = scratch->lists ? (size_t)maxbits : 0;
  }
  lists = (Node* (*)[2])scratch->lists;
  InitLists(&pool, leaves, maxbits, lists);

  /* In the last list, 2 * numsymbols - 2 active chains need to be created. Two
  are already created in the initialization. Each BoundaryPM run creates one. */
  numBoundaryPMRuns = 2 * numsymbols - 4;
  for (i = 0; i < numBoundaryPMRuns - 1; i++) {
    BoundaryPM(lists, leaves, numsymbols, &pool, maxbits - 1);
  }
  BoundaryPMFinal(lists, leaves, numsymbols, &pool, maxbits - 1);

  ExtractBitLengths(lists[maxbits - 1][1], leaves, bitlengths);

  return 0;  /* OK. */
}

int ZopfliLengthLimitedCodeLengths(
    const ZopfliContext* ctx, const size_t* frequencies, int n, int maxbits,
    unsigned* bitlengths) {
  ZopfliKatajainenScratch scratch;
  int result;
  ZopfliInitKatajainenScratch(&scratch);
  result = ZopfliLengthLimitedCodeLengthsScratch(
      ctx, &scratch, frequencies, n, maxbits, bitlengths);
  ZopfliCleanKatajainenScratch(ctx, &scratch);
  return result;
}
