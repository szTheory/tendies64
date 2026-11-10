#!/bin/sh
# Rust-vs-C spike harness (CONTEXT D1, amendments A-2 and A-3).
#
# Spike-only: lives under docs/spikes, never built by CMake, never inside core/.
# Compares, per frozen leak class, the full C gate lint (both layers, on this
# host and in the pinned rust image) with a Rust twin checked by
# `rustc --edition 2021 --crate-type=lib --emit=metadata`.
#
# Usage:
#   run-spike.sh --control        control pair (good.c / good.rs) plus harness
#                                 negative controls; no hash check, no decision
#                                 data
#   run-spike.sh --print-hashes   print the frozen-hash table for PREREG.md
#   run-spike.sh --verify-frozen  check PREREG.md is committed and every frozen
#                                 file still matches it, then stop
#   run-spike.sh --admission-check  C contract table, sanitizer negative control
#                                 (a seeded overread must be caught) and the Rust
#                                 table; runs no fuzz and produces no decision data
#   run-spike.sh --registered     the registered run: verify, per-class table, C
#                                 sanitizer fuzz, C-vs-Rust differential, rules
#   run-spike.sh --decision-selftest  negative controls for the mechanical
#                                 decision on synthetic inputs (no docker, no
#                                 compiler, no decision data); PREREG Amendment 1
#
# The mechanical decision has three values (PREREG Amendment 1):
#   switch-rust   rule (a) or rule (b) fired, as registered
#   stay-c17      neither fired AND the evidence for "neither fired" is complete
#   inconclusive  anything else; exit 3. Only stay-c17 may auto-accept ADR-0002.
#
# Needs: docker with the image rust:1.91 already present (never pulled here),
# a host C compiler. No crates, no cargo, no network (--network none), the repo
# is mounted read-only.
set -eu
LC_ALL=C
export LC_ALL

HERE=$(cd "$(dirname "$0")" && pwd -P)
ROOT=$(cd "$HERE/../../.." && pwd -P)
cd "$ROOT"

SPIKE=docs/spikes/rust-vs-c
SEL=tools/lint/selftest
RDIR=$SPIKE/corpus/rust
IMAGE=rust:1.91

# name|C seed (under tools/lint/selftest)|Rust twin (under corpus/rust)
# bad_runtime_helper.c and bad_include_unlisted.c are Layer-1-only policy seeds
# with no Rust analogue and are not part of the frozen comparison.
CLASSES='control|good.c|good.rs
host-time|bad_time.c|time.rs
file-io|bad_file_io.c|file_io.rs
thread|bad_thread.c|thread.rs
entropy|bad_entropy.c|entropy.rs
environment|bad_env.c|env.rs
stdio|bad_include_stdio.c|stdio.rs
mutable-static|bad_mutable_static.c|mutable_static.rs
mutable-global|bad_mutable_global.c|mutable_global.rs
atomic-static|bad_atomic.c|interior_mutable_static.rs
tls|bad_tls.c|tls.rs
allocation|bad_malloc.c|alloc.rs
libm|bad_libm.c|libm.rs
floating-point|bad_float.c|float.rs
build-macro|bad_time_macro.c|build_macro.rs
inline-asm|bad_asm.c|asm.rs
constructor|bad_ctor.c|ctor.rs
extern-host|bad_extern_host.c|extern_host.rs
std-escape|-|std_escape.rs'

# Set to yes once the registered run has passed verify_frozen: from then on a
# harness error is reported as an inconclusive decision, never as silence.
DECIDING=no

die() {
  echo "run-spike: $*" >&2
  if [ "$DECIDING" = yes ]; then
    echo "completeness-gaps: harness error: $*"
    echo "mechanical-decision: inconclusive"
    exit 3
  fi
  exit 2
}

usage() {
  echo "usage: run-spike.sh --control | --print-hashes | --verify-frozen | --admission-check | --registered | --decision-selftest" >&2
  exit 2
}

preflight() {
  command -v docker >/dev/null 2>&1 || die "docker not found"
  docker info >/dev/null 2>&1 || die "docker daemon not reachable"
  docker image inspect "$IMAGE" >/dev/null 2>&1 || die "image $IMAGE not present (this harness never pulls)"
  command -v cc >/dev/null 2>&1 || die "no host C compiler (cc)"
}

# Read-only repo mount, no network. The script text arrives on stdin.
dki() {
  docker run --rm -i --network none -v "$ROOT":/src:ro -w /src "$IMAGE" "$@"
}

new_tmp() {
  T=$(mktemp -d "${TMPDIR:-/tmp}/t64spike.XXXXXX")
  trap 'rm -rf "$T"' EXIT
  trap 'exit 1' HUP INT TERM
}

# The in-container script: C lint (Linux layer), Rust check, then two negative
# controls that are not corpus members. $1 = the spec lines (name|C path|Rust path).
write_inner_classes() {
  {
    printf "SPECS='%s'\n" "$1"
    cat <<'EOS'
for spec in $SPECS; do
  name=${spec%%|*}
  rest=${spec#*|}
  cpath=${rest%%|*}
  rpath=${rest#*|}
  if [ "$cpath" != - ]; then
    out=$(sh tools/lint/core-lint.sh --files "$cpath" 2>&1 </dev/null)
    rc=$?
    ids=$(printf '%s\n' "$out" | awk '/^L[12]-[A-Z0-9-]+ /{print $1}' | sort -u | paste -s -d, -)
    printf 'C\t%s\t%s\t%s\n' "$name" "$rc" "${ids:--}"
  fi
  rm -f /tmp/spike.rmeta
  out=$(rustc --edition 2021 --crate-type=lib --emit=metadata -o /tmp/spike.rmeta "$rpath" 2>&1 </dev/null)
  rc=$?
  code=$(printf '%s\n' "$out" | sed -n 's/^error\[\(E[0-9][0-9]*\)\].*/\1/p' | head -n 1)
  if [ -z "$code" ]; then
    code=$(printf '%s\n' "$out" | sed -n 's/^error: \(.*\)/\1/p' | head -n 1 | tr -c 'A-Za-z0-9_\n' '_' | cut -c1-48)
    if [ -n "$code" ]; then code="no-code:$code"; fi
  fi
  printf 'R\t%s\t%s\t%s\n' "$name" "$rc" "${code:--}"
done
printf 'fn f( {\n' > /tmp/neg.rs
rustc --edition 2021 --crate-type=lib --emit=metadata -o /tmp/neg.rmeta /tmp/neg.rs >/dev/null 2>&1 </dev/null
printf 'NR\t%s\n' "$?"
printf 'static int t64_neg_counter;\nint t64_neg_bump(void) { return ++t64_neg_counter; }\n' > /tmp/neg.c
sh tools/lint/core-lint.sh --files /tmp/neg.c >/dev/null 2>&1 </dev/null
printf 'NC\t%s\n' "$?"
printf 'I\tcontainer-arch\t%s\n' "$(uname -m)"
printf 'I\trustc\t%s\n' "$(rustc --version)"
printf 'I\tcontainer-gcc\t%s\n' "$(gcc --version | head -n 1)"
EOS
  } > "$T/inner-classes.sh"
}

# Host side of the C lint for the selected classes, plus the host negative control.
run_host_lint() {
  : > "$T/host.tsv"
  while IFS='|' read -r _n _c _r; do
    [ -n "$_n" ] || continue
    if [ "$_c" = - ]; then
      printf '%s\tn/a\t-\n' "$_n" >> "$T/host.tsv"
      continue
    fi
    _o=$(sh tools/lint/core-lint.sh --files "$SEL/$_c" 2>&1 </dev/null) && _rc=0 || _rc=$?
    _ids=$(printf '%s\n' "$_o" | awk '/^L[12]-[A-Z0-9-]+ /{print $1}' | sort -u | paste -s -d, -)
    printf '%s\t%s\t%s\n' "$_n" "$_rc" "${_ids:--}" >> "$T/host.tsv"
  done <<EOF
$1
EOF
  printf 'static int t64_neg_counter;\nint t64_neg_bump(void) { return ++t64_neg_counter; }\n' > "$T/neg.c"
  sh tools/lint/core-lint.sh --files "$T/neg.c" >/dev/null 2>&1 </dev/null && NEG_HOST=0 || NEG_HOST=$?
}

# Container side. $1 = the class lines to run.
run_container_classes() {
  _specs=$(printf '%s\n' "$1" | awk -F'|' -v sel="$SEL" -v rd="$RDIR" '
    { cp = ($2 == "-") ? "-" : sel "/" $2; print $1 "|" cp "|" rd "/" $3 }')
  write_inner_classes "$_specs"
  dki sh -s < "$T/inner-classes.sh" > "$T/box.out"
  grep -E '^(C|R)	' "$T/box.out" > "$T/box.tsv" || true
  NEG_RUST=$(awk -F'\t' '$1=="NR"{print $2}' "$T/box.out")
  NEG_CLINUX=$(awk -F'\t' '$1=="NC"{print $2}' "$T/box.out")
  [ -n "$NEG_RUST" ] && [ -n "$NEG_CLINUX" ] || die "container produced no negative-control lines"
}

# Joins host and container rows into the TSV table and the rule (a) verdict.
# Rule (a) per class: not control, not probe, C host exit 0 and C linux exit 0
# (the gate would be green on both), and the Rust twin exits 1 (compile-time
# rejection; any other non-zero exit is an anomaly, not a rejection).
print_class_table() {
  awk -F'\t' '
    FILENAME == ARGV[1] { hrc[$1] = $2; hid[$1] = $3; ord[++n] = $1; next }
    $1 == "C" { crc[$2] = $3; cid[$2] = $4; next }
    $1 == "R" { rrc[$2] = $3; rcode[$2] = $4; next }
    END {
      print "class\tc_host\tc_linux\trust\trule_a\tdetail"
      trig = ""; anom = 0
      for (i = 1; i <= n; i++) {
        c = ord[i]
        ch = hrc[c]; cl = (c in crc) ? crc[c] : "n/a"; rr = rrc[c]
        verdict = "no"; note = ""
        if (c == "control") verdict = "n/a (control)"
        else if (c == "std-escape") verdict = "n/a (probe)"
        else if (ch == "0" && cl == "0" && rr == "1") { verdict = "yes"; trig = trig (trig == "" ? "" : ",") c }
        if (ch != "n/a" && ch != "0" && ch != "1") { note = note " ANOMALY(c_host exit " ch ")"; anom++ }
        if (cl != "n/a" && cl != "0" && cl != "1") { note = note " ANOMALY(c_linux exit " cl ")"; anom++ }
        if ((c in rrc) && rr != "0" && rr != "1") { note = note " ANOMALY(rust exit " rr ")"; anom++ }
        # Amendment 1: a class with no Linux C row (when it has a C twin) or no
        # Rust row was not measured, which is an anomaly, not a pass.
        if (ch != "n/a" && !(c in crc)) { note = note " ANOMALY(c_linux row missing)"; anom++ }
        if (!(c in rrc)) { note = note " ANOMALY(rust row missing)"; anom++ }
        if (ch != "n/a" && cl != "n/a" && ((ch == "0") != (cl == "0"))) note = note " DISAGREE(host/linux)"
        printf "%s\t%s\t%s\t%s\t%s\thost=%s; linux=%s; rust=%s%s\n", c, ch, cl, rr, verdict, hid[c], (c in cid) ? cid[c] : "n/a", rcode[c], note
      }
      printf "rule-a-triggers: %s\n", (trig == "" ? "none" : trig)
      printf "anomalies: %d\n", anom
    }
  ' "$T/host.tsv" "$T/box.tsv"
}

print_env() {
  echo "host: $(uname -s -m)"
  echo "host-cc: $(cc --version 2>&1 | head -n 1)"
  echo "image: $(docker image inspect "$IMAGE" --format '{{index .RepoDigests 0}}' 2>/dev/null || echo unknown)"
  awk -F'\t' '$1=="I"{printf "%s: %s\n", $2, $3}' "$T/box.out"
}

control_mode() {
  preflight
  new_tmp
  _t0=$(date +%s)
  _sel=$(printf '%s\n' "$CLASSES" | grep '^control|')
  run_host_lint "$_sel"
  run_container_classes "$_sel"
  print_env
  print_class_table > "$T/table.tsv"
  grep -v '^rule-a-triggers' "$T/table.tsv"
  echo "negative-controls: host-c=$NEG_HOST linux-c=$NEG_CLINUX linux-rust=$NEG_RUST (each must be 1)"
  _row=$(awk -F'\t' '$1=="control"{print $2 " " $3 " " $4}' "$T/table.tsv")
  _dt=$(($(date +%s) - _t0))
  echo "control-duration-seconds: $_dt"
  if [ "$_row" = "0 0 0" ] && [ "$NEG_HOST" = 1 ] && [ "$NEG_CLINUX" = 1 ] && [ "$NEG_RUST" = 1 ]; then
    echo "control: PASS"
    return 0
  fi
  echo "control: FAIL (control row '$_row')" >&2
  return 1
}

# Frozen files: the 18 C corpus files, the 19 Rust twins, the 4 admission files
# and this script, one "path" per line.
frozen_paths() {
  printf '%s\n' "$CLASSES" | awk -F'|' '$2 != "-" {print "tools/lint/selftest/" $2}' | sort
  printf '%s\n' "$CLASSES" | awk -F'|' -v d="$RDIR" '{print d "/" $3}' | sort
  printf '%s\n' "$SPIKE/admission/admission.h" "$SPIKE/admission/admission.c" \
    "$SPIKE/admission/fuzz_admission.c" "$SPIKE/admission/admission.rs"
  printf '%s\n' "$SPIKE/run-spike.sh"
}

hash_of() {
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$1" | awk '{print $1}'
  else
    sha256sum "$1" | awk '{print $1}'
  fi
}

print_hashes() {
  frozen_paths | while IFS= read -r _p; do
    [ -f "$_p" ] || die "missing frozen file $_p"
    printf '%s  %s\n' "$(hash_of "$_p")" "$_p"
  done
}

# ---------------------------------------------------------------------------
# Registered-run machinery
# ---------------------------------------------------------------------------

ADM=$SPIKE/admission
PREREG=$SPIKE/PREREG.md
FZ_FLAGS='-std=c17 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all'
FZ_TOOL=""
FZ_RC=0

# The frozen-hash table in PREREG.md, as "hash<TAB>path" lines.
prereg_table() {
  awk '
    /<!-- frozen-hashes:begin -->/ { f = 1; next }
    /<!-- frozen-hashes:end -->/ { f = 0 }
    f && length($1) == 64 && $1 ~ /^[0-9a-f]+$/ && NF == 2 { print $1 "\t" $2 }
  ' "$PREREG"
}

# The hash of run-spike.sh from the latest amendment.
# Prints "none" when no amendment exists and "missing" when the latest has no hash.
latest_amendment_hash() {
  awk '
    /^## Amendment [0-9]+ \(/ { inA = 1; have = 1; cur = ""; next }
    /^## / { inA = 0 }
    inA && /^run-spike\.sh sha256: / { cur = $3 }
    END { if (!have) print "none"; else if (cur == "") print "missing"; else print cur }
  ' "$PREREG"
}

# Refuses unless PREREG.md is committed and every frozen file matches it.
# Only run-spike.sh may differ from the original table, and only by way of the
# latest amendment (harness amendment protocol).
verify_frozen() {
  [ -f "$PREREG" ] || die "PREREG.md is missing: the registered run is refused until the spike is pre-registered"
  git ls-files --error-unmatch "$PREREG" >/dev/null 2>&1 || die "PREREG.md is not committed"
  git diff --quiet HEAD -- "$PREREG" || die "PREREG.md has uncommitted changes"
  prereg_table > "$T/table.frozen"
  frozen_paths > "$T/paths.frozen"
  _np=$(wc -l < "$T/paths.frozen" | tr -d ' ')
  _nt=$(wc -l < "$T/table.frozen" | tr -d ' ')
  [ "$_np" = "$_nt" ] || die "PREREG hash table has $_nt rows, the frozen file set has $_np"
  _nr=$(find "$RDIR" -type f | wc -l | tr -d ' ')
  [ "$_nr" = 19 ] || die "corpus/rust holds $_nr files, expected 19"
  _amend=$(latest_amendment_hash)
  [ "$_amend" != missing ] || die "the latest amendment in PREREG.md has no 'run-spike.sh sha256:' line"
  _bad=0
  HARNESS_HASH=""
  HARNESS_HASH_SOURCE=""
  while IFS= read -r _p; do
    _want=$(awk -F'\t' -v p="$_p" '$2 == p {print $1}' "$T/table.frozen")
    [ -n "$_want" ] || die "PREREG hash table has no row for $_p"
    [ -f "$_p" ] || die "frozen file is missing: $_p"
    if [ "$_p" = "$SPIKE/run-spike.sh" ]; then
      if [ "$_amend" != none ]; then
        _want=$_amend
        HARNESS_HASH_SOURCE="latest amendment"
      else
        HARNESS_HASH_SOURCE="original table"
      fi
    fi
    _got=$(hash_of "$_p")
    if [ "$_got" != "$_want" ]; then
      echo "run-spike: frozen file changed: $_p" >&2
      _bad=1
    fi
    if [ "$_p" = "$SPIKE/run-spike.sh" ]; then HARNESS_HASH=$_got; fi
  done < "$T/paths.frozen"
  [ "$_bad" = 0 ] || die "frozen inputs do not match PREREG.md"
  _want_img=$(sed -n 's/^Image digest prefix: //p' "$PREREG" | head -n 1)
  [ -n "$_want_img" ] || die "PREREG.md has no 'Image digest prefix:' line"
  _have_img=$(docker image inspect "$IMAGE" --format '{{index .RepoDigests 0}}' 2>/dev/null || echo unknown)
  case "$_have_img" in
    *"sha256:$_want_img"*) ;;
    *) die "image digest $_have_img does not match the registered prefix $_want_img" ;;
  esac
  echo "frozen-inputs: ok ($_np files; run-spike.sh hash from $HARNESS_HASH_SOURCE: $HARNESS_HASH)"
}

# Builds fuzz_admission under ASan and UBSan, on the host when possible and in
# the container otherwise (recorded in FZ_TOOL), then runs it with the given
# arguments. $1 = extra compiler flags. Output in $T/fz.out and $T/fz.err.
fuzz_build_run() {
  _extra=$1
  shift
  if [ "$FZ_TOOL" != container ]; then
    # shellcheck disable=SC2086
    if cc $FZ_FLAGS $_extra -o "$T/fz" "$ADM/fuzz_admission.c" "$ADM/admission.c" 2> "$T/cc.err"; then
      FZ_TOOL=host
      ( "$T/fz" "$@" > "$T/fz.out" 2> "$T/fz.err" ) 2> /dev/null && FZ_RC=0 || FZ_RC=$?
      return 0
    fi
    FZ_TOOL=container
  fi
  dki sh -c '
    extra=$1
    shift
    cc -std=c17 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all $extra \
      -o /tmp/fz docs/spikes/rust-vs-c/admission/fuzz_admission.c docs/spikes/rust-vs-c/admission/admission.c || exit 97
    /tmp/fz "$@"
  ' sh "$_extra" "$@" > "$T/fz.out" 2> "$T/fz.err" && FZ_RC=0 || FZ_RC=$?
  [ "$FZ_RC" != 97 ] || die "the fuzz harness did not compile in the container either"
}

fuzz_tool_name() {
  if [ "$FZ_TOOL" = host ]; then
    echo "host $(cc --version 2>&1 | head -n 1)"
  else
    echo "container gcc"
  fi
}

# Amendment 1, rule (b) attribution. A sanitizer report is an ASan "ERROR:
# AddressSanitizer" line or a UBSan "runtime error:" line. A frame or location
# is attributable when it names admission.c itself: the original harness matched
# the bare text "admission.c", which also matches the harness file
# fuzz_admission.c and any non-sanitizer message that mentions the file.
san_report_in() {
  grep -Eq 'ERROR: AddressSanitizer|runtime error:' "$1"
}

admission_frame_in() {
  grep -Eq '(^|[^A-Za-z0-9_])admission\.c:[0-9]' "$1"
}

# Amendment 1 (review WR-22): the C lint is the tool under test for rule (a) but
# is not in the frozen set. The registered run records its hashes and the commit,
# and refuses a "stay-c17" when those files differ from the commit.
TUT_PATHS='tools/lint/core-lint.sh tools/lint/lint-objects.sh tools/lint/rules.txt tools/lint/allowlist.txt'
TUT_CLEAN=no

print_tool_under_test() {
  echo "git-commit: $(git rev-parse HEAD)"
  for _p in $TUT_PATHS; do
    [ -f "$_p" ] || die "tool under test is missing: $_p"
    printf 'tool-under-test sha256: %s  %s\n' "$(hash_of "$_p")" "$_p"
  done
  # shellcheck disable=SC2086
  if git diff --quiet HEAD -- $TUT_PATHS; then
    TUT_CLEAN=yes
    echo "tool-under-test: matches the commit above (recorded, not frozen by PREREG)"
  else
    TUT_CLEAN=no
    echo "tool-under-test: has uncommitted changes"
  fi
}

# Table check, then the harness negative control: a build with a seeded
# one-byte overread must be caught by the sanitizer and attributed to admission.c.
admission_control() {
  fuzz_build_run "" --table
  if [ "$FZ_RC" != 0 ] || ! grep -q '^table: ok$' "$T/fz.out"; then
    echo "admission-control: C contract table failed (exit $FZ_RC)" >&2
    return 1
  fi
  fuzz_build_run "-DT64S_SEEDED_OVERREAD" --mutations 3000
  if [ "$FZ_RC" != 0 ] && san_report_in "$T/fz.err" && admission_frame_in "$T/fz.err" &&
    grep -q '^FAIL: input ' "$T/fz.err"; then
    echo "admission-control: seeded overread caught and attributed to admission.c ($(grep '^FAIL: input ' "$T/fz.err" | head -n 1))"
    return 0
  fi
  echo "admission-control: seeded overread was NOT caught (exit $FZ_RC)" >&2
  return 1
}

# Rust side, in the container: no_std metadata form, the contract table, then
# (when $1 is differential) the differential test over the identical inputs.
rust_admission() {
  dki sh -c '
    cd docs/spikes/rust-vs-c/admission
    rustc --edition 2021 --crate-type=lib --emit=metadata -o /tmp/adm.rmeta admission.rs || exit 98
    echo "rust-metadata-no-std: ok"
    rustc --edition 2021 --test -O admission.rs -o /tmp/adm_test || exit 98
    /tmp/adm_test --exact tests::table --test-threads=1 || exit 99
    echo "rust-table: ok"
    if [ "$1" = differential ]; then
      /tmp/adm_test --exact tests::differential --nocapture --test-threads=1
    fi
  ' sh "$1" > "$T/rust.out" 2> "$T/rust.err" && RUST_RC=0 || RUST_RC=$?
  if [ "$RUST_RC" = 98 ]; then
    cat "$T/rust.err" >&2
    die "the Rust admission parser did not compile"
  fi
}

admission_check_mode() {
  preflight
  new_tmp
  admission_control
  rust_admission table
  grep -E '^rust-' "$T/rust.out"
  [ "$RUST_RC" = 0 ] || die "Rust table test failed"
  echo "admission-check: PASS (fuzz tool: $(fuzz_tool_name))"
}

# ---------------------------------------------------------------------------
# Mechanical decision (PREREG Amendment 1)
# ---------------------------------------------------------------------------
#
# The original harness printed stay-c17 whenever rule (a) listed no trigger and
# rule (b) had no report naming admission.c, so a fuzz that crashed, was killed,
# ran out of memory or found an info-field bug, a broken lint (exit 2) or a
# missing rustc (exit 127) all still read "stay-c17" (review WR-21).
#
# Inputs: $T/table.tsv (print_class_table), $T/box.out, $T/fz.out, $T/fz.err,
# $T/rust.out, and NEG_HOST NEG_CLINUX NEG_RUST ADM_CTRL FZ_RAN FZ_RC TUT_CLEAN.
# Sets DEC and prints the rules block.
#
#   inconclusive  a harness negative control failed (lint or rustc cannot fail,
#                 so neither rule can be read), whatever else happened
#   switch-rust   otherwise, rule (a) lists a trigger, or the fuzz exited
#                 non-zero with an ASan or UBSan report attributable to
#                 admission.c (rule b)
#   stay-c17      otherwise, only when every completeness condition holds:
#                 class table anomalies 0 (no lint exit 2, no rustc exit 127, no
#                 missing row), the container reported a rustc version, the
#                 sanitizer negative control passed, the fuzz ran and exited 0
#                 with "mutations: 2000000", "inputs: 2000040" and
#                 "info-consistency: ok", no sanitizer report text, the Rust
#                 differential printed the same info-consistency line, and the
#                 tool under test matches the recorded commit
#   inconclusive  anything else
GAPS=""

gap() {
  GAPS="${GAPS}${GAPS:+; }$1"
}

decide() {
  GAPS=""
  _untrusted=no
  if [ "${NEG_HOST:-}" != 1 ] || [ "${NEG_CLINUX:-}" != 1 ] || [ "${NEG_RUST:-}" != 1 ]; then
    _untrusted=yes
    gap "harness negative control failed (host-c=${NEG_HOST:-none} linux-c=${NEG_CLINUX:-none} linux-rust=${NEG_RUST:-none}, each must be 1)"
  fi

  # Rule (a) and the class-table completeness.
  _triggers=$(sed -n 's/^rule-a-triggers: //p' "$T/table.tsv")
  _anom=$(sed -n 's/^anomalies: //p' "$T/table.tsv")
  if [ -z "$_triggers" ]; then
    _triggers=none
    gap "class table has no rule-a-triggers line"
  fi
  [ "${_anom:-missing}" = 0 ] || gap "class table anomalies: ${_anom:-missing}"
  grep -Eq '^I	rustc	rustc [0-9]' "$T/box.out" || gap "the container reported no rustc version"
  [ "${TUT_CLEAN:-no}" = yes ] || gap "the tool under test (tools/lint) differs from the recorded commit"

  # Rule (b) and the fuzz completeness.
  [ "${ADM_CTRL:-not-run}" = ok ] || gap "sanitizer negative control: ${ADM_CTRL:-not-run}"
  _b=no
  if [ "${FZ_RAN:-no}" != yes ]; then
    gap "the registered fuzz did not run"
  else
    if [ "$FZ_RC" != 0 ]; then
      if san_report_in "$T/fz.err" && admission_frame_in "$T/fz.err"; then
        _b=yes
      else
        gap "fuzz exited $FZ_RC with no sanitizer report attributable to admission.c (a harness finding, not a trigger)"
      fi
    elif san_report_in "$T/fz.err"; then
      gap "sanitizer report text present although the fuzz exited 0"
    fi
    grep -qx 'mutations: 2000000' "$T/fz.out" || gap "fuzz output lacks 'mutations: 2000000'"
    grep -qx 'inputs: 2000040' "$T/fz.out" || gap "fuzz output lacks 'inputs: 2000040'"
    _ci=$(grep '^info-consistency: ' "$T/fz.out" | head -n 1)
    _ri=$(grep '^info-consistency: ' "$T/rust.out" 2>/dev/null | head -n 1)
    if [ "$_ci" != "info-consistency: ok" ] || [ "$_ri" != "$_ci" ]; then
      gap "info-consistency C='${_ci:-missing}' Rust='${_ri:-missing}' (both must read ok)"
    fi
  fi

  if [ "$_untrusted" = yes ]; then
    DEC=inconclusive
  elif [ "$_triggers" != none ] || [ "$_b" = yes ]; then
    DEC=switch-rust
  elif [ -z "$GAPS" ]; then
    DEC=stay-c17
  else
    DEC=inconclusive
  fi
  echo "rule-a-triggers: $_triggers"
  echo "rule-b-triggered: $_b"
  echo "completeness-gaps: ${GAPS:-none}"
  echo "mechanical-decision: $DEC"
}

registered_mode() {
  preflight
  new_tmp
  verify_frozen
  DECIDING=yes
  NEG_HOST="" NEG_CLINUX="" NEG_RUST="" ADM_CTRL=not-run FZ_RAN=no RUST_RC=""
  : > "$T/fz.out"
  : > "$T/fz.err"
  : > "$T/rust.out"
  _t0=$(date +%s)
  echo "registered-run-start: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  print_tool_under_test
  run_host_lint "$CLASSES"
  run_container_classes "$CLASSES"
  print_env
  print_class_table > "$T/table.tsv"
  cat "$T/table.tsv"
  echo "negative-controls: host-c=$NEG_HOST linux-c=$NEG_CLINUX linux-rust=$NEG_RUST (each must be 1)"

  echo "== fuzz =="
  if admission_control; then
    ADM_CTRL=ok
    FZ_RAN=yes
    fuzz_build_run ""
  else
    ADM_CTRL=failed
    echo "fuzz: not run, because the sanitizer negative control failed"
  fi
  echo "fuzz-tool: $(fuzz_tool_name)"
  echo "fuzz-flags: $FZ_FLAGS"
  echo "fuzz-exit: $FZ_RC"
  grep -E '^(mutations|inputs|boundary |info-consistency)' "$T/fz.out" || true
  if [ "$FZ_RAN" = yes ] && [ "$FZ_RC" = 0 ]; then
    _san="Sanitizers C: ASan+UBSan clean"
  else
    _fails=$(grep '^FAIL: input ' "$T/fz.err" | head -n 3 | paste -s -d';' -)
    _san="Sanitizers C: not clean or not run: input digests: ${_fails:-none recorded}"
    echo "--- first lines of the fuzz stderr ---"
    head -n 25 "$T/fz.err"
  fi

  echo "== differential =="
  rust_admission differential
  grep -E '^rust-' "$T/rust.out" || true
  [ "$RUST_RC" = 0 ] || echo "note: the Rust differential test exited $RUST_RC (a registered finding, not a trigger)"
  grep -E '^(boundary |mutations:|inputs:|outcomes:|fnv1a64:)' "$T/fz.out" > "$T/c.cmp" || true
  grep -E '^(boundary |mutations:|inputs:|outcomes:|fnv1a64:)' "$T/rust.out" > "$T/r.cmp" || true
  grep -E '^(outcomes|fnv1a64|info-consistency):' "$T/fz.out" || true
  grep -E '^(outcomes|fnv1a64|info-consistency):' "$T/rust.out" || true
  if [ "$RUST_RC" = 0 ] && [ -s "$T/c.cmp" ] && cmp -s "$T/c.cmp" "$T/r.cmp"; then
    _diff=match
  else
    _diff=mismatch
    diff "$T/c.cmp" "$T/r.cmp" | head -n 20 || true
  fi
  echo "Differential: $_diff"
  echo "$_san"
  echo "Sanitizers Rust: not run (stable rustc; -Zsanitizer is nightly-only, per PREREG)"

  echo "== rules =="
  DECIDING=no
  decide
  echo "registered-run-end: $(date -u +%Y-%m-%dT%H:%M:%SZ) duration-seconds: $(($(date +%s) - _t0))"
  [ "$DEC" != inconclusive ] || exit 3
}

# ---------------------------------------------------------------------------
# Decision negative controls (PREREG Amendment 1)
# ---------------------------------------------------------------------------
# Each case starts from a synthetic clean fixture that decides stay-c17, changes
# one fact, and checks the decision that the real decide() and
# print_class_table() produce. No docker, no compiler, no decision data.

# st_fixture: a clean run's intermediate files and variables.
st_fixture() {
  : > "$T/host.tsv"
  : > "$T/box.tsv"
  printf '%s\n' "$CLASSES" | while IFS='|' read -r _n _c _r; do
    if [ "$_n" = control ]; then _e=0; else _e=1; fi
    if [ "$_c" = - ]; then
      printf '%s\tn/a\t-\n' "$_n" >> "$T/host.tsv"
      printf 'R\t%s\t0\t-\n' "$_n" >> "$T/box.tsv"
      continue
    fi
    printf '%s\t%s\tL1-X\n' "$_n" "$_e" >> "$T/host.tsv"
    printf 'C\t%s\t%s\tL1-X\n' "$_n" "$_e" >> "$T/box.tsv"
    printf 'R\t%s\t%s\tE0433\n' "$_n" "$_e" >> "$T/box.tsv"
  done
  {
    cat "$T/box.tsv"
    printf 'NR\t1\nNC\t1\n'
    printf 'I\tcontainer-arch\taarch64\nI\trustc\trustc 1.91.1 (selftest)\n'
  } > "$T/box.out"
  printf 'boundary 0: 3333\nmutations: 2000000\ninputs: 2000040\noutcomes: 1 1 1 1 1 1 1\nfnv1a64: 0000000000000000\ninfo-consistency: ok\n' > "$T/fz.out"
  : > "$T/fz.err"
  printf 'mutations: 2000000\ninputs: 2000040\ninfo-consistency: ok\n' > "$T/rust.out"
  NEG_HOST=1 NEG_CLINUX=1 NEG_RUST=1 ADM_CTRL=ok FZ_RAN=yes FZ_RC=0 RUST_RC=0 TUT_CLEAN=yes
}

# st_edit FILE AWK-PROGRAM: rewrite FILE through awk (portable in-place edit).
st_edit() {
  awk -F'\t' -v OFS='\t' "$2" "$1" > "$1.new" && mv "$1.new" "$1"
}

st_drop_box_row() {
  st_edit "$T/box.tsv" "!(\$1 == \"$1\" && \$2 == \"$2\")"
}

# st_perturb CASE: change one fact of the clean fixture.
st_perturb() {
  case $1 in
    clean) ;;
    fuzz-calloc-exit3)
      FZ_RC=3
      printf 'boundary 0: 3333\n' > "$T/fz.out"
      printf 'harness: allocation of 67108864 bytes failed\n' > "$T/fz.err" ;;
    fuzz-outcome-range-exit4)
      FZ_RC=4
      printf 'boundary 0: 3333\n' > "$T/fz.out"
      printf 'harness: outcome code 9 out of range\n' > "$T/fz.err" ;;
    fuzz-info-bad-exit5)
      FZ_RC=5
      st_edit "$T/fz.out" '/^info-consistency:/ { $0 = "info-consistency: BAD" } { print }' ;;
    fuzz-killed-137)
      FZ_RC=137
      printf 'boundary 0: 3333\n' > "$T/fz.out" ;;
    fuzz-docker-125)
      FZ_RC=125
      : > "$T/fz.out"
      printf 'docker: Error response from daemon\n' > "$T/fz.err" ;;
    fuzz-exit0-short)
      st_edit "$T/fz.out" '/^mutations:/ { $0 = "mutations: 1000" } /^inputs:/ { $0 = "inputs: 1040" } { print }' ;;
    fuzz-exit0-no-inputs-line)
      st_edit "$T/fz.out" '!/^inputs:/' ;;
    fuzz-exit0-no-info-line)
      st_edit "$T/fz.out" '!/^info-consistency:/' ;;
    fuzz-exit0-with-report-text)
      printf 'admission.c:23:5: runtime error: load of address\n' > "$T/fz.err" ;;
    fuzz-not-run)
      FZ_RAN=no
      : > "$T/fz.out" ;;
    rust-differential-failed)
      RUST_RC=101
      : > "$T/rust.out" ;;
    sanitizer-control-failed)
      ADM_CTRL=failed ;;
    lint-exit2-host)
      st_edit "$T/host.tsv" '$1 == "host-time" { $2 = 2 } { print }' ;;
    lint-exit2-linux)
      st_edit "$T/box.tsv" '$1 == "C" && $2 == "file-io" { $3 = 2 } { print }' ;;
    lint-linux-row-missing)
      st_drop_box_row C thread ;;
    rustc-missing)
      st_edit "$T/box.tsv" '$1 == "R" { $3 = 127 } { print }'
      st_edit "$T/box.out" '!($1 == "I" && $2 == "rustc")' ;;
    rust-row-missing)
      st_drop_box_row R tls ;;
    tool-under-test-modified)
      TUT_CLEAN=no ;;
    negative-control-host-lint)
      NEG_HOST=0 ;;
    negative-control-rust)
      NEG_RUST=0 ;;
    report-only-in-harness)
      FZ_RC=1
      st_edit "$T/fz.out" '/^boundary/'
      printf '==1==ERROR: AddressSanitizer: heap-buffer-overflow\n    #0 0x1 in run_one fuzz_admission.c:123\n    #1 0x2 in main fuzz_admission.c:199\n' > "$T/fz.err" ;;
    admission-named-without-report)
      FZ_RC=2
      st_edit "$T/fz.out" '/^boundary/'
      printf 'cc: error: docs/spikes/rust-vs-c/admission/admission.c:1: cannot open\n' > "$T/fz.err" ;;
    rule-b-asan)
      FZ_RC=1
      st_edit "$T/fz.out" '/^boundary/'
      printf '==1==ERROR: AddressSanitizer: heap-buffer-overflow\n    #0 0x1 in t64s_admit admission.c:23\n    #1 0x2 in run_one fuzz_admission.c:123\nFAIL: input phase=mutation index=7 len=4096 fnv1a64=0\n' > "$T/fz.err" ;;
    rule-b-ubsan-linux-path)
      FZ_RC=1
      st_edit "$T/fz.out" '/^boundary/'
      printf '/src/docs/spikes/rust-vs-c/admission/admission.c:28:8: runtime error: load of null pointer\n' > "$T/fz.err" ;;
    rule-a)
      st_edit "$T/host.tsv" '$1 == "host-time" { $2 = 0 } { print }'
      st_edit "$T/box.tsv" '$1 == "C" && $2 == "host-time" { $3 = 0 } { print }' ;;
    rule-a-broken-negative-control)
      st_perturb rule-a
      NEG_CLINUX=0 ;;
    *) die "decision-selftest: unknown case $1" ;;
  esac
}

ST_CASES='clean stay-c17
fuzz-calloc-exit3 inconclusive
fuzz-outcome-range-exit4 inconclusive
fuzz-info-bad-exit5 inconclusive
fuzz-killed-137 inconclusive
fuzz-docker-125 inconclusive
fuzz-exit0-short inconclusive
fuzz-exit0-no-inputs-line inconclusive
fuzz-exit0-no-info-line inconclusive
fuzz-exit0-with-report-text inconclusive
fuzz-not-run inconclusive
rust-differential-failed inconclusive
sanitizer-control-failed inconclusive
lint-exit2-host inconclusive
lint-exit2-linux inconclusive
lint-linux-row-missing inconclusive
rustc-missing inconclusive
rust-row-missing inconclusive
tool-under-test-modified inconclusive
negative-control-host-lint inconclusive
negative-control-rust inconclusive
report-only-in-harness inconclusive
admission-named-without-report inconclusive
rule-b-asan switch-rust
rule-b-ubsan-linux-path switch-rust
rule-a switch-rust
rule-a-broken-negative-control inconclusive'

decision_selftest_mode() {
  new_tmp
  _fail=0
  _n=0
  while read -r _case _want; do
    [ -n "$_case" ] || continue
    _n=$((_n + 1))
    _got=$(
      st_fixture
      st_perturb "$_case"
      print_class_table > "$T/table.tsv"
      decide > "$T/decide.out"
      sed -n 's/^mechanical-decision: //p' "$T/decide.out"
    ) || _got="error"
    if [ "$_got" = "$_want" ]; then
      echo "ok   $_case -> $_got"
    else
      echo "FAIL $_case -> ${_got:-none} (expected $_want)"
      sed 's/^/     /' "$T/decide.out" 2>/dev/null || true
      _fail=$((_fail + 1))
    fi
  done <<EOF
$ST_CASES
EOF
  if [ "$_fail" = 0 ]; then
    echo "decision-selftest: PASS ($_n cases)"
    return 0
  fi
  echo "decision-selftest: FAIL ($_fail of $_n cases)"
  return 1
}

case "${1:-}" in
  --control) control_mode ;;
  --print-hashes) print_hashes ;;
  --verify-frozen) preflight; new_tmp; verify_frozen ;;
  --admission-check) admission_check_mode ;;
  --registered) registered_mode ;;
  --decision-selftest) decision_selftest_mode ;;
  *) usage ;;
esac
