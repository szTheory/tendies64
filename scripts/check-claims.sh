#!/bin/sh
# Claims gate (GOV-04, CONTEXT A-4 and A-5). POSIX sh + portable awk.
#
#   check-claims.sh [--root DIR] [--files F...] [--selftest]
#
# Validates every claim block in Markdown files against docs/claims/TEMPLATE.md.
# A claim block opens with a line that is exactly three backticks followed by
# t64-claim and closes with a line of exactly three backticks; one "key: value"
# per line. Blocks fenced t64-claim-example are documentation and are skipped.
#
# Default input: every tracked or untracked-not-ignored *.md outside .planning/
# and outside tests/seeded/claims/ (intentionally invalid seeds, driven one at
# a time by the gov.claims.seeds CTest test). Paths are relative to the
# repository root (--root overrides it, for the selftest).
#
# Rules (stable ids), printed as "RULE <id> path:line: message":
#   C-BLOCK-SYNTAX        malformed fence, unterminated block, line that is not key: value, duplicate key
#   C-FIELD-MISSING       a required or tier-conditional key is absent or empty
#   C-FIELD-UNKNOWN       a key outside the schema, or a conditional key on the wrong tier
#   C-TIER-ENUM           tier is not one of the six claimable tiers
#   C-SYNTHETIC           tier synthetic, profile synthetic-stub, or a build ending +synthetic
#   C-ID-FORMAT           build_id, profile_id, manifest_digest or a count has the wrong shape
#   C-COUNTS              pass > total, or the seven counts do not sum to total
#   C-ORACLE              oracle_kind or oracle_revision is empty
#   C-STATEMENT           statement differs from the rendering of the other keys
#   C-RESERVED-WORD       accurate, compatible or supports outside hardware-derived with a scope,
#                         or on a prose line that starts with "Tendies64 "
#   C-EVIDENCE-SYNTHETIC  the evidence suite holds a synthetic result (via t64-result-check identity)
#   C-EVIDENCE-MISMATCH   evidence profile, build or pass count differs from the claim
#   C-EVIDENCE-INVALID    evidence path unsafe or not a regular file, or the suite is not valid
#   C-ZERO-INPUT          no files were scanned
# Summary: "claims: files=N blocks=B errors=E", then "ok" when E is 0.
# Exit 0 clean, 1 findings, 2 harness error (missing tool, unreadable file, usage).
# The evidence tool is ${T64_BUILD_DIR:-build/gate}/runner/t64-result-check.
set -eu
LC_ALL=C
export LC_ALL

ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
SELF="$ROOT/scripts/check-claims.sh"
TAB=$(printf '\t')

BUILD=${T64_BUILD_DIR:-build/gate}
case $BUILD in
  /*) ;;
  *) BUILD="$ROOT/$BUILD" ;;
esac
CHECK="$BUILD/runner/t64-result-check"

TMP=""
cleanup() {
  if [ -n "$TMP" ] && [ -d "$TMP" ]; then
    rm -rf "$TMP"
  fi
}
trap cleanup EXIT
trap 'cleanup; exit 130' INT TERM HUP

fatal() {
  echo "check-claims: error: $*" >&2
  exit 2
}

usage() {
  echo "usage: check-claims.sh [--root DIR] [--files F...] [--selftest]" >&2
  exit 2
}

# The awk program: all block parsing and validation. It reads the file list from
# LIST, writes RULE lines to REP, evidence requests to EVF, counts to STAT and
# unreadable-file notes to HARN.
AWK_PROG='
function esc(s) {
  gsub(/[^ -~]/, "?", s)
  if (length(s) > 100) s = substr(s, 1, 97) "..."
  return s
}
function emit(id, n, msg) {
  printf "RULE %s %s:%d: %s\n", id, CUR, n, msg > REP
  errs++
}
function has_reserved(s,   t) {
  t = tolower(s)
  return (t ~ /(^|[^a-z0-9_])(accurate|compatible|supports)($|[^a-z0-9_])/)
}
function phrase(t) {
  if (t == "smoke") return "smoke-tested"
  if (t == "regression-pinned") return "regression-pinned"
  if (t == "unit-tested") return "unit-tested"
  if (t == "self-checking-fixture") return "passes self-checking fixtures"
  if (t == "emulator-differential") return "agrees with " V["emulator"] " " V["emulator_revision"]
  if (t == "hardware-derived") return "matches hardware-derived expectations within " V["scope"]
  return ""
}
function render(t,   s) {
  s = "Tendies64 " V["build_id"] " on profile " V["profile_id"] ": " phrase(t) " for " V["fixture_set"] " @ " V["manifest_digest"]
  s = s " (" V["pass"] "/" V["total"] "; missing " V["missing"] ", skipped " V["skipped"] ", timeout " V["timeout"]
  s = s ", harness-error " V["harness_error"] ", guest-fail " V["guest_fail"] ", boot-fail " V["boot_fail"] ")."
  s = s " Oracle: " V["oracle_kind"] " " V["oracle_revision"] ". Not claimed: " V["not_claimed"] "."
  return s
}
function evid_bad(p,   n, parts, i) {
  if (p == "") return "empty evidence path"
  if (p !~ "^[A-Za-z0-9._/-]+$") return "evidence path has characters outside A-Za-z0-9._/-"
  if (substr(p, 1, 1) == "/") return "evidence path is absolute"
  n = split(p, parts, "/")
  for (i = 1; i <= n; i++) {
    if (parts[i] == "") return "evidence path has an empty segment"
    if (parts[i] == "." || parts[i] == "..") return "evidence path has a dot segment"
  }
  return ""
}
function need(k) {
  if (!(k in SEEN)) {
    emit("C-FIELD-MISSING", bstart, "tier " V["tier"] " needs key " k)
    bad_cond = 1
  } else if (V[k] == "") {
    emit("C-FIELD-MISSING", L[k], "empty value for " k)
    bad_cond = 1
  }
}
function end_block(   i, k, tier, claimable, req_ok, cok, sum, hw_scope, why, p, b, v) {
  tier = V["tier"]
  req_ok = 1
  for (i = 1; i <= NREQ; i++) {
    k = REQ[i]
    if (!(k in SEEN)) {
      emit("C-FIELD-MISSING", bstart, "missing key " k)
      req_ok = 0
    } else if (V[k] == "") {
      if (k == "oracle_kind" || k == "oracle_revision") {
        emit("C-ORACLE", L[k], "empty value for " k)
      } else {
        emit("C-FIELD-MISSING", L[k], "empty value for " k)
      }
      req_ok = 0
    }
  }
  claimable = 0
  if (("tier" in SEEN) && tier != "") {
    if (tier == "synthetic") {
      emit("C-SYNTHETIC", L["tier"], "tier synthetic is never claimable")
    } else if (tier in TIERS) {
      claimable = 1
    } else {
      emit("C-TIER-ENUM", L["tier"], "unknown tier " esc(tier))
    }
  }
  bad_cond = 0
  if (claimable) {
    if (tier == "emulator-differential") { need("emulator"); need("emulator_revision") }
    if (tier == "hardware-derived") { need("scope"); need("capture_provenance") }
    if (tier != "emulator-differential") {
      if ("emulator" in SEEN) emit("C-FIELD-UNKNOWN", L["emulator"], "key emulator is only valid for tier emulator-differential")
      if ("emulator_revision" in SEEN) emit("C-FIELD-UNKNOWN", L["emulator_revision"], "key emulator_revision is only valid for tier emulator-differential")
    }
    if (tier != "hardware-derived") {
      if ("scope" in SEEN) emit("C-FIELD-UNKNOWN", L["scope"], "key scope is only valid for tier hardware-derived")
      if ("capture_provenance" in SEEN) emit("C-FIELD-UNKNOWN", L["capture_provenance"], "key capture_provenance is only valid for tier hardware-derived")
    }
  }
  if (("build_id" in SEEN) && V["build_id"] != "") {
    b = V["build_id"]
    if (b !~ /^[0-9]+\.[0-9]+\.[0-9]+(\+[0-9a-z.-]+)?$/) emit("C-ID-FORMAT", L["build_id"], "build_id is not MAJOR.MINOR.PATCH[+metadata]: " esc(b))
    if (b ~ /\+synthetic$/) emit("C-SYNTHETIC", L["build_id"], "a build ending +synthetic is never claimable")
  }
  if (("profile_id" in SEEN) && V["profile_id"] != "") {
    p = V["profile_id"]
    if (p !~ /^[a-z0-9][a-z0-9-]*$/ || length(p) > 64) emit("C-ID-FORMAT", L["profile_id"], "profile_id is not [a-z0-9][a-z0-9-]* of at most 64 characters: " esc(p))
    if (p == "synthetic-stub") emit("C-SYNTHETIC", L["profile_id"], "profile synthetic-stub is never claimable")
  }
  if (("manifest_digest" in SEEN) && V["manifest_digest"] != "") {
    v = V["manifest_digest"]
    if (v !~ /^sha256:[0-9a-f]+$/ || length(v) != 71) emit("C-ID-FORMAT", L["manifest_digest"], "manifest_digest is not sha256: and 64 lowercase hex digits: " esc(v))
  }
  cok = 1
  for (i = 1; i <= NCNT; i++) {
    k = CNT[i]
    if ((k in SEEN) && V[k] != "") {
      if (V[k] !~ /^[0-9]+$/ || length(V[k]) > 12) {
        emit("C-ID-FORMAT", L[k], "count " k " is not a non-negative integer: " esc(V[k]))
        cok = 0
      }
    } else {
      cok = 0
    }
  }
  if (cok) {
    sum = V["pass"] + V["guest_fail"] + V["boot_fail"] + V["timeout"] + V["missing"] + V["harness_error"] + V["skipped"]
    if (V["pass"] + 0 > V["total"] + 0) {
      emit("C-COUNTS", L["pass"], "pass " V["pass"] " is greater than total " V["total"])
    } else if (sum != V["total"] + 0) {
      emit("C-COUNTS", L["total"], "counts sum to " sum " but total is " V["total"])
    }
  }
  hw_scope = (claimable && tier == "hardware-derived" && ("scope" in SEEN) && V["scope"] != "")
  if (!hw_scope) {
    for (i = 1; i <= BN; i++) {
      if (has_reserved(BL[i])) emit("C-RESERVED-WORD", BLN[i], "accurate, compatible and supports are reserved for hardware-derived claims with a scope")
    }
  }
  if (req_ok && claimable && !bad_cond) {
    want = render(tier)
    if (V["statement"] != want) {
      emit("C-STATEMENT", L["statement"], "statement differs from the rendering of the other keys")
      printf "  expected: %s\n", esc(want) > REP
    }
  }
  if ("evidence" in SEEN) {
    why = evid_bad(V["evidence"])
    if (why != "") {
      emit("C-EVIDENCE-INVALID", L["evidence"], why)
    } else {
      p = V["profile_id"]; b = V["build_id"]; v = V["pass"]
      gsub(/[\t]/, " ", p); gsub(/[\t]/, " ", b)
      if (p == "") p = "-"
      if (b == "") b = "-"
      if (v !~ /^[0-9]+$/) v = "-"
      printf "%s\t%d\t%s\t%s\t%s\t%s\n", CUR, L["evidence"], V["evidence"], p, b, v > EVF
    }
  }
}
function prose(s, n,   t) {
  t = s
  sub(/^[ \t>*+-]+/, "", t)
  if (substr(t, 1, 10) == "Tendies64 " && has_reserved(t)) {
    emit("C-RESERVED-WORD", n, "reserved word in a statement that starts with the project name, outside a claim block")
  }
}
function handle(s, n,   k, v) {
  if (state == 1) {
    if (s == "```") { end_block(); state = 0; return }
    BN++; BL[BN] = s; BLN[BN] = n
    if (substr(s, 1, 3) == "```") { emit("C-BLOCK-SYNTAX", n, "fence line inside a t64-claim block"); return }
    if (s !~ /^[a-z][a-z0-9_]*(: .*|:)$/) { emit("C-BLOCK-SYNTAX", n, "line is not key: value"); return }
    k = s; sub(/:.*$/, "", k)
    v = ""
    if (length(s) > length(k) + 2) v = substr(s, length(k) + 3)
    if (k in SEEN) { emit("C-BLOCK-SYNTAX", n, "duplicate key " k); return }
    SEEN[k] = 1; V[k] = v; L[k] = n
    if (!(k in KNOWN)) emit("C-FIELD-UNKNOWN", n, "unknown key " k)
    return
  }
  if (state == 2) { if (s ~ /^```[ \t]*$/) state = 0; return }
  if (state == 3) {
    if (s ~ /^```[ \t]*$/) state = 0
    else prose(s, n)
    return
  }
  if (s == "```t64-claim") {
    state = 1; bstart = n; BN = 0; nblocks++
    split("", V); split("", SEEN); split("", L)
    return
  }
  if (s == "```t64-claim-example") { state = 2; fstart = n; return }
  if (substr(s, 1, 12) == "```t64-claim") {
    emit("C-BLOCK-SYNTAX", n, "malformed t64-claim fence: " esc(s))
    state = 3; fstart = n
    return
  }
  if (substr(s, 1, 3) == "```") { state = 3; fstart = n; return }
  prose(s, n)
}
BEGIN {
  NREQ = split("build_id profile_id tier fixture_set manifest_digest pass total guest_fail boot_fail timeout missing harness_error skipped oracle_kind oracle_revision not_claimed statement", REQ, " ")
  NCNT = split("pass total guest_fail boot_fail timeout missing harness_error skipped", CNT, " ")
  for (i = 1; i <= NREQ; i++) KNOWN[REQ[i]] = 1
  KNOWN["emulator"] = 1; KNOWN["emulator_revision"] = 1; KNOWN["scope"] = 1
  KNOWN["capture_provenance"] = 1; KNOWN["evidence"] = 1
  n = split("smoke regression-pinned unit-tested self-checking-fixture emulator-differential hardware-derived", T, " ")
  for (i = 1; i <= n; i++) TIERS[T[i]] = 1
  nfiles = 0
  while ((r = (getline f < LIST)) > 0) FILES[++nfiles] = f
  close(LIST)
  nblocks = 0; errs = 0
  for (fi = 1; fi <= nfiles; fi++) {
    CUR = FILES[fi]
    state = 0; ln = 0
    while ((r = (getline line < CUR)) > 0) { ln++; handle(line, ln) }
    if (r < 0) print "cannot read " CUR > HARN
    close(CUR)
    if (state == 1) emit("C-BLOCK-SYNTAX", bstart, "unterminated t64-claim block")
    else if (state == 2 || state == 3) emit("C-BLOCK-SYNTAX", fstart, "unterminated code fence")
  }
  printf "files=%d blocks=%d\n", nfiles, nblocks > STAT
  close(STAT); close(REP); close(EVF); close(HARN)
}
'

# rep_rule ID FILE LINE MESSAGE: one finding from the evidence stage.
rep_rule() {
  printf 'RULE %s %s:%s: %s\n' "$1" "$2" "$3" "$4" >> "$REP"
}

clean() {
  printf '%s' "$1" | tr -c '[:print:]' '?'
}

# evidence_check FILE LINE PATH PROFILE BUILD PASS: the checks that need the C tool.
evidence_check() {
  _f=$1
  _ln=$2
  _p=$3
  _prof=$4
  _bld=$5
  _pas=$6
  _acc=""
  _bad=""
  _ifs=$IFS
  IFS=/
  set -f
  for _c in $_p; do
    if [ -z "$_acc" ]; then _acc=$_c; else _acc="$_acc/$_c"; fi
    if [ -L "$_acc" ]; then
      _bad="evidence path passes through a symlink: $_acc"
      break
    fi
  done
  set +f
  IFS=$_ifs
  if [ -z "$_bad" ] && [ ! -f "$_p" ]; then
    _bad="evidence path is not a regular file: $_p"
  fi
  if [ -n "$_bad" ]; then
    rep_rule C-EVIDENCE-INVALID "$_f" "$_ln" "$_bad"
    return 0
  fi
  _rc=0
  "$CHECK" identity "$_p" > "$TMP/id.out" 2> "$TMP/id.err" < /dev/null || _rc=$?
  if [ "$_rc" -ne 0 ]; then
    _why=$(sed -n 1p "$TMP/id.err")
    rep_rule C-EVIDENCE-INVALID "$_f" "$_ln" "t64-result-check identity refused $_p (exit $_rc): $(clean "$_why")"
    return 0
  fi
  tail -n 1 "$_p" > "$TMP/id.last"
  if ! grep -Fq '"schema":"t64.suite"' "$TMP/id.last"; then
    rep_rule C-EVIDENCE-INVALID "$_f" "$_ln" "evidence file does not end with a t64.suite record: $_p"
    return 0
  fi
  if [ ! -s "$TMP/id.out" ]; then
    rep_rule C-EVIDENCE-INVALID "$_f" "$_ln" "evidence file holds no results: $_p"
    return 0
  fi
  # Columns: fixture_id profile_id build_id evidence_tier outcome. The claim's
  # values travel in the environment so awk does not interpret escapes in them.
  CLAIM_PROFILE=$_prof CLAIM_BUILD=$_bld awk -F "$TAB" '
    {
      n++
      if ($4 == "synthetic" || $2 == "synthetic-stub" || $3 ~ /\+synthetic$/) syn++
      if ($2 != ENVIRON["CLAIM_PROFILE"] || $3 != ENVIRON["CLAIM_BUILD"]) mm++
      if ($5 == "pass") pc++
    }
    END { printf "%d %d %d %d\n", n + 0, syn + 0, mm + 0, pc + 0 }' "$TMP/id.out" > "$TMP/id.sum" ||
    fatal "awk failed on the evidence summary"
  read -r _n _syn _mm _pc < "$TMP/id.sum"
  if [ "$_syn" -gt 0 ]; then
    rep_rule C-EVIDENCE-SYNTHETIC "$_f" "$_ln" "evidence $_p holds $_syn synthetic result(s) of $_n; synthetic results are never support evidence"
    return 0
  fi
  if [ "$_mm" -gt 0 ]; then
    rep_rule C-EVIDENCE-MISMATCH "$_f" "$_ln" "evidence $_p has $_mm result(s) whose profile or build differs from the claim"
  fi
  case $_pas in
    '' | *[!0-9]*) ;;
    *)
      if [ "$_pc" -ne "$_pas" ]; then
        rep_rule C-EVIDENCE-MISMATCH "$_f" "$_ln" "evidence $_p has $_pc passing result(s) but the claim says pass: $_pas"
      fi
      ;;
  esac
  return 0
}

# run_scan: scans the list in $TMP/list, prints the report, returns 0/1.
run_scan() {
  LIST="$TMP/list"
  REP="$TMP/rep"
  EVF="$TMP/ev"
  STAT="$TMP/stat"
  HARN="$TMP/harn"
  : > "$REP"
  : > "$EVF"
  : > "$STAT"
  : > "$HARN"
  if [ ! -s "$LIST" ]; then
    echo "RULE C-ZERO-INPUT no files: nothing was scanned (a check that scans nothing is not a pass)"
    echo "claims: files=0 blocks=0 errors=1"
    return 1
  fi
  awk -v LIST="$LIST" -v REP="$REP" -v EVF="$EVF" -v STAT="$STAT" -v HARN="$HARN" \
    "$AWK_PROG" < /dev/null || fatal "awk failed"
  if [ -s "$HARN" ]; then
    cat "$HARN" >&2
    exit 2
  fi
  if [ -s "$EVF" ]; then
    [ -x "$CHECK" ] || fatal "t64-result-check is not built: $CHECK (build the gate preset first, or set T64_BUILD_DIR)"
    while IFS="$TAB" read -r _ef _el _ep _eprof _ebld _epass; do
      evidence_check "$_ef" "$_el" "$_ep" "$_eprof" "$_ebld" "$_epass"
    done < "$EVF"
  fi
  cat "$REP"
  _errs=$(awk '/^RULE /{ n++ } END { print n + 0 }' "$REP") || fatal "awk failed"
  read -r _stat < "$STAT"
  echo "claims: $_stat errors=$_errs"
  if [ "$_errs" -eq 0 ]; then
    echo ok
    return 0
  fi
  return 1
}

# ---------------------------------------------------------------- selftest

# Defaults for a valid self-checking-fixture claim; a case overrides variables
# in a subshell and calls mkclaim.
defaults() {
  c_build=0.1.0
  c_profile=example-profile
  c_tier=self-checking-fixture
  c_set=example-set
  c_digest=sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef
  c_pass=5
  c_total=7
  c_guest=1
  c_boot=0
  c_timeout=0
  c_missing=0
  c_harn=1
  c_skipped=0
  c_okind=guest-self-check
  c_orev=example-1
  c_nc='any other profile, any commercial software'
  c_emulator=""
  c_emurev=""
  c_scope=""
  c_prov=""
  c_evidence=""
  c_stmt=""
  c_extra=""
  c_phrase=""
}

mkclaim() { # FILE
  case $c_tier in
    self-checking-fixture) _ph="passes self-checking fixtures" ;;
    smoke) _ph="smoke-tested" ;;
    emulator-differential) _ph="agrees with $c_emulator $c_emurev" ;;
    hardware-derived) _ph="matches hardware-derived expectations within $c_scope" ;;
    *) _ph=$c_phrase ;;
  esac
  _st="Tendies64 $c_build on profile $c_profile: $_ph for $c_set @ $c_digest ($c_pass/$c_total; missing $c_missing, skipped $c_skipped, timeout $c_timeout, harness-error $c_harn, guest-fail $c_guest, boot-fail $c_boot). Oracle: $c_okind $c_orev. Not claimed: $c_nc."
  [ -z "$c_stmt" ] || _st=$c_stmt
  {
    printf '%s\n' 'Selftest claim file.' '' '```t64-claim'
    printf '%s\n' "build_id: $c_build" "profile_id: $c_profile" "tier: $c_tier" "fixture_set: $c_set" \
      "manifest_digest: $c_digest" "pass: $c_pass" "total: $c_total" "guest_fail: $c_guest" \
      "boot_fail: $c_boot" "timeout: $c_timeout" "missing: $c_missing" "harness_error: $c_harn" \
      "skipped: $c_skipped" "oracle_kind: $c_okind" "oracle_revision: $c_orev" "not_claimed: $c_nc"
    [ -z "$c_emulator" ] || printf '%s\n' "emulator: $c_emulator"
    [ -z "$c_emurev" ] || printf '%s\n' "emulator_revision: $c_emurev"
    [ -z "$c_scope" ] || printf '%s\n' "scope: $c_scope"
    [ -z "$c_prov" ] || printf '%s\n' "capture_provenance: $c_prov"
    [ -z "$c_evidence" ] || printf '%s\n' "evidence: $c_evidence"
    [ -z "$c_extra" ] || printf '%s\n' "$c_extra"
    printf '%s\n' "statement: $_st" '```'
  } > "$1"
}

ST_CASES=0
ST_FAIL=0

# st_expect NAME RC RULE|ok FILE-OR-ARGS...: runs this script on the files in a
# fresh root and checks the exit code and that the output has the rule id.
st_expect() {
  _name=$1
  _erc=$2
  _erule=$3
  shift 3
  ST_CASES=$((ST_CASES + 1))
  _rc=0
  "$@" > "$TMP/st.out" 2>&1 < /dev/null || _rc=$?
  _why=""
  if [ "$_rc" -ne "$_erc" ]; then
    _why="exit $_rc, expected $_erc"
  elif [ "$_erule" = ok ]; then
    grep -Eq '^ok$' "$TMP/st.out" || _why="no ok line"
  elif [ "$_erule" = - ]; then
    :
  elif ! grep -Fq "RULE $_erule " "$TMP/st.out"; then
    _why="no RULE $_erule line"
  fi
  if [ -n "$_why" ]; then
    echo "FAIL $_name: $_why"
    sed 's/^/  | /' "$TMP/st.out"
    ST_FAIL=$((ST_FAIL + 1))
  else
    echo "ok $_name"
  fi
}

# st_absent NAME RULE: the last st_expect output must not hold the rule.
st_absent() {
  ST_CASES=$((ST_CASES + 1))
  if grep -Fq "RULE $2 " "$TMP/st.out"; then
    echo "FAIL $1: RULE $2 present"
    sed 's/^/  | /' "$TMP/st.out"
    ST_FAIL=$((ST_FAIL + 1))
  else
    echo "ok $1"
  fi
}

selftest() {
  [ -x "$CHECK" ] || fatal "t64-result-check is not built: $CHECK (build the gate preset first, or set T64_BUILD_DIR)"
  _sd="$TMP/self"
  mkdir -p "$_sd"
  runner() { sh "$SELF" "$@"; }

  # 1. Template examples are real, valid claims.
  _tpl="$ROOT/docs/claims/TEMPLATE.md"
  [ -f "$_tpl" ] || fatal "missing $_tpl"
  awk -v D="$_sd" '
    $0 == "```t64-claim-example" { n++; f = D "/example" n ".md"; print "```t64-claim" > f; inx = 1; next }
    inx && $0 == "```" { print > f; close(f); inx = 0; next }
    inx { print > f }
    END { print n + 0 > (D "/example.count") }' "$_tpl" || fatal "awk failed on the template"
  read -r _ne < "$_sd/example.count"
  ST_CASES=$((ST_CASES + 1))
  if [ "$_ne" -lt 2 ]; then
    echo "FAIL template examples: found $_ne, need at least 2"
    ST_FAIL=$((ST_FAIL + 1))
  else
    echo "ok template has $_ne examples"
  fi
  _i=1
  while [ "$_i" -le "$_ne" ]; do
    st_expect "template example $_i is a valid claim" 0 ok runner --files "$_sd/example$_i.md"
    _i=$((_i + 1))
  done

  # 2. Synthetic evidence is refused end to end (the seeded synthetic suite).
  _rel="tests/seeded/suites/good.jsonl"
  [ -f "$ROOT/$_rel" ] || fatal "missing $ROOT/$_rel"
  ( defaults; c_evidence=$_rel; mkclaim "$_sd/ev_synth.md" )
  st_expect "synthetic evidence suite" 1 C-EVIDENCE-SYNTHETIC runner --files "$_sd/ev_synth.md"
  st_absent "synthetic evidence reports no mismatch" C-EVIDENCE-MISMATCH

  # 3. A non-synthetic evidence suite, derived from a golden with consistent ids,
  # in a temporary root, so match and mismatch and every invalid path are proven.
  _rt="$TMP/root"
  mkdir -p "$_rt/ev" "$_rt/docs"
  sed 's/synthetic-stub/example-profile/; s/0\.0\.0+synthetic/0.1.0/; s/"evidence_tier":"synthetic"/"evidence_tier":"smoke"/' \
    "$ROOT/tests/controls/golden/positive.json" > "$_rt/ev/one.json"
  cp "$_rt/ev/one.json" "$_rt/ev/good.jsonl"
  printf '%s\n' '{"case_count":1,"inventory_sha256":"0000000000000000000000000000000000000000000000000000000000000000","schema":"t64.suite","schema_version":1}' >> "$_rt/ev/good.jsonl"
  cp "$ROOT/tests/seeded/results/bad_float.json" "$_rt/ev/invalid.jsonl"
  ln -s good.jsonl "$_rt/ev/link.jsonl"
  mkdir -p "$_rt/real"
  cp "$_rt/ev/good.jsonl" "$_rt/real/good.jsonl"
  ln -s real "$_rt/ev/dirlink"
  _c="$_rt/docs"
  ( defaults; c_pass=1; c_total=1; c_guest=0; c_harn=0; c_evidence=ev/good.jsonl; mkclaim "$_c/match.md" )
  st_expect "evidence that agrees with the claim" 0 ok runner --root "$_rt" --files docs/match.md
  ( defaults; c_pass=1; c_total=1; c_guest=0; c_harn=0; c_build=0.2.0; c_evidence=ev/good.jsonl; mkclaim "$_c/mm_build.md" )
  st_expect "evidence build differs" 1 C-EVIDENCE-MISMATCH runner --root "$_rt" --files docs/mm_build.md
  ( defaults; c_pass=1; c_total=1; c_guest=0; c_harn=0; c_profile=other-profile; c_evidence=ev/good.jsonl; mkclaim "$_c/mm_prof.md" )
  st_expect "evidence profile differs" 1 C-EVIDENCE-MISMATCH runner --root "$_rt" --files docs/mm_prof.md
  ( defaults; c_pass=0; c_total=1; c_guest=0; c_harn=0; c_skipped=1; c_evidence=ev/good.jsonl; mkclaim "$_c/mm_pass.md" )
  st_expect "evidence pass count differs" 1 C-EVIDENCE-MISMATCH runner --root "$_rt" --files docs/mm_pass.md
  for _case in "missing:ev/none.jsonl" "dotdot:ev/../ev/good.jsonl" "absolute:/ev/good.jsonl" "directory:ev" \
    "symlink-file:ev/link.jsonl" "symlink-dir:ev/dirlink/good.jsonl" "invalid-suite:ev/invalid.jsonl" \
    "not-a-suite:ev/one.json" "space:ev/good jsonl" "empty:" "dot:./ev/good.jsonl" "double-slash:ev//good.jsonl"; do
    _cn=${_case%%:*}
    _cp=${_case#*:}
    ( defaults; c_pass=1; c_total=1; c_guest=0; c_harn=0; c_evidence=$_cp
      if [ "$_cn" = empty ]; then c_evidence=""; c_extra="evidence:"; fi
      mkclaim "$_c/inv_$_cn.md" )
    st_expect "invalid evidence: $_cn" 1 C-EVIDENCE-INVALID runner --root "$_rt" --files "docs/inv_$_cn.md"
  done

  # 4. Block rules not reached by the seeds.
  ( defaults; c_okind=""; mkclaim "$_sd/oracle.md" )
  st_expect "empty oracle_kind" 1 C-ORACLE runner --files "$_sd/oracle.md"
  ( defaults; c_orev=""; mkclaim "$_sd/oracle2.md" )
  st_expect "empty oracle_revision" 1 C-ORACLE runner --files "$_sd/oracle2.md"
  ( defaults; c_tier=hardware-derived; c_scope=""; c_prov=example; c_extra='scope:'; c_nc='accurate timing'; mkclaim "$_sd/hw_empty.md" )
  st_expect "reserved word with an empty hardware scope" 1 C-RESERVED-WORD runner --files "$_sd/hw_empty.md"
  ( defaults; c_extra='emulator: example-emulator'; mkclaim "$_sd/wrong_cond.md" )
  st_expect "conditional key on the wrong tier" 1 C-FIELD-UNKNOWN runner --files "$_sd/wrong_cond.md"
  ( defaults; c_extra='tier: smoke'; mkclaim "$_sd/dupkey.md" )
  st_expect "duplicate key" 1 C-BLOCK-SYNTAX runner --files "$_sd/dupkey.md"
  ( defaults; c_extra='not a key value line'; mkclaim "$_sd/nokv.md" )
  st_expect "line that is not key: value" 1 C-BLOCK-SYNTAX runner --files "$_sd/nokv.md"
  ( defaults; c_pass=8; c_total=7; mkclaim "$_sd/counts_pass.md" )
  st_expect "pass greater than total" 1 C-COUNTS runner --files "$_sd/counts_pass.md"
  ( defaults; c_pass=3; mkclaim "$_sd/counts_sum.md" )
  st_expect "counts that do not sum to total" 1 C-COUNTS runner --files "$_sd/counts_sum.md"
  ( defaults; c_digest=sha256:0123; mkclaim "$_sd/digest.md" )
  st_expect "short manifest digest" 1 C-ID-FORMAT runner --files "$_sd/digest.md"
  ( defaults; c_pass=five; mkclaim "$_sd/count_fmt.md" )
  st_expect "count that is not an integer" 1 C-ID-FORMAT runner --files "$_sd/count_fmt.md"
  ( defaults; c_tier=emulator-differential; c_emulator=example-emulator; c_emurev=r1; mkclaim "$_sd/emu_ok.md" )
  st_expect "emulator-differential with its keys" 0 ok runner --files "$_sd/emu_ok.md"
  ( defaults; c_tier=smoke; mkclaim "$_sd/smoke_ok.md" )
  st_expect "smoke claim" 0 ok runner --files "$_sd/smoke_ok.md"
  ( defaults; c_nc='whole-word test: Accurate'; mkclaim "$_sd/word_ci.md" )
  st_expect "reserved word is case-insensitive" 1 C-RESERVED-WORD runner --files "$_sd/word_ci.md"
  ( defaults; c_nc='the word accurately is not reserved'; mkclaim "$_sd/word_part.md" )
  st_expect "reserved word is whole-word only" 0 ok runner --files "$_sd/word_part.md"
  printf '%s\n' '- Tendies64 is compatible with everything' > "$_sd/prose_list.md"
  st_expect "reserved word in a list item statement" 1 C-RESERVED-WORD runner --files "$_sd/prose_list.md"
  printf '%s\n' 'A line about a compatible emulator.' > "$_sd/prose_other.md"
  st_expect "reserved word on an unrelated prose line" 0 ok runner --files "$_sd/prose_other.md"
  printf '%s\n' '```t64-claim-example' 'not checked: at all' '```' > "$_sd/example_skip.md"
  st_expect "example block is not checked" 0 ok runner --files "$_sd/example_skip.md"
  printf '%s\n' '```t64-claim extra' 'x' '```' > "$_sd/fence_bad.md"
  st_expect "malformed claim fence" 1 C-BLOCK-SYNTAX runner --files "$_sd/fence_bad.md"
  printf '%s\n' '```t64-claim-example' 'x' > "$_sd/example_open.md"
  st_expect "unterminated example block" 1 C-BLOCK-SYNTAX runner --files "$_sd/example_open.md"

  # 5. Input handling: zero input, exclusions, and a missing file.
  st_expect "--files with no files" 1 C-ZERO-INPUT runner --files
  st_expect "missing file is a harness error" 2 - runner --files "$_sd/does-not-exist.md"
  _gr="$TMP/gitroot"
  mkdir -p "$_gr"
  ( cd "$_gr" && git init -q . ) || fatal "git init failed in the selftest root"
  st_expect "default scan of a tree with no markdown" 1 C-ZERO-INPUT runner --root "$_gr"
  mkdir -p "$_gr/.planning" "$_gr/tests/seeded/claims" "$_gr/docs"
  printf '%s\n' '```t64-claim' 'broken' '```' > "$_gr/.planning/notes.md"
  printf '%s\n' '```t64-claim' 'broken' '```' > "$_gr/tests/seeded/claims/bad.md"
  printf '%s\n' 'Only prose here.' > "$_gr/docs/a.md"
  st_expect "default scan skips .planning and the claim seeds" 0 ok runner --root "$_gr"
  ST_CASES=$((ST_CASES + 1))
  if grep -Fq 'files=1 blocks=0' "$TMP/st.out"; then
    echo "ok default scan counted 1 file and 0 blocks"
  else
    echo "FAIL default scan counts"
    sed 's/^/  | /' "$TMP/st.out"
    ST_FAIL=$((ST_FAIL + 1))
  fi
  printf '%s\n' '```t64-claim' 'broken' '```' > "$_gr/docs/b.md"
  st_expect "default scan sees an untracked markdown file" 1 C-BLOCK-SYNTAX runner --root "$_gr"

  echo "check-claims selftest: cases=$ST_CASES failures=$ST_FAIL"
  [ "$ST_FAIL" -eq 0 ] || return 1
  return 0
}

# ---------------------------------------------------------------- main

DO_SELFTEST=0
HAVE_FILES=0
RDIR=""
: > /dev/null
TMP=$(mktemp -d "${TMPDIR:-/tmp}/check-claims.XXXXXX") || fatal "mktemp failed"
: > "$TMP/args"
while [ $# -gt 0 ]; do
  case "$1" in
    --selftest) DO_SELFTEST=1; shift ;;
    --root)
      [ $# -ge 2 ] || usage
      RDIR=$2
      shift 2
      ;;
    --files)
      HAVE_FILES=1
      shift
      while [ $# -gt 0 ]; do
        case "$1" in
          --*) break ;;
        esac
        printf '%s\n' "$1" >> "$TMP/args"
        shift
      done
      ;;
    *) usage ;;
  esac
done

if [ -n "$RDIR" ]; then
  [ -d "$RDIR" ] || fatal "no such directory: $RDIR"
  cd "$RDIR"
else
  cd "$ROOT"
fi

if [ "$DO_SELFTEST" -eq 1 ]; then
  if [ "$HAVE_FILES" -eq 1 ]; then usage; fi
  selftest
  exit $?
fi

: > "$TMP/list"
if [ "$HAVE_FILES" -eq 1 ]; then
  while IFS= read -r _a; do
    [ -n "$_a" ] || continue
    if [ ! -f "$_a" ] || [ ! -r "$_a" ]; then
      fatal "not a readable regular file: $_a"
    fi
    printf '%s\n' "$_a" >> "$TMP/list"
  done < "$TMP/args"
else
  git ls-files --cached --others --exclude-standard -- '*.md' > "$TMP/all" ||
    fatal "git ls-files failed (run from a git work tree)"
  while IFS= read -r _a; do
    case "$_a" in
      .planning/* | tests/seeded/claims/*) continue ;;
    esac
    # Skip deleted-but-tracked files and symlinks; read only regular files.
    if [ -f "$_a" ] && [ ! -L "$_a" ]; then
      printf '%s\n' "$_a" >> "$TMP/list"
    fi
  done < "$TMP/all"
fi

rc=0
run_scan || rc=$?
exit "$rc"
