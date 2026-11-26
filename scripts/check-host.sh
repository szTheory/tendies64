#!/bin/sh
# Host build and run driver (Phase 12, plan 12-07, API-05, decision DR-10).
# POSIX sh, no bashisms. Configures and builds the standalone host project of
# host/ against the moved prefix of the install check (the prefix that
# scripts/check-install.sh setup leaves in BUILD/install-check, whose path holds a
# space), then runs one unit case of the pump with the committed title fixtures.
# Registered as the CTest fixture t64_host and the names host.pump.* by
# tools/hostcheck/CMakeLists.txt, after the fixture t64_install.
#
#   sh scripts/check-host.sh [--build DIR] [--host-build DIR] build
#   sh scripts/check-host.sh [--build DIR] [--host-build DIR] run CASE [--rom PATH]
#
#   build        configure host/ (Ninja, Release, the compiler of the build tree)
#                into BUILD/host-check with CMAKE_PREFIX_PATH set to the moved prefix
#                alone, build it, and assert that the build names no path of the
#                source tree outside host/ and no trace of the original install path
#   run CASE     run BUILD/host-check/t64h_tests --case CASE with the title ROM
#                (default build/fixtures/phase12-title-s32k.z64, produced by the
#                gate build; --rom PATH names another), the expected neutral frame
#                digest of fixtures/title/title.expected.json and the events of
#                fixtures/title/title.short.input.json, both read by python3 -I -B.
#                Cases that need no guest (input-map, frames) take no ROM; the
#                case script is given the two committed t64.input scripts, each
#                with the events python3 reads from the same file; the case
#                title-two-speeds runs both scripts at 240 and 3 fake-clock Hz and
#                pipes the JSON lines into scripts/check-host-title.py, which judges
#                them against title.expected.json and title.runner-pins.json (its
#                own seeded selftest runs first)
#
#   sh scripts/check-host.sh [--build DIR] [--host-build DIR] sdl [--rom PATH]
#
#   sdl          the LOCAL lane of the optional SDL3 shell (plan 12-09, decision DR-17), never a
#                CTest name, a workflow step or part of scripts/gate.sh: it installs nothing. When
#                no system SDL3 of version 3.2.0 or newer is found (a scratch find_package probe)
#                it prints the recorded skip line "[host-sdl] SKIP: SDL3 >= 3.2.0 not found ..." and
#                exits 0: a skip is NOT a pass, it leaves the SC2 virtual-joystick clause not
#                closed. Otherwise it configures host/ with -DT64H_SDL=ON into build/host-sdl (never
#                under the gate tree; --host-build DIR names another directory, which must be
#                empty or a CMake tree), builds the shell and its smoke, exports
#                SDL_VIDEO_DRIVER=dummy and SDL_AUDIO_DRIVER=dummy, runs t64h_sdl_smoke (a virtual
#                gamepad through the mapping table, the title through the pump into a window and an
#                audio stream) and prints only its sanitized "[host-sdl] ..." lines, ending in
#                "[host-sdl] PASS". The raw logs stay under build/host-sdl and are never committed
#                (SDL can print local paths). A found SDL3 with a failing smoke is a failure
#                (exit 1), never a skip. The pass is self-checking host glue only: no rendering,
#                timing or audio claim. To play the title in a real window (a display is needed):
#                build/host-sdl/t64h_sdl --rom build/fixtures/phase12-title-s32k.z64 [--scale N]
#
# --build DIR   the configured and built tree whose install-check prefix is used
#               (default build/gate); nothing is rebuilt
# --host-build DIR  where the host project is built and run (default BUILD/host-check, or
#               build/host-sdl for the sdl lane); scripts/p12-mutants.py builds a mutated copy of
#               host/ beside the gate tree
# Exit codes: 0 pass (or the recorded sdl skip), 1 a finding, 2 usage error or a missing tool,
# tree or file.
set -eu
LC_ALL=C
export LC_ALL

ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
BUILD=$ROOT/build/gate
HOST_BUILD_ARG=

usage() {
  echo "usage: sh scripts/check-host.sh [--build DIR] [--host-build DIR] build|run CASE [--rom PATH]|sdl [--rom PATH]" >&2
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
    --host-build)
      [ "$#" -ge 2 ] || usage
      HOST_BUILD_ARG=$2
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
if [ -n "$HOST_BUILD_ARG" ]; then
  mkdir -p "$HOST_BUILD_ARG" || die "cannot create the host build directory"
  HOST_BUILD=$(cd "$HOST_BUILD_ARG" && pwd -P)
fi

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
  command -v python3 >/dev/null 2>&1 || die "python3 not found"
  case $_case in
    input-map | frames)
      # Pure cases: no guest, no ROM, no fixture value.
      cd "$ROOT"
      if "$HOST_BUILD/t64h_tests" --case "$_case"; then
        return 0
      fi
      fail "host case $_case failed"
      ;;
  esac
  [ -f "$_rom" ] || die "the title ROM is missing: $_rom (build the target t64-phase12-title-fixture)"
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
  case $_case in
    script)
      # Each committed script with the events python3 reads from the same file (the C
      # reader must find exactly these); paths are relative to the repository root. The
      # title ROM serves the one real-core sub-case (nothing is queued after completion).
      _scripts=$(python3 -I -B -c '
import json, sys
d = sys.argv[1]
out = []
for name in ("short", "long"):
    path = "fixtures/title/title.%s.input.json" % name
    inp = json.load(open(d + "/title.%s.input.json" % name, encoding="utf-8"))
    out += ["--script", path]
    for e in inp["events"]:
        out += ["--event", "%d:%d:%d:%d" % (int(e["tick"]), e["buttons"], e["x"], e["y"])]
print(" ".join(out))
' "$ROOT/fixtures/title") || die "cannot read the title input scripts"
      cd "$ROOT"
      # shellcheck disable=SC2086 # tokens hold no space or glob character
      if "$HOST_BUILD/t64h_tests" --case script --rom "$_rom" $_scripts; then
        return 0
      fi
      fail "host case script failed"
      ;;
    title-two-speeds)
      # The committed short and long scripts at 240 and 3 Hz of fake time; the binary prints one JSON
      # line per run and the comparator judges the lines (oracle and recorded pins).
      _runs="$BUILD/host-title-runs.jsonl"
      python3 -I -B "$ROOT/scripts/check-host-title.py" --selftest || fail "the comparator selftest failed"
      cd "$ROOT"
      if ! "$HOST_BUILD/t64h_tests" --case title-two-speeds --rom "$_rom" \
        --script fixtures/title/title.short.input.json \
        --script fixtures/title/title.long.input.json --slice-hz 240,3 >"$_runs"; then
        cat "$_runs"
        fail "host case title-two-speeds failed"
      fi
      cat "$_runs"
      if ! python3 -I -B "$ROOT/scripts/check-host-title.py" --runs "$_runs"; then
        fail "the pump runs of the title differ from the oracle or the recorded pins"
      fi
      return 0
      ;;
  esac
  # shellcheck disable=SC2086 # tokens hold no space or glob character
  if "$HOST_BUILD/t64h_tests" --case "$_case" --rom "$_rom" $_extra; then
    return 0
  fi
  fail "host case $_case failed"
}

# sdl_probe: 0 when a system SDL3 of version 3.2.0 or newer is found by CMake (the usual search:
# system prefixes, SDL3_DIR, CMAKE_PREFIX_PATH), without installing or building anything.
sdl_probe() {
  _probe=$(mktemp -d "${TMPDIR:-/tmp}/t64h-sdl-probe.XXXXXX") || die "cannot create a scratch directory"
  printf '%s\n' 'cmake_minimum_required(VERSION 3.25)' 'project(t64h_sdl_probe LANGUAGES NONE)' \
    'find_package(SDL3 3.2.0 CONFIG QUIET)' 'if(SDL3_FOUND)' \
    '  message(STATUS "t64h-sdl-probe found")' 'endif()' > "$_probe/CMakeLists.txt"
  _found=1
  if "$CMAKE" -S "$_probe" -B "$_probe/b" > "$_probe/out.log" 2>&1 && grep -q 't64h-sdl-probe found' "$_probe/out.log"; then
    _found=0
  fi
  rm -rf "$_probe"
  return "$_found"
}

cmd_sdl() {
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
  if ! sdl_probe; then
    echo "[host-sdl] SKIP: SDL3 >= 3.2.0 not found (link-only system package; this lane installs nothing)"
    return 0
  fi
  [ -d "$MOVED" ] || die "no moved prefix: run the install setup first (CTest fixture install.setup)"
  [ -f "$_rom" ] || die "the title ROM is missing (build the target t64-phase12-title-fixture)"
  _cc=$(cache_value CMAKE_C_COMPILER)
  [ -n "$_cc" ] || die "the build tree records no C compiler"
  _ninja=$(cache_value CMAKE_MAKE_PROGRAM)
  _sdl_build=$ROOT/build/host-sdl
  if [ -n "$HOST_BUILD_ARG" ]; then
    _sdl_build=$HOST_BUILD
  fi
  if [ -e "$_sdl_build" ]; then
    if [ ! -f "$_sdl_build/CMakeCache.txt" ] && [ -n "$(ls -A "$_sdl_build")" ]; then
      die "the SDL lane directory is neither empty nor a CMake tree"
    fi
    rm -rf "$_sdl_build"
  fi
  mkdir -p "$_sdl_build" || die "cannot create the SDL lane directory"
  set -- -S "$ROOT/host" -B "$_sdl_build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DT64H_SDL=ON \
    "-DCMAKE_PREFIX_PATH=$MOVED" "-DCMAKE_C_COMPILER=$_cc"
  if [ -n "$_ninja" ] && [ -x "$_ninja" ]; then
    set -- "$@" "-DCMAKE_MAKE_PROGRAM=$_ninja"
  fi
  # The raw logs of this lane can hold local paths: they stay under the ignored build directory
  # and are never printed, only named.
  if ! "$CMAKE" "$@" > "$_sdl_build/lane-configure.log" 2>&1; then
    echo "[host-sdl] FAIL: configure (raw log: build/host-sdl/lane-configure.log)"
    return 1
  fi
  if ! "$CMAKE" --build "$_sdl_build" > "$_sdl_build/lane-build.log" 2>&1; then
    echo "[host-sdl] FAIL: build (raw log: build/host-sdl/lane-build.log)"
    return 1
  fi
  [ -x "$_sdl_build/t64h_sdl_smoke" ] && [ -x "$_sdl_build/t64h_sdl" ] || {
    echo "[host-sdl] FAIL: the shell or its smoke was not produced"
    return 1
  }
  _rc=0
  SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy "$_sdl_build/t64h_sdl_smoke" --rom "$_rom" \
    > "$_sdl_build/lane.out" 2> "$_sdl_build/lane.err" || _rc=$?
  # Only the smoke's fixed lines are shown; anything else (and the whole of stderr) stays in the
  # raw logs.
  grep -E '^\[host-sdl\] [A-Za-z0-9 ./:_-]{1,60}$' "$_sdl_build/lane.out" || true
  if [ "$_rc" -ne 0 ]; then
    if ! grep -q '^\[host-sdl\] FAIL: ' "$_sdl_build/lane.out"; then
      echo "[host-sdl] FAIL: the smoke ended with exit $_rc (raw log: build/host-sdl/lane.err)"
    fi
    return 1
  fi
  if ! grep -qx '\[host-sdl\] PASS' "$_sdl_build/lane.out"; then
    echo "[host-sdl] FAIL: the smoke printed no PASS line"
    return 1
  fi
  return 0
}

case $SUB in
  sdl) cmd_sdl "$@" ;;
  build)
    [ "$#" -eq 0 ] || usage
    cmd_build
    ;;
  run) cmd_run "$@" ;;
  *) usage ;;
esac
