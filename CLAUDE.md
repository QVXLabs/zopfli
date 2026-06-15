# CLAUDE.md — Zopfli

Repo-specific guidance. General coding preferences live in the user-global
CLAUDE.md; this file adds what's specific to this codebase.

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

## Performance
This is an optimization effort. The perf playbook — benchmarking method on this
machine, hot-path architecture notes, and the tried-and-rejected ideas — lives
in `optimization.md`; read it before any performance change. Two standing rules
for every change:
- **Keep output byte-identical** to the pre-change baseline unless explicitly
  agreed otherwise (verify `md5` in both `-i15` and `--i200`, text + binary).
- **Single-threaded only** — never add threads/OpenMP/parallelism.
