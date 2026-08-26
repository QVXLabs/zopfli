# Upstream issue audit — google/zopfli open issues vs this fork

Audit date: 2026-08-26. All 94 open issues on
[google/zopfli](https://github.com/google/zopfli/issues) were triaged
(including comment threads), the bug-shaped ones were verified against this
fork's code, and every "already fixed" verdict was re-checked by an
adversarial review pass. Issues still present were fixed on the
`upstream-issue-audit` branch, each with a test exemplifying the original
report, and are tracked as QVXLabs/zopfli issues (linked below).

## Fixed by this audit

| Upstream | Fork issue | Defect | Fix / test |
|---|---|---|---|
| [#65](https://github.com/google/zopfli/issues/65) | [QVX#20](https://github.com/QVXLabs/zopfli/issues/20) | Unknown CLI options silently ignored (`-i1000000` compressed with defaults) | Hard error + help hint, nonzero exit (`zopfli_bin.c`); `cli_test.sh` case 8 |
| [#168](https://github.com/google/zopfli/issues/168) | [QVX#21](https://github.com/QVXLabs/zopfli/issues/21) | No notice when output >= input | stderr notice, file still written (`zopfli_bin.c`); `cli_test.sh` case 9 |
| [#199](https://github.com/google/zopfli/issues/199) | [QVX#22](https://github.com/QVXLabs/zopfli/issues/22) | `katajainen.c` tracked mode 100755 | chmod 644; `cli_test.sh` case 11 |
| [#37](https://github.com/google/zopfli/issues/37) residual | [QVX#23](https://github.com/QVXLabs/zopfli/issues/23) | `ZopfliDeflatePart` never clamped `numiterations <= 0`: with `ZopfliInitOptions` defaults it emitted empty blocks (silent data loss) | Auto-iterations fallback in `ZopfliDeflatePart`; min-1 clamp in `ZopfliLZ77Optimal`; `api_contract_test.cc` |
| [#44](https://github.com/google/zopfli/issues/44) residual | [QVX#24](https://github.com/QVXLabs/zopfli/issues/24) | NULL-`zrealloc` fallback documented in `zopfli.h` but implemented only in `ZopfliCompress`; direct container/deflate calls crashed through a NULL fn ptr | `ZopfliInitContext` helper used by every public entry point; `custom_allocator_test.cc` |
| [#66](https://github.com/google/zopfli/issues/66) residual | [QVX#25](https://github.com/QVXLabs/zopfli/issues/25) | Seekable inputs reporting size 0 (`/dev/urandom`, procfs) compressed to a valid *empty* archive, exit 0 | `LoadFile` reads to EOF instead of trusting `ftell`'s 0 (`zopfli_bin.c`); `cli_test.sh` case 10 |
| [#22](https://github.com/google/zopfli/issues/22) residual | [QVX#26](https://github.com/QVXLabs/zopfli/issues/26) | Splice `GetMatch` read up to wordsize-1 bytes before the buffer (aligned, non-faulting, but UB/ASan-visible on splice targets) | In-bounds accumulator assembly (`lz77.c`); `ZOPFLI_FORCE_SPLICE` CMake option enabled in the ASan/UBSan CI job |
| [#182](https://github.com/google/zopfli/issues/182) | [QVX#27](https://github.com/QVXLabs/zopfli/issues/27) | Upstream-reported adler32 mismatch implies wrong deflate bytes on >1 MB multi-master-block inputs (never root-caused upstream) | Test-only: multi-master-block round-trip (`roundtrip_deflate_test.cc`) |
| [#13](https://github.com/google/zopfli/issues/13) | [QVX#28](https://github.com/QVXLabs/zopfli/issues/28) | Upstream segfaulted on a pre-seeded `(out, outsize)`; fixed here by the `ZopfliBuf` rework | Test-only: append-contract lock-in (`api_contract_test.cc`) |

## Already fixed by the fork's earlier work (verified, verdicts upheld under adversarial re-review)

| Upstream | Report | Where fixed in this fork |
|---|---|---|
| [#200](https://github.com/google/zopfli/issues/200) | "Invalid filename" exited 0 | Every error path returns `EXIT_FAILURE` (`zopfli_bin.c` main/CompressFile/SaveFile) |
| [#136](https://github.com/google/zopfli/issues/136) | 0-byte file → "Invalid filename" | Empty input is legal end-to-end; emits canonical empty stream (`deflate.c` do/while master loop) |
| [#15](https://github.com/google/zopfli/issues/15) / [#185](https://github.com/google/zopfli/issues/185) | Segfault / `tree.c` assert on multi-GB inputs | 64-bit `fseeko/ftello`, `SIZE_MAX` guard, master-block clamp (`deflate.c`), katajainen weight guard with error propagation (`katajainen.c`, `tree.c`) |
| [#148](https://github.com/google/zopfli/issues/148) | 1.8e21% compression on tiny inputs | Signed basis-point math in all three verbose sites |
| [#42](https://github.com/google/zopfli/issues/42) | zlib FLEVEL said "fastest" | `flevel = 3` (`zlib_container.c`); note upstream's copy in lodepng still has FLEVEL=0 |
| [#37](https://github.com/google/zopfli/issues/37) | `numiterations = 0` assert | `<= 0` → auto in `ZopfliDeflateBuf` (this audit extended it to `ZopfliDeflatePart`) |
| [#22](https://github.com/google/zopfli/issues/22) | SIGBUS on strict-alignment MIPS | memcmp path on x86/64-bit; aligned splice path elsewhere (this audit removed its last OOB read) |
| [#175](https://github.com/google/zopfli/issues/175) / [#24](https://github.com/google/zopfli/issues/24) | Unchecked mallocs in `katajainen.c` | All allocation routes through `ZopfliRealloc`, which aborts on OOM (`util.c`) |
| [#44](https://github.com/google/zopfli/issues/44) | Crash on failed 2 GB+ alloc | Central OOM abort (`util.c`); no unchecked deref remains |
| [#66](https://github.com/google/zopfli/issues/66) | Crash reading a fifo | `fseeko`/`ftello` failures rejected (`zopfli_bin.c`) |
| [#141](https://github.com/google/zopfli/issues/141) / [#150](https://github.com/google/zopfli/issues/150) | `OptimizeHuffmanForRle` alloc-size warning / length underflow | Trailing-zero loop returns before the alloc; length >= 1 guaranteed (`deflate.c`) |
| [#145](https://github.com/google/zopfli/issues/145) | `%lu` vs `size_t` | `%zu` everywhere |
| [#147](https://github.com/google/zopfli/issues/147) | Missing `io.h` on Windows | Included beside `fcntl.h` under `_WIN32` (`zopfli_bin.c`) |
| [#86](https://github.com/google/zopfli/issues/86) | `ZopfliVerifyLenDist` ran in release | `#ifndef NDEBUG` definition + no-op macro (`lz77.h`) |
| [#159](https://github.com/google/zopfli/issues/159) / [#149](https://github.com/google/zopfli/issues/149) | `detect_block_size` dead store | Value is consumed by the verbose output (`deflate.c`) |
| [#20](https://github.com/google/zopfli/issues/20) | Non-deterministic output across platforms | Float cost model replaced by integer fixed-point; no float/double in compression decisions |
| [#25](https://github.com/google/zopfli/issues/25) | `unsigned long` CRC on LP64 | CRC is `uint32_t` (`gzip_container.c`) |
| [#23](https://github.com/google/zopfli/issues/23) | `lz77.c` LMC asserts via zopflipng `--splitting=3` | Trigger removed: `blocksplittinglast` is a dead field, `--splitting` accepted-but-ignored; LMC reworked with the consistency asserts retained |
| [#13](https://github.com/google/zopfli/issues/13) | Segfault on non-zero `*outsize` | `ZopfliBuf` rework: explicit cap, growth via `ZopfliRealloc` |

## Unverifiable / documented only

- [#191](https://github.com/google/zopfli/issues/191) — OSS-Fuzz 57919. Core
  by construction (OSS-Fuzz builds only `libzopfli`: `ZopfliCompress` <= 8 KB
  and an uncapped `ZopfliDeflate` harness), but the crash report is
  login-gated and nothing public describes it. Possible follow-up: vendor the
  two upstream fuzz harnesses.
- [#5](https://github.com/google/zopfli/issues/5) — verbose block size in
  bytes, excluding the tree. Mitigated: the fork prints `treesize:`
  separately.
- [#171](https://github.com/google/zopfli/issues/171) — MSVC C4267/C4244
  warnings. Not verifiable without an MSVC build; candidates for a future
  MSVC CI pass. The specific lines upstream flagged were largely rewritten.
- Latent, unreachable: `ZopfliCalculateEntropy`'s `uint32_t` sum would wrap
  only above 2^32 symbol counts (master-block clamp keeps counts <= 2^24);
  katajainen's 9-bit weight guard trips `exit(1)` only on 32-bit builds with
  master blocks disabled and a single count > 2^23.

## Out of scope (zopflipng / vendored lodepng)

[#132](https://github.com/google/zopfli/issues/132) (malformed-PNG decode),
[#85](https://github.com/google/zopfli/issues/85) (lodepng segfault on huge
images), [#75](https://github.com/google/zopfli/issues/75) (color-type
selection), [#113](https://github.com/google/zopfli/issues/113) (iCCP
stripping, by design), [#172](https://github.com/google/zopfli/issues/172)
(PLTE order), [#178](https://github.com/google/zopfli/issues/178)
(`--lossy_transparent` growth), [#174](https://github.com/google/zopfli/issues/174)
(no runtime CRC-skip switch), [#43](https://github.com/google/zopfli/issues/43)
(pHYs stripped), [#41](https://github.com/google/zopfli/issues/41) (Windows
filename encoding), [#63](https://github.com/google/zopfli/issues/63) (no
`--` terminator), [#144](https://github.com/google/zopfli/issues/144)
(filters regression, bisected upstream to lodepng update),
[#84](https://github.com/google/zopfli/issues/84) (`getchar()` overwrite
prompt hangs when stdin is not a tty),
[#118](https://github.com/google/zopfli/issues/118) (APNG silently
flattened), and the lodepng half of
[#42](https://github.com/google/zopfli/issues/42) (`lodepng.cpp` still emits
FLEVEL=0).

## Not bugs

Everything else open upstream is a feature request, question, or
build-system ask (parallelism, piping, man page, bindings, Makefile/install
issues, version output, mtime options, iOS, APNG support requests, etc.):
2, 3, 4, 7, 8, 9, 11, 12, 14, 17, 21, 26, 27, 29, 31, 34, 38, 39, 40, 45,
46, 47, 68, 82, 89, 104, 108, 109, 116, 120, 121, 123, 127, 131, 133, 134,
135, 137, 140, 143, 153, 154, 165, 169, 170, 177, 179, 183, 184, 186, 204.
Notable: [#89](https://github.com/google/zopfli/issues/89) (gzip MTIME=0) is
RFC 1952-conformant and keeps output reproducible;
[#108](https://github.com/google/zopfli/issues/108) (float vs double ratio)
is moot since the fixed-point cost rework.
