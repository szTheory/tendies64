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
#   run-spike.sh --registered     (completed by the registration task)
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

die() {
  echo "run-spike: $*" >&2
  exit 2
}

usage() {
  echo "usage: run-spike.sh --control | --print-hashes | --registered" >&2
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
        if (rr != "0" && rr != "1") { note = note " ANOMALY(rust exit " rr ")"; anom++ }
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

case "${1:-}" in
  --control) control_mode ;;
  --print-hashes) print_hashes ;;
  --registered)
    [ -f "$SPIKE/PREREG.md" ] || die "PREREG.md is missing: the registered run is refused until the spike is pre-registered"
    die "the registered run is completed by the registration task"
    ;;
  *) usage ;;
esac
