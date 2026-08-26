#!/bin/sh
# CLI error-path tests. Usage: cli_test.sh /path/to/zopfli
# Prints PASS/FAIL per case; exits nonzero if any case fails.
set -u

ZOPFLI=$1
TMPDIR_T=$(mktemp -d) || exit 2
trap 'rm -rf "$TMPDIR_T"' EXIT
status=0

fail() { echo "FAIL: $1"; status=1; }
pass() { echo "PASS: $1"; }

# A compressible test input.
i=0
while [ $i -lt 200 ]; do
  printf 'zopfli cli test data line %d\n' $i
  i=$((i+1))
done > "$TMPDIR_T/in.txt"

# Incompressible input: output exceeds any pipe buffer, so a closed pipe
# makes the output write fail.
dd if=/dev/urandom of="$TMPDIR_T/big.bin" bs=1024 count=300 2> /dev/null

# 1. Junk suffix in --i must be rejected, not silently parsed as a prefix.
if "$ZOPFLI" --i5x -c "$TMPDIR_T/in.txt" > /dev/null 2> "$TMPDIR_T/err1"; then
  fail "--i5x accepted (exit 0)"
else
  pass "--i5x rejected"
fi

# 2. Out-of-range --i must be rejected, not overflow/hang.
if "$ZOPFLI" --i9999999999999 -c "$TMPDIR_T/in.txt" \
    > /dev/null 2> "$TMPDIR_T/err2"; then
  fail "--i9999999999999 accepted (exit 0)"
else
  pass "--i9999999999999 rejected"
fi

# 3. --i0 (auto) stays valid.
if "$ZOPFLI" --i0 -c "$TMPDIR_T/in.txt" > "$TMPDIR_T/out0.gz" \
    2> /dev/null && [ -s "$TMPDIR_T/out0.gz" ]; then
  pass "--i0 accepted"
else
  fail "--i0 rejected or empty output"
fi

# 4. Missing input file must yield a nonzero exit.
if "$ZOPFLI" --i5 "$TMPDIR_T/no-such-file" > /dev/null 2>&1; then
  fail "missing input file exited 0"
else
  pass "missing input file rejected"
fi

# 5. No filename at all must yield a nonzero exit.
if "$ZOPFLI" --i5 > /dev/null 2>&1; then
  fail "no-filename invocation exited 0"
else
  pass "no-filename invocation rejected"
fi

# 6. Write failure on stdout must be detected. The exec'd child inherits the
# ignored SIGPIPE, so the write fails with EPIPE instead of killing the
# process. A fifo captures the compressor's exit code (POSIX sh has no
# PIPESTATUS).
mkfifo "$TMPDIR_T/fifo"
head -c 1 < "$TMPDIR_T/fifo" > /dev/null &
headpid=$!
( trap '' PIPE; exec "$ZOPFLI" --i1 -c "$TMPDIR_T/big.bin" 2> /dev/null ) \
    > "$TMPDIR_T/fifo"
zstatus=$?
wait $headpid 2> /dev/null
if [ "$zstatus" -eq 0 ]; then
  fail "stdout write failure (EPIPE) exited 0"
else
  pass "stdout write failure detected (exit $zstatus)"
fi

# 7. Write failure to a full device (Linux only; /dev/full).
if [ -w /dev/full ] 2> /dev/null; then
  if "$ZOPFLI" --i1 -c "$TMPDIR_T/big.bin" > /dev/full 2> /dev/null; then
    fail "write to /dev/full exited 0"
  else
    pass "write to /dev/full detected"
  fi
else
  echo "SKIP: /dev/full not available"
fi

# 8. Unknown options must be a hard error, not silently ignored
# (google/zopfli#65: -i1000000 used to compress with default iterations).
for badopt in -i1000000 --foo; do
  if "$ZOPFLI" "$badopt" -c "$TMPDIR_T/in.txt" \
      > "$TMPDIR_T/out8.gz" 2> "$TMPDIR_T/err8"; then
    fail "$badopt accepted (exit 0)"
  elif [ -s "$TMPDIR_T/out8.gz" ]; then
    fail "$badopt rejected but output was written"
  elif grep -q "unknown option" "$TMPDIR_T/err8"; then
    pass "$badopt rejected with diagnostic"
  else
    fail "$badopt rejected without 'unknown option' diagnostic"
  fi
  rm -f "$TMPDIR_T/out8.gz"
done

# 9. Output not smaller than input prints a notice but still writes
# (google/zopfli#168); a compressible input stays quiet.
printf 'hi' > "$TMPDIR_T/tiny"
if "$ZOPFLI" --i1 "$TMPDIR_T/tiny" 2> "$TMPDIR_T/err9" \
    && [ -s "$TMPDIR_T/tiny.gz" ] \
    && grep -q "not smaller than the input" "$TMPDIR_T/err9"; then
  pass "expansion notice printed, file still written"
else
  fail "expansion notice missing, or file not written, or exit nonzero"
fi
if "$ZOPFLI" --i1 -c "$TMPDIR_T/in.txt" > /dev/null 2> "$TMPDIR_T/err9b" \
    && ! grep -q "not smaller than the input" "$TMPDIR_T/err9b"; then
  pass "no expansion notice for compressible input"
else
  fail "unexpected expansion notice (or failure) on compressible input"
fi

# 10. Seekable inputs that report size 0 must still be read to EOF
# (google/zopfli#66 residual: /proc files, char devices). Linux only.
if [ -r /proc/self/status ]; then
  if "$ZOPFLI" --i1 -c /proc/self/status > "$TMPDIR_T/out10.gz" \
      2> /dev/null && [ -s "$TMPDIR_T/out10.gz" ] \
      && command -v gzip > /dev/null \
      && [ "$(gzip -cd "$TMPDIR_T/out10.gz" | wc -c)" -gt 0 ]; then
    pass "size-0-reporting input read to EOF"
  else
    fail "/proc/self/status compressed empty or failed"
  fi
else
  echo "SKIP: /proc not available"
fi

# 11. Repo hygiene: no source file may carry the executable bit
# (google/zopfli#199: katajainen.c was mode 100755).
SRCDIR=$(dirname "$0")/../src/zopfli
execs=""
for f in "$SRCDIR"/*.c "$SRCDIR"/*.h; do
  [ -x "$f" ] && execs="$execs $f"
done
if [ -n "$execs" ]; then
  fail "executable bit set on:$execs"
else
  pass "no executable source files"
fi

exit $status
