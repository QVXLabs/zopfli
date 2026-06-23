# Changelog

All notable changes to this project are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html). New
changes go under a new top section as they land.

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

### Fixed
- Minor CLI tool bug; assorted correctness fixes (MSVC build, review findings).

## [1.0.5] - 2026-06-13

### Changed
- Modernized and fixed the CMake and Makefile builds; raised the minimum CMake
  version. Updated vendored LodePNG (`lodepng_util`).

## [1.0.4] - 2021-06-14

- Fork baseline tracking upstream google/zopfli, including the i686 Android
  build fix and LodePNG updates.
