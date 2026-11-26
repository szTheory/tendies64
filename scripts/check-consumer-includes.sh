#!/bin/sh
# Consumer independence guard (Phase 11, plan 11-16, VER-05, T-11-28).
#
# The installed-package consumers under tests/installed-consumer/ prove that the
# header and the archive an outside host receives are enough. They may therefore
# see nothing of the source tree: the only repository header a consumer may
# include is the public one through <tendies64/...>.
#
# Rules (one finding line per violation, "RULE path:line: text"):
#   CONSUMER-INCLUDE  a #include in a *.c or *.h file below tests/installed-consumer
#                     that is none of: <tendies64/NAME> (no ".." in the name), a
#                     system header of the fixed list SYSTEM_HEADERS below, or a
#                     quoted header that exists inside tests/installed-consumer
#                     (no ".." and not a .c file, and never one of the names in
#                     DENY_NAMES, which are the repository's private headers); a
#                     computed include (#include MACRO) is also a finding
#   CONSUMER-CMAKE    a line of a CMakeLists.txt or *.cmake file below
#                     tests/installed-consumer (comments stripped) that has
#                     add_subdirectory, the directory-wide include_directories,
#                     CMAKE_SOURCE_DIR, a "../" path, or a path starting at core/,
#                     runner/ or tools/ (target_include_directories with a local
#                     path stays legal)
#
# Usage:
#   check-consumer-includes.sh              scan the repository containing this script
#   check-consumer-includes.sh --root DIR   scan DIR instead
#   check-consumer-includes.sh --selftest   seeded cases in temporary copies
# Output: the findings and a final "consumer-includes: files=N findings=M".
# Exit 0 clean, 1 findings (or a failed selftest), 2 usage error or a scan that
# covered zero files. POSIX sh and awk only, no git needed.
set -eu
LC_ALL=C
export LC_ALL

CONSUMER_DIR=tests/installed-consumer

# C standard headers and the few POSIX ones a consumer may use.
SYSTEM_HEADERS='assert.h ctype.h errno.h float.h inttypes.h limits.h math.h stdalign.h stdbool.h stddef.h stdint.h stdio.h stdlib.h string.h time.h fcntl.h unistd.h pthread.h sys/stat.h sys/types.h'

# Private repository headers that a consumer must never include, even when a
# file of the same name were placed inside the consumer directory.
DENY_NAMES='t64_internal.h t64_test.h t64_sha256.h t64_facade.h t64_prefix.h t64_json.h t64_utf8.h t64_jsonw.h'

SELF=$0
HERE=$(cd "$(dirname "$0")" && pwd -P)
ROOT=$(cd "$HERE/.." && pwd -P)

usage() {
  echo "usage: check-consumer-includes.sh [--root DIR | --selftest]" >&2
  exit 2
}

# scan_includes ROOT FILE: print CONSUMER-INCLUDE findings of one C file. The
# directory of FILE (relative to the consumer directory) is where a quoted
# include is looked up first, then the consumer directory itself.
scan_includes() {
  _root=$1
  _file=$2
  _cdir="$_root/$CONSUMER_DIR"
  _fdir=$(dirname "$_file")
  awk -v rel="${_file#"$_root"/}" -v cdir="$_cdir" -v fdir="$_fdir" \
      -v sys="$SYSTEM_HEADERS" -v deny="$DENY_NAMES" '
    function bad(n, text) { printf "CONSUMER-INCLUDE %s:%d: %s\n", rel, n, text; found++ }
    function exists(path,   cmd, rc) {
      cmd = "test -f \"" path "\""
      rc = system(cmd)
      return rc == 0
    }
    BEGIN {
      ns = split(sys, s, " "); for (i = 1; i <= ns; i++) system_ok[s[i]] = 1
      nd = split(deny, d, " "); for (i = 1; i <= nd; i++) denied[d[i]] = 1
    }
    /^[ \t]*#[ \t]*include/ {
      line = $0
      sub(/^[ \t]*#[ \t]*include[ \t]*/, "", line)
      sub(/[ \t]*(\/\*.*|\/\/.*)?$/, "", line)
      if (line ~ /^<[^>]+>$/) {
        name = substr(line, 2, length(line) - 2)
        if (name ~ /^tendies64\// && name !~ /\.\./) next
        if (name in system_ok) next
        bad(NR, $0); next
      }
      if (line ~ /^"[^"]+"$/) {
        name = substr(line, 2, length(line) - 2)
        base = name; sub(/^.*\//, "", base)
        if (name ~ /\.\./ || name ~ /^\// || name ~ /\.c$/ || (base in denied)) { bad(NR, $0); next }
        if (exists(fdir "/" name) || exists(cdir "/" name)) next
        bad(NR, $0); next
      }
      bad(NR, $0)
    }
    END { exit found ? 1 : 0 }
  ' "$_file"
}

# scan_cmake ROOT FILE: print CONSUMER-CMAKE findings of one CMake file, after
# stripping comments (a "#" starts a comment; the consumer files carry no
# quoted "#").
scan_cmake() {
  _root=$1
  _file=$2
  awk -v rel="${_file#"$_root"/}" '
    {
      line = $0
      sub(/#.*/, "", line)
      hit = 0
      if (tolower(line) ~ /add_subdirectory/) hit = 1
      if (tolower(line) ~ /(^|[^a-z_])include_directories[ \t]*\(/) hit = 1
      if (line ~ /CMAKE_SOURCE_DIR/) hit = 1
      if (line ~ /\.\.\//) hit = 1
      if (line ~ /(^|[^A-Za-z0-9_.])(core|runner|tools)\//) hit = 1
      if (hit) { printf "CONSUMER-CMAKE %s:%d: %s\n", rel, NR, $0; found++ }
    }
    END { exit found ? 1 : 0 }
  ' "$_file"
}

# scan DIR: print findings and the summary; return 0 clean, 1 findings, 2 when
# nothing was scanned.
scan() {
  _dir=$1
  _cdir="$_dir/$CONSUMER_DIR"
  _tmp=$(mktemp "${TMPDIR:-/tmp}/t64consumer.XXXXXX")
  _list=$(mktemp "${TMPDIR:-/tmp}/t64consumer-list.XXXXXX")
  if [ -d "$_cdir" ]; then
    find "$_cdir" -type f \( -name '*.c' -o -name '*.h' -o -name 'CMakeLists.txt' -o -name '*.cmake' \) | sort > "$_list"
  fi
  _files=$(wc -l < "$_list" | tr -d ' ')
  if [ "$_files" -eq 0 ]; then
    rm -f "$_tmp" "$_list"
    echo "consumer-includes: no consumer file to scan under $_cdir" >&2
    return 2
  fi
  while IFS= read -r _f; do
    case $_f in
      *.c | *.h) scan_includes "$_dir" "$_f" >> "$_tmp" || true ;;
      *) scan_cmake "$_dir" "$_f" >> "$_tmp" || true ;;
    esac
  done < "$_list"
  _n=$(wc -l < "$_tmp" | tr -d ' ')
  cat "$_tmp"
  rm -f "$_tmp" "$_list"
  echo "consumer-includes: files=$_files findings=$_n"
  [ "$_n" -eq 0 ]
}

SELF_FAIL=0

# seed NAME EXPECT FILE ACTION TEXT [WANT]: copy the committed consumer into a
# fresh tree, apply ACTION (append to FILE, write FILE new, or write FILE new and
# include it from main.c), scan it and
# require the outcome: clean, or fail with a line starting at WANT.
seed() {
  _name=$1
  _expect=$2
  _path=$3
  _action=$4
  _text=$5
  _want=${6:-}
  _tree="$SELF_TMP/$_name"
  mkdir -p "$_tree/$CONSUMER_DIR"
  cp -R "$ROOT/$CONSUMER_DIR/." "$_tree/$CONSUMER_DIR/"
  case "$_action" in
    append) printf '%s\n' "$_text" >> "$_tree/$CONSUMER_DIR/$_path" ;;
    create) printf '%s\n' "$_text" > "$_tree/$CONSUMER_DIR/$_path" ;;
    local-include)
      printf '%s\n' "$_text" > "$_tree/$CONSUMER_DIR/$_path"
      printf '#include "%s"\n' "$_path" >> "$_tree/$CONSUMER_DIR/main.c"
      ;;
    none) : ;;
  esac
  _rc=0
  scan "$_tree" > "$_tree/out.txt" 2> "$_tree/err.txt" || _rc=$?
  case "$_expect" in
    clean)
      if [ "$_rc" -eq 0 ]; then
        echo "selftest ok: $_name is accepted"
      else
        echo "selftest FAIL: $_name should be accepted (exit $_rc)"
        cat "$_tree/out.txt"
        SELF_FAIL=$((SELF_FAIL + 1))
      fi
      ;;
    fail)
      if [ "$_rc" -eq 1 ] && grep -q "^$_want" "$_tree/out.txt"; then
        echo "selftest ok: $_name reports $_want"
      else
        echo "selftest FAIL: $_name should report $_want (exit $_rc)"
        cat "$_tree/out.txt"
        SELF_FAIL=$((SELF_FAIL + 1))
      fi
      ;;
  esac
}

selftest() {
  [ -d "$ROOT/$CONSUMER_DIR" ] || { echo "consumer-includes: no $CONSUMER_DIR to seed from" >&2; exit 2; }
  SELF_TMP=$(mktemp -d "${TMPDIR:-/tmp}/t64consumer-self.XXXXXX")
  trap 'rm -rf "$SELF_TMP"' EXIT
  # The three mandatory seeds of the plan.
  seed seeded-internal-include fail main.c append '#include "t64_internal.h"' 'CONSUMER-INCLUDE tests/installed-consumer/main.c:'
  seed seeded-relative-include fail main.c append '#include "../../core/include/tendies64/t64_machine.h"' 'CONSUMER-INCLUDE tests/installed-consumer/main.c:'
  seed seeded-cmake-path fail CMakeLists.txt append 'add_subdirectory(../../core core)' 'CONSUMER-CMAKE tests/installed-consumer/CMakeLists.txt:'
  # More ways to leave the installed package.
  seed seeded-angle-internal fail main.c append '#include <t64_internal.h>' 'CONSUMER-INCLUDE '
  seed seeded-quoted-public-path fail main.c append '#include "tendies64/t64_machine.h"' 'CONSUMER-INCLUDE '
  seed seeded-dotdot-public fail main.c append '#include <tendies64/../t64_internal.h>' 'CONSUMER-INCLUDE '
  seed seeded-computed-include fail main.c append '#include T64_HEADER' 'CONSUMER-INCLUDE '
  seed seeded-denied-local fail t64_sha256.h local-include '/* a local copy does not make a private header legal */' 'CONSUMER-INCLUDE '
  seed seeded-cmake-core-path fail CMakeLists.txt append 'target_include_directories(t64c PRIVATE core/include)' 'CONSUMER-CMAKE '
  seed seeded-cmake-source-dir fail CMakeLists.txt append 'target_include_directories(t64c PRIVATE ${CMAKE_SOURCE_DIR}/x)' 'CONSUMER-CMAKE '
  seed seeded-cmake-dotdot fail CMakeLists.txt append 'target_sources(t64c PRIVATE ../x.c)' 'CONSUMER-CMAKE '
  seed seeded-cmake-include-directories fail CMakeLists.txt append 'include_directories(/somewhere)' 'CONSUMER-CMAKE '
  # Clean controls: the committed consumer, a comment that names a forbidden
  # command, a local header and a local target include directory.
  seed clean-committed clean main.c none ''
  seed clean-comment clean CMakeLists.txt append '# never add_subdirectory ../../core or include_directories here'
  seed clean-local-header clean local.h local-include '/* local */'
  seed clean-target-include clean CMakeLists.txt append 'target_include_directories(t64c PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")'
  # A scan that covers no file is an error, never a pass.
  _empty="$SELF_TMP/empty"
  mkdir -p "$_empty/$CONSUMER_DIR"
  _rc=0
  scan "$_empty" > /dev/null 2>&1 || _rc=$?
  if [ "$_rc" -eq 2 ]; then
    echo "selftest ok: an empty scan is an error"
  else
    echo "selftest FAIL: an empty scan should exit 2 (exit $_rc)"
    SELF_FAIL=$((SELF_FAIL + 1))
  fi
  echo "consumer-includes selftest: failures=$SELF_FAIL"
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
    [ -d "$2" ] || { echo "consumer-includes: not a directory: $2" >&2; exit 2; }
    scan "$2"
    ;;
  *) usage ;;
esac
