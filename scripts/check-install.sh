#!/bin/sh
# Install, relocate and consumer driver (Phase 11, plan 11-16, VER-05, DR-20).
# POSIX sh, no bashisms. Installs the Development component of an existing
# build tree into a prefix whose path contains spaces, moves the prefix, then
# configures, builds and runs the independent consumer of tests/installed-consumer
# against the moved prefix. Registered as the CTest fixtures t64_install and
# t64_consumer by tools/abi/CMakeLists.txt.
#
#   sh scripts/check-install.sh [--build DIR] [--work DIR] SUBCOMMAND [CASE ARGS]
#
#   setup          remove WORK, cmake --install BUILD --component Development
#                  --prefix "WORK/prefix with space", move it to "WORK/moved prefix"
#                  and assert that it holds exactly the six files of the package
#   build          configure tests/installed-consumer against the moved prefix
#                  (Ninja, Release, the compiler of the build tree) into
#                  WORK/consumer, build it, and assert that neither the moved
#                  prefix nor the consumer tree mentions the original install path
#   run CASE ...   run WORK/consumer/t64c --case CASE with the remaining arguments
#
# --build DIR   the configured and built tree to install from (default
#               build/gate); nothing is rebuilt
# --work DIR    scratch directory (default BUILD/install-check)
#
# The consumer is a separate top-level CMake project: this script never adds it
# to the main build and passes it nothing but the prefix, the generator, the
# build type, the compiler and the compile-commands switch.
# Exit codes: 0 pass, 1 a finding, 2 usage error or a missing tool or tree.
set -eu
LC_ALL=C
export LC_ALL

ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
BUILD=$ROOT/build/gate
WORK=

usage() {
  echo "usage: sh scripts/check-install.sh [--build DIR] [--work DIR] setup|build|run CASE [ARGS]" >&2
  exit 2
}

fail() {
  echo "check-install: FAIL: $*" >&2
  exit 1
}

die() {
  echo "check-install: $*" >&2
  exit 2
}

while [ "$#" -gt 0 ]; do
  case $1 in
    --build)
      [ "$#" -ge 2 ] || usage
      BUILD=$2
      shift 2
      ;;
    --work)
      [ "$#" -ge 2 ] || usage
      WORK=$2
      shift 2
      ;;
    --*) usage ;;
    *) break ;;
  esac
done
[ "$#" -ge 1 ] || usage
SUB=$1
shift

[ -d "$BUILD" ] || die "build tree not found: $BUILD"
BUILD=$(cd "$BUILD" && pwd -P)
[ -f "$BUILD/CMakeCache.txt" ] || die "not a configured CMake tree: $BUILD"
[ -n "$WORK" ] || WORK=$BUILD/install-check
case $WORK in
  "" | / | "$ROOT" | "$BUILD" | "${HOME:-/nonexistent}") die "refusing the work directory: $WORK" ;;
esac

PREFIX="$WORK/prefix with space"
MOVED="$WORK/moved prefix"
CONSUMER_BUILD="$WORK/consumer"

# cache_value NAME: the value of NAME:TYPE=value in the build tree cache.
cache_value() {
  sed -n "s/^$1:[A-Z]*=//p" "$BUILD/CMakeCache.txt" | head -n 1
}

CMAKE=$(cache_value CMAKE_COMMAND)
[ -n "$CMAKE" ] && [ -x "$CMAKE" ] || CMAKE=cmake
command -v "$CMAKE" >/dev/null 2>&1 || die "cmake not found"

# The single configuration the tree was built for names the targets file of
# the package: Release gives tendies64Targets-release.cmake.
BUILD_TYPE=$(cache_value CMAKE_BUILD_TYPE)
CFG=$(printf '%s' "$BUILD_TYPE" | tr 'A-Z' 'a-z')
[ -n "$CFG" ] || CFG=noconfig

# leak_scan DIR...: no file below DIR mentions the original install path.
leak_scan() {
  for _dir in "$@"; do
    if grep -rlF -e "$PREFIX" -e "prefix with space" "$_dir" >"$WORK/leak.list" 2>/dev/null; then
      echo "check-install: the original install path appears in:" >&2
      cat "$WORK/leak.list" >&2
      fail "relocation: $_dir still names the original install path"
    fi
  done
}

cmd_setup() {
  [ -f "$BUILD/core/libt64_core.a" ] || die "the core archive is not built in $BUILD (build the gate first)"
  rm -rf "$WORK"
  mkdir -p "$WORK"
  if ! "$CMAKE" --install "$BUILD" --component Development --prefix "$PREFIX" \
    >"$WORK/install.log" 2>&1; then
    cat "$WORK/install.log" >&2
    fail "cmake --install of the Development component failed"
  fi
  [ -d "$PREFIX" ] || fail "cmake --install installed nothing (no Development component rules in $BUILD)"
  mv "$PREFIX" "$MOVED"
  [ ! -e "$PREFIX" ] || fail "the original prefix still exists after the move"
  (cd "$MOVED" && find . -type f | sed 's|^\./||' | sort) >"$WORK/installed.list"
  {
    echo include/tendies64/t64_machine.h
    echo lib/cmake/tendies64/tendies64Config.cmake
    echo lib/cmake/tendies64/tendies64ConfigVersion.cmake
    echo "lib/cmake/tendies64/tendies64Targets-$CFG.cmake"
    echo lib/cmake/tendies64/tendies64Targets.cmake
    echo lib/libt64_core.a
  } | sort >"$WORK/expected.list"
  if ! cmp -s "$WORK/expected.list" "$WORK/installed.list"; then
    echo "check-install: installed files differ from the six-file package (expected, installed):" >&2
    diff "$WORK/expected.list" "$WORK/installed.list" >&2 || true
    fail "install tree is not the reviewed package"
  fi
  echo "check-install: setup ok: 6 files in the moved prefix"
}

cmd_build() {
  [ -d "$MOVED" ] || die "no moved prefix: run setup first"
  CC_PATH=$(cache_value CMAKE_C_COMPILER)
  [ -n "$CC_PATH" ] || die "the build tree records no C compiler"
  NINJA=$(cache_value CMAKE_MAKE_PROGRAM)
  rm -rf "$CONSUMER_BUILD"
  set -- -S "$ROOT/tests/installed-consumer" -B "$CONSUMER_BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    "-DCMAKE_PREFIX_PATH=$MOVED" "-DCMAKE_C_COMPILER=$CC_PATH"
  if [ -n "$NINJA" ] && [ -x "$NINJA" ]; then
    set -- "$@" "-DCMAKE_MAKE_PROGRAM=$NINJA"
  fi
  if ! "$CMAKE" "$@" >"$WORK/consumer-configure.log" 2>&1; then
    cat "$WORK/consumer-configure.log" >&2
    fail "consumer configure failed (find_package(tendies64 0.1 REQUIRED CONFIG))"
  fi
  if ! "$CMAKE" --build "$CONSUMER_BUILD" >"$WORK/consumer-build.log" 2>&1; then
    cat "$WORK/consumer-build.log" >&2
    fail "consumer build failed"
  fi
  [ -x "$CONSUMER_BUILD/t64c" ] || fail "the consumer executable was not produced"
  leak_scan "$MOVED" "$CONSUMER_BUILD"
  echo "check-install: build ok: consumer linked against the moved prefix"
}

cmd_run() {
  [ "$#" -ge 1 ] || usage
  [ -x "$CONSUMER_BUILD/t64c" ] || die "no consumer executable: run build first"
  _case=$1
  shift
  if "$CONSUMER_BUILD/t64c" --case "$_case" "$@"; then
    return 0
  fi
  fail "consumer case $_case failed"
}

case $SUB in
  setup) [ "$#" -eq 0 ] || usage; cmd_setup ;;
  build) [ "$#" -eq 0 ] || usage; cmd_build ;;
  run) cmd_run "$@" ;;
  *) usage ;;
esac
