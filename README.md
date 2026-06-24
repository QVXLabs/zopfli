# Zopfli

[![Linux](https://github.com/QVXLabs/zopfli/actions/workflows/linux.yml/badge.svg)](https://github.com/QVXLabs/zopfli/actions/workflows/linux.yml)
[![macOS](https://github.com/QVXLabs/zopfli/actions/workflows/macos.yml/badge.svg)](https://github.com/QVXLabs/zopfli/actions/workflows/macos.yml)
[![Windows](https://github.com/QVXLabs/zopfli/actions/workflows/windows.yml/badge.svg)](https://github.com/QVXLabs/zopfli/actions/workflows/windows.yml)

An **actively maintained** fork of
[google/zopfli](https://github.com/google/zopfli), which was archived (made
read-only) in October 2025. Zopfli produces very good — but slow — DEFLATE /
zlib / gzip compression; the output is standard and decompresses with any zlib
or gzip library.

This fork is a **drop-in replacement** that is faster, leaner, and
deterministic:

- **~1.9× faster** on text and **~2.5–3.5× faster** on incompressible data at a
  matched iteration count.
- **34–38% lower** peak memory.
- **Bit-identical output across every CPU, compiler, and floating-point mode**,
  with no `libm`/FPU dependency — it runs on FPU-less microcontrollers. (Upstream
  zopfli's output varies by platform.)

Output is a valid DEFLATE/zlib/gzip stream but is **not** byte-identical to
upstream zopfli — it defines a new, platform-independent canonical encoding. See
[Modifications from Stock Zopfli](#modifications-from-stock-zopfli) for the full
numbers and [Migrating from google/zopfli](#migrating-from-googlezopfli) before
you switch.

> **Maintenance status:** actively maintained following upstream's archival. Bug
> reports, fixes, and PRs are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md).

The basic function to compress data is `ZopfliCompress` in `<zopfli/zopfli.h>`.
Use the `ZopfliOptions` object to set parameters that affect the speed and
compression. Use the `ZopfliInitOptions` function to place the default values in
the `ZopfliOptions` first.

`ZopfliCompress` supports deflate, gzip and zlib output format with a parameter.
To support only one individual format, you can instead use `ZopfliDeflate`,
`ZopfliZlibCompress` or `ZopfliGzipCompress` — all declared, alongside
`ZopfliCompress`, in `<zopfli/zopfli.h>`.

`ZopfliDeflate` creates a valid deflate stream in memory, see:
http://www.ietf.org/rfc/rfc1951.txt
`ZopfliZlibCompress` creates a valid zlib stream in memory, see:
http://www.ietf.org/rfc/rfc1950.txt
`ZopfliGzipCompress` creates a valid gzip stream in memory, see:
http://www.ietf.org/rfc/rfc1952.txt

This library can only compress, not decompress. Existing zlib or deflate
libraries can decompress the data.

The source code of Zopfli is under `src/zopfli`. `zopfli_bin.c` is separate from
the library and contains an example program to create very well compressed gzip
files.

## Migrating from google/zopfli

Two things differ from upstream when you switch; nothing else in your pipeline
needs to change.

- **Include path.** Public headers install under `include/zopfli/`, so consumers
  include `<zopfli/zopfli.h>` (was `<zopfli.h>`). The per-format entry points
  (`ZopfliGzipCompress`, `ZopfliZlibCompress`, `ZopfliDeflate`,
  `ZopfliDeflatePart`) are all declared there now — the separate
  `gzip_container.h` / `zlib_container.h` headers were removed.
- **Output is not byte-identical to upstream.** It is a valid DEFLATE/zlib/gzip
  stream any decoder reads, and it is reproducible across platforms — but if you
  compare against stored golden files produced by upstream zopfli, they will not
  match. Regenerate any such fixtures from this fork. Compression ratio is within
  ~0.02% of upstream (sometimes better).

The CLI flags (`zopfli`, `zopflipng`) are unchanged, except the default
iteration count is now `0` = auto (see the **Iterations** section below); pass
`--i15` for upstream-like speed and pass count.

## Modifications from Stock Zopfli

This fork is a drop-in replacement — it emits standard DEFLATE/zlib/gzip that any
existing decoder reads — but differs from upstream zopfli in four ways. Each is
detailed in its own section below; the highlights:

**Faster.** At a matched iteration count the optimal parse is **~1.9× faster
than stock zopfli on text and ~2.5–3.5× on incompressible data**:

| Input    | Stock zopfli | QVXLabs/Zopfli | Speedup | Default `--i` | QVXLabs/Zopfli (default) |
|----------|-------------:|---------------:|:-------:|:-------------:|-------------------------:|
| 256 KB text   |   0.90 s |   0.47 s | 1.9× | 106 |   1.60 s |
| 1 MB text     |   2.71 s |   1.40 s | 1.9× | 130 |   6.56 s |
| 3 MB text     |   8.13 s |   4.22 s | 1.9× | 142 |  21.33 s |
| 10 MB text    |  27.16 s |  13.98 s | 1.9× | 166 |  82.50 s |
| 256 KB binary |   0.36 s |   0.12 s | 2.9× | 106 |   0.42 s |
| 1 MB binary   |   1.58 s |   0.63 s | 2.5× | 130 |   2.01 s |
| 3 MB binary   |   5.69 s |   1.61 s | 3.5× | 142 |   6.44 s |
| 10 MB binary  |  16.46 s |   5.06 s | 3.3× | 166 |  24.58 s |

<sub>Measured on an Intel Core i9-8950HK (Coffee Lake), release `-O3 -DNDEBUG`.
The `Stock zopfli`/`QVXLabs/Zopfli`/`Speedup` columns are at a matched 15
iterations (`--i15` for both; stock's default), min of 2 runs. `Default --i` is
the fork's auto-selected pass count for that size; `QVXLabs/Zopfli (default)` is
its wall-clock at `--i0` (auto), single run. Text = concatenated source (zopfli
+ zopflipng + lodepng + README), extended by repetition for the larger sizes;
binary = incompressible random data.</sub>

**Faster per pass, but more passes by default.** The `Speedup` column matches
both encoders at `--i15` to isolate the per-iteration win. The fork's default is
no longer a fixed 15, though: `--i0` (auto) scales the pass count with input
size (106 → 166 from 256 KB → 10 MB), so at **default** settings it does several
times more work and its wall-clock (the `QVXLabs/Zopfli (default)` column) is
*higher* than stock's fixed `--i15` — e.g. 82.5 s vs 27.2 s on 10 MB text. That
buys a slightly smaller file on inputs whose ratio gains have a long tail; pass
an explicit `--i#` to cap runtime (`--i15` for stock-like speed). See the
**Iterations** section below.

It also scales much better at high iteration counts: the longest-match cache
makes passes after the first nearly free, whereas stock's cost is roughly linear
in the iteration count. The main changes (all preserving the encoder's output):
an integer fixed-point cost model replacing the `double` function-pointer cost
callbacks; carrying match distances forward so the final path walk never
re-searches the hash; fusing the longest-match cache directly into the cost
dynamic-program; a variable-length match cache that lets iterations ≥2 skip the
per-byte hash rebuild entirely; and reusing scratch buffers instead of per-call
allocations.

**Deterministic & floating-point-free.** The cost model (including the entropy /
`-log2`) is computed entirely in integer fixed point, so output is **bit-identical
on every CPU, compiler, and FP mode**, and the core needs no `libm` — it runs on
FPU-less microcontrollers. Output is *not* byte-identical to stock zopfli; it
defines a new canonical, platform-independent encoding. See the
**Deterministic, floating-point-free** section below.

**Lower memory.** Hot data structures were trimmed to what the data actually
needs — 16-bit hash tables (right-sized to the used bucket count), 32-bit LZ77
cumulative histograms, and dropping two precomputed per-symbol arrays that are
cheaply recomputed on the fly — so peak resident memory is well below stock
zopfli's. Measured peak RSS (`/usr/bin/time -l`, 3 MB input, `--i200`):

| Input                         | Stock zopfli | QVXLabs/Zopfli | Saved          |
|-------------------------------|-------------:|---------------:|:--------------:|
| Text (~3 MB)                  |   23.1 MB    |    15.2 MB     | 7.9 MB (−34%)  |
| Incompressible binary (~3 MB) |  122.5 MB    |    75.4 MB     | 47.1 MB (−38%) |

Incompressible input has ~1 LZ77 symbol per byte, so the per-symbol arrays — and
thus the savings — are largest there; text compresses to fewer symbols.

**Auto iteration count.** The default `numiterations` is `0` = auto: the number
of optimization passes scales with input size (larger inputs have a longer tail
of gains) instead of a fixed 15. See the **Iterations** section below.

## Deterministic, floating-point-free

This fork's core library (`src/zopfli`) contains **no floating point**. The cost
model that drives the optimal parse — including the entropy / `-log2`
computation — is computed entirely in **integer fixed point** (`IntLog2Fixed`,
with the precision derived from the per-block cost shift; block sizes and costs
are `uint32_t`). Two consequences:

- **Bit-identical output on every CPU.** Upstream zopfli computes symbol costs
  with `libm`'s `log()`, whose result varies by platform, compiler, and FP mode
  (x87 vs SSE, FMA contraction, `-ffast-math`); that can change which parse is
  chosen and therefore the output bytes. Here the output is reproducible across
  all of those — verified by building at `-O0`, `-O3`, and
  `-O3 -ffast-math -ffp-contract=fast` and checking the compressed `md5` is
  identical (with no FP in the code, these flags cannot change the result).
- **Runs well on devices without an FPU.** No soft-float emulation and no `libm`
  dependency, so the library is smaller and faster on embedded / microcontroller
  targets. The build uses `-std=gnu99` (for `<stdint.h>`) and links no `-lm`.

Compression ratio is within ~0.02% of the floating-point version (sometimes
better). Note the output is **not** byte-identical to upstream zopfli — it
defines a new, canonical, platform-independent encoding.

## Compression options

Compression is controlled by `ZopfliOptions` (set defaults with
`ZopfliInitOptions`); the `zopfli` binary exposes the main knobs as flags. Every
setting produces standard DEFLATE that any zlib/gzip decoder can read — they only
trade encoder time for output size. Zopfli already operates near the practical
DEFLATE size limit, so the gains are small in absolute terms.

### Iterations — `numiterations` / `--i#` (default: `0` = auto)

The dominant quality/speed knob. Each iteration reruns the optimal LZ77 parse
with a cost model refined from the previous pass, converging toward a smaller
encoding. Runtime is roughly **linear** in the count; ratio gains have steep
**diminishing returns**.

**Default is auto** (`numiterations == 0`, or `--i0`): the count scales with
input size — `10 + 12·floor(log2(size/1KB))` (size quantized to a power of two),
clamped to `[15, 400]`. Small inputs saturate in a few iterations, while larger
inputs have a longer tail of gains, so they get more passes (e.g. 15 at ≤1 KB,
58 at 16 KB, 106 at 256 KB, 130 at 1 MB, 142 at 3 MB, 166 at 10 MB). Because
runtime scales with
`size × iterations`, large inputs in auto mode are intentionally slow; pass an
explicit `--i#` to force a fixed count (e.g. `--i15` for the old default's speed).

The table below compresses ~415 KB of text at various **fixed** `--i#` counts
(relative to `--i15`):

| Iterations (`--i`) | Compressed size | Reduction vs `--i15` | Relative runtime |
|:------------------:|----------------:|:--------------------:|:----------------:|
| 5                  |   101,322 bytes | −0.03% (**worse**)   |      ~0.8×        |
| 15                 |   101,292 bytes | —                    |        1×        |
| 30                 |   101,279 bytes | 0.01%                |       ~1.4×       |
| 50                 |   101,281 bytes | 0.01%                |       ~2.2×       |
| 100                |   101,266 bytes | 0.03%                |       ~3.3×       |
| 200                |   101,264 bytes | 0.03%                |       ~6.2×       |
| 500                |   101,257 bytes | 0.03%                |       ~14×        |
| 1000               |   101,225 bytes | 0.07%                |       ~28×        |

Takeaways: `--i15` already captures all but ~0.07% of what `--i1000` achieves;
that thin tail is the *entire* remaining headroom from iterations, and most of it
lands in the first ~8 passes. `--i5` hasn't converged (output is larger). The
longest-match cache makes passes after the first cheap, so the runtime cost of
high counts is far below stock's roughly linear scaling. The auto default spends
more passes on larger inputs, where that tail is worth chasing; force a fixed
`--i#` to cap runtime. Highly compressible (text-like) data benefits most —
incompressible or binary data converges flatter.

### Block splitting — `blocksplitting`, `blocksplittingmax`, `blocksplittinglast`

DEFLATE lets each block carry its own Huffman trees, so splitting the input into
well-chosen blocks lets the trees fit each region better. This usually shrinks
output, so `blocksplitting` defaults to on.

- `blocksplittingmax` (default 15) caps the number of blocks. `0` means
  unlimited, which can *hurt* compression on some files (per-block tree overhead
  outweighs the gain) — the cap exists on purpose.
- `blocksplittinglast` (default off) chooses *when* split points are picked: the
  default picks them from a fast initial pass, which — perhaps unintuitively —
  yields better boundaries than splitting after the expensive optimal parse.
  Leave it off unless experimenting.

### Output format and verbosity

The container (`--deflate` / `--zlib` / `--gzip`, or the `ZopfliCompress` format
argument) does **not** change the compressed payload — only the few header/footer
bytes and the checksum (gzip uses CRC-32, zlib Adler-32, raw DEFLATE none).
Choose by what the consumer expects, not for ratio. `verbose` / `-v` only prints
progress to stderr and has no effect on the output.

### Custom allocator — `zrealloc` / `alloc_context`

By default zopfli allocates with the standard `realloc`/`free`. To route every
allocation through your own allocator (e.g. an arena, or a fixed pool on an
embedded target), set `options.zrealloc` to a `realloc`-style callback and,
optionally, `options.alloc_context` to a pointer handed back to it unchanged:

```c
void* my_realloc(void* alloc_context, void* ptr, size_t size);
```

It follows `realloc` semantics with one addition: `size == 0` must free `ptr` and
return `NULL`; `ptr == NULL` allocates; it returns `NULL` only on genuine failure
(which zopfli treats as fatal). `ZopfliInitOptions` points `zrealloc` at an
internal default backed by the standard library; leaving it at that default (or
setting it back to `NULL`) keeps standard-library allocation. This changes only
*how* memory is obtained, never the compressed output.

## Getting started

### Prerequisites

- A C and C++ compiler (gcc, clang, or MSVC)
- [CMake](https://cmake.org/) 3.10 or newer (recommended build path)
- Git (to fetch the GoogleTest submodule used by the test suite)

### Clone

The test suite depends on GoogleTest, which is vendored as a git submodule under
`third-party/google-test`. Clone with submodules:

```sh
git clone --recurse-submodules https://github.com/QVXLabs/zopfli.git
```

If you already cloned without `--recurse-submodules`, initialize it after the
fact:

```sh
git submodule update --init third-party/google-test
```

The submodule is only needed to build the tests. If you have GoogleTest
installed system-wide (or via a package manager such as Conan), CMake will find
it with `find_package(GTest)` and the submodule is not required.

## Building

### CMake (recommended)

```sh
cmake -B build
cmake --build build
```

This produces the `zopfli` and `zopflipng` binaries plus the `libzopfli` and
`libzopflipng` libraries in the `build` directory. By default a standalone build
uses the `Release` configuration and builds static libraries; pass
`-DBUILD_SHARED_LIBS=ON` to build shared libraries instead.

To install:

```sh
cmake --install build
```

Useful options (pass with `-D<option>=<value>` at configure time):

- `BUILD_SHARED_LIBS` — build shared instead of static libraries (default `OFF`)
- `BUILD_TESTING` — build the test suite (default `ON`)
- `ZOPFLI_PIC` — build with position-independent code (default `OFF`)
- `ZOPFLI_COVERAGE` — build with line-coverage instrumentation (gcc/clang)

### Make (Linux/macOS)

A makefile is also provided:

```sh
make            # build the zopfli and zopflipng binaries and libraries
make zopfli     # build just the zopfli binary
make libzopfli  # build zopfli as a shared library
```

The makefile does not build the test suite; use CMake for that.

### Compiling directly

To build zopfli by hand, compile all `.c` source files under `src/zopfli` to a
single binary — the library is integer-only, so no `-lm` is needed, e.g.:

```sh
gcc src/zopfli/*.c -O2 -W -Wall -Wextra -Wno-unused-function -std=gnu99 -pedantic -o zopfli
```

## Running the tests

The tests are built with CMake and run by executing the GoogleTest binary:

    cmake -B build
    cmake --build build
    ./build/zopfli_tests

You can also run the test binary directly:

```sh
./build/zopfli_tests
```

## Credits

Zopfli Compression Algorithm was created by Lode Vandevenne and Jyrki
Alakuijala, based on an algorithm by Jyrki Alakuijala.
