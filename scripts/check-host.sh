#!/bin/sh
# Host build and run driver (Phase 12, plan 12-07, API-05, decision DR-10).
# POSIX sh, no bashisms. Configures and builds the standalone host project of
# host/ against the moved prefix of the install check (the prefix that
# scripts/check-install.sh setup leaves in BUILD/install-check, whose path holds a
# space), then runs one unit case of the pump with the committed title fixtures.
# Registered as the CTest fixture t64_host and the names host.pump.* by
# tools/hostcheck/CMakeLists.txt, after the fixture t64_install.
#
#   sh scripts/check-host.sh [--build DIR] build
#   sh scripts/check-host.sh [--build DIR] run CASE [--rom PATH]
#
#   build        configure host/ (Ninja, Release, the compiler of the build tree)
#                into BUILD/host-check with CMAKE_PREFIX_PATH set to the moved prefix
#                alone, build it, and assert that the build names no path of the
#                source tree outside host/ and no trace of the original install path
#   run CASE     run BUILD/host-check/t64h_tests --case CASE with the title ROM
#                (default build/fixtures/phase12-title-s32k.z64, produced by the
#                gate build; --rom PATH names another), the expected neutral frame
#                digest of fixtures/title/title.expected.json and the events of
#                fixtures/title/title.short.input.json, both read by python3 -I -B
#
# --build DIR   the configured and built tree whose install-check prefix is used
#               (default build/gate); nothing is rebuilt
# Exit codes: 0 pass, 1 a finding, 2 usage error or a missing tool, tree or file.
set -eu
LC_ALL=C
export LC_ALL

ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
BUILD=$ROOT/build/gate

usage() {
  echo "usage: sh scripts/check-host.sh [--build DIR] build|run CASE [--rom PATH]" >&2
  exit 2
}

fail() {
  echo "check-host: FAIL: $*" >&2
  exit 1
}

die() {
  echo "check-host: $*" >&2
  exit 2
}

while [ "$#" -gt 0 ]; do
  case $1 in
    --build)
      [ "$#" -ge 2 ] || usage
      BUILD=$2
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

MOVED="$BUILD/install-check/moved prefix"
HOST_BUILD="$BUILD/host-check"

# cache_value NAME: the value of NAME:TYPE=value in the build tree cache.
cache_value() {
  sed -n "s/^$1:[A-Z]*=//p" "$BUILD/CMakeCache.txt" | head -n 1
}

CMAKE=$(cache_value CMAKE_COMMAND)
[ -n "$CMAKE" ] && [ -x "$CMAKE" ] || CMAKE=cmake
command -v "$CMAKE" >/dev/null 2>&1 || die "cmake not found"

cmd_build() {
  [ -d "$MOVED" ] || die "no moved prefix: run the install setup first (CTest fixture install.setup)"
  _cc=$(cache_value CMAKE_C_COMPILER)
  [ -n "$_cc" ] || die "the build tree records no C compiler"
  _ninja=$(cache_value CMAKE_MAKE_PROGRAM)
  rm -rf "$HOST_BUILD"
  set -- -S "$ROOT/host" -B "$HOST_BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "-DCMAKE_PREFIX_PATH=$MOVED" "-DCMAKE_C_COMPILER=$_cc"
  if [ -n "$_ninja" ] && [ -x "$_ninja" ]; then
    set -- "$@" "-DCMAKE_MAKE_PROGRAM=$_ninja"
  fi
  if ! env -u CMAKE_PREFIX_PATH "$CMAKE" "$@" >"$BUILD/host-check-configure.log" 2>&1; then
    cat "$BUILD/host-check-configure.log" >&2
    fail "host configure failed (find_package(tendies64 CONFIG REQUIRED) against the moved prefix)"
  fi
  if ! "$CMAKE" --build "$HOST_BUILD" >"$BUILD/host-check-build.log" 2>&1; then
    cat "$BUILD/host-check-build.log" >&2
    fail "host build failed"
  fi
  [ -x "$HOST_BUILD/t64h_tests" ] || fail "the host test executable was not produced"
  # The host sees the installed package only: no compile or link command names the
  # core, runner, tools or tests directory of the source tree, and no file of the
  # build tree names the original install path (the prefix was moved).
  if grep -F -e "$ROOT/core/" -e "$ROOT/runner/" -e "$ROOT/tools/" -e "$ROOT/tests/" \
    "$HOST_BUILD/build.ninja" "$HOST_BUILD/compile_commands.json" >"$BUILD/host-check-leak.list" 2>&1; then
    cat "$BUILD/host-check-leak.list" >&2
    fail "the host build names a source-tree path outside host/"
  fi
  if grep -rlF -e "prefix with space" "$HOST_BUILD" >"$BUILD/host-check-leak.list" 2>/dev/null; then
    cat "$BUILD/host-check-leak.list" >&2
    fail "the host build tree still names the original install path"
  fi
  echo "check-host: build ok: host/ linked against the moved prefix"
}

cmd_run() {
  [ "$#" -ge 1 ] || usage
  _case=$1
  shift
  _rom=$ROOT/build/fixtures/phase12-title-s32k.z64
  while [ "$#" -gt 0 ]; do
    case $1 in
      --rom)
        [ "$#" -ge 2 ] || usage
        _rom=$2
        shift 2
        ;;
      *) usage ;;
    esac
  done
  [ -x "$HOST_BUILD/t64h_tests" ] || die "no host test executable: run build first"
  [ -f "$_rom" ] || die "the title ROM is missing: $_rom (build the target t64-phase12-title-fixture)"
  command -v python3 >/dev/null 2>&1 || die "python3 not found"
  # The expected values of the title scenario come from the committed fixtures: the
  # digest of the neutral first frame and the events of the short script. Every token
  # is a hex string, a decimal or a colon list: no space or glob character.
  _extra=$(python3 -I -B -c '
import json, sys
d = sys.argv[1]
exp = json.load(open(d + "/title.expected.json", encoding="utf-8"))
inp = json.load(open(d + "/title.short.input.json", encoding="utf-8"))
out = ["--expect", exp["neutral_frame"]["sha256"]]
for e in inp["events"]:
    out += ["--event", "%d:%d:%d:%d" % (int(e["tick"]), e["buttons"], e["x"], e["y"])]
print(" ".join(out))
' "$ROOT/fixtures/title") || die "cannot read the title fixtures"
  # shellcheck disable=SC2086 # tokens hold no space or glob character
  if "$HOST_BUILD/t64h_tests" --case "$_case" --rom "$_rom" $_extra; then
    return 0
  fi
  fail "host case $_case failed"
}

case $SUB in
  build)
    [ "$#" -eq 0 ] || usage
    cmd_build
    ;;
  run) cmd_run "$@" ;;
  *) usage ;;
esac
