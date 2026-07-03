#!/bin/sh
# CLI error-path tests. Usage: cli_test.sh /path/to/zopfli
# Each case prints PASS/FAIL; exits nonzero if any case fails.
# These cases expose (pre-fix) silently-ignored failures: junk/overflowing
# --i values accepted, missing input files and write errors exiting 0.
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

# An incompressible input whose compressed size exceeds any pipe buffer, so
# a closed pipe actually makes the output fwrite fail.
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

# 6. Write failure on stdout (EPIPE with SIGPIPE ignored) must be detected.
# The subshell ignores SIGPIPE; the exec'd child inherits that disposition,
# so fwrite fails with EPIPE instead of the process being killed. The reader
# takes 1 byte and closes; the ~300 KB output cannot fit any pipe buffer, so
# the write must fail. A fifo is used because POSIX sh has no PIPESTATUS to
# read the left side of a pipeline.
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

exit $status
