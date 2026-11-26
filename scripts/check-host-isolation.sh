#!/bin/sh
# SDL isolation guard (Phase 12, plan 12-07, API-05, SC3, T-12-23, decision DR-16).
#
# The core builds and links with no SDL dependency, and the gate's build never builds
# host/. This guard proves the first half mechanically over the tree and the archive:
#
# Rules (one finding line per violation, "RULE path:line: text"):
#   ISO-SDL       find_package(SDL3, an SDL3:: target, an <SDL3/ include or an SDL_
#                 identifier in a file below core/, runner/, tools/ or tests/, or in
#                 the root CMakeLists.txt. The bare word SDL is no token: it is the
#                 MIPS store-doubleword-left mnemonic of the CPU tests and the English
#                 text of comments
#   ISO-ROOT      a host/ path or an add_subdirectory(host) in the root CMakeLists.txt
#                 (the root build never references the host project)
#   ISO-ARCHIVE   an SDL_ symbol in the symbol table (nm) of the core archive of the
#                 build tree, BUILD/core/libt64_core.a, and of the installed copy
#                 BUILD/install-check/moved prefix/lib/libt64_core.a when it is there
#   ISO-GOLDEN    an SDL_ name in docs/abi/installed-undefined.txt (the undefined
#                 symbols the installed archive may have)
#
# Usage:
#   check-host-isolation.sh [--root DIR] [--build DIR] [--with-selftest]
#   check-host-isolation.sh --selftest
#
#   --root DIR         scan the tree DIR instead of the repository of this script
#   --build DIR        also scan the archives of that build tree (default: no archive
#                      scan; with --build a missing core archive is an error)
#   --with-selftest    run the seeded cases first, then the scan
#   --selftest         only the seeded cases (small temporary trees and one archive
#                      built from a seeded C file)
# Output: the findings and a final "host-isolation: files=N archives=M findings=K".
# Exit 0 clean, 1 findings (or a failed selftest), 2 usage error, a scan that covered
# zero files or a missing archive. POSIX sh, grep, find, nm.
set -eu
LC_ALL=C
export LC_ALL

HERE=$(cd "$(dirname "$0")" && pwd -P)
ROOT=$(cd "$HERE/.." && pwd -P)

usage() {
  echo "usage: check-host-isolation.sh [--root DIR] [--build DIR] [--with-selftest] | --selftest" >&2
  exit 2
}

# The tokens an SDL3 dependency would show: the package, the imported target, the
# include and any SDL_ identifier. SDL alone (the MIPS mnemonic) is not among them.
SDL_TOKENS='find_package[[:space:]]*\([[:space:]]*SDL3|SDL3::|include[[:space:]]*<SDL3/|(^|[^A-Za-z0-9_])SDL_[A-Za-z0-9_]'

# scan_tree ROOT: print ISO-SDL, ISO-ROOT and ISO-GOLDEN findings; the file count goes to the
# file named by $COUNT_FILE.
scan_tree() {
  _root=$1
  _out=$(mktemp "${TMPDIR:-/tmp}/t64iso.XXXXXX")
  _n=0
  for _d in core runner tools tests; do
    if [ -d "$_root/$_d" ]; then
      _c=$(find "$_root/$_d" -type f | wc -l | tr -d ' ')
      _n=$((_n + _c))
      # A fixed-string pass picks the few candidate files, the token expression judges them.
      grep -rIlF -e 'SDL_' -e 'SDL3' "$_root/$_d" 2>/dev/null > "$_out.cand" || true
      while IFS= read -r _cand; do
        grep -nE "$SDL_TOKENS" "$_cand" | sed "s|^|ISO-SDL ${_cand#"$_root"/}:|" >> "$_out" || true
      done < "$_out.cand"
      rm -f "$_out.cand"
    fi
  done
  if [ -f "$_root/CMakeLists.txt" ]; then
    _n=$((_n + 1))
    grep -nE "$SDL_TOKENS" "$_root/CMakeLists.txt" | sed 's|^|ISO-SDL CMakeLists.txt:|' >> "$_out" || true
    grep -nE 'host/|add_subdirectory[[:space:]]*\([[:space:]]*host' "$_root/CMakeLists.txt" |
      sed 's|^|ISO-ROOT CMakeLists.txt:|' >> "$_out" || true
  fi
  if [ -f "$_root/docs/abi/installed-undefined.txt" ]; then
    _n=$((_n + 1))
    grep -nE '(^|[^A-Za-z0-9_])_?SDL_[A-Za-z0-9_]' "$_root/docs/abi/installed-undefined.txt" |
      sed 's|^|ISO-GOLDEN docs/abi/installed-undefined.txt:|' >> "$_out" || true
  fi
  cat "$_out"
  echo "$_n" > "$COUNT_FILE"
  rm -f "$_out"
}

# scan_archive ARCHIVE: print an ISO-ARCHIVE finding per SDL_ symbol of nm.
scan_archive() {
  _archive=$1
  command -v nm >/dev/null 2>&1 || { echo "host-isolation: nm not found" >&2; exit 2; }
  _syms=$(mktemp "${TMPDIR:-/tmp}/t64iso-nm.XXXXXX")
  if ! nm "$_archive" > "$_syms" 2>/dev/null; then
    rm -f "$_syms"
    echo "host-isolation: nm failed on the archive" >&2
    exit 2
  fi
  grep -E '(^|[[:space:]])_?SDL_[A-Za-z0-9_]' "$_syms" | sed 's|^|ISO-ARCHIVE archive symbol: |' || true
  rm -f "$_syms"
}

# scan ROOT BUILD: the tree, then the archive when BUILD is given; the summary line.
scan() {
  _root=$1
  _build=$2
  COUNT_FILE=$(mktemp "${TMPDIR:-/tmp}/t64iso-count.XXXXXX")
  _all=$(mktemp "${TMPDIR:-/tmp}/t64iso-all.XXXXXX")
  scan_tree "$_root" > "$_all" || true
  _files=$(cat "$COUNT_FILE")
  rm -f "$COUNT_FILE"
  _archives=0
  if [ -n "$_build" ]; then
    # The core archive of the build tree is the one that cmake --install copies, byte
    # for byte, into the prefix: it is always scanned (and never races with the install
    # fixture that rewrites BUILD/install-check). The installed copy in the moved
    # prefix is scanned as well when it is there and readable.
    _archive="$_build/core/libt64_core.a"
    if [ ! -f "$_archive" ]; then
      rm -f "$_all"
      echo "host-isolation: no core archive under $_build (build the gate first)" >&2
      return 2
    fi
    _archives=1
    scan_archive "$_archive" >> "$_all" || true
    _installed="$_build/install-check/moved prefix/lib/libt64_core.a"
    if [ -f "$_installed" ] && nm "$_installed" > /dev/null 2>&1; then
      _archives=2
      scan_archive "$_installed" >> "$_all" || true
    fi
  fi
  if [ "$_files" -eq 0 ]; then
    rm -f "$_all"
    echo "host-isolation: no file to scan under $_root" >&2
    return 2
  fi
  _n=$(wc -l < "$_all" | tr -d ' ')
  cat "$_all"
  rm -f "$_all"
  echo "host-isolation: files=$_files archives=$_archives findings=$_n"
  [ "$_n" -eq 0 ]
}

SELF_FAIL=0

# seed NAME EXPECT RULE: scan the temporary tree SEED_TREE as seeded and require a
# finding line starting at RULE (EXPECT fail) or none (EXPECT clean).
judge() {
  _name=$1
  _expect=$2
  _want=$3
  _tree=$4
  _build=${5:-}
  _rc=0
  scan "$_tree" "$_build" > "$_tree.out" 2> "$_tree.err" || _rc=$?
  case "$_expect" in
    clean)
      if [ "$_rc" -eq 0 ]; then
        echo "selftest ok: $_name is accepted"
      else
        echo "selftest FAIL: $_name should be accepted (exit $_rc)"
        cat "$_tree.out"
        SELF_FAIL=$((SELF_FAIL + 1))
      fi
      ;;
    fail)
      if [ "$_rc" -eq 1 ] && grep -q "^$_want" "$_tree.out"; then
        echo "selftest ok: $_name reports $_want"
      else
        echo "selftest FAIL: $_name should report $_want (exit $_rc)"
        cat "$_tree.out"
        SELF_FAIL=$((SELF_FAIL + 1))
      fi
      ;;
  esac
}

# new_tree NAME: a minimal clean tree (one file in each scanned root, the root
# CMakeLists.txt and the undefined-symbol golden); prints its path.
new_tree() {
  _t="$SELF_TMP/$1"
  mkdir -p "$_t/core" "$_t/runner" "$_t/tools/x" "$_t/tests" "$_t/docs/abi"
  printf 'int core_value(void) { return 0; }\n' > "$_t/core/a.c"
  printf 'int runner_value(void) { return 0; }\n' > "$_t/runner/b.c"
  printf 'add_test(NAME x COMMAND true)\n' > "$_t/tools/x/CMakeLists.txt"
  printf 'int tests_value(void) { return 0; }\n' > "$_t/tests/c.c"
  printf 'cmake_minimum_required(VERSION 3.25)\nproject(t)\n' > "$_t/CMakeLists.txt"
  printf '# the undefined symbols\nmemcpy\nmemset\n' > "$_t/docs/abi/installed-undefined.txt"
  echo "$_t"
}

selftest() {
  SELF_TMP=$(mktemp -d "${TMPDIR:-/tmp}/t64iso-self.XXXXXX")
  trap 'rm -rf "$SELF_TMP"' EXIT
  _t=$(new_tree clean-baseline)
  judge clean-baseline clean '' "$_t"
  _t=$(new_tree sdl-package-in-tools)
  printf 'find_package(SDL3 3.2.0 CONFIG REQUIRED)\n' >> "$_t/tools/x/CMakeLists.txt"
  judge sdl-package-in-tools fail 'ISO-SDL tools/x/CMakeLists.txt:' "$_t"
  _t=$(new_tree sdl-target-in-root-cmake)
  printf 'target_link_libraries(t PRIVATE SDL3::SDL3)\n' >> "$_t/CMakeLists.txt"
  judge sdl-target-in-root-cmake fail 'ISO-SDL CMakeLists.txt:' "$_t"
  _t=$(new_tree sdl-include-in-tests)
  printf '#include <SDL3/SDL.h>\n' >> "$_t/tests/c.c"
  judge sdl-include-in-tests fail 'ISO-SDL tests/c.c:' "$_t"
  _t=$(new_tree sdl-identifier-in-core)
  printf 'static int flag = SDL_INIT_VIDEO;\n' >> "$_t/core/a.c"
  judge sdl-identifier-in-core fail 'ISO-SDL core/a.c:' "$_t"
  _t=$(new_tree sdl-identifier-in-runner)
  printf 'static void *w = SDL_CreateWindow;\n' >> "$_t/runner/b.c"
  judge sdl-identifier-in-runner fail 'ISO-SDL runner/b.c:' "$_t"
  _t=$(new_tree host-subdirectory-in-root-cmake)
  printf 'add_subdirectory(host)\n' >> "$_t/CMakeLists.txt"
  judge host-subdirectory-in-root-cmake fail 'ISO-ROOT CMakeLists.txt:' "$_t"
  _t=$(new_tree host-path-in-root-cmake)
  printf 'include(host/shell.cmake)\n' >> "$_t/CMakeLists.txt"
  judge host-path-in-root-cmake fail 'ISO-ROOT CMakeLists.txt:' "$_t"
  _t=$(new_tree sdl-name-in-undefined-golden)
  printf 'SDL_Init\n' >> "$_t/docs/abi/installed-undefined.txt"
  judge sdl-name-in-undefined-golden fail 'ISO-GOLDEN docs/abi/installed-undefined.txt:' "$_t"
  # The MIPS store mnemonic and the plain word SDL are not tokens.
  _t=$(new_tree mips-mnemonic-clean)
  printf '/* LDL/LDR/SDL/SDR offsets match independent big-endian tables */\nstatic const char *name = "SDL";\n' >> "$_t/core/a.c"
  judge mips-mnemonic-clean clean '' "$_t"
  # The archive: a seeded object with an SDL_ symbol must be found, a clean one not.
  if command -v cc >/dev/null 2>&1 && command -v ar >/dev/null 2>&1; then
    _t=$(new_tree archive-with-sdl-symbol)
    _b="$SELF_TMP/archive-with-sdl-symbol-build"
    mkdir -p "$_b/core"
    printf 'int SDL_Seeded(void) { return 1; }\n' > "$_b/seed.c"
    ( cd "$_b" && cc -c seed.c -o seed.o && ar rcs core/libt64_core.a seed.o ) >/dev/null 2>&1 || {
      echo "host-isolation: cannot build the seeded archive" >&2
      exit 2
    }
    judge archive-with-sdl-symbol fail 'ISO-ARCHIVE ' "$_t" "$_b"
    _t=$(new_tree archive-clean)
    _b="$SELF_TMP/archive-clean-build"
    mkdir -p "$_b/core"
    printf 'int plain_symbol(void) { return 1; }\n' > "$_b/seed.c"
    ( cd "$_b" && cc -c seed.c -o seed.o && ar rcs core/libt64_core.a seed.o ) >/dev/null 2>&1 || {
      echo "host-isolation: cannot build the clean archive" >&2
      exit 2
    }
    judge archive-clean clean '' "$_t" "$_b"
  else
    echo "host-isolation: no cc or ar: the archive seeds are not run" >&2
    exit 2
  fi
  # A missing archive under --build is an error, never a pass.
  _t=$(new_tree archive-missing)
  _rc=0
  scan "$_t" "$SELF_TMP/no-such-build" > /dev/null 2>&1 || _rc=$?
  if [ "$_rc" -eq 2 ]; then
    echo "selftest ok: a missing archive is an error"
  else
    echo "selftest FAIL: a missing archive should exit 2 (exit $_rc)"
    SELF_FAIL=$((SELF_FAIL + 1))
  fi
  # A tree with no scanned file is an error.
  mkdir -p "$SELF_TMP/empty"
  _rc=0
  scan "$SELF_TMP/empty" "" > /dev/null 2>&1 || _rc=$?
  if [ "$_rc" -eq 2 ]; then
    echo "selftest ok: an empty scan is an error"
  else
    echo "selftest FAIL: an empty scan should exit 2 (exit $_rc)"
    SELF_FAIL=$((SELF_FAIL + 1))
  fi
  echo "host-isolation selftest: failures=$SELF_FAIL"
  [ "$SELF_FAIL" -eq 0 ]
}

SCAN_ROOT=$ROOT
SCAN_BUILD=
WITH_SELFTEST=0
ONLY_SELFTEST=0
while [ "$#" -gt 0 ]; do
  case $1 in
    --root)
      [ "$#" -ge 2 ] || usage
      [ -d "$2" ] || { echo "host-isolation: not a directory: $2" >&2; exit 2; }
      SCAN_ROOT=$(cd "$2" && pwd -P)
      shift 2
      ;;
    --build)
      [ "$#" -ge 2 ] || usage
      SCAN_BUILD=$2
      shift 2
      ;;
    --with-selftest)
      WITH_SELFTEST=1
      shift
      ;;
    --selftest)
      ONLY_SELFTEST=1
      shift
      ;;
    *) usage ;;
  esac
done
if [ "$ONLY_SELFTEST" -eq 1 ]; then
  selftest
  exit $?
fi
if [ "$WITH_SELFTEST" -eq 1 ]; then
  selftest || exit 1
fi
_status=0
scan "$SCAN_ROOT" "$SCAN_BUILD" || _status=$?
exit "$_status"
