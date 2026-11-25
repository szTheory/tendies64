#!/bin/sh
# Stale freeze-text guard (Phase 11, plan 11-11, DR-18).
#
# The API freeze moved from Phase 10 to the Phase 11 candidate (final ABI bind
# at the end of Phase 12), so no production comment, header banner or accepted
# decision record may still say that a freeze is, or comes, at Phase 10. This
# guard scans core/, runner/, tools/ and docs/adr/0002-language-build-and-abi.md
# for the deny phrases below (case-insensitive, extended regular expressions):
#   until (the )?Phase 10      freeze is Phase 10      before Phase 10
#   Phase 10 interface review  provisional until Phase 10
# History and component names stay legal: "Phase 10 composite family (plan
# 10-09)", test-history headers and fixture comments do not match any phrase.
# Closed Phase 9 and Phase 10 records under docs/claims, docs/evidence and
# fixtures/ are outside the scan on purpose (they are never relabeled).
#
# Usage:
#   check-stale-freeze-text.sh              scan the repository containing this script
#   check-stale-freeze-text.sh --root DIR   scan DIR instead
#   check-stale-freeze-text.sh --selftest   seeded cases in a temporary tree
# Output: "STALE-FREEZE path:line: text" per finding and a final
# "stale-freeze-text: files=N findings=M". Exit 0 clean, 1 findings, 2 usage
# error or a scan that covered zero files. POSIX sh, no git needed.
set -eu
LC_ALL=C
export LC_ALL

DENY='until (the )?Phase 10|freeze is Phase 10|before Phase 10|Phase 10 interface review|provisional until Phase 10'
SCAN_PATHS='core runner tools docs/adr/0002-language-build-and-abi.md'

SELF=$0
HERE=$(cd "$(dirname "$0")" && pwd -P)
ROOT=$(cd "$HERE/.." && pwd -P)

usage() {
  echo "usage: check-stale-freeze-text.sh [--root DIR | --selftest]" >&2
  exit 2
}

# scan DIR: print findings, then the summary line; return 0 clean, 1 findings,
# 2 when no file was scanned.
scan() {
  _dir=$1
  _paths=""
  for _p in $SCAN_PATHS; do
    if [ -e "$_dir/$_p" ]; then
      _paths="$_paths $_p"
    fi
  done
  _tmp=$(mktemp "${TMPDIR:-/tmp}/t64stale.XXXXXX")
  # shellcheck disable=SC2086
  _files=$(cd "$_dir" && find $_paths -type f 2>/dev/null | wc -l | tr -d ' ')
  if [ -z "$_paths" ] || [ "$_files" -eq 0 ]; then
    rm -f "$_tmp"
    echo "stale-freeze-text: no file to scan under $_dir" >&2
    return 2
  fi
  # grep exits 1 for no match; only status 2 (an error) is a failure here.
  _grc=0
  # shellcheck disable=SC2086
  (cd "$_dir" && grep -rn -I -i -E -e "$DENY" $_paths) > "$_tmp" 2>/dev/null || _grc=$?
  if [ "$_grc" -gt 1 ]; then
    rm -f "$_tmp"
    echo "stale-freeze-text: grep failed (status $_grc)" >&2
    return 2
  fi
  _n=$(wc -l < "$_tmp" | tr -d ' ')
  sed 's/^/STALE-FREEZE /' "$_tmp"
  rm -f "$_tmp"
  echo "stale-freeze-text: files=$_files findings=$_n"
  [ "$_n" -eq 0 ]
}

# seed NAME EXPECT PATH TEXT: write TEXT to PATH in a fresh tree, scan it and
# require the expected outcome (clean or fail, and for fail the file:line).
SELF_FAIL=0
seed() {
  _name=$1
  _expect=$2
  _path=$3
  _text=$4
  _tree="$SELF_TMP/$_name"
  mkdir -p "$_tree/core/include/tendies64" "$_tree/runner" "$_tree/tools" "$_tree/docs/adr"
  printf '/* clean file */\n' > "$_tree/core/clean.c"
  case "$_path" in
    docs/adr/*) printf '%s\n' "$_text" > "$_tree/$_path" ;;
    *) printf '/* clean first line */\n%s\n' "$_text" > "$_tree/$_path" ;;
  esac
  _rc=0
  scan "$_tree" > "$_tree/out.txt" 2> "$_tree/err.txt" || _rc=$?
  case "$_expect" in
    clean)
      if [ "$_rc" -eq 0 ]; then
        echo "selftest ok: $_name is accepted"
      else
        echo "selftest FAIL: $_name should be accepted (exit $_rc)"
        SELF_FAIL=$((SELF_FAIL + 1))
      fi
      ;;
    fail)
      _want=$5
      if [ "$_rc" -eq 1 ] && grep -q "^STALE-FREEZE $_want" "$_tree/out.txt"; then
        echo "selftest ok: $_name reports $_want"
      else
        echo "selftest FAIL: $_name should report $_want (exit $_rc)"
        SELF_FAIL=$((SELF_FAIL + 1))
      fi
      ;;
  esac
}

selftest() {
  SELF_TMP=$(mktemp -d "${TMPDIR:-/tmp}/t64stale-self.XXXXXX")
  trap 'rm -rf "$SELF_TMP"' EXIT
  seed seeded-until fail core/t64_seed.c '/* provisional until Phase 10 */' 'core/t64_seed.c:2:'
  seed seeded-until-the fail runner/t64_seed.h '/* names stay provisional until the Phase 10 interface review */' 'runner/t64_seed.h:2:'
  seed seeded-freeze fail core/include/tendies64/t64_facade.h ' * PROVISIONAL: the API freeze is Phase 10.' 'core/include/tendies64/t64_facade.h:2:'
  seed seeded-before fail tools/seed.c '/* decide this before Phase 10 */' 'tools/seed.c:2:'
  seed seeded-review fail core/t64_review.c '/* waits for the Phase 10 interface review */' 'core/t64_review.c:2:'
  seed seeded-case fail core/t64_case.c '/* Provisional Until Phase 10 */' 'core/t64_case.c:2:'
  seed seeded-adr fail docs/adr/0002-language-build-and-abi.md 'the API freeze is Phase 10.' 'docs/adr/0002-language-build-and-abi.md:1:'
  seed clean-history clean core/t64_history.c '/* Phase 10 composite family (plan 10-09) */'
  seed clean-relabel clean core/t64_relabel.c '/* provisional until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12) */'
  # A scan that covers no file is an error, never a pass.
  _empty="$SELF_TMP/empty"
  mkdir -p "$_empty/core"
  _rc=0
  scan "$_empty" > /dev/null 2>&1 || _rc=$?
  if [ "$_rc" -eq 2 ]; then
    echo "selftest ok: an empty scan is an error"
  else
    echo "selftest FAIL: an empty scan should exit 2 (exit $_rc)"
    SELF_FAIL=$((SELF_FAIL + 1))
  fi
  echo "stale-freeze-text selftest: failures=$SELF_FAIL"
  [ "$SELF_FAIL" -eq 0 ]
}

case "${1:-}" in
  "") scan "$ROOT" ;;
  --selftest)
    [ $# -eq 1 ] || usage
    selftest
    ;;
  --root)
    [ $# -eq 2 ] || usage
    [ -d "$2" ] || { echo "stale-freeze-text: not a directory: $2" >&2; exit 2; }
    scan "$2"
    ;;
  *) usage ;;
esac
