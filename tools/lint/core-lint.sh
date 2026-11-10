#!/bin/sh
# Determinism lint entry point (PROF-07). Two layers over the same file set:
#   Layer 1: POSIX awk source scan that explains violations (comments, strings
#            and character literals are stripped first).
#   Layer 2: lint-objects.sh compiles each .c file with fixed flags and checks
#            sections and undefined symbols (authoritative).
# Usage:
#   core-lint.sh                scan core/**/*.c and *.h (headers: Layer 1 only)
#   core-lint.sh --root DIR     the same over DIR
#   core-lint.sh --files F...   the same over the named .c/.h files
#   core-lint.sh --selftest     run the seeded corpus and the controls
# Output: "RULE-ID path:line: message (hint: ...)" (Layer 1), "RULE-ID path:
# message (hint: ...)" (Layer 2) and a final "core-lint: files=N violations=M".
# Exit 0 clean, 1 violations, 2 usage or tool error.
set -eu
LC_ALL=C
export LC_ALL

LINT_DIR=$(cd "$(dirname "$0")" && pwd -P)
REPO=$(cd "$LINT_DIR/../.." && pwd -P)
RULES="$LINT_DIR/rules.txt"
SELFDIR="$LINT_DIR/selftest"
SELF="$LINT_DIR/core-lint.sh"

usage() {
  echo "usage: core-lint.sh [--root DIR | --files F... | --selftest]" >&2
  exit 2
}

# Layer-1 awk program. Rule ids and hints come from rules.txt.
L1_AWK='
function report(id, ln, msg,   h) {
  h = (id in hint) ? hint[id] : "no hint recorded"
  printf "%s %s:%d: %s (hint: %s)\n", id, FILENAME, ln, msg, h
}

# Strip comments, string contents and character-literal contents. Block-comment
# state is carried across lines; a // comment ending in a backslash continues.
function strip(s,   out, i, n, c, d, q) {
  out = ""; n = length(s); i = 1
  if (linecont) { linecont = (n > 0 && substr(s, n, 1) == bs); return "" }
  while (i <= n) {
    c = substr(s, i, 1)
    if (inblk) {
      if (c == "*" && substr(s, i + 1, 1) == "/") { inblk = 0; i += 2; out = out " " } else i++
      continue
    }
    d = substr(s, i, 2)
    if (d == "/*") { inblk = 1; i += 2; continue }
    if (d == "//") { linecont = (substr(s, n, 1) == bs); break }
    if (c == "\"" || c == sq) {
      q = c; out = out q; i++
      while (i <= n) {
        c = substr(s, i, 1)
        if (c == bs) { i += 2; continue }
        if (c == q) break
        i++
      }
      out = out q; i++
      continue
    }
    out = out c; i++
  }
  return out
}

function check_include(c,   h, p) {
  if (!match(c, /^[ \t]*#[ \t]*include[ \t]*</)) return
  h = substr(c, RLENGTH + 1)
  p = index(h, ">"); if (p > 0) h = substr(h, 1, p - 1)
  if ((h in banhdr) || substr(h, 1, 4) == "sys/") report("L1-INCLUDE-BANNED", FNR, "banned header <" h ">")
  else if (substr(h, 1, 9) == "tendies64/") { }
  else if (!(h in okhdr)) report("L1-INCLUDE-UNLISTED", FNR, "header <" h "> is not on the core allowlist")
}

function addnames(rule, list,   n, a, i) {
  n = split(list, a, " ")
  for (i = 1; i <= n; i++) nameRule[a[i]] = rule
}

function addprefixes(rule, list,   n, a, i) {
  n = split(list, a, " ")
  for (i = 1; i <= n; i++) { nprefix++; pfx[nprefix] = a[i]; pfxRule[nprefix] = rule }
}

# Attribute-like constructs take a parenthesised argument list that is skipped
# as a unit. Returns 1 for every token that belongs to such a construct.
function attr_track(t) {
  if (attrdepth > 0) {
    if (t == "(") attrdepth++
    else if (t == ")") attrdepth--
    return 1
  }
  if (attrwait) {
    attrwait = 0
    if (t == "(") { attrdepth = 1; return 1 }
    return 0
  }
  if (t == "__attribute__" || t == "__attribute" || t == "__declspec" || t == "_Alignas" || t == "alignas" || t == "_Static_assert" || t == "static_assert") {
    attrwait = 1
    return 1
  }
  return 0
}

# Top-level declaration tracking for the file-scope state heuristics. Tokens
# are collected only at brace depth 0; bodies are skipped as units.
function feed(t) {
  if (depth > 0) return
  if (dn == 0) dline = FNR
  dn++; dt[dn] = t
}

function open_brace(   i) {
  if (depth == 0) {
    bodykind = 3
    if (dn > 0 && dt[dn] == ")") bodykind = 1
    else {
      for (i = 1; i <= dn; i++) if (dt[i] == "=") bodykind = 2
      if (bodykind == 3) { if (dn == 0) dline = FNR; dn++; dt[dn] = "{}" }
    }
  }
  depth++
}

function close_brace() {
  if (depth > 0) depth--
  if (depth == 0) { if (bodykind == 1) dn = 0; bodykind = 0 }
}

# A top-level declaration ended with ";". Report a non-const object.
function end_decl(   i, lim, eq, fp, st, ex, td, cn, rem, skipnext, t) {
  if (dn == 0) return
  eq = 0; fp = 0
  for (i = 1; i <= dn; i++) if (dt[i] == "=") { eq = i; break }
  lim = (eq > 0) ? eq - 1 : dn
  st = 0; ex = 0; td = 0; cn = 0; rem = 0; skipnext = 0
  for (i = 1; i <= lim; i++) {
    t = dt[i]
    if (t == "static") st = 1
    else if (t == "extern") ex = 1
    else if (t == "typedef") td = 1
    else if (t == "const") cn = 1
    if (t == "(" && fp == 0) fp = i
    if (skipnext) { skipnext = 0; continue }
    if (t == "struct" || t == "union" || t == "enum") { skipnext = 1; continue }
    if (t == "{}" || (t in declq)) continue
    rem++
  }
  dn = 0
  if (td || ex) return
  if (rem == 0) return
  if (fp > 0 && dt[fp + 1] != "*") return
  if (cn) return
  if (st) report("L1-STATIC-MUTABLE", dline, "non-const static object at file scope")
  else report("L1-GLOBAL-MUTABLE", dline, "non-const object with external linkage at file scope")
}

# A non-hex pp-number with "." or an e/E exponent, or a hex one with "." or a
# p/P exponent, is a floating-point literal.
function number(t) {
  if (t ~ /^0[xX]/) {
    if (t ~ /[.pP]/) report("L1-FLOAT-LITERAL", FNR, "floating-point literal " t)
  } else if (t ~ /[.eE]/) report("L1-FLOAT-LITERAL", FNR, "floating-point literal " t)
}

function ident(tok, pp,   i) {
  if (attr_track(tok)) {
    if (attrdepth > 0 && (tok in ctorword)) report("L1-ATTR-CTOR", FNR, "attribute \"" tok "\" runs code at load time")
    return
  }
  if (tok in nameRule) report(nameRule[tok], FNR, "banned name \"" tok "\"")
  else {
    for (i = 1; i <= nprefix; i++)
      if (substr(tok, 1, length(pfx[i])) == pfx[i]) { report(pfxRule[i], FNR, "banned name \"" tok "\""); break }
  }
  if (!pp) feed(tok)
}

function punct(ch, pp) {
  if (attr_track(ch)) return
  if (pp) return
  if (ch == "{") open_brace()
  else if (ch == "}") close_brace()
  else if (depth > 0) return
  else if (ch == ";") end_decl()
  else feed(ch)
}

function scan(c, pp,   rest, ch, tok) {
  rest = c
  while (length(rest) > 0) {
    ch = substr(rest, 1, 1)
    if (ch == " " || ch == "\t") { rest = substr(rest, 2); continue }
    if (ch ~ /[A-Za-z_]/) {
      match(rest, /^[A-Za-z_][A-Za-z0-9_]*/)
      tok = substr(rest, 1, RLENGTH); rest = substr(rest, RLENGTH + 1)
      ident(tok, pp)
      continue
    }
    if (ch ~ /[0-9]/ || (ch == "." && substr(rest, 2, 1) ~ /[0-9]/)) {
      match(rest, /^[0-9.]([eEpP][+-]|[A-Za-z0-9_.])*/)
      tok = substr(rest, 1, RLENGTH); rest = substr(rest, RLENGTH + 1)
      number(tok)
      continue
    }
    punct(ch, pp)
    rest = substr(rest, 2)
  }
}

BEGIN {
  sq = sprintf("%c", 39); bs = "\\"
  while ((getline ln < rules) > 0) {
    n = split(ln, f, "|")
    if (n >= 5 && f[1] ~ /^L[12]-/) hint[f[1]] = f[5]
  }
  close(rules)
  addnames("L1-NAME-TIME", "time clock clock_gettime gettimeofday timespec_get mach_absolute_time localtime gmtime mktime difftime strftime")
  addnames("L1-NAME-ENTROPY", "rand srand rand_r random srandom drand48 arc4random arc4random_buf getrandom getentropy")
  addnames("L1-NAME-FILE", "fopen freopen fclose fread fwrite fseek ftell fflush open openat creat unlink remove rename mmap munmap stat fstat lstat opendir readdir")
  addnames("L1-NAME-STDIO", "printf fprintf sprintf snprintf vprintf puts putchar getchar scanf sscanf perror")
  addnames("L1-NAME-ALLOC", "malloc calloc realloc free aligned_alloc posix_memalign alloca")
  addnames("L1-NAME-ENV", "getenv secure_getenv setenv setlocale")
  addprefixes("L1-NAME-THREAD", "pthread_ thrd_ mtx_ cnd_ tss_")
  addnames("L1-TOKEN-FLOAT", "float double")
  addnames("L1-BUILD-MACRO", "__DATE__ __TIME__ __TIMESTAMP__")
  addnames("L1-ASM", "asm __asm __asm__")
  addnames("L1-TLS", "__thread _Thread_local thread_local")
  addnames("L1-TOKEN-ATOMIC", "_Atomic")
  addnames("L1-RUNTIME-HELPER", "__int128")
  addprefixes("L1-RUNTIME-HELPER", "__builtin_popcount")
  n = split("constructor destructor __constructor__ __destructor__", a, " ")
  for (i = 1; i <= n; i++) ctorword[a[i]] = 1
  n = split("static extern const volatile register inline _Thread_local __inline __inline__ restrict _Noreturn auto", a, " ")
  for (i = 1; i <= n; i++) declq[a[i]] = 1
  n = split("stdio.h stdlib.h math.h time.h errno.h assert.h setjmp.h signal.h pthread.h threads.h stdatomic.h unistd.h fcntl.h", a, " ")
  for (i = 1; i <= n; i++) banhdr[a[i]] = 1
  n = split("stdint.h stddef.h stdbool.h stdalign.h limits.h string.h", a, " ")
  for (i = 1; i <= n; i++) okhdr[a[i]] = 1
}

{
  code = strip($0)
  ispp = ppcont || (code ~ /^[ \t]*#/)
  isinc = !ppcont && (code ~ /^[ \t]*#[ \t]*include/)
  ppcont = ispp && (substr(code, length(code), 1) == bs)
  if (isinc) { check_include(code); next }
  scan(code, ispp)
}
'

# collect_files MODE ARG...: write the sorted file list to $TMP/files.
# Modes: root DIR | files F...
collect_files() {
  _mode=$1
  shift
  : > "$TMP/files.raw"
  if [ "$_mode" = root ]; then
    [ -d "$1" ] || { echo "core-lint: root is not a directory: $1" >&2; exit 2; }
    find "$1" -type f \( -name '*.c' -o -name '*.h' \) > "$TMP/files.raw" || exit 2
  else
    for _f in "$@"; do
      [ -f "$_f" ] || { echo "core-lint: no such file: $_f" >&2; exit 2; }
      case "$_f" in
        *.c|*.h) printf '%s\n' "$_f" >> "$TMP/files.raw" ;;
        *) echo "core-lint: not a .c or .h file: $_f" >&2; exit 2 ;;
      esac
    done
  fi
  sort "$TMP/files.raw" > "$TMP/files"
}

# lint_list [-I DIR]...: run both layers over the files listed in $TMP/files.
# Prints violations and the summary line; returns 0 clean, 1 violations.
lint_list() {
  _nfiles=$(wc -l < "$TMP/files" | tr -d ' ')
  _incs=""
  while [ $# -gt 0 ]; do
    _incs="$_incs $1 $2"
    shift 2
  done
  : > "$TMP/l1.out"
  : > "$TMP/l2.out"
  : > "$TMP/clist"
  _ifs=$IFS
  IFS='
'
  set -f
  for _f in $(cat "$TMP/files"); do
    awk -v rules="$RULES" "$L1_AWK" "$_f" >> "$TMP/l1.out" || { echo "core-lint: awk failed on $_f" >&2; exit 2; }
    case "$_f" in
      *.c) printf '%s\n' "$_f" >> "$TMP/clist" ;;
    esac
  done
  set +f
  IFS=$_ifs
  if [ -s "$TMP/clist" ]; then
    _rc=0
    # Include flags first, then the .c list; both are space-free paths.
    # shellcheck disable=SC2086
    sh "$LINT_DIR/lint-objects.sh" $_incs -- $(cat "$TMP/clist") > "$TMP/l2.out" || _rc=$?
    if [ "$_rc" -gt 1 ]; then
      echo "core-lint: lint-objects.sh failed (exit $_rc)" >&2
      exit 2
    fi
  fi
  _nviol=0
  if [ "$_nfiles" -eq 0 ]; then
    printf '%s\n' "$(zero_files_line)" >> "$TMP/l1.out"
  fi
  cat "$TMP/l1.out" "$TMP/l2.out"
  _n1=$(grep -c '^L[12]-' "$TMP/l1.out" || true)
  _n2=$(grep -c '^L[12]-' "$TMP/l2.out" || true)
  _nviol=$((_n1 + _n2))
  echo "core-lint: files=$_nfiles violations=$_nviol"
  [ "$_nviol" -eq 0 ]
}

zero_files_line() {
  _h=$(awk -F'|' '$1 == "L1-ZERO-FILES" { print $5; exit }' "$RULES")
  printf 'L1-ZERO-FILES %s: no .c or .h files were scanned (hint: %s)' "${SCAN_ROOT:-core}" "$_h"
}

# expect_ids SEED: print the ids on the "/* EXPECT: ... */" first line, or fail.
expect_ids() {
  head -n 1 "$1" | awk '
    /^\/\* EXPECT:[ \t]/ { s = $0; sub(/^\/\* EXPECT:[ \t]*/, "", s); sub(/[ \t]*\*\/.*$/, "", s); print s; found = 1; exit }
    END { exit !found }'
}

selftest() {
  cd "$REPO"
  _fail=0
  _seeds=0
  _ok=0
  for _seed in tools/lint/selftest/bad_*.c tools/lint/selftest/good.c; do
    [ -f "$_seed" ] || continue
    _seeds=$((_seeds + 1))
    _ids=$(expect_ids "$_seed") || {
      echo "SELFTEST-SEED-MALFORMED $_seed: first line must be /* EXPECT: ID... */ or /* EXPECT: PASS */"
      _fail=$((_fail + 1))
      continue
    }
    if [ "$_ids" != PASS ]; then
      for _id in $_ids; do printf '%s\n' "$_id" >> "$TMP/expect.ids"; done
    fi
    _rc=0
    sh "$SELF" --files "$_seed" > "$TMP/seed.out" 2> "$TMP/seed.err" || _rc=$?
    if [ "$_ids" = PASS ]; then
      if [ "$_rc" -eq 0 ]; then
        echo "selftest ok: $_seed passes with no violations"
        _ok=$((_ok + 1))
      else
        echo "SELFTEST-FAIL $_seed: expected a clean pass, got exit $_rc"
        cat "$TMP/seed.out" "$TMP/seed.err"
        _fail=$((_fail + 1))
      fi
      continue
    fi
    _missing=""
    for _id in $_ids; do
      grep -q "^$_id " "$TMP/seed.out" || _missing="$_missing $_id"
    done
    if [ "$_rc" -ne 1 ]; then
      echo "SELFTEST-FAIL $_seed: expected exit 1, got $_rc"
      cat "$TMP/seed.out" "$TMP/seed.err"
      _fail=$((_fail + 1))
    elif [ -n "$_missing" ]; then
      echo "SELFTEST-FAIL $_seed: output lacks expected id(s):$_missing"
      cat "$TMP/seed.out"
      _fail=$((_fail + 1))
    else
      echo "selftest ok: $_seed reports $_ids"
      _ok=$((_ok + 1))
    fi
  done
  if [ "$_seeds" -eq 0 ]; then
    echo "SELFTEST-FAIL no seeds found under tools/lint/selftest"
    _fail=$((_fail + 1))
  fi

  # Coverage gate: every non-meta rule in rules.txt appears in some EXPECT
  # line, every EXPECT id is a rule in rules.txt, and a meta rule is only
  # legitimate when this function has a dedicated control for it.
  : >> "$TMP/expect.ids"
  sort -u "$TMP/expect.ids" > "$TMP/expect.s"
  awk -F'|' '/^L[12]-/ { print $1 }' "$RULES" > "$TMP/rules.all"
  sort -u "$TMP/rules.all" > "$TMP/rules.all.s"
  awk -F'|' '/^L[12]-/ && $3 != "yes" { print $1 }' "$RULES" > "$TMP/rules.seeded"
  sort -u "$TMP/rules.seeded" > "$TMP/rules.seeded.s"
  awk -F'|' '/^L[12]-/ && $3 == "yes" { print $1 }' "$RULES" > "$TMP/rules.meta"
  sort -u "$TMP/rules.meta" > "$TMP/rules.meta.s"
  printf '%s\n' L1-ZERO-FILES L2-COMPILE L2-W-COMMON | sort > "$TMP/controls.s"
  _nrules=$(wc -l < "$TMP/rules.all.s" | tr -d ' ')
  comm -23 "$TMP/rules.seeded.s" "$TMP/expect.s" > "$TMP/unproven"
  comm -23 "$TMP/rules.meta.s" "$TMP/controls.s" >> "$TMP/unproven"
  comm -23 "$TMP/expect.s" "$TMP/rules.all.s" > "$TMP/unknown"
  while IFS= read -r _id; do
    echo "SELFTEST-RULE-UNPROVEN $_id"
    _fail=$((_fail + 1))
  done < "$TMP/unproven"
  while IFS= read -r _id; do
    echo "SELFTEST-RULE-UNKNOWN $_id"
    _fail=$((_fail + 1))
  done < "$TMP/unknown"
  if [ "$_nrules" -eq 0 ]; then
    echo "SELFTEST-FAIL rules.txt lists no rules"
    _fail=$((_fail + 1))
  elif [ ! -s "$TMP/unproven" ] && [ ! -s "$TMP/unknown" ]; then
    echo "selftest ok: coverage gate checked $_nrules rule ids against the seeds"
  fi

  # Control 1: scanning zero files must fail (green-on-empty control).
  mkdir "$TMP/emptyroot"
  _rc=0
  sh "$SELF" --root "$TMP/emptyroot" > "$TMP/ctl.out" 2>&1 || _rc=$?
  if [ "$_rc" -eq 1 ] && grep -q '^L1-ZERO-FILES ' "$TMP/ctl.out"; then
    echo "selftest ok: empty root reports L1-ZERO-FILES"
  else
    echo "SELFTEST-FAIL empty-root control: expected exit 1 with L1-ZERO-FILES, got $_rc"
    cat "$TMP/ctl.out"
    _fail=$((_fail + 1))
  fi

  # Control 2: a file that does not compile must report L2-COMPILE.
  printf '/* EXPECT: control */\nint t64_broken( { return ; }\n' > "$TMP/broken.c"
  _rc=0
  sh "$SELF" --files "$TMP/broken.c" > "$TMP/ctl.out" 2>/dev/null || _rc=$?
  if [ "$_rc" -eq 1 ] && grep -q '^L2-COMPILE ' "$TMP/ctl.out"; then
    echo "selftest ok: syntax error reports L2-COMPILE"
  else
    echo "SELFTEST-FAIL compile control: expected exit 1 with L2-COMPILE, got $_rc"
    cat "$TMP/ctl.out"
    _fail=$((_fail + 1))
  fi

  # Control 3: a common symbol (only a precompiled object can carry one, since
  # the lint flags include -fno-common) must report L2-W-COMMON via --objects.
  printf 'int t64_tentative;\n' > "$TMP/common.c"
  _rc=0
  # shellcheck disable=SC2086
  ${CC:-cc} -std=c17 -fcommon -c "$TMP/common.c" -o "$TMP/common.o" 2> "$TMP/common.err" || _rc=$?
  if [ "$_rc" -ne 0 ]; then
    echo "SELFTEST-FAIL common control: could not build the -fcommon object"
    cat "$TMP/common.err"
    _fail=$((_fail + 1))
  else
    _rc=0
    sh "$LINT_DIR/lint-objects.sh" --objects "$TMP/common.o" > "$TMP/ctl.out" 2>&1 || _rc=$?
    if [ "$_rc" -eq 1 ] && grep -q '^L2-W-COMMON ' "$TMP/ctl.out"; then
      echo "selftest ok: -fcommon object reports L2-W-COMMON"
    else
      echo "SELFTEST-FAIL common control: expected exit 1 with L2-W-COMMON, got $_rc"
      cat "$TMP/ctl.out"
      _fail=$((_fail + 1))
    fi
  fi

  echo "core-lint selftest: seeds=$_seeds seeds_ok=$_ok failures=$_fail"
  [ "$_fail" -eq 0 ]
}

[ -f "$RULES" ] || { echo "core-lint: rules.txt missing" >&2; exit 2; }
TMP=$(mktemp -d "${TMPDIR:-/tmp}/t64lint.XXXXXX")
trap 'rm -rf "$TMP"' EXIT
trap 'exit 2' INT TERM HUP

SCAN_ROOT=core
if [ $# -eq 0 ]; then
  cd "$REPO"
  collect_files root core
  lint_list
  exit $?
fi
case "$1" in
  --selftest)
    [ $# -eq 1 ] || usage
    selftest
    ;;
  --root)
    [ $# -eq 2 ] || usage
    SCAN_ROOT=$2
    collect_files root "$2"
    lint_list -I "$2" -I "$2/include"
    ;;
  --files)
    shift
    [ $# -gt 0 ] || usage
    SCAN_ROOT=files
    collect_files files "$@"
    lint_list
    ;;
  *) usage ;;
esac
