#!/bin/sh
# Host independence guard (Phase 12, plan 12-07, API-05, T-12-22, decision DR-16).
#
# A sibling of scripts/check-consumer-includes.sh for the minimal host of host/. The
# host is the second consumer of the installed package: the pump and its tests see
# the library only through <tendies64/t64_machine.h>, and only the optional SDL3
# shell may see SDL3. The consumer guard scans tests/installed-consumer alone, so
# this one scans every C and CMake file below host/ (host/pump, host/tests and,
# from plan 12-09, host/sdl).
#
# Rules (one finding line per violation, "RULE path:line: text"):
#   HOST-INCLUDE      a #include in a *.c or *.h file below host/ that is none of: <tendies64/t64_machine.h> (the one
#                     public header; any other tendies64 header, and any ".." in the
#                     name, is a finding), a C99 standard header of the fixed list
#                     STD_HEADERS below, or a quoted header that exists inside host/
#                     (no ".." and not a .c file, and never one of the names in
#                     DENY_NAMES, which are the repository's private headers); a
#                     computed include (#include MACRO) is also a finding
#   HOST-SDL-INCLUDE  an <SDL3/...> include in a file outside host/sdl (inside
#                     host/sdl it is allowed)
#   HOST-SDL-IDENT    an SDL_ identifier in code (comments are stripped) of a file
#                     outside host/sdl: the pump and its tests name no SDL type
#   HOST-PUMP-TIME    <time.h> included by a file below host/pump: the pump takes
#                     time only from the clock callback it is given
#   HOST-CMAKE        a line of a CMakeLists.txt or *.cmake file below host/
#                     (comments stripped) that has the directory-wide
#                     include_directories, CMAKE_SOURCE_DIR (the host is a standalone
#                     project), an add_subdirectory or other path with "../", or a
#                     path starting at core/, runner/ or tools/ (target_include_
#                     directories with a local path stays legal, and so does
#                     find_package(SDL3) of the shell)
#
# Usage:
#   check-host-includes.sh              scan the repository containing this script
#   check-host-includes.sh --root DIR   scan DIR instead
#   check-host-includes.sh --selftest   seeded cases in temporary copies of host/
# Output: the findings and a final "host-includes: files=N findings=M".
# Exit 0 clean, 1 findings (or a failed selftest), 2 usage error or a scan that
# covered zero files. POSIX sh and awk only, no git needed.
set -eu
LC_ALL=C
export LC_ALL

HOST_DIR=host

# The C99 standard headers (ISO/IEC 9899:1999 section 4, clause 7).
STD_HEADERS='assert.h complex.h ctype.h errno.h fenv.h float.h inttypes.h iso646.h limits.h locale.h math.h setjmp.h signal.h stdarg.h stdbool.h stddef.h stdint.h stdio.h stdlib.h string.h tgmath.h time.h wchar.h wctype.h'

# Private repository headers that the host must never include, even when a file of
# the same name were placed inside host/.
DENY_NAMES='t64_internal.h t64_test.h t64_sha256.h t64_facade.h t64_prefix.h t64_json.h t64_utf8.h t64_jsonw.h'

SELF=$0
HERE=$(cd "$(dirname "$0")" && pwd -P)
ROOT=$(cd "$HERE/.." && pwd -P)

usage() {
  echo "usage: check-host-includes.sh [--root DIR | --selftest]" >&2
  exit 2
}

# scan_source ROOT FILE HEADERS: print the findings of one C file. HEADERS is the
# space-separated list of the *.h files below host/ (paths relative to host/, none holds
# a space), the set a quoted include may name. Exit 0 clean, 1 findings, 2 an awk failure.
scan_source() {
  _root=$1
  _file=$2
  _headers=$3
  _rel=${_file#"$_root"/}
  awk -v rel="$_rel" -v std="$STD_HEADERS" -v deny="$DENY_NAMES" -v headers="$_headers" '
    function bad(rule, n, text) { printf "%s %s:%d: %s\n", rule, rel, n, text; found++ }
    function basename(path) { sub(/^.*\//, "", path); return path }
    function dirname_rel(path) { if (path !~ /\//) return ""; sub(/\/[^\/]*$/, "", path); return path }
    BEGIN {
      ns = split(std, s, " "); for (i = 1; i <= ns; i++) std_ok[s[i]] = 1
      nd = split(deny, d, " "); for (i = 1; i <= nd; i++) denied[d[i]] = 1
      nh = split(headers, h, " "); for (i = 1; i <= nh; i++) if (h[i] != "") {
        have[h[i]] = 1; have_base[basename(h[i])] = 1
      }
      in_sdl = (rel ~ /^host\/sdl\//)
      in_pump = (rel ~ /^host\/pump\//)
      here = dirname_rel(rel); sub(/^host\/?/, "", here)
      in_block = 0
    }
    {
      line = $0
    }
    /^[ \t]*#[ \t]*include/ && !in_block {
      inc = line
      sub(/^[ \t]*#[ \t]*include[ \t]*/, "", inc)
      sub(/[ \t]*(\/\*.*|\/\/.*)?$/, "", inc)
      if (inc ~ /^<[^>]+>$/) {
        name = substr(inc, 2, length(inc) - 2)
        if (name ~ /^SDL3\//) {
          if (!in_sdl) bad("HOST-SDL-INCLUDE", NR, line)
        } else if (name ~ /^tendies64\//) {
          if (name != "tendies64/t64_machine.h") bad("HOST-INCLUDE", NR, line)
        } else if (name in std_ok) {
          if (name == "time.h" && in_pump) bad("HOST-PUMP-TIME", NR, line)
        } else {
          bad("HOST-INCLUDE", NR, line)
        }
      } else if (inc ~ /^"[^"]+"$/) {
        name = substr(inc, 2, length(inc) - 2)
        base = basename(name)
        if (name ~ /\.\./ || name ~ /^\// || name ~ /\.c$/ || (base in denied)) {
          bad("HOST-INCLUDE", NR, line)
        } else {
          cand = (here != "") ? (here "/" name) : name
          if (!(cand in have) && !(name in have) && !(name !~ /\// && (name in have_base))) {
            bad("HOST-INCLUDE", NR, line)
          }
        }
      } else {
        bad("HOST-INCLUDE", NR, line)
      }
    }
    {
      # Code with the comments removed (a block comment may span lines; a "//" comment
      # ends at the line end), for the SDL identifier rule.
      code = ""
      rest = line
      while (rest != "") {
        if (in_block) {
          p = index(rest, "*/")
          if (p == 0) { rest = ""; break }
          rest = substr(rest, p + 2); in_block = 0
        } else {
          a = index(rest, "/*"); b = index(rest, "//")
          if (b > 0 && (a == 0 || b < a)) { code = code substr(rest, 1, b - 1); rest = ""; break }
          if (a > 0) { code = code substr(rest, 1, a - 1); rest = substr(rest, a + 2); in_block = 1 }
          else { code = code rest; rest = "" }
        }
      }
      if (!in_sdl && code !~ /^[ \t]*#[ \t]*include/ && code ~ /(^|[^A-Za-z0-9_])SDL_[A-Za-z0-9_]/) {
        bad("HOST-SDL-IDENT", NR, line)
      }
    }
    END { exit found ? 1 : 0 }
  ' "$_file"
}

# scan_cmake ROOT FILE: print HOST-CMAKE findings of one CMake file, after stripping
# comments (a "#" starts a comment; the host files carry no quoted "#").
scan_cmake() {
  _root=$1
  _file=$2
  awk -v rel="${_file#"$_root"/}" '
    {
      line = $0
      sub(/#.*/, "", line)
      hit = 0
      if (tolower(line) ~ /(^|[^a-z_])include_directories[ \t]*\(/ && tolower(line) !~ /target_include_directories/) hit = 1
      if (line ~ /CMAKE_SOURCE_DIR/) hit = 1
      if (line ~ /\.\.\//) hit = 1
      if (line ~ /(^|[^A-Za-z0-9_.])(core|runner|tools)\//) hit = 1
      if (hit) { printf "HOST-CMAKE %s:%d: %s\n", rel, NR, $0; found++ }
    }
    END { exit found ? 1 : 0 }
  ' "$_file"
}

# scan DIR: print findings and the summary; return 0 clean, 1 findings, 2 when
# nothing was scanned.
scan() {
  _dir=$1
  _tmp=$(mktemp "${TMPDIR:-/tmp}/t64host.XXXXXX")
  _list=$(mktemp "${TMPDIR:-/tmp}/t64host-list.XXXXXX")
  if [ -d "$_dir/$HOST_DIR" ]; then
    find "$_dir/$HOST_DIR" -type f \( -name '*.c' -o -name '*.h' -o -name 'CMakeLists.txt' -o -name '*.cmake' \) | sort > "$_list"
  fi
  _files=$(wc -l < "$_list" | tr -d ' ')
  if [ "$_files" -eq 0 ]; then
    rm -f "$_tmp" "$_list"
    echo "host-includes: no host file to scan under $_dir/$HOST_DIR" >&2
    return 2
  fi
  _headers=$(grep '\.h$' "$_list" | sed "s|^$_dir/$HOST_DIR/||" | tr '\n' ' ' || true)
  # awk exits 1 for findings and 2 when it failed: a failed scan is never a clean one.
  while IFS= read -r _f; do
    _rc=0
    case $_f in
      *.c | *.h) scan_source "$_dir" "$_f" "$_headers" >> "$_tmp" || _rc=$? ;;
      *) scan_cmake "$_dir" "$_f" >> "$_tmp" || _rc=$? ;;
    esac
    if [ "$_rc" -gt 1 ]; then
      rm -f "$_tmp" "$_list"
      echo "host-includes: the scanner failed on $_f (exit $_rc)" >&2
      return 2
    fi
  done < "$_list"
  _n=$(wc -l < "$_tmp" | tr -d ' ')
  cat "$_tmp"
  rm -f "$_tmp" "$_list"
  echo "host-includes: files=$_files findings=$_n"
  [ "$_n" -eq 0 ]
}

SELF_FAIL=0

# seed NAME EXPECT FILE ACTION TEXT [WANT]: copy the committed host/ into a fresh tree,
# apply ACTION (append to FILE, or write FILE new, or write FILE new and include it from
# host/tests/t64h_tests.c), scan it and require the outcome: clean, or fail with a line
# starting at WANT.
seed() {
  _name=$1
  _expect=$2
  _path=$3
  _action=$4
  _text=$5
  _want=${6:-}
  _tree="$SELF_TMP/$_name"
  mkdir -p "$_tree/$HOST_DIR"
  cp -R "$ROOT/$HOST_DIR/." "$_tree/$HOST_DIR/"
  case "$_action" in
    append) printf '%s\n' "$_text" >> "$_tree/$HOST_DIR/$_path" ;;
    create)
      mkdir -p "$(dirname "$_tree/$HOST_DIR/$_path")"
      printf '%s\n' "$_text" > "$_tree/$HOST_DIR/$_path"
      ;;
    local-include)
      printf '%s\n' "$_text" > "$_tree/$HOST_DIR/$_path"
      printf '#include "%s"\n' "$(basename "$_path")" >> "$_tree/$HOST_DIR/tests/t64h_tests.c"
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
  [ -d "$ROOT/$HOST_DIR" ] || { echo "host-includes: no $HOST_DIR to seed from" >&2; exit 2; }
  SELF_TMP=$(mktemp -d "${TMPDIR:-/tmp}/t64host-self.XXXXXX")
  trap 'rm -rf "$SELF_TMP"' EXIT
  _pump=pump/t64h_pump.c
  _test=tests/t64h_tests.c
  # One seed per rule, each reported with its own rule id.
  seed seeded-internal-include fail "$_pump" append '#include "t64_internal.h"' 'HOST-INCLUDE host/pump/t64h_pump.c:'
  seed seeded-other-tendies64-header fail "$_pump" append '#include <tendies64/t64_facade.h>' 'HOST-INCLUDE host/pump/t64h_pump.c:'
  seed seeded-relative-include fail "$_test" append '#include "../../core/include/tendies64/t64_machine.h"' 'HOST-INCLUDE host/tests/t64h_tests.c:'
  seed seeded-dotdot-public fail "$_pump" append '#include <tendies64/../t64_internal.h>' 'HOST-INCLUDE '
  seed seeded-posix-header fail "$_pump" append '#include <unistd.h>' 'HOST-INCLUDE '
  seed seeded-computed-include fail "$_pump" append '#include T64_HEADER' 'HOST-INCLUDE '
  seed seeded-denied-local fail tests/t64_sha256.h local-include '/* a local copy does not make a private header legal */' 'HOST-INCLUDE '
  seed seeded-sdl-include-in-pump fail "$_pump" append '#include <SDL3/SDL.h>' 'HOST-SDL-INCLUDE host/pump/t64h_pump.c:'
  seed seeded-sdl-include-in-tests fail "$_test" append '#include <SDL3/SDL.h>' 'HOST-SDL-INCLUDE host/tests/t64h_tests.c:'
  seed seeded-sdl-ident-in-pump fail "$_pump" append 'static int shell_flag = SDL_INIT_VIDEO;' 'HOST-SDL-IDENT host/pump/t64h_pump.c:'
  seed seeded-sdl-ident-in-tests fail "$_test" append 'static void *window = SDL_CreateWindow;' 'HOST-SDL-IDENT host/tests/t64h_tests.c:'
  seed seeded-pump-time-include fail "$_pump" append '#include <time.h>' 'HOST-PUMP-TIME host/pump/t64h_pump.c:'
  seed seeded-cmake-core-path fail CMakeLists.txt append 'target_include_directories(t64h_pump PRIVATE core/include)' 'HOST-CMAKE host/CMakeLists.txt:'
  seed seeded-cmake-source-dir fail CMakeLists.txt append 'target_include_directories(t64h_pump PRIVATE ${CMAKE_SOURCE_DIR}/x)' 'HOST-CMAKE '
  seed seeded-cmake-dotdot fail CMakeLists.txt append 'add_subdirectory(../core core)' 'HOST-CMAKE '
  seed seeded-cmake-include-directories fail CMakeLists.txt append 'include_directories(/somewhere)' 'HOST-CMAKE '
  # Clean controls: the committed host, an SDL3 include and identifier inside host/sdl, a
  # comment that names a forbidden thing, a local header, a local target include
  # directory and the SDL3 package of the shell in the host CMakeLists.
  seed clean-committed clean "$_pump" none ''
  seed clean-sdl-shell clean sdl/shell.c create '#include <SDL3/SDL.h>
#include <tendies64/t64_machine.h>
static int shell(void) { return SDL_Init(SDL_INIT_VIDEO); }'
  seed clean-comment-names-sdl clean "$_pump" append '/* the shell, not the pump, calls SDL_Init
   and includes <SDL3/SDL.h> and "t64_internal.h" */
// SDL_Quit is the shell too'
  seed clean-local-header clean tests/local.h local-include '/* local */'
  seed clean-target-include clean CMakeLists.txt append 'target_include_directories(t64h_pump PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/pump")'
  seed clean-cmake-comment clean CMakeLists.txt append '# never add_subdirectory ../core or include_directories here'
  seed clean-cmake-sdl3-package clean CMakeLists.txt append 'find_package(SDL3 3.2.0 CONFIG REQUIRED)'
  # A scan that covers no file is an error, never a pass.
  _empty="$SELF_TMP/empty"
  mkdir -p "$_empty/$HOST_DIR"
  _rc=0
  scan "$_empty" > /dev/null 2>&1 || _rc=$?
  if [ "$_rc" -eq 2 ]; then
    echo "selftest ok: an empty scan is an error"
  else
    echo "selftest FAIL: an empty scan should exit 2 (exit $_rc)"
    SELF_FAIL=$((SELF_FAIL + 1))
  fi
  echo "host-includes selftest: failures=$SELF_FAIL"
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
    [ -d "$2" ] || { echo "host-includes: not a directory: $2" >&2; exit 2; }
    scan "$2"
    ;;
  *) usage ;;
esac
