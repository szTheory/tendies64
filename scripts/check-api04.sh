#!/bin/sh
# API-04 on the shipped archive (Phase 11, plan 11-17, DR-19, T-11-31, T-11-32).
# POSIX sh, no bashisms. The determinism lint compiles core sources with its own
# flags, so only the members of the INSTALLED archive show what ships (the
# platform stack protector, position-independent code, -O3). This script
# extracts the members of an archive with ar x into a space-free temporary
# directory and checks them:
#
#   sh scripts/check-api04.sh [--expect FILE] members  --lib LIB
#   sh scripts/check-api04.sh disasm   --lib LIB
#   sh scripts/check-api04.sh headers  --include DIR
#   sh scripts/check-api04.sh seeded   --lib LIB --include DIR
#   sh scripts/check-api04.sh undefined --lib LIB       (print the archive-level set)
#
#   members   tools/lint/lint-objects.sh --allow tools/lint/allowlist-installed.txt
#             --objects over all members in one invocation must be clean, and the
#             archive-level undefined set (undefined minus defined over the members)
#             must equal the rows of docs/abi/installed-undefined.txt for uname -s,
#             so a new import fails even if its name is allowlisted; the same run
#             without the installed allowlist must fail naming exactly the names of
#             the set that the source allowlist does not allow (the control). The
#             archive is extracted in place from its own path, which for the CTest
#             fixture contains a space (RESEARCH Open Question 6); if that fails,
#             the archive is copied into the temporary directory and the fallback
#             is reported.
#   disasm    objdump -d --no-show-raw-insn over every member; no hardware clock or
#             entropy instruction (mrs of CNTVCT/CNTPCT/RNDR, rdtsc, rdtscp, rdrand,
#             rdseed, rdpmc).
#   headers   tools/lint/core-lint.sh --files over every installed header (the
#             L1-INCLUDE allowlist and the other Layer-1 rules).
#   seeded    proves the checks can fail: members plus tools/lint/installed-seeds/
#             seed_host_services.c (clock_gettime, fopen, getentropy, pthread_create)
#             fail with exactly four L2-UNDEF-SYMBOL lines; members plus an object that
#             imports the allowlisted but unrecorded memmove fail the expected-set
#             comparison; members plus seed_hw_counter.c fail the disassembly scan;
#             an include directory plus seed_banned_include.h fails with
#             L1-INCLUDE-BANNED. Exit 0 only when every seeded run failed as expected.
#
# Environment: CC (default cc) compiles the seeds; NM, OBJDUMP and AR override the
# tools. Exit codes: 0 pass, 1 a finding, 2 usage error or a missing tool.
set -eu
LC_ALL=C
export LC_ALL

ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
LINT=$ROOT/tools/lint/lint-objects.sh
CORELINT=$ROOT/tools/lint/core-lint.sh
ALLOW_SRC=$ROOT/tools/lint/allowlist.txt
ALLOW_INST=$ROOT/tools/lint/allowlist-installed.txt
EXPECT=$ROOT/docs/abi/installed-undefined.txt
SEEDS=$ROOT/tools/lint/installed-seeds
OS=$(uname -s)
NM=${NM:-nm}
AR=${AR:-ar}
OBJDUMP=${OBJDUMP:-objdump}
NFIND=0
FINDING_TAG=FINDING

usage() {
  echo "usage: sh scripts/check-api04.sh [--expect FILE] members|disasm|undefined --lib LIB | headers --include DIR | seeded --lib LIB --include DIR" >&2
  exit 2
}

die() {
  echo "check-api04: $*" >&2
  exit 2
}

finding() {
  echo "check-api04: $FINDING_TAG: $*"
  NFIND=$((NFIND + 1))
}

LIB=
INC=
while [ "$#" -gt 0 ]; do
  case $1 in
    --expect) [ "$#" -ge 2 ] || usage; EXPECT=$2; shift 2 ;;
    --*) usage ;;
    *) break ;;
  esac
done
[ "$#" -ge 1 ] || usage
SUB=$1
shift
while [ "$#" -gt 0 ]; do
  case $1 in
    --lib) [ "$#" -ge 2 ] || usage; LIB=$2; shift 2 ;;
    --include) [ "$#" -ge 2 ] || usage; INC=$2; shift 2 ;;
    *) usage ;;
  esac
done

for _tool in "$NM" "$AR" awk sort comm; do
  command -v "$_tool" >/dev/null 2>&1 || die "required tool not found: $_tool"
done

TMP=$(mktemp -d "${TMPDIR:-/tmp}/t64api04.XXXXXX")
trap 'rm -rf "$TMP"' EXIT
trap 'exit 2' INT TERM HUP
case $TMP in
  *" "*) die "the temporary directory path contains a space: $TMP (set TMPDIR)" ;;
esac

# abs_file PATH: the absolute path of an existing file (it may contain spaces).
abs_file() {
  [ -f "$1" ] || die "file not found: $1"
  printf '%s/%s\n' "$(cd "$(dirname "$1")" && pwd -P)" "$(basename "$1")"
}

# tidy: strip the temporary directory from the lines on stdin, so logs name
# members and not scratch paths.
tidy() {
  sed "s|$TMP/[A-Za-z0-9_.-]*/||g; s|$TMP/||g"
}

# extract_members LIB DEST: ar x of the archive into DEST, in place from its own
# path first (Q6), from a copy when that fails or yields a different member
# count. Sets ROUTE and NMEMBERS.
extract_members() {
  _lib=$(abs_file "$1")
  _dest=$2
  mkdir -p "$_dest"
  "$AR" t "$_lib" | grep -v '^__\.SYMDEF' > "$TMP/ar.members" || true
  _want=$(wc -l < "$TMP/ar.members" | tr -d ' ')
  [ "$_want" -gt 0 ] || die "the archive lists no members: $_lib"
  ROUTE=in-place
  if (cd "$_dest" && "$AR" x "$_lib") > "$TMP/ar.out" 2>&1; then
    NMEMBERS=$(find "$_dest" -maxdepth 1 -type f -name '*.o' | wc -l | tr -d ' ')
  else
    NMEMBERS=0
  fi
  if [ "$NMEMBERS" -ne "$_want" ]; then
    echo "check-api04: Q6: ar x from the archive path extracted $NMEMBERS of $_want members ($(tr '\n' ' ' < "$TMP/ar.out")); extracting from a copy"
    find "$_dest" -maxdepth 1 -type f -exec rm -f {} +
    cp "$_lib" "$TMP/copy.a"
    ROUTE=copy
    (cd "$_dest" && "$AR" x "$TMP/copy.a") || die "ar x failed on the copy of the archive"
    NMEMBERS=$(find "$_dest" -maxdepth 1 -type f -name '*.o' | wc -l | tr -d ' ')
    [ "$NMEMBERS" -eq "$_want" ] || die "ar x extracted $NMEMBERS of $_want members"
  fi
  case $_lib in
    *" "*) SPACED=yes ;;
    *) SPACED=no ;;
  esac
}

# name_rows FILE KEYCOL OSCOL: names of FILE for the running OS (any or OS).
allow_names() {
  awk -F'|' -v os="$OS" '
    /^[ \t]*#/ || /^[ \t]*$/ { next }
    NF == 4 { n = $1; o = $4; gsub(/[ \t]/, "", n); gsub(/[ \t]/, "", o)
              if (o == "any" || o == os) print n }' "$1" | sort -u
}

# recorded_set: the rows of the expectation file for the running OS.
recorded_set() {
  [ -f "$EXPECT" ] || die "expectation file not found: $EXPECT"
  awk -F'|' -v os="$OS" '
    /^[ \t]*#/ || /^[ \t]*$/ { next }
    NF >= 2 && $1 == os { print $2 }' "$EXPECT" | sort -u
}

# undefined_set DIR OUT: the archive-level undefined set of the objects of DIR
# (undefined minus defined, Darwin underscore stripped), sorted, into OUT.
undefined_set() {
  _out=$2
  set -- "$1"/*.o
  [ -e "$1" ] || die "no member objects in the extraction directory"
  : > "$TMP/und.raw"
  : > "$TMP/def.raw"
  "$NM" -P "$@" 2>/dev/null | awk -v os="$OS" -v und="$TMP/und.raw" -v def="$TMP/def.raw" '
    NF >= 2 && $2 ~ /^[A-Za-z]$/ {
      n = $1
      if (os == "Darwin") sub(/^_/, "", n)
      t = $2
      if (t == "U" || t == "w" || t == "v") print n >> und
      else if (t ~ /^[A-Z]$/ && t != "C") print n >> def
    }'
  sort -u "$TMP/und.raw" > "$TMP/und.s"
  sort -u "$TMP/def.raw" > "$TMP/def.s"
  comm -23 "$TMP/und.s" "$TMP/def.s" > "$_out"
}

# compare_set DIR: findings for a difference between the archive-level undefined
# set of DIR and the recorded rows of the running OS.
compare_set() {
  recorded_set > "$TMP/rec.s"
  [ -s "$TMP/rec.s" ] || die "no recorded undefined set for $OS in $EXPECT"
  undefined_set "$1" "$TMP/und.set"
  comm -13 "$TMP/rec.s" "$TMP/und.set" > "$TMP/new.imports"
  comm -23 "$TMP/rec.s" "$TMP/und.set" > "$TMP/gone.imports"
  while IFS= read -r _n; do
    finding "unrecorded import: $_n is in the archive-level undefined set but not in the recorded $OS rows"
  done < "$TMP/new.imports"
  while IFS= read -r _n; do
    finding "recorded import gone: $_n is a recorded $OS row but the archive no longer imports it"
  done < "$TMP/gone.imports"
}

# lint_members DIR ALLOW|none OUT: run lint-objects over every member of DIR in
# one invocation, with the installed allowlist or without; sets LINT_RC.
lint_members() {
  _allow=$2
  _out=$3
  set -- "$1"/*.o
  [ -e "$1" ] || die "no member objects in the extraction directory"
  LINT_RC=0
  if [ "$_allow" = none ]; then
    sh "$LINT" --objects "$@" > "$_out" 2>&1 || LINT_RC=$?
  else
    sh "$LINT" --allow "$_allow" --objects "$@" > "$_out" 2>&1 || LINT_RC=$?
  fi
}

# lint_members_bg DIR ALLOW|none OUT RCFILE: lint_members for a background job;
# the exit status goes to RCFILE (a background job cannot set LINT_RC).
lint_members_bg() {
  _bg_rc=0
  lint_members "$1" "$2" "$3" || true
  echo "$LINT_RC" > "$4"
}

# undef_lines FILE: the L2-UNDEF-SYMBOL lines of a lint output.
undef_lines() {
  grep -c '^L2-UNDEF-SYMBOL ' "$1" || true
}

# undef_names FILE: the distinct symbol names of those lines.
undef_names() {
  sed -n "s/^L2-UNDEF-SYMBOL .*undefined symbol '\\([^']*\\)'.*/\\1/p" "$1" | sort -u
}

finish() {
  if [ "$NFIND" -gt 0 ]; then
    echo "check-api04: $SUB findings=$NFIND"
    exit 1
  fi
  echo "check-api04: $SUB ok"
}

cmd_members() {
  [ -n "$LIB" ] || usage
  [ -f "$ALLOW_INST" ] || die "installed allowlist not found: $ALLOW_INST"
  extract_members "$LIB" "$TMP/members"
  echo "check-api04: members extracted=$NMEMBERS route=$ROUTE archive-path-has-space=$SPACED"
  # The two lint runs (with the installed allowlist, and the control without it)
  # are independent, so they run side by side.
  lint_members_bg "$TMP/members" "$ALLOW_INST" "$TMP/lint.out" "$TMP/lint.rc" &
  _bg1=$!
  lint_members_bg "$TMP/members" none "$TMP/lint.none" "$TMP/lint.none.rc" &
  _bg2=$!
  compare_set "$TMP/members"
  wait "$_bg1" "$_bg2" || true
  LINT_RC=$(cat "$TMP/lint.rc")
  if [ "$LINT_RC" -ne 0 ]; then
    tidy < "$TMP/lint.out" | sed 's/ (hint: .*$//' | sort | uniq -c | sort -rn | head -40
    finding "lint-objects --allow tools/lint/allowlist-installed.txt --objects exited $LINT_RC over the $NMEMBERS installed members ($(undef_lines "$TMP/lint.out") L2-UNDEF-SYMBOL lines: $(undef_names "$TMP/lint.out" | tr '\n' ' '))"
  fi
  echo "check-api04: members archive-level undefined set ($OS): $(tr '\n' ' ' < "$TMP/und.set")"
  # Control: without the installed allowlist the same run must fail and must
  # name exactly the names of the set that the source allowlist lacks.
  allow_names "$ALLOW_SRC" > "$TMP/src.allow"
  comm -23 "$TMP/und.set" "$TMP/src.allow" > "$TMP/need.installed"
  LINT_RC=$(cat "$TMP/lint.none.rc")
  undef_names "$TMP/lint.none" > "$TMP/flagged"
  if [ -s "$TMP/need.installed" ]; then
    [ "$LINT_RC" -eq 1 ] || finding "control: without the installed allowlist the lint exited $LINT_RC, not 1"
  else
    [ "$LINT_RC" -eq 0 ] || finding "control: the set needs no installed allowlist row yet the lint exited $LINT_RC"
  fi
  if ! cmp -s "$TMP/need.installed" "$TMP/flagged"; then
    finding "control: without the installed allowlist the lint named [$(tr '\n' ' ' < "$TMP/flagged")] but the undefined set minus the source allowlist is [$(tr '\n' ' ' < "$TMP/need.installed")]"
  else
    echo "check-api04: members control: without the installed allowlist the lint names exactly [$(tr '\n' ' ' < "$TMP/flagged")] ($(undef_lines "$TMP/lint.none") L2-UNDEF-SYMBOL lines)"
  fi
  finish
}

cmd_undefined() {
  [ -n "$LIB" ] || usage
  extract_members "$LIB" "$TMP/members"
  undefined_set "$TMP/members" "$TMP/und.set"
  cat "$TMP/und.set"
}

# disasm_findings DIR: findings of the disassembly scan over the objects of DIR.
disasm_findings() {
  command -v "$OBJDUMP" >/dev/null 2>&1 || die "required tool not found: $OBJDUMP"
  set -- "$1"/*.o
  [ -e "$1" ] || die "no member objects in the extraction directory"
  "$OBJDUMP" -d --no-show-raw-insn "$@" > "$TMP/disasm.txt" 2> "$TMP/disasm.err" ||
    die "objdump failed: $(head -n 3 "$TMP/disasm.err" | tr '\n' ' ')"
  _insns=$(grep -c -E '^[[:space:]]+[0-9a-f]+:' "$TMP/disasm.txt" || true)
  if [ "$_insns" -lt 1000 ]; then
    finding "the disassembly holds only $_insns instruction lines (an empty scan proves nothing)"
  fi
  if grep -i -n -E '[[:space:]](mrs[[:space:]]+[a-z0-9]+,[[:space:]]*(cntvct|cntpct|rndr)|(rdtsc|rdtscp|rdrand|rdseed|rdpmc)([^a-z0-9_]|$))' "$TMP/disasm.txt" > "$TMP/disasm.hits"; then
    while IFS= read -r _hit; do
      finding "hardware clock or entropy instruction: $(printf '%s' "$_hit" | tr '\t' ' ' | sed 's/  */ /g')"
    done < "$TMP/disasm.hits"
  fi
  DISASM_INSNS=$_insns
}

cmd_disasm() {
  [ -n "$LIB" ] || usage
  extract_members "$LIB" "$TMP/members"
  disasm_findings "$TMP/members"
  echo "check-api04: disasm members=$NMEMBERS instruction-lines=$DISASM_INSNS"
  finish
}

# header_findings DIR: findings of the Layer-1 scan of every header below DIR.
header_findings() {
  [ -d "$1" ] || die "include directory not found: $1"
  find "$1" -type f -name '*.h' | sort > "$TMP/headers.list"
  [ -s "$TMP/headers.list" ] || die "no header below $1"
  NHEADERS=0
  while IFS= read -r _h; do
    NHEADERS=$((NHEADERS + 1))
    _rc=0
    sh "$CORELINT" --files "$_h" > "$TMP/hdr.out" 2>&1 || _rc=$?
    if [ "$_rc" -ne 0 ]; then
      sed 's| [^ ]*/\([^/ :]*:[0-9]*:\)| \1|; s/ (hint: .*$//' "$TMP/hdr.out"
      finding "core-lint --files reported violations (exit $_rc) for an installed header"
    fi
  done < "$TMP/headers.list"
}

cmd_headers() {
  [ -n "$INC" ] || usage
  header_findings "$INC"
  echo "check-api04: headers scanned=$NHEADERS"
  finish
}

# compile_seed SOURCE OUT: compile one seed with the shipped-style flags.
compile_seed() {
  command -v "${CC:-cc}" >/dev/null 2>&1 || die "compiler not found: ${CC:-cc}"
  ${CC:-cc} -c -O0 -fno-stack-protector "$1" -o "$2" 2> "$TMP/seed.cc.err" ||
    { cat "$TMP/seed.cc.err" >&2; die "the seed does not compile: $1"; }
}

SEED_FAILED=0
# seed_missed NAME DETAIL: record a seeded run that did not fail as expected.
seed_missed() {
  SEED_FAILED=$((SEED_FAILED + 1))
  echo "check-api04: seeded $1: NOT DETECTED ($2)"
}

cmd_seeded() {
  [ -n "$LIB" ] && [ -n "$INC" ] || usage
  [ -d "$SEEDS" ] || die "seed directory not found: $SEEDS"
  extract_members "$LIB" "$TMP/members"
  # Findings printed below belong to seeded violations that must be detected.
  FINDING_TAG="expected finding of a seeded run"
  # The clean members are the members check's own subject (api04.members); a
  # seeded run below states the exact expected output, so it cannot pass vacuously.

  # 1. host-service imports: exactly four L2-UNDEF-SYMBOL lines.
  mkdir "$TMP/seed1"
  cp "$TMP/members"/*.o "$TMP/seed1/"
  compile_seed "$SEEDS/seed_host_services.c" "$TMP/seed1/seed_host_services.o"
  lint_members "$TMP/seed1" "$ALLOW_INST" "$TMP/lint.seed1"
  _n=$(undef_lines "$TMP/lint.seed1")
  _names=$(undef_names "$TMP/lint.seed1" | tr '\n' ' ')
  tidy < "$TMP/lint.seed1" | sed 's/ (hint: .*$//'
  if [ "$LINT_RC" -eq 1 ] && [ "$_n" -eq 4 ] && [ "$_names" = "clock_gettime fopen getentropy pthread_create " ]; then
    echo "check-api04: seeded host services: detected, 4 L2-UNDEF-SYMBOL lines naming $_names"
  else
    seed_missed "host services" "lint exit $LINT_RC, $_n lines, names: $_names"
  fi

  # 2. an allowlisted but unrecorded import fails the expected-set comparison.
  mkdir "$TMP/seed2" "$TMP/seed2only"
  cp "$TMP/members"/*.o "$TMP/seed2/"
  printf '%s\n' 'extern void *memmove(void *, const void *, unsigned long);' \
    'void *t64_seed_unrecorded_import(void *d, const void *s, unsigned long n) { return memmove(d, s, n); }' \
    > "$TMP/seed_new_import.c"
  compile_seed "$TMP/seed_new_import.c" "$TMP/seed2/seed_new_import.o"
  cp "$TMP/seed2/seed_new_import.o" "$TMP/seed2only/"
  lint_members "$TMP/seed2only" "$ALLOW_INST" "$TMP/lint.seed2"
  _seed_rc=$LINT_RC
  NFIND=0
  compare_set "$TMP/seed2"
  if [ "$_seed_rc" -eq 0 ] && [ "$NFIND" -eq 1 ] && grep -q '^memmove$' "$TMP/new.imports"; then
    echo "check-api04: seeded unrecorded import: detected (the lint passes it: memmove is on the source allowlist; the expected-set comparison fails it)"
  else
    seed_missed "unrecorded import" "lint exit $_seed_rc, $NFIND set findings"
  fi
  NFIND=0

  # 3. a hardware counter read fails the disassembly scan.
  mkdir "$TMP/seed3"
  cp "$TMP/members"/*.o "$TMP/seed3/"
  compile_seed "$SEEDS/seed_hw_counter.c" "$TMP/seed3/seed_hw_counter.o"
  NFIND=0
  disasm_findings "$TMP/seed3" > "$TMP/disasm.seed3" 2>&1 || true
  _hits=$(grep -c 'hardware clock or entropy instruction' "$TMP/disasm.seed3" || true)
  grep 'hardware clock or entropy instruction' "$TMP/disasm.seed3" | tidy || true
  if [ "$_hits" -ge 1 ]; then
    echo "check-api04: seeded hardware counter: detected ($_hits hit)"
  else
    seed_missed "hardware counter" "no instruction named"
  fi
  NFIND=0

  # 4. a banned include in an installed header fails the Layer-1 scan.
  mkdir -p "$TMP/seed4"
  cp -R "$INC"/. "$TMP/seed4/"
  mkdir -p "$TMP/seed4/tendies64"
  cp "$SEEDS/seed_banned_include.h" "$TMP/seed4/tendies64/seed_banned_include.h"
  NFIND=0
  header_findings "$TMP/seed4" > "$TMP/hdr.seed4" 2>&1 || true
  cat "$TMP/hdr.seed4" | tidy
  if grep -q '^L1-INCLUDE-BANNED ' "$TMP/hdr.seed4"; then
    echo "check-api04: seeded banned include: detected (L1-INCLUDE-BANNED)"
  else
    seed_missed "banned include" "no L1-INCLUDE-BANNED line"
  fi
  NFIND=0

  if [ "$SEED_FAILED" -ne 0 ]; then
    echo "check-api04: seeded findings=$SEED_FAILED (a seeded violation passed)"
    exit 1
  fi
  echo "check-api04: seeded ok (4 of 4 seeded violations detected)"
}

case $SUB in
  members) cmd_members ;;
  undefined) cmd_undefined ;;
  disasm) cmd_disasm ;;
  headers) cmd_headers ;;
  seeded) cmd_seeded ;;
  *) usage ;;
esac
