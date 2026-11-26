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
#   three-way [--rom PATH] [--supervisor PATH]
#                  run the in-tree runner (the two supervised runs of the authored
#                  composite ROM that composite.expected.json was proved against:
#                  a raw-capture run and a save-out run) and the installed
#                  consumer's composite case (which itself checks the independent
#                  literals), then compare every value both sides report and print
#                  one line per value naming its source class (plan 11-18, G6)
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
  echo "usage: sh scripts/check-install.sh [--build DIR] [--work DIR] setup|build|run CASE [ARGS]|three-way [--rom PATH] [--supervisor PATH]" >&2
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
  # The case list is closed: the binary lists exactly the cases of cases.txt, so
  # a case cannot exist unregistered and a registered case cannot be missing.
  grep '^[a-z]' "$ROOT/tests/installed-consumer/cases.txt" | LC_ALL=C sort >"$WORK/cases.expected"
  "$CONSUMER_BUILD/t64c" --list-cases | LC_ALL=C sort >"$WORK/cases.listed"
  if ! cmp -s "$WORK/cases.expected" "$WORK/cases.listed"; then
    echo "check-install: cases.txt and the consumer's --list-cases differ (cases.txt, binary):" >&2
    diff "$WORK/cases.expected" "$WORK/cases.listed" >&2 || true
    fail "the consumer case list is not closed"
  fi
  echo "check-install: build ok: consumer linked against the moved prefix ($(wc -l <"$WORK/cases.listed" | tr -d ' ') cases)"
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

# sha256_of FILE: the lower-case SHA-256 of a file (sha256sum, else shasum).
sha256_of() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | cut -d ' ' -f 1
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$1" | cut -d ' ' -f 1
  else
    die "no sha256sum or shasum"
  fi
}

# json_fields RESULT_JSON: "outcome", "guest_tick", "state_digest" and the
# guest_stop kind of a supervisor result record, one "key value" line each.
json_fields() {
  python3 -I -B -c '
import json, sys
d = json.load(open(sys.argv[1], encoding="utf-8"))
print("outcome", d["outcome"])
print("detail", d["detail"])
print("guest_tick", d["guest_tick"])
print("state_digest", d["state_digest"])
print("stop_kind", d["guest_stop"]["kind"])
' "$1"
}

# field KEY LINES: the value of KEY in "key value" lines.
field() {
  printf '%s\n' "$2" | sed -n "s/^$1 //p" | head -n 1
}

# same SOURCE_CLASS LABEL RUNNER CONSUMER: print one comparison line and fail on a difference.
same() {
  if [ "$3" = "$4" ]; then
    echo "three-way: $2: runner=$3 consumer=$4 equal ($1)"
  else
    echo "three-way: $2: runner=$3 consumer=$4 DIFFERENT ($1)" >&2
    fail "three-way: $2 differs between the in-tree runner and the installed consumer"
  fi
}

# same_file SOURCE_CLASS LABEL RUNNER_FILE CONSUMER_FILE: byte-for-byte comparison.
same_file() {
  [ -f "$3" ] || fail "three-way: the runner produced no $2 ($3)"
  [ -f "$4" ] || fail "three-way: the consumer produced no $2 ($4)"
  if cmp -s "$3" "$4"; then
    echo "three-way: $2: runner and consumer bytes equal, $(wc -c <"$3" | tr -d ' ') bytes ($1)"
  else
    echo "three-way: $2: runner and consumer bytes DIFFER ($1)" >&2
    fail "three-way: $2 differs between the in-tree runner and the installed consumer"
  fi
}

cmd_three_way() {
  _rom="$ROOT/build/fixtures/phase10-composite-s32k.z64"
  _supervisor="$BUILD/runner/t64-supervisor"
  while [ "$#" -gt 0 ]; do
    case $1 in
      --rom) [ "$#" -ge 2 ] || usage; _rom=$2; shift 2 ;;
      --supervisor) [ "$#" -ge 2 ] || usage; _supervisor=$2; shift 2 ;;
      *) usage ;;
    esac
  done
  [ -x "$CONSUMER_BUILD/t64c" ] || die "no consumer executable: run build first"
  [ -f "$_rom" ] || die "the authored composite ROM is not built: $_rom"
  [ -x "$_supervisor" ] || die "the in-tree supervisor is not built: $_supervisor"
  _input="$ROOT/fixtures/composite/composite.input.json"
  [ -f "$_input" ] || die "missing $_input"
  _size=$(wc -c <"$_input" | tr -d ' ')
  _sha=$(sha256_of "$_input")
  _run="$WORK/three-way"
  rm -rf "$_run"
  mkdir -p "$_run"
  for _d in raw-work raw-aux save-work save-aux consumer-out; do
    mkdir -m 700 "$_run/$_d"
  done

  # The in-tree runner: the same two supervised runs as runner.composite-two-run.
  if ! "$_supervisor" --real --result "$_run/raw-work/result.json" --meta "$_run/raw-work/meta.json" \
    --work-dir "$_run/raw-work" --wall-ms 30000 --bound 600000 --fixture "$_rom" \
    --input "$_input" --input-size "$_size" --input-sha256 "$_sha" \
    --raw-capture-dir "$_run/raw-aux/raw" >"$_run/raw-work/supervisor.out" 2>"$_run/raw-work/supervisor.err"; then
    cat "$_run/raw-work/supervisor.err" >&2
    fail "three-way: the in-tree raw-capture run failed"
  fi
  if ! "$_supervisor" --real --result "$_run/save-work/result.json" --meta "$_run/save-work/meta.json" \
    --work-dir "$_run/save-work" --wall-ms 30000 --bound 600000 --fixture "$_rom" \
    --input "$_input" --input-size "$_size" --input-sha256 "$_sha" \
    --capture-out "$_run/save-work/capture.bin" --artifact-dir "$_run/save-aux" \
    --artifact-max-bytes 32768 --save-kind sram32k --save-format raw-v1 \
    --save-export-leaf composite.sra >"$_run/save-work/supervisor.out" 2>"$_run/save-work/supervisor.err"; then
    cat "$_run/save-work/supervisor.err" >&2
    fail "three-way: the in-tree save-out run failed"
  fi
  _raw=$(json_fields "$_run/raw-work/result.json")
  _save=$(json_fields "$_run/save-work/result.json")

  # The installed consumer, stepping like the runner (16 ticks per advance). Its
  # exit status carries the independent-literal leg: it fails on any literal.
  if ! "$CONSUMER_BUILD/t64c" --case composite --rom "$_rom" --slice 16 --print \
    --out "$_run/consumer-out" >"$_run/consumer.txt" 2>"$_run/consumer.err"; then
    cat "$_run/consumer.txt" "$_run/consumer.err" >&2
    fail "three-way: the installed consumer's composite case failed its independent literals"
  fi
  echo "three-way: the installed consumer matched every independent literal ($(grep -c '^independent literal:' "$_run/consumer.txt") comparisons)"
  _c() { sed -n "s/^print $1 //p" "$_run/consumer.txt" | head -n 1; }

  _pin="runner-recorded pin, not independent"
  _lit="independent literal checked by the consumer, runner equals consumer"
  [ "$(field outcome "$_raw")" = pass ] && [ "$(field outcome "$_save")" = pass ] ||
    fail "three-way: a supervised run of the in-tree runner did not pass"
  same "$_pin" "final state digest (raw-capture run vs save-out run)" "$(field state_digest "$_raw")" "$(field state_digest "$_save")"
  same "$_pin" "final state digest" "$(field state_digest "$_raw")" "$(_c final_digest)"
  same "$_pin" "completion tick" "$(field guest_tick "$_raw")" "$(_c completion_tick)"
  # The runner's qualified completion of the authored ROM is the library's
  # GUEST_COMPLETION stop; the mapping is the only translation in this script.
  [ "$(field stop_kind "$_raw")" = authored-completion ] || fail "three-way: the runner did not stop with its authored completion"
  same "$_pin" "stop reason (runner authored-completion = GUEST_COMPLETION)" GUEST_COMPLETION "$(_c stop_reason)"
  _video=$(ls "$_run/raw-aux/raw/video" | wc -l | tr -d ' ')
  same "$_pin" "video packet count" "$_video" "$(_c video_packet_count)"
  _i=0
  while [ "$_i" -lt "$_video" ]; do
    _n=$(printf '%06d' "$_i")
    same_file "$_lit" "video packet $_i" "$_run/raw-aux/raw/video/frame-$_n.rgba8" "$_run/consumer-out/video-$_i"
    _i=$((_i + 1))
  done
  _audio=$(ls "$_run/raw-aux/raw/audio" | wc -l | tr -d ' ')
  same "$_lit" "audio packet count" "$_audio" "$(_c audio_packet_count)"
  _i=0
  while [ "$_i" -lt "$_audio" ]; do
    _n=$(printf '%06d' "$_i")
    same_file "$_lit" "audio packet $_i" "$_run/raw-aux/raw/audio/packet-$_n.s16be" "$_run/consumer-out/audio-$_i"
    _i=$((_i + 1))
  done
  same_file "$_lit" "SRAM save" "$_run/save-aux/composite.sra" "$_run/consumer-out/save"
  same_file "$_lit" "ISViewer transcript" "$_run/save-work/capture.bin" "$_run/consumer-out/transcript"
  echo "three-way: ok: installed consumer, in-tree runner and independent literals agree"
}

case $SUB in
  setup) [ "$#" -eq 0 ] || usage; cmd_setup ;;
  build) [ "$#" -eq 0 ] || usage; cmd_build ;;
  run) cmd_run "$@" ;;
  three-way) cmd_three_way "$@" ;;
  *) usage ;;
esac
