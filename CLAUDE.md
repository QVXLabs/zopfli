# CLAUDE.md — Zopfli

Repo-specific guidance. General coding preferences live in the user-global
CLAUDE.md; this file adds what's specific to this codebase.

## Language & style
- **C89 / ANSI** — built with `-ansi -pedantic -W -Wall -Wextra`. No C99/C11:
  - Declarations at the top of a block (no mixed declarations and statements).
  - No VLAs. No `//` line comments — use `/* */` block comments.
  - For `inline`, use a compiler-guarded macro (e.g. `__inline__` under
    GCC/Clang), never bare `inline`.
- **80-column hard wrap** for source. Exception: Makefile flag lists, where
  breaking would harm grep/readability.
- Comments concise, matching the existing files; explain *why*, not *what*.
- Public API is `Zopfli`-prefixed and declared in headers; internal helpers are
  `static`.
- When you modify a source file that carries an `Author:` block, append:
  `Author: afalls@qvxlabs.com (Ardavon Falls)`

## Build & test
- **Two build systems:**
  - `make` → the release build: `-O3 -DNDEBUG -fPIC`. `NDEBUG` strips asserts and
    the `ZopfliVerifyLenDist` check. `make CFLAGS=-UNDEBUG` re-enables them.
  - CMake → honors `CMAKE_BUILD_TYPE`. `Release`/`RelWithDebInfo`/`MinSizeRel`
    auto-add `-DNDEBUG`; `Debug` keeps asserts. The project intentionally does
    not default the build type.
- **Tests:** `cmake-build-debug/` (Debug → asserts ON), C++11 / GoogleTest via
  conan's GTest provider; run with `ctest`. Don't use `gtest_*` CMake helper
  functions. Keep asserts enabled in the test build.

## Performance-work discipline
This repo is an optimization effort. For any perf change:
- **Keep output byte-identical** to the pre-change baseline unless explicitly
  agreed otherwise. Verify with `md5` of compressed output in **both regimes** —
  default (`-i15`) and high-iteration (`--i200`) — on text and binary inputs.
- Validate with **asserts ON** (build without `-DNDEBUG`): `ZopfliVerifyLenDist`
  then checks every emitted match — a free per-match correctness net.
- Run the `ctest` suite.
- **Single-threaded only.** Never propose or add threads/OpenMP/parallelism,
  even though blocks and iterations are independent. (Explicit project decision.)

## Benchmarking on this machine
- Wall-clock/user-CPU is dominated by background CPU contention (a `b2`/conan
  build at 100%, CLion/Rider). The same binary has varied 26–53 s for one run.
- Before timing: `ps -Ao pcpu,comm | sort -rn | head` and ensure no
  `b2`/`cc`/`clang`/`ninja`/`cmake` is busy. Interleave runs (A/B/A/B…) and take
  the **minimum**.
- For small deltas, the `sample` profiler's **relative function shares** are more
  reliable than wall-clock (load-independent). gprof is unavailable (clang
  rejects `-pg` on macOS); use `sample`.

## Architecture notes (hot path)
- The cost model in `squeeze.c` is **fixed-point `int`** (`ZopfliCost`, per-block
  shift from `ZopfliGetCostShift`). `ZopfliCalculateEntropy` and
  `ZopfliCalculateBlockSize` remain floating-point on purpose.
- Nearly all time is in `ZopfliLZ77Optimal` → `LZ77OptimalRun` →
  `GetBestLengths` (forward DP: hash bookkeeping + `ZopfliFindLongestMatch` +
  cost DP). `FollowPath` no longer touches the hash — distances are carried
  forward in `dist_array` alongside `length_array`.
- Regimes differ: default iterations are match-finding + katajainen bound; high
  iterations are squeeze-inner-loop + hash bound.
- The remaining `GetBestLengths` per-byte `ZopfliUpdateHash` (~20% high-iter)
  cannot be safely skipped: `ZopfliFindLongestMatch` can still miss the
  longest-match cache (matches with >8 distinct sub-distances overflow the
  8-entry sublen cache), and the `prev[]` chain is all-or-nothing per pass.

## Tried and rejected (don't redo without new evidence)
- `static inline` of `ZopfliUpdateHash`: measured ~3% but reverted — not worth
  the complexity.
- One-step-ahead `__builtin_prefetch` in `ZopfliUpdateHash`: regressed ~8%
  (prefetch distance too short).
- Making entropy fixed-point: ~0% (entropy is ~0.04% of runtime; its precision
  is already discarded by `ScaleCost`). Only relevant for cross-platform
  determinism, which would also require converting `ZopfliCalculateBlockSize`.
- Ratio lever `ZOPFLI_MAX_CHAIN_HITS` 8192→32768: 0% size change on a mixed
  corpus and on constructed pathological input — the cap effectively never binds
  within the 32 KB window. Only costs speed.
- Ratio lever: making `FindMinimum`'s block-split search finer (more samples +
  exhaustive converged-window scan): +0.03% (worse). `SplitCost` minimizes a
  *greedy*-LZ77 proxy while output uses optimal LZ77; precise proxy-minimization
  overfits and diverges. The coarse 9-point search is intentional tuning.
