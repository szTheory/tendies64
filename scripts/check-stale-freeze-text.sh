#!/bin/sh
# Stale freeze-text guard (Phase 11, plan 11-11, DR-18; extended in Phase 12,
# plan 12-15).
#
# The API freeze moved from Phase 10 to the Phase 11 candidate and was bound at
# ABI 1.0 by the final bind of Phase 12, so no production comment, header
# banner, specification line or accepted decision record may still say that a
# freeze is, or comes, at Phase 10 or at Phase 11. This guard scans core/,
# runner/, tools/, docs/spec/, docs/abi/ and
# docs/adr/0002-language-build-and-abi.md for the deny phrases below
# (case-insensitive, extended regular expressions):
#   until (the )?Phase 10      freeze is Phase 10      before Phase 10
#   Phase 10 interface review  provisional until Phase 10
#   until (the )?Phase 11      the API is Phase 11
# Two more rules are scoped to the headers under core/include/tendies64, where a
# phrase wrapped across comment lines would hide from a line scan:
#   any "provisional" word is a finding ("path:line: provisional in the
#   installed header"); the one exemption is t64_facade.h, the in-tree runner
#   header that is not installed and not in the freeze record, whose
#   "PROVISIONAL, in-tree only" is accurate (the deny phrases still scan it);
#   each file joined into one line (newlines to spaces, then " * " comment
#   continuations squeezed and space runs collapsed, so a break inside a
#   comment joins) must not hold "until the Phase 11", "the API is Phase 11" or
#   "not a frozen ABI yet" ("path:joined: phrase split across comment lines in
#   the installed header").
# History and component names stay legal: "Phase 10 composite family (plan
# 10-09)", "Phase 11 freeze candidate (plan 11-23)", test-history headers and
# fixture comments do not match any phrase. Closed Phase 9 to Phase 11 records
# under docs/claims, docs/evidence and fixtures/ are outside the scan on
# purpose (they are never relabeled).
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

DENY='until (the )?Phase 10|freeze is Phase 10|before Phase 10|Phase 10 interface review|provisional until Phase 10|until (the )?Phase 11|the API is Phase 11'
SCAN_PATHS='core runner tools docs/spec docs/abi docs/adr/0002-language-build-and-abi.md'
HEADER_DIR='core/include/tendies64'
HEADER_JOINED='until the Phase 11|the API is Phase 11|not a frozen ABI yet'
# Not installed and outside the freeze record (docs/abi/FREEZE-CANDIDATE.md): the
# "provisional" word rule skips this one file by exact name.
HEADER_RULE_EXEMPT='t64_facade.h'

SELF=$0
HERE=$(cd "$(dirname "$0")" && pwd -P)
ROOT=$(cd "$HERE/.." && pwd -P)

usage() {
  echo "usage: check-stale-freeze-text.sh [--root DIR | --selftest]" >&2
  exit 2
}

# header_rules DIR: print the header-scoped findings (without the STALE-FREEZE
# prefix) for the files under DIR/core/include/tendies64.
header_rules() {
  _hd=$1
  [ -d "$_hd/$HEADER_DIR" ] || return 0
  (cd "$_hd" && find "$HEADER_DIR" -type f | sort) | while IFS= read -r _hf; do
    if [ "${_hf##*/}" != "$HEADER_RULE_EXEMPT" ]; then
      grep -n -I -i -e provisional "$_hd/$_hf" | cut -d: -f1 | while IFS= read -r _ln; do
        echo "$_hf:$_ln: provisional in the installed header"
      done
    fi
    if tr '\n' ' ' < "$_hd/$_hf" | sed 's/ \* / /g; s/  */ /g' | grep -q -I -i -E -e "$HEADER_JOINED"; then
      echo "$_hf:joined: phrase split across comment lines in the installed header"
    fi
  done
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
  header_rules "$_dir" >> "$_tmp"
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
  mkdir -p "$_tree/core/include/tendies64" "$_tree/runner" "$_tree/tools" "$_tree/docs/adr" "$_tree/docs/spec" "$_tree/docs/abi"
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
  seed clean-relabel clean core/t64_relabel.c '/* bound by ABI 1.0, docs/abi/FREEZE-CANDIDATE.md */'
  # Phase 12 (plan 12-15): the Phase 11 promise phrases, the header-scoped
  # provisional rule and the header joined-text rule.
  seed seeded-until-11 fail core/t64_seed11.c '/* provisional until Phase 11 */' 'core/t64_seed11.c:2:'
  seed seeded-until-the-11 fail core/t64_seed11the.c '/* revisable until the Phase 11 freeze candidate (final ABI bind at the end of Phase 12) */' 'core/t64_seed11the.c:2:'
  seed seeded-api-is-11 fail core/t64_seedapi.c '/* the string is not public API work (the API is Phase 11) */' 'core/t64_seedapi.c:2:'
  seed seeded-spec fail docs/spec/seed.md 'numbers are revisable until the Phase 11 freeze candidate.' 'docs/spec/seed.md:2:'
  seed seeded-abi-dir fail docs/abi/seed.md 'the API is Phase 11.' 'docs/abi/seed.md:2:'
  seed seeded-header-provisional fail core/include/tendies64/t64_machine.h ' * a note that stays provisional.' 'core/include/tendies64/t64_machine.h:2: provisional in the installed header'
  seed seeded-header-other-name fail core/include/tendies64/t64_other.h ' * PROVISIONAL, in-tree only.' 'core/include/tendies64/t64_other.h:2: provisional in the installed header'
  seed seeded-header-wrapped fail core/include/tendies64/t64_machine.h "$(printf '/* a note that is provisional\n * until the\n * Phase 11 freeze candidate */')" 'core/include/tendies64/t64_machine.h:2: provisional in the installed header'
  seed seeded-header-joined fail core/include/tendies64/t64_seed_joined.h "$(printf '/*\n * it is not a frozen\n * ABI yet.\n */')" 'core/include/tendies64/t64_seed_joined.h:joined:'
  seed seeded-header-joined-until fail core/include/tendies64/t64_seed_joined.h "$(printf '/* kept until the\n * Phase 11 freeze candidate */')" 'core/include/tendies64/t64_seed_joined.h:joined:'
  seed clean-facade-provisional clean core/include/tendies64/t64_facade.h ' * Tendies64 synthetic-guest runner header (PROVISIONAL, in-tree only).'
  seed clean-history-11 clean core/t64_history11.c '/* Phase 11 freeze candidate (plan 11-23) */'
  seed clean-history-record clean docs/abi/FREEZE-CANDIDATE.md 'Plan 11-23 of Phase 11 bound this record as the freeze candidate ABI 0.1.'
  seed clean-header-bound clean core/include/tendies64/t64_machine.h ' * the ABI bound at 1.0 by docs/abi/FREEZE-CANDIDATE.md'
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
