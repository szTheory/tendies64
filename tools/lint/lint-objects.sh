#!/bin/sh
# Determinism lint, Layer 2 (authoritative, PROF-07): compile each core .c file
# with fixed flags and check the object for writable data, commons,
# constructors and undefined symbols that the core does not define and
# allowlist.txt does not allow. Objects are only compiled, never linked or run.
#
# Usage:
#   lint-objects.sh [-I DIR]... [--] FILE.c...   compile, then check
#   lint-objects.sh --objects OBJ...             check precompiled objects only
# Output: "RULE-ID path: message (hint: ...)" per violation. Exit 0 clean,
# 1 violations, 2 usage or tool error. Include paths must not contain spaces.
set -eu
LC_ALL=C
export LC_ALL

LINT_DIR=$(cd "$(dirname "$0")" && pwd -P)
REPO=$(cd "$LINT_DIR/../.." && pwd -P)
RULES="$LINT_DIR/rules.txt"
ALLOW="$LINT_DIR/allowlist.txt"
OS=$(uname -s)
NVIOL=0

usage() {
  echo "usage: lint-objects.sh [-I DIR]... [--] FILE.c... | --objects OBJ..." >&2
  exit 2
}

hint_of() {
  awk -F'|' -v id="$1" '$1 == id { print $5; exit }' "$RULES"
}

# viol RULE PATH MESSAGE
viol() {
  printf '%s %s: %s (hint: %s)\n' "$1" "$2" "$3" "$(hint_of "$1")"
  NVIOL=$((NVIOL + 1))
}

MODE=compile
INCFLAGS=""
while [ $# -gt 0 ]; do
  case "$1" in
    --objects) MODE=objects; shift; break ;;
    -I) [ $# -ge 2 ] || usage; INCFLAGS="$INCFLAGS -I$2"; shift 2 ;;
    --) shift; break ;;
    -*) usage ;;
    *) break ;;
  esac
done
[ $# -gt 0 ] || usage

for _tool in nm size ${CC:-cc}; do
  if ! command -v "$_tool" >/dev/null 2>&1; then
    echo "lint-objects: required tool not found: $_tool" >&2
    exit 2
  fi
done
[ -f "$RULES" ] && [ -f "$ALLOW" ] || { echo "lint-objects: rules.txt or allowlist.txt missing" >&2; exit 2; }

TMP=$(mktemp -d "${TMPDIR:-/tmp}/t64lint.XXXXXX")
trap 'rm -rf "$TMP"' EXIT
trap 'exit 2' INT TERM HUP

# LIST holds "object|name" lines: name is the path used in messages.
LIST="$TMP/list"
: > "$LIST"
_i=0
for _f in "$@"; do
  _i=$((_i + 1))
  if [ "$MODE" = objects ]; then
    printf '%s|%s\n' "$_f" "$_f" >> "$LIST"
    continue
  fi
  _obj="$TMP/obj$_i.o"
  # shellcheck disable=SC2086
  if ${CC:-cc} -std=c17 -O2 -fno-common -fno-stack-protector -fno-pic \
      -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -I"$REPO/core/include" \
      -I"$(dirname "$_f")" $INCFLAGS -c "$_f" -o "$_obj" 2> "$TMP/cc$_i.err"; then
    printf '%s|%s\n' "$_obj" "$_f" >> "$LIST"
  else
    viol L2-COMPILE "$_f" "compiler rejected the file"
    sed 's/^/    cc: /' "$TMP/cc$_i.err" >&2
  fi
done

# Allowlist for this OS, sorted. A malformed row is a tool error.
awk -F'|' -v os="$OS" '
  /^[ \t]*#/ || /^[ \t]*$/ { next }
  NF != 4 { print "lint-objects: malformed allowlist row " NR ": " $0 > "/dev/stderr"; bad = 1; next }
  { n = $1; o = $4; gsub(/[ \t]/, "", n); gsub(/[ \t]/, "", o)
    if (o == "any" || o == os) print n }
  END { exit bad }' "$ALLOW" > "$TMP/allow.raw" || exit 2
sort -u "$TMP/allow.raw" > "$TMP/allow.sorted"

: > "$TMP/def.all"
_i=0
while IFS='|' read -r _obj _name; do
  _i=$((_i + 1))

  # Sections: size -m on Mach-O, size -A on ELF. Size-zero sections are ignored.
  if [ "$OS" = Darwin ]; then
    size -m "$_obj" > "$TMP/size.out" || { echo "lint-objects: size failed on $_name" >&2; exit 2; }
  else
    size -A "$_obj" > "$TMP/size.out" || { echo "lint-objects: size failed on $_name" >&2; exit 2; }
  fi
  awk -v os="$OS" '
    os == "Darwin" && /^[ \t]*Section \(/ {
      l = $0; sub(/^[ \t]*Section \(/, "", l)
      p = index(l, "): "); if (p == 0) next
      s = substr(l, 1, p - 1); gsub(/, /, ",", s); z = substr(l, p + 3) + 0
      if (z > 0 && s ~ /^__DATA,(__mod_init_func|__mod_term_func)/) print "C|" s "|" z
      else if (z > 0 && s ~ /^__DATA,(__data|__bss|__common|__thread_)/) print "W|" s "|" z
    }
    os != "Darwin" && NF == 3 && $2 ~ /^[0-9]+$/ && $2 + 0 > 0 {
      n = $1
      if (n ~ /^\.(init_array|fini_array|preinit_array|ctors|dtors)(\.|$)/) print "C|" n "|" $2
      else if (n ~ /^\.(data|bss|tdata|tbss|sdata|sbss|ldata|lbss)(\.|$)/ && n !~ /^\.data\.rel\.ro/) print "W|" n "|" $2
    }' "$TMP/size.out" > "$TMP/sec.out"
  while IFS='|' read -r _kind _sec _sz; do
    case "$_kind" in
      W) viol L2-W-SECTION "$_name" "writable data section $_sec ($_sz bytes)" ;;
      C) viol L2-CTOR "$_name" "constructor or destructor section $_sec ($_sz bytes)" ;;
    esac
  done < "$TMP/sec.out"

  # Symbols: undefined (U, w, v), common (C, c) and defined (other letters).
  nm -P "$_obj" > "$TMP/nm.out" || { echo "lint-objects: nm failed on $_name" >&2; exit 2; }
  : > "$TMP/und.$_i"
  : > "$TMP/def.$_i"
  awk -v os="$OS" -v und="$TMP/und.$_i" -v def="$TMP/def.$_i" '
    { n = $1; t = $2
      if (os == "Darwin") sub(/^_/, "", n)
      if (t == "U" || t == "w" || t == "v") print n >> und
      else if (t == "C" || t == "c") print "COMMON|" n
      else if (t ~ /^[A-Z]$/) print n >> def }' "$TMP/nm.out" > "$TMP/com.out"
  while IFS='|' read -r _kind _sym; do
    viol L2-W-COMMON "$_name" "common symbol '$_sym'"
  done < "$TMP/com.out"
  sort -u "$TMP/und.$_i" > "$TMP/und.$_i.s"
  sort -u "$TMP/def.$_i" > "$TMP/def.$_i.s"
  cat "$TMP/def.$_i.s" >> "$TMP/def.all"
done < "$LIST"
sort -u "$TMP/def.all" > "$TMP/def.all.s"

# Undefined symbols that no object in the set defines and the allowlist lacks.
_i=0
while IFS='|' read -r _obj _name; do
  _i=$((_i + 1))
  comm -23 "$TMP/und.$_i.s" "$TMP/def.all.s" > "$TMP/miss1.$_i"
  comm -23 "$TMP/miss1.$_i" "$TMP/allow.sorted" > "$TMP/miss2.$_i"
  while IFS= read -r _sym; do
    viol L2-UNDEF-SYMBOL "$_name" "undefined symbol '$_sym' is neither defined by the core nor on tools/lint/allowlist.txt"
  done < "$TMP/miss2.$_i"
done < "$LIST"

[ "$NVIOL" -eq 0 ] || exit 1
exit 0
