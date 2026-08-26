# CLAUDE.md — Zopfli

Repo-specific guidance. General coding preferences live in the user-global
CLAUDE.md; this file adds what's specific to this codebase.

## Upstream
Upstream zopfli is read-only — this fork does not merge from it. Don't preserve
upstream's file layout or structure for merge-ability; the code is free to
diverge (reorganize, rename, restructure) when it improves the codebase.

## Language & style
- **gnu99** — built with `-std=gnu99 -pedantic -W -Wall -Wextra` (C99 for
  `<stdint.h>` fixed-width cost types, plus the GNU `__builtin_clz` the code
  uses). Keep the conservative house style otherwise:
  - Declarations at the top of a block (no mixed declarations and statements),
    except short-lived locals in the few verbose-print blocks.
  - No VLAs. No `//` line comments — use `/* */` block comments.
  - Fixed-width integers via `<stdint.h>` (`uint32_t`/`uint64_t`); avoid `size_t`
    for fixed-width numeric values (it varies by platform). For `inline`, use a
    compiler-guarded macro, never bare `inline`.
- **80-column hard wrap** for source. Exception: Makefile flag lists, where
  breaking would harm grep/readability.
- Comments concise, matching the existing files; explain *why*, not *what*.
- Public API is `Zopfli`-prefixed and declared in headers; internal helpers are
  `static`.
- When you modify a source file that carries an `Author:` block, append:
  `Author: afalls@qvxlabs.com (QVXLabs)` (unless an afalls@qvxlabs.com line
  is already present)

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

## Performance
This is an optimization effort. The perf playbook — benchmarking method on this
machine, hot-path architecture notes, and the tried-and-rejected ideas — lives
in `optimization.md`; read it before any performance change. Two standing rules
for every change:
- **Keep output byte-identical** to the pre-change baseline unless explicitly
  agreed otherwise (verify `md5` in both `-i15` and `--i200`, text + binary).
- **Single-threaded only** — never add threads/OpenMP/parallelism.

### Benchmark corpus (README tables)
The README perf/memory tables are reproducible with this corpus and method (no
private corpus is checked in; regenerate it locally):
- **Text** = the repo's own C/C++ source concatenated
  (`cat src/zopfli/*.c src/zopfli/*.h src/zopflipng/*.cc
  src/zopflipng/lodepng/*.cpp src/zopflipng/lodepng/*.h README.md`, ~690 KB
  base), then **extended by repetition** (`head -c` of the base repeated) to hit
  the 256 KB / 1 MB / 3 MB / 10 MB sizes. The iterations table uses the first
  ~415 KB of that base (real, un-repeated source).
- **Binary** = incompressible random data (`head -c <size> /dev/urandom`).
- **Stock baseline** = upstream `google/zopfli` built from a shallow clone, for
  the fork-vs-stock columns.
- **Method**: i9-8950HK, release `-O3 -DNDEBUG`; speed = `--i15` for both
  binaries (stock's default), min of 2 runs; memory = peak RSS via
  `/usr/bin/time -l` at `--i200` on the 3 MB inputs. Re-measure on an idle
  machine (`ps -Ao pcpu,comm | sort -rn | head`) — wall times are contention-
  sensitive. Compressed sizes are deterministic; only timings are noisy.
