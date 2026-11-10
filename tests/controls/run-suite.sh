#!/bin/sh
# Live suite controls: runs every matrix case once, seals the results into a
# suite verified against INVENTORY, and proves the suite-level negative
# controls on that live output. POSIX sh.
#
#   run-suite.sh --bin-dir DIR --mode suite|dropped|truncated|drift|inventory
#
# Prints "ok <mode>" or "FAIL <mode>: <reason>" and exits 0 or 1.
#   suite      seal in sorted-id order and verify; expect exit 0.
#   dropped    one result line removed; expect exit 1, S-INVENTORY, inventory-mismatch.
#   truncated  suite record removed; expect exit 1, S-TRUNCATED, suite-truncated.
#   drift      a temp INVENTORY with one extra id; expect exit 1, S-INVENTORY,
#              inventory-mismatch.
#   inventory  the sorted matrix ids are byte-equal to INVENTORY and non-empty.
set -eu
LC_ALL=C
export LC_ALL

BIN_DIR=""
MODE=""
while [ $# -gt 0 ]; do
  case "$1" in
    --bin-dir) BIN_DIR=${2:-}; shift 2 ;;
    --mode) MODE=${2:-}; shift 2 ;;
    *) echo "run-suite: unknown argument: $1" >&2; exit 2 ;;
  esac
done
if [ -z "$BIN_DIR" ] || [ -z "$MODE" ]; then
  echo "usage: run-suite.sh --bin-dir DIR --mode suite|dropped|truncated|drift|inventory" >&2
  exit 2
fi

HERE=$(cd "$(dirname "$0")" && pwd -P)
CHECK="$BIN_DIR/t64-result-check"
INV="$HERE/INVENTORY"
MATRIX="$HERE/matrix.tsv"

die() {
  echo "FAIL $MODE: $1" >&2
  exit 1
}

matrix_ids() {
  awk -F '\t' 'NF > 0 && substr($1, 1, 1) != "#" { print $1 }' "$MATRIX" | sort
}

if [ "$MODE" = inventory ]; then
  N=$(matrix_ids | wc -l | tr -d ' ')
  [ "$N" -gt 0 ] || die "the matrix has no cases"
  matrix_ids | cmp -s - "$INV" || die "sorted matrix ids differ from INVENTORY"
  [ "$(wc -l < "$INV" | tr -d ' ')" = "$N" ] || die "INVENTORY line count differs from the matrix"
  echo "ok inventory"
  exit 0
fi

case "$MODE" in
  suite | dropped | truncated | drift) ;;
  *) echo "run-suite: unknown mode: $MODE" >&2; exit 2 ;;
esac

TMPD=$(mktemp -d "${TMPDIR:-/tmp}/t64suite.XXXXXX")
trap 'rm -rf "$TMPD"' EXIT INT TERM HUP
mkdir "$TMPD/results"

# Run every case once (run-case.sh itself runs two slice sizes and checks the
# golden), keeping the canonical result of each, in sorted-id order.
IDS=$(matrix_ids)
RESULTS=""
for id in $IDS; do
  sh "$HERE/run-case.sh" --bin-dir "$BIN_DIR" --case "$id" \
    --result-out "$TMPD/results/$id.json" > /dev/null 2> "$TMPD/case.err" ||
    die "case $id failed: $(head -n 1 "$TMPD/case.err")"
  RESULTS="$RESULTS $TMPD/results/$id.json"
done
[ -n "$RESULTS" ] || die "no cases ran"

SUITE="$TMPD/suite.jsonl"
# shellcheck disable=SC2086
"$CHECK" seal --inventory "$INV" --out "$SUITE" $RESULTS > "$TMPD/seal.out" 2>&1 ||
  die "seal failed: $(head -n 1 "$TMPD/seal.out")"

# expect_fail FILE INV RULE DETAIL: the suite check must exit exactly 1 and name
# both the rule id and the harness-error detail.
expect_fail() {
  rc=0
  "$CHECK" suite --inventory "$2" "$1" > "$TMPD/check.out" 2>&1 || rc=$?
  [ "$rc" = 1 ] || die "expected exit 1, got $rc"
  grep -q "^$3 " "$TMPD/check.out" || die "expected rule $3: $(head -n 1 "$TMPD/check.out")"
  grep -q "outcome=harness-error detail=$4\$" "$TMPD/check.out" ||
    die "expected detail $4: $(tail -n 1 "$TMPD/check.out")"
}

case "$MODE" in
  suite)
    "$CHECK" suite --inventory "$INV" "$SUITE" > "$TMPD/check.out" 2>&1 ||
      die "the sealed suite does not verify: $(head -n 1 "$TMPD/check.out")"
    [ "$(wc -l < "$SUITE" | tr -d ' ')" = "$(($(wc -l < "$INV" | tr -d ' ') + 1))" ] ||
      die "the suite does not hold one line per case plus the suite record"
    ;;
  dropped)
    sed '1d' "$SUITE" > "$TMPD/dropped.jsonl"
    expect_fail "$TMPD/dropped.jsonl" "$INV" S-INVENTORY inventory-mismatch
    ;;
  truncated)
    sed '$d' "$SUITE" > "$TMPD/truncated.jsonl"
    expect_fail "$TMPD/truncated.jsonl" "$INV" S-TRUNCATED suite-truncated
    ;;
  drift)
    { cat "$INV"; echo zzz-drift-extra; } > "$TMPD/INVENTORY.drift"
    expect_fail "$SUITE" "$TMPD/INVENTORY.drift" S-INVENTORY inventory-mismatch
    ;;
esac
echo "ok $MODE"
