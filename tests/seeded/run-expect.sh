#!/bin/sh
# Shared seeded-case driver (plans 01-05, 01-07, 01-09, 01-12). POSIX sh.
#
#   run-expect.sh --tool "<command words>" --dir DIR [--ext ".json .jsonl .md"]
#
# DIR/EXPECT.tsv has one row per seed: file<TAB>exit<TAB>rule_id<TAB>needle.
# Lines starting with '#' and empty lines are comments; the needle is optional.
#
# The seed set is every regular file directly in DIR (hidden files included)
# whose name ends in one of the --ext extensions, except EXPECT.tsv. Anything
# else in DIR (INVENTORY, README, *.sh, subdirectories) is a support file, not
# a seed. The run fails on zero rows, on a seed that EXPECT.tsv does not list
# (UNLISTED <file>), and on a listed file that is not there (MISSING <file>).
#
# For each row it runs `<tool> DIR/<file>` with stdout and stderr captured
# together, then checks: the exit code equals the row's, the output contains
# rule_id, and the output contains the needle when one is given. Rows that
# expect exit 0 use rule_id "ok", which must appear as a whole word. It prints
# "ok <file> <rule_id>" or "FAIL <file> <rule_id>" per row, then a summary,
# and exits 1 when anything failed, 2 on a usage error.
set -eu
LC_ALL=C
export LC_ALL

TAB=$(printf '\t')
TOOL=""
DIR=""
EXTS=".json .jsonl .md"

usage() {
  echo 'usage: run-expect.sh --tool "<command words>" --dir DIR [--ext ".json .jsonl .md"]' >&2
  exit 2
}

while [ $# -gt 0 ]; do
  case "$1" in
    --tool) [ $# -ge 2 ] || usage; TOOL=$2; shift 2 ;;
    --dir) [ $# -ge 2 ] || usage; DIR=$2; shift 2 ;;
    --ext) [ $# -ge 2 ] || usage; EXTS=$2; shift 2 ;;
    *) usage ;;
  esac
done
[ -n "$TOOL" ] || usage
[ -n "$DIR" ] || usage
if [ ! -d "$DIR" ]; then
  echo "FAIL usage: no such directory: $DIR" >&2
  exit 2
fi
EXPECT="$DIR/EXPECT.tsv"
if [ ! -f "$EXPECT" ]; then
  echo "MISSING EXPECT.tsv"
  exit 1
fi

WORK=$(mktemp -d "${TMPDIR:-/tmp}/run-expect.XXXXXX")
cleanup() { rm -rf "$WORK"; }
trap cleanup EXIT
trap 'cleanup; exit 130' INT TERM HUP
: >"$WORK/listed"
OUT="$WORK/out"

# The tool string is split into words on purpose; stop globbing from expanding them.
set -f

ROWS=0
PASS=0
FAILS=0

fail_row() { # file rule reason
  echo "FAIL $1 $2"
  echo "  reason: $3"
  FAILS=$((FAILS + 1))
}

while IFS="$TAB" read -r f x r n || [ -n "$f" ]; do
  case "$f" in
    '' | '#'*) continue ;;
  esac
  ROWS=$((ROWS + 1))
  case "$x" in
    '' | *[!0-9]*)
      fail_row "$f" "${r:-?}" "malformed row: exit column is not a number"
      continue
      ;;
  esac
  if [ -z "$r" ]; then
    fail_row "$f" "?" "malformed row: rule_id column is empty"
    continue
  fi
  case "$f" in
    */* | .. | .)
      fail_row "$f" "$r" "malformed row: file must be directly in the directory"
      continue
      ;;
  esac
  if grep -Fxq -- "$f" "$WORK/listed"; then
    fail_row "$f" "$r" "duplicate row for this file"
    continue
  fi
  printf '%s\n' "$f" >>"$WORK/listed"
  if [ ! -f "$DIR/$f" ]; then
    echo "MISSING $f"
    FAILS=$((FAILS + 1))
    continue
  fi
  rc=0
  # shellcheck disable=SC2086
  $TOOL "$DIR/$f" >"$OUT" 2>&1 </dev/null || rc=$?
  why=""
  if [ "$rc" -ne "$x" ]; then
    why="exit $rc, expected $x"
  elif [ "$r" = ok ]; then
    if ! grep -Eq '(^|[^A-Za-z0-9_.-])ok($|[^A-Za-z0-9_.-])' "$OUT"; then
      why="output does not contain ok"
    fi
  elif ! grep -Fq -- "$r" "$OUT"; then
    why="output does not contain rule_id $r"
  fi
  if [ -z "$why" ] && [ -n "$n" ]; then
    if ! grep -Fq -- "$n" "$OUT"; then
      why="output does not contain needle: $n"
    fi
  fi
  if [ -n "$why" ]; then
    fail_row "$f" "$r" "$why"
    sed 's/^/  | /' "$OUT"
  else
    echo "ok $f $r"
    PASS=$((PASS + 1))
  fi
done <"$EXPECT"

if [ "$ROWS" -eq 0 ]; then
  echo "FAIL zero rows in EXPECT.tsv (a driver that checks nothing is not a pass)"
  exit 1
fi

# Seeds that are on disk but not listed.
find "$DIR/." ! -name . -prune -type f -print >"$WORK/found"
sort "$WORK/found" >"$WORK/found.sorted"
while IFS= read -r p; do
  name=${p#"$DIR"/./}
  [ "$name" = EXPECT.tsv ] && continue
  is_seed=0
  for e in $EXTS; do
    case "$name" in
      *"$e") is_seed=1 ;;
    esac
  done
  [ "$is_seed" -eq 1 ] || continue
  if ! grep -Fxq -- "$name" "$WORK/listed"; then
    echo "UNLISTED $name"
    FAILS=$((FAILS + 1))
  fi
done <"$WORK/found.sorted"

echo "run-expect: rows=$ROWS pass=$PASS fail=$FAILS"
[ "$FAILS" -eq 0 ] || exit 1
