# Zopfli

[![Linux](https://github.com/QVXLabs/zopfli/actions/workflows/linux.yml/badge.svg)](https://github.com/QVXLabs/zopfli/actions/workflows/linux.yml)
[![macOS](https://github.com/QVXLabs/zopfli/actions/workflows/macos.yml/badge.svg)](https://github.com/QVXLabs/zopfli/actions/workflows/macos.yml)
[![Windows](https://github.com/QVXLabs/zopfli/actions/workflows/windows.yml/badge.svg)](https://github.com/QVXLabs/zopfli/actions/workflows/windows.yml)

Zopfli Compression Algorithm is a compression library programmed in C to perform
very good, but slow, deflate or zlib compression.

The basic function to compress data is `ZopfliCompress` in `zopfli.h`. Use the
`ZopfliOptions` object to set parameters that affect the speed and compression.
Use the `ZopfliInitOptions` function to place the default values in the
`ZopfliOptions` first.

`ZopfliCompress` supports deflate, gzip and zlib output format with a parameter.
To support only one individual format, you can instead use `ZopfliDeflate` in
`deflate.h`, `ZopfliZlibCompress` in `zlib_container.h` or `ZopfliGzipCompress`
in `gzip_container.h`.

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
58 at 16 KB, 106 at 256 KB, 130 at 1 MB). Because runtime scales with
`size × iterations`, large inputs in auto mode are intentionally slow; pass an
explicit `--i#` to force a fixed count (e.g. `--i15` for the old default's speed).

The table below compresses ~415 KB of text at various **fixed** `--i#` counts
(relative to `--i15`):

| Iterations (`--i`) | Compressed size | Reduction vs `--i15` | Relative runtime |
|:------------------:|----------------:|:--------------------:|:----------------:|
| 5                  |   101,950 bytes | −0.06% (**worse**)   |       ~0.3×       |
| 15                 |   101,886 bytes | —                    |        1×        |
| 30                 |   101,857 bytes | 0.03%                |        ~2×        |
| 50                 |   101,854 bytes | 0.03%                |        ~3×        |
| 100                |   101,796 bytes | 0.09%                |        ~7×        |
| 200                |   101,704 bytes | 0.18%                |       ~13×        |
| 500                |   101,674 bytes | 0.21%                |       ~33×        |
| 1000               |   101,661 bytes | 0.22%                |       ~67×        |

Takeaways: `--i15` already captures ~99.8% of what `--i1000` achieves; the
*entire* remaining headroom from iterations is ~0.22%, and most of it is in the
first ~8 passes. `--i5` hasn't converged (output is larger). The auto default
spends more passes on larger inputs, where that thin tail is worth chasing;
force a fixed `--i#` to cap runtime. Highly compressible (text-like) data
benefits most — incompressible or binary data converges flatter.

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
single binary and link to the standard C math library, e.g.:

```sh
gcc src/zopfli/*.c -O2 -W -Wall -Wextra -Wno-unused-function -ansi -pedantic -lm -o zopfli
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
