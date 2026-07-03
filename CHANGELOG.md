# Changelog

All notable changes to this project are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html). New
changes go under a new top section as they land.

## [1.1.1] - 2026-07-02

### Fixed
- Library: a negative `ZopfliOptions.numiterations` silently produced a
  stream that inflates to less than the input in release builds (the
  iteration loop never ran, emitting empty block bodies). Non-positive
  values now select the auto default, matching the documented 0 semantics.
- CLI: write failures (disk full, closed pipe) were ignored — truncated
  output with exit status 0. `fwrite`/`fclose`/`fflush` are now checked and
  any failure exits nonzero, as do missing input files and argument errors.
- CLI: `--i` values are validated (`strtol`): junk suffixes like `--i5x`
  (previously parsed as 5) and out-of-range counts (previously undefined
  behavior via `atoi`, in practice an unbounded run) are rejected.
- Internal: Huffman code-length construction errors are no longer swallowed
  in release builds (they would encode an invalid all-zero code); the
  encoder now fails loudly. Unreachable for valid inputs.

### Changed
- Behavior-preserving simplifications (dead fields/conditions/temporaries,
  deduplicated tree-combo selection and split-point conversion); output
  verified bit-identical.
- Tests: round-trip verification now runs in Linux CI (zlib installed); raw
  DEFLATE round-trip coverage added; CLI error paths covered by a ctest
  script; new `ZOPFLI_SANITIZE` CMake option and an ASan/UBSan CI job;
  `enable_testing()` was missing, so `ctest` on a fresh clone ran nothing.
- Memory reductions, output bit-identical. Peak RSS at `--i200` on 3 MB
  inputs (i9-8950HK): text 15.4 → 12.3 MB (−20%), incompressible binary
  63.7 → 45.6 MB (−28%). Internals: per-symbol LZ77 byte positions replaced
  by sparse chunk checkpoints (8 bytes per symbol saved per resident store),
  cumulative store histograms allocated only on first use, the longest-match
  cache's run pool grown on demand instead of allocated at its full budget
  up front (also a large allocated-footprint cut for small devices), and no
  longest-match cache for the single-pass fixed-tree squeeze (it was written
  but never read). Costs ~1-2% speed on text at matched settings; measured
  ~6% faster on incompressible input.
- Further hot-path performance work; compressed output is bit-identical in
  every configuration. Measured on an i9-8950HK: ~9% faster at the default
  iteration count and ~8% at `--i200` on text, ~12% faster on incompressible
  input, and ~21% lower peak memory at `--i200` on incompressible input.
  Internals: packed length/distance array in the squeeze DP, inlined
  longest-match-cache probe, shared tree-encoding preprocessing across the
  RLE combos, repeated-parse block-size shortcut, lazily materialized store
  histograms, and assorted match-finder/cache cleanups.

## [1.1.0] - 2026-06-23

### Added
- Custom allocator hook: `ZopfliOptions.zrealloc` + `alloc_context` route all
  allocations through a user-supplied realloc-style callback.
- `ZOPFLI_VERSION` compile-time macro, exposed via `zopfli.h` and generated from
  `version.h.in` with the repo-root `VERSION` file as the single source.
- This changelog.

### Changed
- Internals rewritten in fixed-point integer math instead of float/double:
  output is now fully deterministic across platforms (within ~0.02% of the
  float version) with no FPU dependency, and faster.
- Significantly lower memory use and better cache locality (scratch buffer in
  place of repeated malloc/free, cached distance costs, removed large memsets,
  smaller static tables).
- Default `numiterations` (0) now auto-scales with input size.
- Public headers install under `include/zopfli/`; consumers include
  `<zopfli/zopfli.h>` (was `<zopfli.h>`).
- Consolidated the public compression entry points: `ZopfliGzipCompress`,
  `ZopfliZlibCompress`, `ZopfliDeflate`, and `ZopfliDeflatePart` are now declared
  in `zopfli.h` (the main installed public header) so installed consumers can
  reach the per-format functions, not just `ZopfliCompress`. Removed
  `gzip_container.h` and `zlib_container.h`; `deflate.h` now declares only
  internal deflate helpers.

### Fixed
- Minor CLI tool bug; assorted correctness fixes (MSVC build, review findings).

## [1.0.5] - 2026-06-13

### Changed
- Modernized and fixed the CMake and Makefile builds; raised the minimum CMake
  version. Updated vendored LodePNG (`lodepng_util`).

## [1.0.4] - 2021-06-14

- Fork baseline tracking upstream google/zopfli, including the i686 Android
  build fix and LodePNG updates.
