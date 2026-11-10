#!/bin/sh
# Runs one row of matrix.tsv twice (slice_a, slice_b) through the supervisor and
# checks exit codes, slice independence and the golden canonical bytes.
# Usage: run-case.sh --bin-dir DIR --case ID
set -eu
LC_ALL=C
export LC_ALL

BIN_DIR=""
CASE=""
while [ $# -gt 0 ]; do
  case "$1" in
    --bin-dir) BIN_DIR=${2:-}; shift 2 ;;
    --case) CASE=${2:-}; shift 2 ;;
    *) echo "run-case: unknown argument: $1" >&2; exit 2 ;;
  esac
done
if [ -z "$BIN_DIR" ] || [ -z "$CASE" ]; then
  echo "usage: run-case.sh --bin-dir DIR --case ID" >&2
  exit 2
fi

HERE=$(cd "$(dirname "$0")" && pwd -P)
SUP="$BIN_DIR/t64-supervisor"
RUNNER="$BIN_DIR/t64-runner-synthetic"
LOG_LIMIT=65536

ROW=$(awk -F '\t' -v id="$CASE" '$1 == id && substr($1, 1, 1) != "#" { print; exit }' "$HERE/matrix.tsv")
if [ -z "$ROW" ]; then
  echo "[$CASE] FAIL row-present: no such case in matrix.tsv" >&2
  exit 1
fi
field() { printf '%s\n' "$ROW" | awk -F '\t' -v n="$1" '{ print $n }'; }
SCRIPT=$(field 2)
EXPECT_PASS=$(field 3)
BOUND=$(field 4)
WALL_MS=$(field 5)
SLICE_A=$(field 6)
SLICE_B=$(field 7)
MODE=$(field 8)
SHA=$(field 9)
OUTCOME=$(field 10)
DETAIL=$(field 11)
EXIT=$(field 12)

TMPD=$(mktemp -d "${TMPDIR:-/tmp}/t64.XXXXXX")
trap 'rm -rf "$TMPD"' EXIT INT TERM HUP

case "$MODE" in
  ok) FIXTURE="$HERE/scripts/$SCRIPT" ;;
  deleted) FIXTURE="$TMPD/missing-fixture.t64s" ;;
  tampered)
    FIXTURE="$TMPD/tampered.t64s"
    cp "$HERE/scripts/$SCRIPT" "$FIXTURE"
    printf '#' >> "$FIXTURE"
    ;;
  *) echo "[$CASE] FAIL fixture-mode: unknown mode $MODE" >&2; exit 1 ;;
esac

FAILED=0
fail() {
  echo "[$CASE] FAIL $1" >&2
  FAILED=1
}

run_one() { # run_one TAG SLICE
  mkdir "$TMPD/$1"
  rc=0
  "$SUP" --identity synthetic --result "$TMPD/$1/result.json" \
    --meta "$TMPD/$1/run-meta.json" --work-dir "$TMPD/$1" \
    --wall-ms "$WALL_MS" --bound "$BOUND" --log-limit "$LOG_LIMIT" \
    --fixture-id "$CASE" --fixture "$FIXTURE" --fixture-sha256 "$SHA" \
    --expect-pass "$EXPECT_PASS" -- "$RUNNER" --slice "$2" || rc=$?
  eval "RC_$1=$rc"
}

run_one a "$SLICE_A"
run_one b "$SLICE_B"

[ "$RC_a" = "$EXIT" ] || fail "exit-code-a: got $RC_a want $EXIT"
[ "$RC_b" = "$EXIT" ] || fail "exit-code-b: got $RC_b want $EXIT"
for t in a b; do
  [ -f "$TMPD/$t/result.json" ] || fail "result-present-$t"
done
if [ "$FAILED" = 0 ]; then
  cmp -s "$TMPD/a/result.json" "$TMPD/b/result.json" || fail "slice-independence"
  cmp -s "$TMPD/a/result.json" "$HERE/golden/$CASE.json" || fail "golden-a"
  cmp -s "$TMPD/b/result.json" "$HERE/golden/$CASE.json" || fail "golden-b"
  grep -q "\"outcome\":\"$OUTCOME\"" "$TMPD/a/result.json" || fail "outcome"
  grep -q "\"detail\":\"$DETAIL\"" "$TMPD/a/result.json" || fail "detail"
fi
[ "$FAILED" = 0 ] || exit 1
echo "[$CASE] ok ($OUTCOME/$DETAIL, exit $EXIT)"
