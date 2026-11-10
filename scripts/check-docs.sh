#!/bin/sh
# check-docs.sh: decision-record and policy checks (GOV-01, GOV-02, GOV-06).
# POSIX sh + awk only.
#
# Usage: check-docs.sh [--root DIR] [--selftest]
# Output: one line per violation, "RULE-ID path: message".
# Exit:   0 clean, 1 violations (or a failed selftest), 2 usage or tool error.
#
# Rules:
#   DOC-TEMPLATE          the template headings are present and in order
#   DOC-ADR-NAME          NNNN-slug.md naming, unique numbers
#   DOC-ADR-STRUCT        front-matter status enum, heading order, Reversal evidence not empty
#   DOC-STATUS-MISMATCH   front-matter status differs from the body Status line
#   DOC-ZERO-ADRS         no ADR files at all
#   DOC-ADR-MISSING       a required number has no file
#   DOC-ADR-NOT-ACCEPTED  a required ADR (other than PROPOSED_OK) is not accepted
#   DOC-POLICY-HEADING    a policy file or a listed heading is missing
#   DOC-CORE-BEFORE-ADR   a file under core/ outside CORE_ALLOW while ADR-0002 is not accepted
#                         (CONTEXT A-1: the rule is path-based; file content is never consulted)
set -eu
LC_ALL=C
export LC_ALL

SELF=$0

# Required ADR numbers (one variable; later tasks append).
REQUIRED="0001 0002 0003 0004 0005 0006"
# Required ADRs allowed to stay proposed. Plan 01-08 accepts ADR-0002 and removes this exemption.
PROPOSED_OK="0002"
# The only two paths under core/ allowed while ADR-0002 is not accepted (CONTEXT A-1).
CORE_ALLOW="core/t64_core_stub.c core/include/tendies64/t64_facade.h"
# Headings every ADR and the template carry, in this order (extra headings are allowed between them).
ADR_HEADINGS="## Context and Problem Statement|## Decision Drivers|## Considered Options|## Decision Outcome|### Consequences|### Confirmation|## Reversal evidence|## Pros and Cons of the Options|## More Information"
# Policy files, one per line: "path|heading|heading|...". Every listed heading must appear as a full line.
POLICIES=""

usage() {
  echo "usage: check-docs.sh [--root DIR] [--selftest]" >&2
}

# fm_status FILE: print the front-matter status value, or nothing.
fm_status() {
  awk '
    NR == 1 && $0 == "---" { fm = 1; next }
    fm && $0 == "---" { exit }
    fm && /^status:/ { v = $0; sub(/^status:[ \t]*/, "", v); sub(/[ \t\r]+$/, "", v); print v; exit }
  ' "$1"
}

# scan_adr PATH KIND: structure checks for one ADR (KIND adr) or the template (KIND template).
scan_adr() {
  awk -v path="$1" -v kind="$2" -v heads="$ADR_HEADINGS" '
    function out(rule, msg) { printf "%s %s: %s\n", rule, path, msg }
    BEGIN {
      n = split(heads, req, "|"); pos = 1
      rule = (kind == "template") ? "DOC-TEMPLATE" : "DOC-ADR-STRUCT"
    }
    NR == 1 && $0 == "---" { infm = 1; hasfm = 1; next }
    infm {
      if ($0 == "---") { infm = 0; next }
      k = $0; sub(/:.*/, "", k); keys[k] = 1
      if (k == "status") { v = $0; sub(/^status:[ \t]*/, "", v); sub(/[ \t\r]+$/, "", v); fm = v }
      next
    }
    /^```/ { fence = !fence; if (inrev) revn++; next }
    fence { if (inrev && $0 ~ /[^ \t\r]/) revn++; next }
    /^#+ / {
      if ($0 ~ /^# /) { h1++; inrev = 0; next }
      if ($0 ~ /^## /) { inrev = ($0 == "## Reversal evidence") } else if (inrev) { revn++ }
      if (pos <= n && $0 == req[pos]) pos++
      next
    }
    /^\*\*Status:\*\*/ {
      v = $0; sub(/^\*\*Status:\*\*[ \t]*/, "", v); split(v, w, /[ \t]+/)
      body = w[1]; hasbody = 1
      next
    }
    inrev && $0 ~ /[^ \t\r]/ { revn++ }
    END {
      if (!hasfm) out(rule, "missing front matter")
      else {
        m = split("status date decision-makers consulted informed", ks, " ")
        for (i = 1; i <= m; i++) if (!(ks[i] in keys)) out(rule, "front matter lacks key " ks[i])
      }
      if (h1 < 1) out(rule, "missing H1 title")
      if (pos <= n) out(rule, "heading missing or out of order: " req[pos])
      if (kind == "template") {
        if (!hasbody) out(rule, "missing body Status line")
      } else {
        if (hasfm && fm !~ /^(proposed|accepted|rejected|deprecated|superseded)$/)
          out(rule, "front-matter status is not one of proposed, accepted, rejected, deprecated, superseded: " fm)
        if (!hasbody) out("DOC-STATUS-MISMATCH", "no body Status line")
        else if (hasfm && body != fm) out("DOC-STATUS-MISMATCH", "front matter says " fm " but the Status line says " body)
        if (revn == 0) out(rule, "the Reversal evidence section is missing or empty")
      }
    }
  ' "$1"
}

# scan_tree ROOT: print violations. Always returns 0 (the caller counts output lines).
scan_tree() {
  cd "$1"

  if [ -f docs/adr/0000-template.md ]; then
    scan_adr docs/adr/0000-template.md template
  else
    echo "DOC-TEMPLATE docs/adr/0000-template.md: the template file is missing"
  fi

  _nadr=0
  _seen=" "
  for _f in docs/adr/*.md; do
    if [ ! -e "$_f" ]; then continue; fi
    _b=${_f##*/}
    if [ "$_b" = "0000-template.md" ]; then continue; fi
    if ! printf '%s\n' "$_b" | grep -Eq '^[0-9]{4}-[a-z0-9-]+\.md$'; then
      echo "DOC-ADR-NAME $_f: the name must match NNNN-slug.md (four digits, lowercase slug of a-z, 0-9 and -)"
      continue
    fi
    _num=${_b%%-*}
    if [ "$_num" = "0000" ]; then
      echo "DOC-ADR-NAME $_f: number 0000 is reserved for the template"
      continue
    fi
    case "$_seen" in
      *" $_num "*) echo "DOC-ADR-NAME $_f: duplicate ADR number $_num" ;;
    esac
    _seen="$_seen$_num "
    _nadr=$((_nadr + 1))
    scan_adr "$_f" adr
  done
  if [ "$_nadr" -eq 0 ]; then
    echo "DOC-ZERO-ADRS docs/adr: no ADR files found (a check over zero files proves nothing)"
  fi

  for _n in $REQUIRED; do
    _file=""
    for _g in docs/adr/"$_n"-*.md; do
      if [ -e "$_g" ]; then _file=$_g; break; fi
    done
    if [ -z "$_file" ]; then
      echo "DOC-ADR-MISSING docs/adr/$_n-*.md: required ADR $_n has no file"
      continue
    fi
    _st=$(fm_status "$_file")
    case " $PROPOSED_OK " in
      *" $_n "*) ;;
      *)
        if [ "$_st" != "accepted" ]; then
          echo "DOC-ADR-NOT-ACCEPTED $_file: required ADR $_n has status '$_st', expected accepted"
        fi
        ;;
    esac
  done

  printf '%s\n' "$POLICIES" | while IFS= read -r _line; do
    if [ -z "$_line" ]; then continue; fi
    _path=${_line%%|*}
    _rest=${_line#*|}
    if [ ! -f "$_path" ]; then
      echo "DOC-POLICY-HEADING $_path: the policy file is missing"
      continue
    fi
    while [ -n "$_rest" ]; do
      _h=${_rest%%|*}
      if [ "$_rest" = "$_h" ]; then _rest=""; else _rest=${_rest#*|}; fi
      if ! grep -Fxq -- "$_h" "$_path"; then
        echo "DOC-POLICY-HEADING $_path: missing heading '$_h'"
      fi
    done
  done

  # CONTEXT A-1: path-based. Any file under core/ outside the two allowlisted paths needs ADR-0002 accepted.
  if [ -e core ] || [ -L core ]; then
    _acc=no
    for _g in docs/adr/0002-*.md; do
      if [ -e "$_g" ] && [ "$(fm_status "$_g")" = "accepted" ]; then _acc=yes; fi
    done
    if [ "$_acc" = "no" ]; then
      _all=$(find core ! -type d | sort) || _all=""
      while IFS= read -r _p; do
        if [ -z "$_p" ]; then continue; fi
        _ok=no
        for _a in $CORE_ALLOW; do
          if [ "$_p" = "$_a" ]; then _ok=yes; fi
        done
        if [ "$_ok" = "no" ]; then
          echo "DOC-CORE-BEFORE-ADR $_p: only $CORE_ALLOW may exist under core/ until ADR-0002 is accepted"
        fi
      done <<EOF
$_all
EOF
    fi
  fi
  return 0
}

# ---- selftest -------------------------------------------------------------------------------

# mk_doc FILE STATUS: write a conforming ADR.
mk_doc() {
  printf -- '---\nstatus: %s\ndate: 2026-11-09\ndecision-makers: szTheory\nconsulted: none\ninformed: none\n---\n# Title\n\n**Status:** %s\n\n## Context and Problem Statement\n\nx\n\n## Decision Drivers\n\nx\n\n## Considered Options\n\nx\n\n## Decision Outcome\n\n### Consequences\n\nx\n\n### Confirmation\n\nx\n\n## Reversal evidence\n\nx\n\n## Pros and Cons of the Options\n\nx\n\n## More Information\n\nx\n' "$2" "$2" > "$1"
}

# mk_template FILE: the template differs from an ADR only in its status text.
mk_template() {
  mk_doc "$1" proposed
}

# new_tree NAME: a clean tree (template, every required ADR, every policy file) under $W.
new_tree() {
  T=$W/$1
  mkdir -p "$T/docs/adr" "$T/docs/policy"
  mk_template "$T/docs/adr/0000-template.md"
  for _n in $REQUIRED; do
    if [ "$_n" = "0002" ]; then _s=proposed; else _s=accepted; fi
    mk_doc "$T/docs/adr/$_n-case.md" "$_s"
  done
  printf '%s\n' "$POLICIES" | while IFS= read -r _line; do
    if [ -z "$_line" ]; then continue; fi
    _path=${_line%%|*}
    _rest=${_line#*|}
    mkdir -p "$T/$(dirname "$_path")"
    : > "$T/$_path"
    while [ -n "$_rest" ]; do
      _h=${_rest%%|*}
      if [ "$_rest" = "$_h" ]; then _rest=""; else _rest=${_rest#*|}; fi
      printf '%s\n\nx\n\n' "$_h" >> "$T/$_path"
    done
  done
}

FAILS=0
CASES=0

# run_tree: run the checker on $T, set OUT and RC.
run_tree() {
  OUT=$(sh "$SELF" --root "$T" 2>&1) && RC=0 || RC=$?
}

# expect_fail RULE DESCRIPTION: the checker must exit 1 and print the rule id.
expect_fail() {
  CASES=$((CASES + 1))
  run_tree
  case "$OUT" in
    *"$1 "*)
      if [ "$RC" -eq 1 ]; then echo "ok $1: $2"; else echo "FAIL $1: $2 (exit $RC, expected 1)"; FAILS=$((FAILS + 1)); fi
      ;;
    *) echo "FAIL $1: $2 (rule id not printed; exit $RC)"; FAILS=$((FAILS + 1)) ;;
  esac
}

# expect_absent RULE DESCRIPTION: the rule id must not appear in the output.
expect_absent() {
  CASES=$((CASES + 1))
  run_tree
  case "$OUT" in
    *"$1 "*) echo "FAIL $1: $2 (rule fired: $OUT)"; FAILS=$((FAILS + 1)) ;;
    *) if [ "$RC" -eq 2 ]; then echo "FAIL $1: $2 (tool error, exit 2)"; FAILS=$((FAILS + 1)); else echo "ok $1: $2"; fi ;;
  esac
}

# expect_clean DESCRIPTION: exit 0 and no output.
expect_clean() {
  CASES=$((CASES + 1))
  run_tree
  if [ "$RC" -eq 0 ] && [ "$OUT" = "check-docs: ok" ]; then echo "ok clean: $1"; else echo "FAIL clean: $1 (exit $RC: $OUT)"; FAILS=$((FAILS + 1)); fi
}

# drop_line FILE LINE: copy FILE without the exact line LINE (never in place).
drop_line() {
  grep -vxF -- "$2" "$1" > "$1.new" || true
  mv "$1.new" "$1"
}

selftest() {
  W=$(mktemp -d "${TMPDIR:-/tmp}/t64docs.XXXXXX")
  trap 'rm -rf "$W"' EXIT HUP INT TERM

  new_tree clean
  expect_clean "a tree with the template, every required ADR and every policy file"

  # DOC-TEMPLATE
  new_tree tpl-missing
  rm "$T/docs/adr/0000-template.md"
  expect_fail DOC-TEMPLATE "template file missing"
  new_tree tpl-order
  drop_line "$T/docs/adr/0000-template.md" "## Decision Drivers"
  expect_fail DOC-TEMPLATE "template heading missing"

  # DOC-ADR-NAME
  new_tree name-bad
  mk_doc "$T/docs/adr/9-short.md" accepted
  expect_fail DOC-ADR-NAME "number is not four digits"
  new_tree name-upper
  mk_doc "$T/docs/adr/0042-Bad_Name.md" accepted
  expect_fail DOC-ADR-NAME "slug is not lowercase"
  new_tree name-dup
  mk_doc "$T/docs/adr/0001-second.md" accepted
  expect_fail DOC-ADR-NAME "duplicate ADR number"

  # DOC-ADR-STRUCT and DOC-STATUS-MISMATCH
  new_tree struct-status
  mk_doc "$T/docs/adr/0042-bad-status.md" maybe
  expect_fail DOC-ADR-STRUCT "front-matter status outside the enum"
  new_tree struct-heading
  mk_doc "$T/docs/adr/0042-no-heading.md" accepted
  drop_line "$T/docs/adr/0042-no-heading.md" "## Considered Options"
  expect_fail DOC-ADR-STRUCT "a required heading is missing"
  new_tree struct-order
  mk_doc "$T/docs/adr/0042-order.md" accepted
  drop_line "$T/docs/adr/0042-order.md" "## Decision Drivers"
  printf '\n## Decision Drivers\n\nx\n' >> "$T/docs/adr/0042-order.md"
  expect_fail DOC-ADR-STRUCT "headings out of order"
  new_tree struct-rev
  mk_doc "$T/docs/adr/0042-empty-rev.md" accepted
  awk '$0 == "## Reversal evidence" { print; getline; getline; next } { print }' "$T/docs/adr/0042-empty-rev.md" > "$T/x.new"
  mv "$T/x.new" "$T/docs/adr/0042-empty-rev.md"
  expect_fail DOC-ADR-STRUCT "the Reversal evidence section is empty"
  new_tree mismatch
  mk_doc "$T/docs/adr/0042-mismatch.md" accepted
  sed 's/^\*\*Status:\*\* accepted/**Status:** proposed/' "$T/docs/adr/0042-mismatch.md" > "$T/x.new"
  mv "$T/x.new" "$T/docs/adr/0042-mismatch.md"
  expect_fail DOC-STATUS-MISMATCH "front matter and body Status line differ"

  # DOC-ZERO-ADRS and DOC-ADR-MISSING
  new_tree zero
  rm -f "$T"/docs/adr/0*-case.md
  expect_fail DOC-ZERO-ADRS "no ADR files"
  new_tree missing
  _first=${REQUIRED%% *}
  rm "$T/docs/adr/$_first-case.md"
  expect_fail DOC-ADR-MISSING "a required number has no file"

  # DOC-ADR-NOT-ACCEPTED (the first required number that is not exempt)
  new_tree notacc
  for _n in $REQUIRED; do
    case " $PROPOSED_OK " in *" $_n "*) ;; *) _target=$_n; break ;; esac
  done
  mk_doc "$T/docs/adr/$_target-case.md" proposed
  expect_fail DOC-ADR-NOT-ACCEPTED "a required ADR is proposed"
  for _ex in $PROPOSED_OK; do
    case " $REQUIRED " in
      *" $_ex "*)
        new_tree exempt
        mk_doc "$T/docs/adr/$_ex-case.md" proposed
        expect_absent DOC-ADR-NOT-ACCEPTED "ADR $_ex may stay proposed"
        ;;
    esac
  done

  # DOC-POLICY-HEADING (only when the policy table is non-empty)
  if [ -n "$POLICIES" ]; then
    _pol=$(printf '%s\n' "$POLICIES" | sed -n '1p')
    _ppath=${_pol%%|*}
    new_tree policy-missing
    rm "$T/$_ppath"
    expect_fail DOC-POLICY-HEADING "a policy file is missing"
    new_tree policy-heading
    _ph=${_pol#*|}
    _ph=${_ph%%|*}
    drop_line "$T/$_ppath" "$_ph"
    expect_fail DOC-POLICY-HEADING "a listed policy heading is missing"
  fi

  # DOC-CORE-BEFORE-ADR (CONTEXT A-1): exact-path allowlist; content is never consulted.
  new_tree core-a
  rm -f "$T"/docs/adr/0002-*.md
  mkdir -p "$T/core/include/tendies64"
  printf 'int t64_stub;\n' > "$T/core/t64_core_stub.c"
  printf '/* facade */\n' > "$T/core/include/tendies64/t64_facade.h"
  expect_absent DOC-CORE-BEFORE-ADR "(a) only the two allowlisted paths and no ADR-0002 pass"
  new_tree core-b
  mk_doc "$T/docs/adr/0002-x.md" proposed
  mkdir -p "$T/core"
  printf 'int x;\n' > "$T/core/x.c"
  expect_fail DOC-CORE-BEFORE-ADR "(b) an extra core/x.c with ADR-0002 proposed fails"
  new_tree core-c
  rm -f "$T"/docs/adr/0002-*.md
  mkdir -p "$T/core/include/tendies64"
  printf 'int t64_stub;\n' > "$T/core/t64_core_stub.c"
  printf '/* facade */\n' > "$T/core/include/tendies64/t64_facade.h"
  printf '/* y */\n' > "$T/core/include/tendies64/y.h"
  expect_fail DOC-CORE-BEFORE-ADR "(c) an extra core/include/tendies64/y.h with no ADR-0002 fails"
  new_tree core-d
  mk_doc "$T/docs/adr/0002-x.md" proposed
  mkdir -p "$T/core"
  printf 'project(core)\n' > "$T/core/CMakeLists.txt"
  expect_fail DOC-CORE-BEFORE-ADR "(d) core/CMakeLists.txt with ADR-0002 proposed fails"
  new_tree core-e
  mk_doc "$T/docs/adr/0002-x.md" accepted
  mkdir -p "$T/core"
  printf 'int x;\n' > "$T/core/x.c"
  expect_absent DOC-CORE-BEFORE-ADR "(e) an extra core/x.c with ADR-0002 accepted passes"
  new_tree core-f
  rm -f "$T"/docs/adr/0002-*.md
  mkdir -p "$T/core/include/tendies64"
  printf '/* placeholder: this file claims to be a stub with no behavior */\n' > "$T/core/t64_core_stub.c"
  printf '/* facade */\n' > "$T/core/include/tendies64/t64_facade.h"
  printf 'int z;\n' > "$T/core/z.c"
  expect_fail DOC-CORE-BEFORE-ADR "(f) stub-claiming allowlisted text plus an unmarked extra file fails: content is never consulted"

  echo "check-docs selftest: cases=$CASES failures=$FAILS"
  if [ "$FAILS" -ne 0 ]; then return 1; fi
  return 0
}

# ---- main -----------------------------------------------------------------------------------

ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
MODE=check
while [ $# -gt 0 ]; do
  case "$1" in
    --root)
      if [ $# -lt 2 ]; then usage; exit 2; fi
      ROOT=$2
      shift 2
      ;;
    --selftest)
      MODE=selftest
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage
      exit 2
      ;;
  esac
done

if [ "$MODE" = "selftest" ]; then
  if selftest; then exit 0; else exit 1; fi
fi

if [ ! -d "$ROOT" ]; then
  echo "check-docs: root is not a directory: $ROOT" >&2
  exit 2
fi
ROOT=$(cd "$ROOT" && pwd -P)
OUT=$(scan_tree "$ROOT") || { echo "check-docs: tool error while scanning $ROOT" >&2; exit 2; }
if [ -n "$OUT" ]; then
  printf '%s\n' "$OUT"
  exit 1
fi
echo "check-docs: ok"
exit 0
