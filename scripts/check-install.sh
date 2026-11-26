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
#   control ID [--rom PATH]
#                  perform one negative control of tests/installed-consumer/controls.tsv
#                  in its own directory below WORK/controls (plan 11-19, VER-05): the row
#                  names the action (a failing configure, a source-tree scan, a doctored
#                  copy of the installed header, a relocated copy of the prefix, the
#                  symbol-collision consumer, a plain cc line) and the exact text or create
#                  status that must be observed; exit 0 only when it is observed, with the
#                  expected and the observed value in the log, exit 1 otherwise
#   sanitize shipped|instrumented [--steps N] [--digest-stride N] [--rom PATH]
#                  LOCAL lane, never in the gate (plan 11-20, API-02, T-11-43): build the
#                  consumer with -fsanitize=address,undefined -fno-sanitize-recover=all
#                  against a fresh install and run every case of cases.txt, the call-
#                  sequence generator with --steps N (default 20000). `shipped` installs
#                  the Release archive of BUILD (the library as shipped, an instrumented
#                  consumer); `instrumented` configures and builds the asan preset
#                  (build/asan), installs its Development component and runs the same
#                  consumer (library and consumer instrumented). The scratch trees are
#                  BUILD/sanitize-lane/MODE (not WORK, which the CTest fixture
#                  install.setup clears); a sanitizer report or a failing case is exit 1
#   tsan [--seed-race] [--rom PATH]
#                  LOCAL lane (plan 11-20, T-11-44): build t64_core with -fsanitize=thread
#                  into a scratch tree below BUILD/sanitize-lane/tsan, install it, build the consumer with
#                  -DT64C_THREADS=ON and -fsanitize=thread and run the threads case (two
#                  threads, one instance each); a ThreadSanitizer report is exit 1.
#                  --seed-race builds the case with both threads on ONE instance (the
#                  use the one-instance rule forbids) and passes only when
#                  ThreadSanitizer reports it: the lane is shown able to fail
#   fresh-archive [--rom PATH]
#                  LOCAL or manual lane, never in the gate or the hosted workflow (plan 11-24,
#                  VER-05, T-11-53): `git archive HEAD` into a mktemp directory outside the
#                  repository, configure that tree as the gate preset does (Ninja, Release,
#                  T64_WERROR=ON, developer warnings as errors), build only t64_core,
#                  cmake --install the Development component into BUILD/fresh-archive/
#                  "prefix with space", assert the six-file package and that no byte of it names
#                  the exported tree, its build tree, the repository or a per-user path, then
#                  build the independent consumer against that prefix and run the smoke and
#                  composite cases. Prints the installed archive's build id; the scratch
#                  source and build tree are removed at exit. The qualifier of plan 11-24
#                  runs it and reads the archive's identity from the kept prefix
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
  echo "usage: sh scripts/check-install.sh [--build DIR] [--work DIR] setup|build|run CASE [ARGS]|control ID [--rom PATH]|sanitize shipped|instrumented [--steps N] [--digest-stride N] [--rom PATH]|tsan [--seed-race] [--rom PATH]|fresh-archive [--rom PATH]|three-way [--rom PATH] [--supervisor PATH]" >&2
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

# consumer_configure DIR PREFIX LOG [CMAKE_ARGS...]: configure tests/installed-consumer
# into DIR (Ninja, Release, the compiler of the build tree, a compile database) with
# CMAKE_PREFIX_PATH=PREFIX and nothing else but the extra arguments; the output goes to
# LOG and the status is cmake's. The environment's CMAKE_PREFIX_PATH is unset, so only
# PREFIX can supply the package.
consumer_configure() {
  _cdir=$1
  _cprefix=$2
  _clog=$3
  shift 3
  CC_PATH=$(cache_value CMAKE_C_COMPILER)
  [ -n "$CC_PATH" ] || die "the build tree records no C compiler"
  NINJA=$(cache_value CMAKE_MAKE_PROGRAM)
  rm -rf "$_cdir"
  set -- -S "$ROOT/tests/installed-consumer" -B "$_cdir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    "-DCMAKE_PREFIX_PATH=$_cprefix" "-DCMAKE_C_COMPILER=$CC_PATH" "$@"
  if [ -n "$NINJA" ] && [ -x "$NINJA" ]; then
    set -- "$@" "-DCMAKE_MAKE_PROGRAM=$NINJA"
  fi
  env -u CMAKE_PREFIX_PATH "$CMAKE" "$@" >"$_clog" 2>&1
}

cmd_build() {
  [ -d "$MOVED" ] || die "no moved prefix: run setup first"
  if ! consumer_configure "$CONSUMER_BUILD" "$MOVED" "$WORK/consumer-configure.log"; then
    cat "$WORK/consumer-configure.log" >&2
    fail "consumer configure failed (find_package(tendies64 1.0 REQUIRED CONFIG))"
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

# ---- negative controls (plan 11-19, VER-05) ------------------------------------
# One row of tests/installed-consumer/controls.tsv per control: id, action, argument,
# kind (text or status), expected, detail. The action runs in WORK/controls/ID, appends
# everything it observes to the log, and the judge compares the log (text) or the create
# status the smoke case printed (status) with the expected value. A control that passes
# removes its directory, a failing one keeps it.

CTL_TSV=$ROOT/tests/installed-consumer/controls.tsv

ctl_say() {
  echo "control $CTL_ID: $*"
}

# ctl_repo_paths DIR: the lines of DIR/compile_commands.json and DIR/build.ninja that
# still name the repository root after the consumer's own source directory and the work
# directory (the build trees and prefixes of this driver) are removed from every line.
ctl_repo_paths() {
  [ -f "$1/compile_commands.json" ] || fail "control $CTL_ID: no compile_commands.json in the consumer tree"
  [ -f "$1/build.ninja" ] || fail "control $CTL_ID: no build.ninja in the consumer tree"
  cat "$1/compile_commands.json" "$1/build.ninja" |
    awk -v root="$ROOT" -v a1="$ROOT/tests/installed-consumer" -v a2="$WORK" '
      function strip(line, lit,   out, i) {
        out = ""
        while ((i = index(line, lit)) > 0) {
          out = out substr(line, 1, i - 1)
          line = substr(line, i + length(lit))
        }
        return out line
      }
      { rest = strip(strip($0, a1), a2); if (index(rest, root) > 0) print rest }'
}

# ctl_smoke DIR: run t64c_smoke (the smoke case alone) of the consumer tree DIR, output
# to CTL_DIR/smoke.txt and the log; CTL_SMOKE_RC is its exit status.
ctl_smoke() {
  if "$1/t64c_smoke" >"$CTL_DIR/smoke.txt" 2>&1; then
    CTL_SMOKE_RC=0
  else
    CTL_SMOKE_RC=$?
  fi
  cat "$CTL_DIR/smoke.txt" >>"$CTL_LOG"
}

# ctl_build_smoke PREFIX: configure the consumer against PREFIX in CTL_BUILD, build its
# t64c_smoke target (the smoke case alone, so a header that no longer fits the other
# cases still builds) and run it.
ctl_build_smoke() {
  if ! consumer_configure "$CTL_BUILD" "$1" "$CTL_DIR/configure.log"; then
    cat "$CTL_DIR/configure.log" >&2
    fail "control $CTL_ID: the consumer does not configure against the copied prefix"
  fi
  if ! "$CMAKE" --build "$CTL_BUILD" --target t64c_smoke >"$CTL_DIR/build.log" 2>&1; then
    cat "$CTL_DIR/build.log" >&2
    fail "control $CTL_ID: the consumer does not build against the copied prefix"
  fi
  [ -x "$CTL_BUILD/t64c_smoke" ] || fail "control $CTL_ID: the smoke executable was not produced"
  ctl_smoke "$CTL_BUILD"
}

ctl_configure_missing() {
  if consumer_configure "$CTL_BUILD" "" "$CTL_LOG"; then
    fail "control $CTL_ID: the configure succeeded with an empty CMAKE_PREFIX_PATH"
  fi
}

ctl_configure_version() {
  if consumer_configure "$CTL_BUILD" "$MOVED" "$CTL_LOG" "-DT64C_REQUIRED_VERSION=$CTL_ARG"; then
    fail "control $CTL_ID: the configure succeeded although version $CTL_ARG is not compatible with the package"
  fi
}

ctl_source_tree() {
  _seed=
  [ "$CTL_ARG" != seed ] || _seed="-DCMAKE_C_FLAGS=-I$ROOT/core/include"
  if ! consumer_configure "$CTL_BUILD" "$MOVED" "$CTL_DIR/configure.log" ${_seed:+"$_seed"}; then
    cat "$CTL_DIR/configure.log" >&2
    fail "control $CTL_ID: the consumer does not configure against the moved prefix"
  fi
  ctl_repo_paths "$CTL_BUILD" >"$CTL_DIR/findings.txt"
  _n=$(wc -l <"$CTL_DIR/findings.txt" | tr -d ' ')
  if [ "$CTL_ARG" = seed ]; then
    if [ "$_n" -ge 1 ] && grep -qF "$ROOT/core/include" "$CTL_DIR/findings.txt"; then
      echo "seed detected: $_n line(s) of compile_commands.json and build.ninja name the source include directory" >>"$CTL_LOG"
    else
      echo "seed NOT detected: the scan found $_n line(s) and none names the source include directory" >>"$CTL_LOG"
    fi
  else
    if [ "$_n" -eq 0 ]; then
      echo "no repository path: compile_commands.json and build.ninja name nothing of the repository outside the consumer's own sources and build tree" >>"$CTL_LOG"
    else
      echo "$_n line(s) still name the repository root:" >>"$CTL_LOG"
      cat "$CTL_DIR/findings.txt" >>"$CTL_LOG"
    fi
  fi
}

# ctl_doctor_header ARG IN OUT: write IN to OUT with the doctoring named by ARG.
ctl_doctor_header() {
  case $1 in
    abi)
      awk '
        /^#define T64_ABI_VERSION_MINOR UINT32_C\([0-9]+\)$/ {
          n = $3
          sub(/^UINT32_C\(/, "", n)
          sub(/\)$/, "", n)
          print "#define T64_ABI_VERSION_MINOR UINT32_C(" (n + 1) ")"
          found = 1
          next
        }
        { print }
        END { if (!found) exit 3 }' "$2" >"$3"
      ;;
    size)
      awk '
        { line[NR] = $0 }
        END {
          for (i = 1; i <= NR; i++) {
            if (line[i] == "} t64_machine_create_info;") drop = i - 1
            if (line[i] ~ /^ +\(allocator_ptr\), \(profile_ptr\)\}$/) fix = i
          }
          if (!drop || line[drop] !~ /const t64_profile_descriptor \*profile;/ || !fix) exit 3
          for (i = 1; i <= NR; i++) {
            if (i == drop) continue
            if (i == fix) sub(/\(allocator_ptr\), \(profile_ptr\)\}/, "(allocator_ptr)}", line[i])
            print line[i]
          }
        }' "$2" >"$3"
      ;;
    *) die "unknown doctoring: $1" ;;
  esac
}

# ctl_abi_minor FILE: the number inside T64_ABI_VERSION_MINOR UINT32_C(N) of a header.
ctl_abi_minor() {
  sed -n 's/^#define T64_ABI_VERSION_MINOR UINT32_C(\([0-9][0-9]*\))$/\1/p' "$1"
}

ctl_doctored() {
  _prefix="$CTL_DIR/doctored prefix"
  cp -R "$MOVED" "$_prefix"
  chmod -R u+w "$_prefix"
  _hdr="$_prefix/include/tendies64/t64_machine.h"
  [ -f "$_hdr" ] || fail "control $CTL_ID: the copied prefix has no installed header"
  cp "$_hdr" "$CTL_DIR/header.orig"
  if ! ctl_doctor_header "$CTL_ARG" "$CTL_DIR/header.orig" "$_hdr"; then
    fail "control $CTL_ID: the text this doctoring edits is not in the installed header any more"
  fi
  if cmp -s "$CTL_DIR/header.orig" "$_hdr"; then
    fail "control $CTL_ID: the doctoring changed nothing"
  fi
  if [ "$CTL_ARG" = abi ]; then
    _was=$(ctl_abi_minor "$CTL_DIR/header.orig")
    _now=$(ctl_abi_minor "$_hdr")
    [ -n "$_was" ] && [ -n "$_now" ] || fail "control $CTL_ID: cannot read the ABI minor of the header and of its doctored copy"
    if [ "$_now" -ne $((_was + 1)) ]; then
      fail "control $CTL_ID: the doctored ABI minor $_now is not the original minor $_was plus one"
    fi
    echo "doctored ABI minor: $_was -> $_now (exactly the original plus one)" >>"$CTL_LOG"
  fi
  echo "doctored copy of the installed header (diff of the copy, never the source header):" >>"$CTL_LOG"
  diff "$CTL_DIR/header.orig" "$_hdr" >>"$CTL_LOG" || true
  ctl_build_smoke "$_prefix"
  if [ "$CTL_SMOKE_RC" -eq 0 ]; then
    fail "control $CTL_ID: the smoke case passed against a doctored header"
  fi
  CTL_OBSERVED_STATUS=$(sed -n 's/^smoke: create status=\([0-9][0-9]*\) machine=NULL$/\1/p' "$CTL_DIR/smoke.txt" | head -n 1)
}

ctl_relocate() {
  cp -R "$MOVED" "$CTL_DIR/first prefix"
  mv "$CTL_DIR/first prefix" "$CTL_DIR/another moved prefix"
  _prefix="$CTL_DIR/another moved prefix"
  [ ! -e "$CTL_DIR/first prefix" ] || fail "control $CTL_ID: the first prefix still exists after the move"
  ctl_build_smoke "$_prefix"
  if [ "$CTL_SMOKE_RC" -ne 0 ]; then
    fail "control $CTL_ID: the smoke case failed against the relocated prefix"
  fi
  if grep -rlF -e "$CTL_DIR/first prefix" -e "first prefix" -e "$MOVED" -e "$PREFIX" \
    "$_prefix" "$CTL_BUILD" >"$CTL_DIR/leak.list" 2>/dev/null; then
    cat "$CTL_DIR/leak.list" >&2
    fail "control $CTL_ID: the relocated prefix or its build tree names an earlier prefix"
  fi
  echo "relocation: the prefix was copied, moved to another path with spaces, and neither it nor the new build tree names an earlier prefix" >>"$CTL_LOG"
}

# archive_names ARCHIVE: the external names of an archive (defined and undefined),
# one per line, the leading underscore of Darwin symbols stripped.
archive_names() {
  nm -P -g "$1" 2>/dev/null | awk 'NF >= 2 && $1 !~ /:$/ { n = $1; sub(/^_/, "", n); print n }' | LC_ALL=C sort -u
}

# ctl_collision: a consumer that defines its own f32_add and t64_sha256_init must link
# against the installed archive, the composite case must give the unchanged digest, and
# the core must never call the host's definitions. The unprefixed archive fails this.
ctl_collision() {
  [ -f "$CTL_ROM" ] || die "the authored composite ROM is not built: $CTL_ROM"
  if ! consumer_configure "$CTL_BUILD" "$MOVED" "$CTL_DIR/configure.log" -DT64C_COLLISION=ON; then
    cat "$CTL_DIR/configure.log" >&2
    fail "control $CTL_ID: the collision consumer does not configure"
  fi
  if ! "$CMAKE" --build "$CTL_BUILD" --target t64c_collision >"$CTL_DIR/build.log" 2>&1; then
    cat "$CTL_DIR/build.log" >&2
    if grep -qiE 'duplicate symbol|multiple definition' "$CTL_DIR/build.log"; then
      fail "control $CTL_ID: the link stopped on a duplicate symbol: the archive defines a name the host also defines"
    fi
    fail "control $CTL_ID: the consumer that defines its own f32_add and t64_sha256_init does not link"
  fi
  echo "link: the consumer with its own f32_add and t64_sha256_init linked against the archive" >>"$CTL_LOG"
  archive_names "$MOVED/lib/libt64_core.a" >"$CTL_DIR/archive.names"
  for _name in t64i_f32_add t64i_sha256_init; do
    grep -qx "$_name" "$CTL_DIR/archive.names" || fail "control $CTL_ID: the archive has no $_name (the check of its names is empty or the rename is gone)"
  done
  for _name in f32_add t64_sha256_init; do
    if grep -qx "$_name" "$CTL_DIR/archive.names"; then
      fail "control $CTL_ID: the archive defines or references the unprefixed name $_name, which a host definition would override"
    fi
  done
  echo "archive: it has t64i_f32_add and t64i_sha256_init and defines or references neither f32_add nor t64_sha256_init" >>"$CTL_LOG"
  if "$CTL_BUILD/t64c_collision" --case composite --rom "$CTL_ROM" >"$CTL_DIR/run.txt" 2>&1; then
    CTL_RUN_RC=0
  else
    CTL_RUN_RC=$?
  fi
  cat "$CTL_DIR/run.txt" >>"$CTL_LOG"
  [ "$CTL_RUN_RC" -eq 0 ] || { tail -n 20 "$CTL_DIR/run.txt" >&2; fail "control $CTL_ID: the composite case failed or the core called a host symbol"; }
  _digest=$(sed -n 's/^#define T64C_PIN_FINAL_DIGEST_HEX "\(.*\)"$/\1/p' "$ROOT/tests/installed-consumer/t64c_composite.h")
  [ -n "$_digest" ] || die "no final digest pin in t64c_composite.h"
  grep -qF "final state digest $_digest: equal" "$CTL_DIR/run.txt" || fail "control $CTL_ID: the final digest changed"
  grep -qF "t64_sha256_init calls: 0" "$CTL_DIR/run.txt" || fail "control $CTL_ID: the core called the host's t64_sha256_init"
  echo "digest: the composite case ends at the pinned final digest $_digest with the host symbols linked in" >>"$CTL_LOG"
}

# ctl_plain_cc: no CMake. One cc command compiles every source of the consumer (but the
# collision file and the standalone smoke main) against the installed include directory
# and links the installed archive; the smoke case then runs.
ctl_plain_cc() {
  _cc=$(cache_value CMAKE_C_COMPILER)
  [ -n "$_cc" ] || die "the build tree records no C compiler"
  _id=$(sed -n 's/^set(tendies64_BUILD_ID "\(.*\)")$/\1/p' "$MOVED/lib/cmake/tendies64/tendies64Config.cmake")
  [ -n "$_id" ] || die "the installed package records no build identity"
  set --
  for _f in "$ROOT"/tests/installed-consumer/*.c; do
    case ${_f##*/} in
      collision.c | smoke_main.c) continue ;;
    esac
    set -- "$@" "$_f"
  done
  echo "cc line: cc -std=c99 -I <prefix>/include -DT64C_EXPECTED_BUILD_ID=<id> <$# consumer sources> <prefix>/lib/libt64_core.a" >>"$CTL_LOG"
  if ! "$_cc" -std=c99 -I "$MOVED/include" "-DT64C_EXPECTED_BUILD_ID=\"$_id\"" \
    -o "$CTL_DIR/t64c_plain" "$@" "$MOVED/lib/libt64_core.a" >"$CTL_DIR/cc.log" 2>&1; then
    cat "$CTL_DIR/cc.log" >&2
    fail "control $CTL_ID: the plain cc line does not compile and link the consumer"
  fi
  if "$CTL_DIR/t64c_plain" --case smoke >"$CTL_DIR/smoke.txt" 2>&1; then
    CTL_SMOKE_RC=0
  else
    CTL_SMOKE_RC=$?
  fi
  cat "$CTL_DIR/smoke.txt" >>"$CTL_LOG"
  [ "$CTL_SMOKE_RC" -eq 0 ] || fail "control $CTL_ID: the smoke case of the plain cc build failed"
}

# ctl_judge: compare what the action observed with the row.
ctl_judge() {
  case $CTL_KIND in
    text)
      ctl_say "expected text: $CTL_EXPECTED"
      if grep -qF -- "$CTL_EXPECTED" "$CTL_LOG"; then
        ctl_say "observed text: $CTL_EXPECTED"
      else
        ctl_say "observed text: not found; the log ends with:"
        tail -n 20 "$CTL_LOG" >&2
        fail "control $CTL_ID: the expected text did not occur"
      fi
      ;;
    status)
      ctl_say "expected create status: $CTL_EXPECTED, machine NULL"
      ctl_say "observed create status: ${CTL_OBSERVED_STATUS:-none}"
      if [ "$CTL_OBSERVED_STATUS" != "$CTL_EXPECTED" ]; then
        tail -n 20 "$CTL_LOG" >&2
        fail "control $CTL_ID: the create status is not the expected one"
      fi
      ;;
    *) die "unknown expected kind $CTL_KIND in controls.tsv" ;;
  esac
}

cmd_control() {
  [ "$#" -ge 1 ] || usage
  CTL_ID=$1
  shift
  CTL_ROM="$ROOT/build/fixtures/phase10-composite-s32k.z64"
  while [ "$#" -gt 0 ]; do
    case $1 in
      --rom) [ "$#" -ge 2 ] || usage; CTL_ROM=$2; shift 2 ;;
      *) usage ;;
    esac
  done
  [ -f "$CTL_TSV" ] || die "missing $CTL_TSV"
  _row=$(awk -F '\t' -v id="$CTL_ID" '$0 !~ /^#/ && $1 == id { print; exit }' "$CTL_TSV")
  [ -n "$_row" ] || die "no row $CTL_ID in controls.tsv"
  CTL_ACTION=$(printf '%s\n' "$_row" | cut -f 2)
  CTL_ARG=$(printf '%s\n' "$_row" | cut -f 3)
  CTL_KIND=$(printf '%s\n' "$_row" | cut -f 4)
  CTL_EXPECTED=$(printf '%s\n' "$_row" | cut -f 5)
  [ -d "$MOVED" ] || die "no moved prefix: run setup first"
  CTL_DIR="$WORK/controls/$CTL_ID"
  CTL_BUILD="$CTL_DIR/build"
  CTL_LOG="$CTL_DIR/log.txt"
  CTL_OBSERVED_STATUS=
  CTL_SMOKE_RC=
  rm -rf "$CTL_DIR"
  mkdir -p "$CTL_DIR"
  : >"$CTL_LOG"
  ctl_say "action $CTL_ACTION ${CTL_ARG}"
  case $CTL_ACTION in
    configure-missing) ctl_configure_missing ;;
    configure-version) ctl_configure_version ;;
    source-tree) ctl_source_tree ;;
    doctored) ctl_doctored ;;
    relocate) ctl_relocate ;;
    collision) ctl_collision ;;
    plain-cc) ctl_plain_cc ;;
    *) die "unknown control action $CTL_ACTION in controls.tsv" ;;
  esac
  ctl_judge
  rm -rf "$CTL_DIR"
  ctl_say "ok"
}

# ---- local sanitizer lanes (plan 11-20): never run by CTest or the hosted gate ----

SAN_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"
SAN_ROM="$ROOT/build/fixtures/phase10-composite-s32k.z64"

# san_scan LOG...: no sanitizer report in the logs.
san_scan() {
  if grep -l -e "ERROR: AddressSanitizer" -e "runtime error:" -e "WARNING: ThreadSanitizer" \
    -e "ERROR: LeakSanitizer" "$@" >"$BUILD/sanitize-lane/hits.list" 2>/dev/null; then
    echo "check-install: a sanitizer report is in:" >&2
    cat "$BUILD/sanitize-lane/hits.list" >&2
    fail "sanitizer report (a defect: record it and stop for a new numbered plan)"
  fi
}

# san_runtime_refs FILE RUNTIME: how many undefined symbols of FILE (an archive or an
# executable) name the sanitizer runtime (RUNTIME is "asan" or "tsan"): the lane proves
# which side is instrumented instead of trusting its flags.
san_runtime_refs() {
  nm -P -u "$1" 2>/dev/null | grep -c "_$2_" || true
}

# san_build_consumer BASE PREFIX [CMAKE_ARGS...]: configure and build the consumer with the
# sanitizer flags against PREFIX into BASE/consumer.
san_build_consumer() {
  _sb=$1
  _sp=$2
  shift 2
  if ! consumer_configure "$_sb/consumer" "$_sp" "$_sb/configure.log" "-DCMAKE_C_FLAGS=$SAN_FLAGS" "$@"; then
    cat "$_sb/configure.log" >&2
    fail "sanitizer consumer configure failed"
  fi
  if ! "$CMAKE" --build "$_sb/consumer" >"$_sb/build.log" 2>&1; then
    cat "$_sb/build.log" >&2
    fail "sanitizer consumer build failed"
  fi
  [ -x "$_sb/consumer/t64c" ] || fail "the sanitizer consumer executable was not produced"
}

cmd_sanitize() {
  [ "$#" -ge 1 ] || usage
  SAN_MODE=$1
  shift
  SAN_STEPS=20000
  SAN_STRIDE=
  while [ "$#" -gt 0 ]; do
    case $1 in
      --steps) [ "$#" -ge 2 ] || usage; SAN_STEPS=$2; shift 2 ;;
      --digest-stride) [ "$#" -ge 2 ] || usage; SAN_STRIDE=$2; shift 2 ;;
      --rom) [ "$#" -ge 2 ] || usage; SAN_ROM=$2; shift 2 ;;
      *) usage ;;
    esac
  done
  [ -f "$SAN_ROM" ] || die "the composite ROM is not built: $SAN_ROM"
  _base="$BUILD/sanitize-lane/$SAN_MODE"
  rm -rf "$_base"
  mkdir -p "$_base"
  _t_start=$(date +%s)
  case $SAN_MODE in
    shipped)
      [ -f "$BUILD/core/libt64_core.a" ] || die "the core archive is not built in $BUILD (build the gate first)"
      _src=$BUILD
      [ -n "$SAN_STRIDE" ] || SAN_STRIDE=1
      ;;
    instrumented)
      _src=$ROOT/build/asan
      (cd "$ROOT" && "$CMAKE" --preset asan) >"$_base/asan-configure.log" 2>&1 \
        || { cat "$_base/asan-configure.log" >&2; fail "cmake --preset asan failed"; }
      (cd "$ROOT" && "$CMAKE" --build --preset asan --target t64_core) >"$_base/asan-build.log" 2>&1 \
        || { tail -n 30 "$_base/asan-build.log" >&2; fail "building t64_core with the asan preset failed"; }
      [ -n "$SAN_STRIDE" ] || SAN_STRIDE=8
      ;;
    *) usage ;;
  esac
  _t_built=$(date +%s)
  if ! "$CMAKE" --install "$_src" --component Development --prefix "$_base/prefix with space" \
    >"$_base/install.log" 2>&1; then
    cat "$_base/install.log" >&2
    fail "cmake --install of the Development component failed"
  fi
  san_build_consumer "$_base" "$_base/prefix with space"
  _t_consumer=$(date +%s)
  _lib_refs=$(san_runtime_refs "$_base/prefix with space/lib/libt64_core.a" asan)
  _exe_refs=$(san_runtime_refs "$_base/consumer/t64c" asan)
  [ "$_exe_refs" -gt 0 ] || fail "sanitize $SAN_MODE: the consumer executable references no AddressSanitizer runtime symbol"
  if [ "$SAN_MODE" = shipped ]; then
    [ "$_lib_refs" -eq 0 ] || fail "sanitize shipped: the installed archive is instrumented ($_lib_refs references), it is not the library as shipped"
  else
    [ "$_lib_refs" -gt 0 ] || fail "sanitize instrumented: the installed archive references no AddressSanitizer runtime symbol"
  fi
  _id=$(sed -n 's/^set(tendies64_BUILD_ID "\(.*\)")$/\1/p' "$_base/prefix with space/lib/cmake/tendies64/tendies64Config.cmake")
  echo "check-install: sanitize $SAN_MODE: AddressSanitizer runtime references: archive $_lib_refs, consumer $_exe_refs"
  echo "check-install: sanitize $SAN_MODE: library tree ${_src#"$ROOT"/} (build id $_id), consumer flags: $SAN_FLAGS"
  echo "check-install: sanitize $SAN_MODE: library build and install $((_t_built - _t_start)) s, consumer build $((_t_consumer - _t_built)) s"
  _failed=0
  for _case in $(grep '^[a-z]' "$ROOT/tests/installed-consumer/cases.txt"); do
    _c0=$(date +%s)
    # The title cases (plan 12-05) run the authored title ROMs, as the CTest names do
    # (tools/abi/CMakeLists.txt, T64_CONSUMER_ROM_<case>); every other case gets --rom.
    _case_rom=$SAN_ROM
    case $_case in
      title | title-persist) _case_rom=$ROOT/build/fixtures/phase12-title-s32k.z64 ;;
      title-refused) _case_rom=$ROOT/build/fixtures/phase12-title-refused-s32k.z64 ;;
    esac
    [ -f "$_case_rom" ] || fail "sanitize $SAN_MODE: the ROM of case $_case is not built: ${_case_rom#"$ROOT"/}"
    if [ "$_case" = sequence ]; then
      "$_base/consumer/t64c" --case sequence --rom "$SAN_ROM" --steps "$SAN_STEPS" \
        --digest-stride "$SAN_STRIDE" >"$_base/$_case.log" 2>&1 && _rc=0 || _rc=$?
    else
      "$_base/consumer/t64c" --case "$_case" --rom "$_case_rom" >"$_base/$_case.log" 2>&1 && _rc=0 || _rc=$?
    fi
    _c1=$(date +%s)
    if [ "$_rc" -eq 0 ]; then
      echo "check-install: sanitize $SAN_MODE: case $_case ok, $((_c1 - _c0)) s ($(grep -h 'checks=' "$_base/$_case.log" | tail -n 1))"
    else
      echo "check-install: sanitize $SAN_MODE: case $_case FAILED (exit $_rc), log $_base/$_case.log" >&2
      tail -n 20 "$_base/$_case.log" >&2
      _failed=$((_failed + 1))
    fi
  done
  san_scan "$_base"/*.log
  [ "$_failed" -eq 0 ] || fail "sanitize $SAN_MODE: $_failed case(s) failed"
  _t_end=$(date +%s)
  echo "check-install: sanitize $SAN_MODE ok: every case of cases.txt, sequence with --steps $SAN_STEPS --digest-stride $SAN_STRIDE, $((_t_end - _t_start)) s in all"
  grep -h '^sequence: ' "$_base/sequence.log" | sed 's/^/check-install: /' | cut -c1-240
}

cmd_tsan() {
  TSAN_FLAGS="-fsanitize=thread -fno-omit-frame-pointer"
  TSAN_SEED=0
  while [ "$#" -gt 0 ]; do
    case $1 in
      --seed-race) TSAN_SEED=1; shift ;;
      --rom) [ "$#" -ge 2 ] || usage; SAN_ROM=$2; shift 2 ;;
      *) usage ;;
    esac
  done
  [ -f "$SAN_ROM" ] || die "the composite ROM is not built: $SAN_ROM"
  if [ "$TSAN_SEED" -eq 1 ]; then
    _base="$BUILD/sanitize-lane/tsan-seed-race"
  else
    _base="$BUILD/sanitize-lane/tsan"
  fi
  rm -rf "$_base"
  mkdir -p "$_base"
  _t_start=$(date +%s)
  CC_PATH=$(cache_value CMAKE_C_COMPILER)
  [ -n "$CC_PATH" ] || die "the build tree records no C compiler"
  NINJA=$(cache_value CMAKE_MAKE_PROGRAM)
  set -- -S "$ROOT" -B "$_base/core" -G Ninja -DCMAKE_BUILD_TYPE=Release -DT64_WERROR=ON \
    "-DCMAKE_C_FLAGS=$TSAN_FLAGS" "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread" "-DCMAKE_C_COMPILER=$CC_PATH"
  if [ -n "$NINJA" ] && [ -x "$NINJA" ]; then
    set -- "$@" "-DCMAKE_MAKE_PROGRAM=$NINJA"
  fi
  env -u CMAKE_PREFIX_PATH "$CMAKE" "$@" >"$_base/core-configure.log" 2>&1 \
    || { tail -n 30 "$_base/core-configure.log" >&2; fail "configuring the ThreadSanitizer library tree failed"; }
  "$CMAKE" --build "$_base/core" --target t64_core >"$_base/core-build.log" 2>&1 \
    || { tail -n 30 "$_base/core-build.log" >&2; fail "building t64_core with -fsanitize=thread failed"; }
  "$CMAKE" --install "$_base/core" --component Development --prefix "$_base/prefix with space" \
    >"$_base/install.log" 2>&1 || { cat "$_base/install.log" >&2; fail "cmake --install of the ThreadSanitizer library failed"; }
  _t_lib=$(date +%s)
  set -- -DT64C_THREADS=ON
  if [ "$TSAN_SEED" -eq 1 ]; then
    set -- "$@" -DT64C_THREADS_SEED_RACE=ON
  fi
  if ! consumer_configure "$_base/consumer" "$_base/prefix with space" "$_base/configure.log" \
    "-DCMAKE_C_FLAGS=$TSAN_FLAGS" "$@"; then
    cat "$_base/configure.log" >&2
    fail "ThreadSanitizer consumer configure failed"
  fi
  "$CMAKE" --build "$_base/consumer" >"$_base/build.log" 2>&1 \
    || { cat "$_base/build.log" >&2; fail "ThreadSanitizer consumer build failed"; }
  _t_consumer=$(date +%s)
  _lib_refs=$(san_runtime_refs "$_base/prefix with space/lib/libt64_core.a" tsan)
  _exe_refs=$(san_runtime_refs "$_base/consumer/t64c" tsan)
  [ "$_lib_refs" -gt 0 ] && [ "$_exe_refs" -gt 0 ] || fail "tsan: the archive ($_lib_refs) and the consumer ($_exe_refs) must both reference the ThreadSanitizer runtime"
  _id=$(sed -n 's/^set(tendies64_BUILD_ID "\(.*\)")$/\1/p' "$_base/prefix with space/lib/cmake/tendies64/tendies64Config.cmake")
  echo "check-install: tsan: ThreadSanitizer runtime references: archive $_lib_refs, consumer $_exe_refs"
  echo "check-install: tsan: library with $TSAN_FLAGS (build id $_id), consumer with T64C_THREADS=ON"
  TSAN_OPTIONS="halt_on_error=0 exitcode=66" "$_base/consumer/t64c" --case threads --rom "$SAN_ROM" \
    >"$_base/threads.log" 2>&1 && _rc=0 || _rc=$?
  _t_run=$(date +%s)
  if [ "$TSAN_SEED" -eq 1 ]; then
    if grep -q "WARNING: ThreadSanitizer" "$_base/threads.log"; then
      echo "check-install: tsan --seed-race ok: ThreadSanitizer reported the seeded race ($(grep -c 'WARNING: ThreadSanitizer' "$_base/threads.log") report(s), exit $_rc), so the lane can fail"
      return 0
    fi
    tail -n 20 "$_base/threads.log" >&2
    fail "tsan --seed-race: ThreadSanitizer reported nothing for two threads on one instance"
  fi
  if grep -q "WARNING: ThreadSanitizer" "$_base/threads.log"; then
    grep -A 12 "WARNING: ThreadSanitizer" "$_base/threads.log" | head -n 60 >&2
    fail "ThreadSanitizer report (RESEARCH A9: a finding; record it and stop for a new numbered plan)"
  fi
  if [ "$_rc" -ne 0 ]; then
    tail -n 20 "$_base/threads.log" >&2
    fail "the threads case failed (exit $_rc)"
  fi
  _pin=97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c
  _equal=$(grep -c "digest=$_pin" "$_base/threads.log" || true)
  [ "$_equal" -eq 2 ] || { cat "$_base/threads.log" >&2; fail "tsan: expected both thread digests to equal $_pin"; }
  grep '^threads' "$_base/threads.log" | sed 's/^/check-install: /' | cut -c1-260
  echo "check-install: tsan ok: two threads, one instance each, no ThreadSanitizer report; both digests $_pin; library $((_t_lib - _t_start)) s, consumer $((_t_consumer - _t_lib)) s, run $((_t_run - _t_consumer)) s"
}

# ---- fresh-archive lane (plan 11-24): LOCAL or manual, never run by CTest or the hosted gate ----

# fa_say TEXT: one line of the lane's own output.
fa_say() {
  echo "check-install: fresh-archive: $*"
}

cmd_fresh_archive() {
  FA_ROM=$SAN_ROM
  while [ "$#" -gt 0 ]; do
    case $1 in
      --rom) [ "$#" -ge 2 ] || usage; FA_ROM=$2; shift 2 ;;
      *) usage ;;
    esac
  done
  [ -f "$FA_ROM" ] || die "the composite ROM is not built: $FA_ROM"
  command -v git >/dev/null 2>&1 || die "git not found"
  command -v tar >/dev/null 2>&1 || die "tar not found"
  CC_PATH=$(cache_value CMAKE_C_COMPILER)
  [ -n "$CC_PATH" ] || die "the build tree records no C compiler"
  NINJA=$(cache_value CMAKE_MAKE_PROGRAM)
  _base="$BUILD/fresh-archive"
  rm -rf "$_base"
  mkdir -p "$_base"
  # The exported source and its build tree live outside the repository, so nothing the
  # working tree holds beyond the commit (ignored build trees, untracked files, a .git
  # directory) can reach the build; only the installed prefix and the consumer are kept
  # below BUILD for the qualifier to read.
  _tmp=$(mktemp -d "${TMPDIR:-/tmp}/t64-fresh-archive.XXXXXX") || die "mktemp -d failed"
  trap 'rm -rf "$_tmp"' EXIT
  _fa_prefix="$_base/prefix with space"
  _t_start=$(date +%s)
  _head=$(git -C "$ROOT" rev-parse HEAD)
  mkdir "$_tmp/src"
  git -C "$ROOT" archive --format=tar HEAD | tar -x -C "$_tmp/src"
  _files=$(cd "$_tmp/src" && find . -type f | wc -l | tr -d ' ')
  [ -f "$_tmp/src/CMakeLists.txt" ] || fail "fresh-archive: the exported tree has no CMakeLists.txt"
  [ ! -e "$_tmp/src/.git" ] || fail "fresh-archive: the exported tree holds a .git entry"
  if [ -n "$(git -C "$ROOT" status --porcelain --untracked-files=no)" ]; then
    fa_say "note: the working tree has uncommitted changes; the archive is HEAD only"
  fi
  # The gate preset configuration spelled out (CMakePresets.json): Ninja, Release,
  # T64_WERROR=ON and the developer warnings as errors.
  set -- -S "$_tmp/src" -B "$_tmp/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DT64_WERROR=ON \
    -Wdev -Werror=dev --warn-uninitialized "-DCMAKE_C_COMPILER=$CC_PATH"
  if [ -n "$NINJA" ] && [ -x "$NINJA" ]; then
    set -- "$@" "-DCMAKE_MAKE_PROGRAM=$NINJA"
  fi
  env -u CMAKE_PREFIX_PATH "$CMAKE" "$@" >"$_base/configure.log" 2>&1 \
    || { tail -n 30 "$_base/configure.log" >&2; fail "fresh-archive: configuring the exported tree failed"; }
  "$CMAKE" --build "$_tmp/build" --target t64_core >"$_base/build.log" 2>&1 \
    || { tail -n 30 "$_base/build.log" >&2; fail "fresh-archive: building t64_core of the exported tree failed"; }
  _t_built=$(date +%s)
  "$CMAKE" --install "$_tmp/build" --component Development --prefix "$_fa_prefix" \
    >"$_base/install.log" 2>&1 || { cat "$_base/install.log" >&2; fail "fresh-archive: cmake --install of the Development component failed"; }
  (cd "$_fa_prefix" && find . -type f | sed 's|^\./||' | sort) >"$_base/installed.list"
  {
    echo include/tendies64/t64_machine.h
    echo lib/cmake/tendies64/tendies64Config.cmake
    echo lib/cmake/tendies64/tendies64ConfigVersion.cmake
    echo lib/cmake/tendies64/tendies64Targets-release.cmake
    echo lib/cmake/tendies64/tendies64Targets.cmake
    echo lib/libt64_core.a
  } | sort >"$_base/expected.list"
  if ! cmp -s "$_base/expected.list" "$_base/installed.list"; then
    echo "check-install: fresh-archive: installed files differ from the six-file package (expected, installed):" >&2
    diff "$_base/expected.list" "$_base/installed.list" >&2 || true
    fail "fresh-archive: install tree is not the reviewed package"
  fi
  # No byte of the installed tree names the exported source, its build tree, the
  # repository or a per-user location (finding names the root by position).
  python3 -B "$ROOT/scripts/check-abi.py" abspaths --prefix "$_fa_prefix" \
    --forbid "$_tmp/src" "$_tmp/build" "$ROOT" /Users/ /home/ /private/ /var/folders \
    >"$_base/abspaths.log" 2>&1 || { cat "$_base/abspaths.log" >&2; fail "fresh-archive: the installed tree names a build or source path"; }
  _id=$(sed -n 's/^set(tendies64_BUILD_ID "\(.*\)")$/\1/p' "$_fa_prefix/lib/cmake/tendies64/tendies64Config.cmake")
  [ -n "$_id" ] || fail "fresh-archive: the installed package config names no build id"
  if ! consumer_configure "$_base/consumer" "$_fa_prefix" "$_base/consumer-configure.log"; then
    cat "$_base/consumer-configure.log" >&2
    fail "fresh-archive: consumer configure failed (find_package(tendies64 1.0 REQUIRED CONFIG))"
  fi
  "$CMAKE" --build "$_base/consumer" >"$_base/consumer-build.log" 2>&1 \
    || { cat "$_base/consumer-build.log" >&2; fail "fresh-archive: consumer build failed"; }
  [ -x "$_base/consumer/t64c" ] || fail "fresh-archive: the consumer executable was not produced"
  _t_consumer=$(date +%s)
  for _case in smoke composite; do
    if ! "$_base/consumer/t64c" --case "$_case" --rom "$FA_ROM" >"$_base/$_case.log" 2>&1; then
      tail -n 20 "$_base/$_case.log" >&2
      fail "fresh-archive: consumer case $_case failed"
    fi
    fa_say "case $_case ok ($(grep -h 'checks=' "$_base/$_case.log" | tail -n 1))"
  done
  _t_end=$(date +%s)
  fa_say "archive of ${_head} ($_files files), library build $((_t_built - _t_start)) s, consumer $((_t_consumer - _t_built)) s"
  fa_say "installed archive build id $_id"
  echo "check-install: fresh-archive ok: exported tree built outside the repository, six files installed into a prefix with a space, consumer smoke and composite agree, no source or build path in the prefix, $((_t_end - _t_start)) s in all"
}

case $SUB in
  setup) [ "$#" -eq 0 ] || usage; cmd_setup ;;
  build) [ "$#" -eq 0 ] || usage; cmd_build ;;
  run) cmd_run "$@" ;;
  control) cmd_control "$@" ;;
  sanitize) cmd_sanitize "$@" ;;
  tsan) cmd_tsan "$@" ;;
  fresh-archive) cmd_fresh_archive "$@" ;;
  three-way) cmd_three_way "$@" ;;
  *) echo "check-install: unknown subcommand: $SUB" >&2; usage ;;
esac
