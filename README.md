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

The tests are built with CMake and run through CTest:

```sh
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

You can also run the test binary directly:

```sh
./build/zopfli_tests
```

## Credits

Zopfli Compression Algorithm was created by Lode Vandevenne and Jyrki
Alakuijala, based on an algorithm by Jyrki Alakuijala.
