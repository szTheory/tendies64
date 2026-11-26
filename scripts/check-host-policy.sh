#!/bin/sh
# Host policy scan (Phase 12, plan 12-09, API-05, threats T-12-28 to T-12-31, decisions DR-16 to DR-18).
#
# The minimal host of host/ may use a system SDL3 and nothing else of it, and it may show
# nothing that names the product. This guard turns those project rules into findings over every
# file below host/ (one finding line per violation, "RULE path:line: text"):
#
#   HP-FETCH        FetchContent in a CMake file (SDL3 is never fetched)
#   HP-EXTPROJ      ExternalProject in a CMake file
#   HP-ADDSUB       add_subdirectory of an SDL, third_party or vendor path (SDL3 is never built from source)
#   HP-VENDOR-DIR   a directory named third_party, third-party, thirdparty, vendor or vendored
#   HP-VENDOR-FILE  a file named sdl*.c or sdl*.h (any letter case): a vendored SDL source
#   HP-INSTALL      an install( rule in a CMake file (the host is never installed or shipped)
#   HP-BUNDLE       MACOSX_BUNDLE (no application bundle)
#   HP-ICON         a *.icns, *.ico or *.rc file (no icon or resource script)
#   HP-PRODUCT      a string literal holding the product name, in any letter case, in a C or header
#                   file, or a quoted CMake string that does (find_package lines are exempt: the
#                   CMake package name is not user-visible)
#   HP-TITLE        a window title other than exactly "t64 minimal host (development)": a literal
#                   that names the minimal host differently, a T64H_SDL_TITLE definition that is not
#                   the exact title, an SDL_CreateWindow or SDL_SetWindowTitle call whose title
#                   argument is neither T64H_SDL_TITLE nor the exact literal, or a window created
#                   through properties (a title that cannot be read from the call)
#   HP-RENDERER     SDL_CreateRenderer and the renderer, GPU, OpenGL, Vulkan and Metal window paths:
#                   the shell presents through the window surface only (decision DR-18)
#   HP-KEYBOARD     a keyboard or text-input event, scancode or keycode below host/sdl: the shell is
#                   controller-only (decision DR-12)
#
# Comments are removed before the C rules read a file (so a comment may name a forbidden thing),
# and the contents of string literals are removed before the token rules read it (so a string
# may name one); the literal rules read the literals themselves.
#
# Usage:
#   check-host-policy.sh              scan the repository containing this script
#   check-host-policy.sh --root DIR   scan DIR instead
#   check-host-policy.sh --selftest   seeded cases in temporary copies of host/
# Output: the findings and a final "host-policy: files=N findings=M".
# Exit 0 clean, 1 findings (or a failed selftest), 2 usage error or a scan that covered zero
# files. POSIX sh, find and awk only, no git needed.
set -eu
LC_ALL=C
export LC_ALL

HOST_DIR=host
TITLE='t64 minimal host (development)'

HERE=$(cd "$(dirname "$0")" && pwd -P)
ROOT=$(cd "$HERE/.." && pwd -P)

usage() {
  echo "usage: check-host-policy.sh [--root DIR | --selftest]" >&2
  exit 2
}

# scan_c ROOT FILE...: print the findings of the C and header files. Exit 0 clean, 1 findings,
# 2 an awk failure.
scan_c() {
  _root=$1
  shift
  awk -v root="$_root/" -v title="$TITLE" '
    function bad(rule, n, text) { printf "%s %s:%d: %s\n", rule, cur_rel, n, text; found++ }
    function trim(s) { sub(/^[ \t\r\n]+/, "", s); sub(/[ \t\r\n]+$/, "", s); return s }
    # strip: g_full is the line without comments (literals kept), g_bare is the same with the
    # contents of the literals dropped.
    function strip(line,    i, n, c, d, q) {
      g_full = ""; g_bare = ""; q = ""; n = length(line)
      if (!in_block && line !~ /[\/"\047]/) { g_full = line; g_bare = line; return }
      for (i = 1; i <= n; i++) {
        c = substr(line, i, 1); d = substr(line, i, 2)
        if (in_block) { if (d == "*/") { in_block = 0; i++ } continue }
        if (q != "") {
          g_full = g_full c
          if (c == "\\") { i++; g_full = g_full substr(line, i, 1) }
          else if (c == q) { q = ""; g_bare = g_bare c }
          continue
        }
        if (d == "/*") { in_block = 1; i++; continue }
        if (d == "//") break
        if (c == "\"" || c == "\047") q = c
        g_full = g_full c; g_bare = g_bare c
      }
    }
    # literals: judge every double-quoted literal of one line of code.
    function literals(code, n, orig,    i, len, c, lit, inq, low) {
      len = length(code); inq = 0; lit = ""
      for (i = 1; i <= len; i++) {
        c = substr(code, i, 1)
        if (!inq) {
          if (c == "\"") { inq = 1; lit = "" }
          else if (c == "\047") {
            i++
            while (i <= len && substr(code, i, 1) != "\047") { if (substr(code, i, 1) == "\\") i++; i++ }
          }
        } else if (c == "\\") { lit = lit c substr(code, i + 1, 1); i++ }
        else if (c == "\"") {
          inq = 0
          low = tolower(lit)
          if (low ~ /tendies/) bad("HP-PRODUCT", n, orig)
          if (low ~ /minimal host/ && lit != title) { bad("HP-TITLE", n, orig); title_bad = 1 }
        } else lit = lit c
      }
    }
    function line_of(pos,    k) {
      for (k = nlines; k >= 1; k--) if (lstart[k] <= pos) return k
      return 1
    }
    # scan_call: the NAME(...) calls of the joined file; ARGNO is the title argument.
    function scan_call(name, argno,    rest, off, abs, start, j, c, cur, arg, depth, inq, re) {
      re = name "[ \t\n]*[(]"
      rest = all; off = 0
      while (match(rest, re)) {
        abs = off + RSTART
        start = abs + RLENGTH
        off = off + RSTART + RLENGTH - 1
        rest = substr(all, off + 1)
        if (abs > 1 && substr(all, abs - 1, 1) ~ /[A-Za-z0-9_]/) continue
        cur = 1; arg = ""; depth = 0; inq = ""
        for (j = start; j <= length(all); j++) {
          c = substr(all, j, 1)
          if (inq != "") {
            if (cur == argno) arg = arg c
            if (c == "\\") { j++; if (cur == argno) arg = arg substr(all, j, 1) }
            else if (c == inq) inq = ""
            continue
          }
          if (c == "\"" || c == "\047") { inq = c; if (cur == argno) arg = arg c; continue }
          if (c == "(") depth++
          else if (c == ")") { if (depth == 0) break; depth-- }
          else if (c == "," && depth == 0) { cur++; if (cur > argno) break; continue }
          if (cur == argno) arg = arg c
        }
        arg = trim(arg)
        if (arg != "T64H_SDL_TITLE" && arg != "\"" title "\"") {
          bad("HP-TITLE", line_of(abs), name "(" arg " ...) names a title other than the fixed one")
        }
      }
    }
    function finish_file() {
      if (cur_rel == "") return
      scan_call("SDL_CreateWindow", 1)
      scan_call("SDL_SetWindowTitle", 2)
    }
    FNR == 1 {
      finish_file()
      cur_rel = FILENAME
      if (index(cur_rel, root) == 1) cur_rel = substr(cur_rel, length(root) + 1)
      in_sdl = (cur_rel ~ /^host\/sdl\//)
      in_block = 0; all = ""; nlines = 0
    }
    {
      orig = $0
      title_bad = 0
      strip($0)
      nlines++; lstart[nlines] = length(all) + 1
      all = all g_full "\n"
      if (g_full !~ /^[ \t]*#[ \t]*include/) literals(g_full, FNR, orig)
      if (g_full ~ /^[ \t]*#[ \t]*define[ \t]+T64H_SDL_TITLE([ \t]|$)/) {
        def = g_full
        sub(/^[ \t]*#[ \t]*define[ \t]+T64H_SDL_TITLE[ \t]*/, "", def)
        if (!title_bad && trim(def) != "\"" title "\"") bad("HP-TITLE", FNR, orig)
      }
      if (g_bare ~ /(^|[^A-Za-z0-9_])SDL_(CreateWindowWithProperties|PROP_WINDOW_CREATE_TITLE_STRING)/) {
        bad("HP-TITLE", FNR, orig)
      }
      if (g_bare ~ /(^|[^A-Za-z0-9_])SDL_(CreateRenderer|CreateWindowAndRenderer|CreateSoftwareRenderer|Renderer|Render[A-Z]|CreateGPU|GPU|WINDOW_OPENGL|WINDOW_VULKAN|WINDOW_METAL|GL_)/) {
        bad("HP-RENDERER", FNR, orig)
      }
      if (in_sdl && g_bare ~ /(^|[^A-Za-z0-9_])(SDL_EVENT_KEY_|SDL_EVENT_TEXT_|SDL_GetKeyboard|SDL_KeyboardEvent|SDL_SCANCODE_|SDLK_|SDL_Scancode|SDL_Keycode|SDL_StartTextInput)/) {
        bad("HP-KEYBOARD", FNR, orig)
      }
    }
    END { finish_file(); exit found ? 1 : 0 }
  ' "$@"
}

# scan_cmake ROOT FILE...: print the findings of the CMake files (comments removed).
scan_cmake() {
  _root=$1
  shift
  awk -v root="$_root/" '
    function bad(rule, n, text) { printf "%s %s:%d: %s\n", rule, cur_rel, n, text; found++ }
    FNR == 1 {
      cur_rel = FILENAME
      if (index(cur_rel, root) == 1) cur_rel = substr(cur_rel, length(root) + 1)
    }
    {
      line = $0
      sub(/#.*/, "", line)
      low = tolower(line)
      if (low ~ /fetchcontent/) bad("HP-FETCH", FNR, $0)
      if (low ~ /externalproject/) bad("HP-EXTPROJ", FNR, $0)
      if (low ~ /add_subdirectory[ \t]*[(]/ && line ~ /(SDL|sdl3|third_party|third-party|thirdparty|vendor)/) {
        bad("HP-ADDSUB", FNR, $0)
      }
      if (low ~ /(^|[^a-z0-9_])install[ \t]*[(]/) bad("HP-INSTALL", FNR, $0)
      if (line ~ /MACOSX_BUNDLE/) bad("HP-BUNDLE", FNR, $0)
      if (low ~ /"[^"]*tendies[^"]*"/ && low !~ /find_package[ \t]*[(]/) bad("HP-PRODUCT", FNR, $0)
    }
    END { exit found ? 1 : 0 }
  ' "$@"
}

# scan_files ROOT LIST: the file-name rules over the list of files below host/ (one awk pass).
scan_files() {
  awk -v root="$1/" '
    {
      path = $0
      if (index(path, root) == 1) path = substr(path, length(root) + 1)
      base = path; sub(/^.*\//, "", base); base = tolower(base)
      dir = path; if (dir !~ /\//) dir = ""; else sub(/\/[^\/]*$/, "", dir)
      dir = "/" tolower(dir) "/"
      if (base ~ /^sdl.*\.[ch]$/) printf "HP-VENDOR-FILE %s:1: a vendored SDL source file\n", path
      if (base ~ /\.(icns|ico|rc)$/) printf "HP-ICON %s:1: an icon or resource file\n", path
      if (dir ~ /\/(third_party|third-party|thirdparty|vendor|vendored)\//) {
        printf "HP-VENDOR-DIR %s:1: a third_party or vendor directory\n", path
      }
    }
  ' "$2"
}

# scan DIR: print the findings and the summary; return 0 clean, 1 findings, 2 when nothing was
# scanned or a scanner failed.
scan() {
  _dir=$1
  _w=$(mktemp -d "${TMPDIR:-/tmp}/t64policy.XXXXXX")
  _tmp=$_w/findings
  : > "$_tmp"
  if [ -d "$_dir/$HOST_DIR" ]; then
    find "$_dir/$HOST_DIR" -type f | sort > "$_w/all"
  else
    : > "$_w/all"
  fi
  _files=$(wc -l < "$_w/all" | tr -d ' ')
  if [ "$_files" -eq 0 ]; then
    rm -rf "$_w"
    echo "host-policy: no host file to scan under $_dir/$HOST_DIR" >&2
    return 2
  fi
  grep -E '\.[ch]$' "$_w/all" > "$_w/c" || true
  grep -E '(^|/)CMakeLists\.txt$|\.cmake$' "$_w/all" > "$_w/cmake" || true
  scan_files "$_dir" "$_w/all" >> "$_tmp"
  # awk exits 1 for findings and 2 when it failed: a failed scan is never a clean one.
  for _kind in c cmake; do
    set --
    while IFS= read -r _f; do
      set -- "$@" "$_f"
    done < "$_w/$_kind"
    [ "$#" -gt 0 ] || continue
    _rc=0
    if [ "$_kind" = c ]; then
      scan_c "$_dir" "$@" >> "$_tmp" || _rc=$?
    else
      scan_cmake "$_dir" "$@" >> "$_tmp" || _rc=$?
    fi
    if [ "$_rc" -gt 1 ]; then
      rm -rf "$_w"
      echo "host-policy: the $_kind scanner failed (exit $_rc)" >&2
      return 2
    fi
  done
  _n=$(wc -l < "$_tmp" | tr -d ' ')
  cat "$_tmp"
  rm -rf "$_w"
  echo "host-policy: files=$_files findings=$_n"
  [ "$_n" -eq 0 ]
}

SELF_FAIL=0
SEEDED_RULES=
ALL_RULES='HP-FETCH HP-EXTPROJ HP-ADDSUB HP-VENDOR-DIR HP-VENDOR-FILE HP-INSTALL HP-BUNDLE HP-ICON HP-PRODUCT HP-TITLE HP-RENDERER HP-KEYBOARD'

# seed NAME EXPECT FILE ACTION TEXT [WANT]: copy the committed host/ (or its template) into a fresh tree, apply
# ACTION (append TEXT to FILE, or write FILE new with TEXT), scan it and require the outcome:
# clean, or fail with a line starting at WANT.
seed() {
  _name=$1
  _expect=$2
  _path=$3
  _action=$4
  _text=$5
  _want=${6:-}
  _tree="$SELF_TMP/$_name"
  mkdir -p "$_tree/$HOST_DIR"
  if [ "$_action" = none ]; then
    cp -R "$ROOT/$HOST_DIR/." "$_tree/$HOST_DIR/"
  else
    cp -R "$SELF_TMP/template/." "$_tree/$HOST_DIR/"
  fi
  case "$_action" in
    append) printf '%s\n' "$_text" >> "$_tree/$HOST_DIR/$_path" ;;
    create)
      mkdir -p "$(dirname "$_tree/$HOST_DIR/$_path")"
      printf '%s\n' "$_text" > "$_tree/$HOST_DIR/$_path"
      ;;
    none) : ;;
  esac
  _rc=0
  scan "$_tree" > "$_tree/out.txt" 2> "$_tree/err.txt" || _rc=$?
  case "$_expect" in
    clean)
      if [ "$_rc" -eq 0 ]; then
        echo "selftest ok: $_name is accepted"
      else
        echo "selftest FAIL: $_name should be accepted (exit $_rc)"
        cat "$_tree/out.txt"
        SELF_FAIL=$((SELF_FAIL + 1))
      fi
      ;;
    fail)
      if [ "$_rc" -eq 1 ] && grep -q "^$_want" "$_tree/out.txt"; then
        echo "selftest ok: $_name reports $_want"
        SEEDED_RULES="$SEEDED_RULES ${_want%% *}"
      else
        echo "selftest FAIL: $_name should report $_want (exit $_rc)"
        cat "$_tree/out.txt"
        SELF_FAIL=$((SELF_FAIL + 1))
      fi
      ;;
  esac
}

selftest() {
  [ -d "$ROOT/$HOST_DIR" ] || { echo "host-policy: no $HOST_DIR to seed from" >&2; exit 2; }
  SELF_TMP=$(mktemp -d "${TMPDIR:-/tmp}/t64policy-self.XXXXXX")
  trap 'rm -rf "$SELF_TMP"' EXIT
  # The seeded trees start from a small template (the host CMake file and the pump) so that the
  # selftest stays cheap; only the clean control reads the whole committed host.
  mkdir -p "$SELF_TMP/template/pump"
  cp "$ROOT/$HOST_DIR/CMakeLists.txt" "$SELF_TMP/template/CMakeLists.txt"
  cp "$ROOT/$HOST_DIR/pump/t64h_pump.c" "$ROOT/$HOST_DIR/pump/t64h_pump.h" "$SELF_TMP/template/pump/"
  _cm=CMakeLists.txt
  _pump=pump/t64h_pump.c
  _shell=sdl/seeded_shell.c
  _t='"t64 minimal host (development)"'
  # One seed per rule (and per way of breaking it), each reported with its own rule id.
  seed seeded-fetchcontent fail "$_cm" append 'include(FetchContent)' 'HP-FETCH host/CMakeLists.txt:'
  seed seeded-fetchcontent-declare fail "$_cm" append 'FetchContent_Declare(sdl3 URL https://example.invalid/x.tar.gz)' 'HP-FETCH '
  seed seeded-externalproject fail "$_cm" append 'include(ExternalProject)' 'HP-EXTPROJ host/CMakeLists.txt:'
  seed seeded-add-subdirectory-sdl fail "$_cm" append 'add_subdirectory(third_party/SDL sdl3)' 'HP-ADDSUB host/CMakeLists.txt:'
  seed seeded-add-subdirectory-sdl3-lower fail "$_cm" append 'add_subdirectory(deps/sdl3-src sdl3)' 'HP-ADDSUB '
  seed seeded-vendor-dir-third-party fail third_party/readme.txt create 'x' 'HP-VENDOR-DIR host/third_party/readme.txt:'
  seed seeded-vendor-dir-vendor fail sdl/vendor/x.txt create 'x' 'HP-VENDOR-DIR host/sdl/vendor/x.txt:'
  seed seeded-vendor-file-c fail sdl/SDL_video.c create '/* x */' 'HP-VENDOR-FILE host/sdl/SDL_video.c:'
  seed seeded-vendor-file-h fail sdl/SDL.h create '/* x */' 'HP-VENDOR-FILE host/sdl/SDL.h:'
  seed seeded-install-rule fail "$_cm" append 'install(TARGETS t64h_tests DESTINATION bin)' 'HP-INSTALL host/CMakeLists.txt:'
  seed seeded-macosx-bundle fail "$_cm" append 'set_target_properties(t64h_tests PROPERTIES MACOSX_BUNDLE ON)' 'HP-BUNDLE host/CMakeLists.txt:'
  seed seeded-icon-icns fail sdl/app.icns create 'x' 'HP-ICON host/sdl/app.icns:'
  seed seeded-icon-ico fail sdl/app.ico create 'x' 'HP-ICON host/sdl/app.ico:'
  seed seeded-resource-rc fail sdl/app.rc create 'x' 'HP-ICON host/sdl/app.rc:'
  seed seeded-product-literal fail "$_pump" append 'static const char *seeded_name = "Tendies64";' 'HP-PRODUCT host/pump/t64h_pump.c:'
  seed seeded-product-literal-upper fail "$_shell" create 'static const char *seeded_name = "run the TENDIES64 host";' 'HP-PRODUCT host/sdl/seeded_shell.c:'
  seed seeded-product-cmake-string fail "$_cm" append 'target_compile_definitions(t64h_tests PRIVATE "NAME=tendies64")' 'HP-PRODUCT host/CMakeLists.txt:'
  seed seeded-title-other-literal fail "$_shell" create 'static void seeded(void) { SDL_CreateWindow("Another title", 512, 384, 0); }' 'HP-TITLE host/sdl/seeded_shell.c:'
  seed seeded-title-variant-literal fail "$_shell" create 'static const char *seeded_title = "t64 minimal host";' 'HP-TITLE host/sdl/seeded_shell.c:'
  seed seeded-title-define-wrong fail "$_shell" create '#define T64H_SDL_TITLE "t64 minimal host (dev)"' 'HP-TITLE host/sdl/seeded_shell.c:'
  seed seeded-title-set-title fail "$_shell" create 'static void seeded(SDL_Window *w, const char *name) { SDL_SetWindowTitle(w, name); }' 'HP-TITLE host/sdl/seeded_shell.c:'
  seed seeded-title-properties fail "$_shell" create 'static void seeded(SDL_PropertiesID props) { SDL_CreateWindowWithProperties(props); }' 'HP-TITLE host/sdl/seeded_shell.c:'
  seed seeded-title-call-on-two-lines fail "$_shell" create 'static void seeded(int w) {
  SDL_CreateWindow(
      seeded_title(), w, w, 0);
}' 'HP-TITLE host/sdl/seeded_shell.c:2:'
  seed seeded-renderer fail "$_shell" create 'static void seeded(SDL_Window *w) { SDL_CreateRenderer(w, NULL); }' 'HP-RENDERER host/sdl/seeded_shell.c:'
  seed seeded-window-and-renderer fail "$_shell" create 'static int seeded = SDL_CreateWindowAndRenderer(0, 0, 0, 0, 0);' 'HP-RENDERER '
  seed seeded-gpu fail "$_shell" create 'static void seeded(void) { SDL_GPUDevice *device = NULL; (void)device; }' 'HP-RENDERER '
  seed seeded-opengl-window fail "$_shell" create 'static const int seeded = SDL_WINDOW_OPENGL;' 'HP-RENDERER '
  seed seeded-keyboard-event fail "$_shell" create 'static int seeded(const SDL_Event *e) { return e->type == SDL_EVENT_KEY_DOWN; }' 'HP-KEYBOARD host/sdl/seeded_shell.c:'
  seed seeded-keyboard-state fail "$_shell" create 'static const bool *seeded(void) { return SDL_GetKeyboardState(NULL); }' 'HP-KEYBOARD '
  seed seeded-scancode fail "$_shell" create 'static const int seeded = SDL_SCANCODE_A;' 'HP-KEYBOARD '
  # Clean controls: the committed host, the exact title through the macro and through the
  # literal, a call over several lines, comments and string contents that name forbidden things,
  # the SDL3 and product CMake packages, a controller event, and the shell's own sdl directory.
  seed clean-committed clean "$_pump" none ''
  seed clean-title-macro clean "$_shell" create "#define T64H_SDL_TITLE $_t
static void seeded(int w) { SDL_CreateWindow(T64H_SDL_TITLE, w, w, 0); }"
  seed clean-title-literal clean "$_shell" create "static void seeded(int w) { SDL_CreateWindow($_t, w, w, 0); }"
  seed clean-title-two-lines clean "$_shell" create "static void seeded(int w) {
  SDL_CreateWindow(
      T64H_SDL_TITLE, w, w, 0);
  SDL_SetWindowTitle(w, $_t);
}"
  seed clean-comment-names-forbidden clean "$_shell" create '/* no SDL_CreateRenderer, no SDL_GPUDevice, no SDL_EVENT_KEY_DOWN and no Tendies64 here */
// SDL_SCANCODE_A and SDL_CreateWindowWithProperties are comments too
static const int seeded = 1;'
  seed clean-string-names-forbidden clean "$_shell" create 'static const char *seeded = "SDL_CreateRenderer and SDL_EVENT_KEY_DOWN are words";'
  seed clean-cmake-comment clean "$_cm" append '# never FetchContent, ExternalProject, install( or MACOSX_BUNDLE here'
  seed clean-cmake-packages clean "$_cm" append 'find_package(SDL3 3.2.0 CONFIG REQUIRED)
find_package(tendies64 0.1 CONFIG REQUIRED)
target_link_libraries(t64h_pump PUBLIC tendies64::core)'
  seed clean-sdl-dir clean sdl/t64h_seeded.c create '#include <SDL3/SDL.h>
static int seeded(const SDL_Event *e) { return e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN; }'
  # Every rule of the header list has at least one seeded case that fails with its own id.
  for _rule in $ALL_RULES; do
    case " $SEEDED_RULES " in
      *" $_rule "*) : ;;
      *)
        echo "selftest FAIL: no seeded case asserts the rule $_rule"
        SELF_FAIL=$((SELF_FAIL + 1))
        ;;
    esac
  done
  # A scan that covers no file is an error, never a pass.
  _empty="$SELF_TMP/empty"
  mkdir -p "$_empty/$HOST_DIR"
  _rc=0
  scan "$_empty" > /dev/null 2>&1 || _rc=$?
  if [ "$_rc" -eq 2 ]; then
    echo "selftest ok: an empty scan is an error"
  else
    echo "selftest FAIL: an empty scan should exit 2 (exit $_rc)"
    SELF_FAIL=$((SELF_FAIL + 1))
  fi
  echo "host-policy selftest: failures=$SELF_FAIL"
  [ "$SELF_FAIL" -eq 0 ]
}

case "${1:-}" in
  "") scan "$ROOT" ;;
  --selftest)
    [ $# -eq 1 ] || usage
    selftest
    ;;
  --root)
    [ $# -eq 2 ] || usage
    [ -d "$2" ] || { echo "host-policy: not a directory: $2" >&2; exit 2; }
    scan "$2"
    ;;
  *) usage ;;
esac
