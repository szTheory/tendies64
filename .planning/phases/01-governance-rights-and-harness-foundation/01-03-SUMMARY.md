---
phase: 01-governance-rights-and-harness-foundation
plan: "03"
subsystem: infra
tags: [posix-sh, awk, determinism-lint, nm, size, selftest, gate, prof-07]
status: complete

requires:
  - phase: 01-governance-rights-and-harness-foundation
    provides: "gate.sh, CMake presets, the two-file stub core (CONTEXT A-1)"
provides:
  - "tools/lint/core-lint.sh: Layer-1 awk source scan plus Layer-2 driver; modes default, --root, --files, --selftest"
  - "tools/lint/lint-objects.sh: fixed-flag compile and section, common, constructor and undefined-symbol checks; --objects mode"
  - "tools/lint/rules.txt: 25 rule ids with layer, meta flag, description and hint (single source of truth)"
  - "tools/lint/allowlist.txt: name|why|ADR/date|os rows, bzero Darwin-only"
  - "tools/lint/selftest/: 19 bad_*.c seeds plus good.c (directory-owned corpus)"
  - "gate steps lint-selftest (before configure) and core-lint (after ctest)"
affects: [01-04, 01-08, 01-13]

actuals:
  tokens: 8000
  tasks: 3
  commits: 3
plan_head_before: 055fed0641de47695e6edeeaedcac1b5f18b83c7
plan_head_after: 0ee84327ec50f8bf834d8acbf004d8bd415f5dbd

tech-stack:
  added: []
  patterns:
    - "Layer 2 is authoritative: compile with fixed flags, then size/nm; Layer 1 only explains"
    - "Hints and ids live in rules.txt; both layers read it, the selftest coverage gate checks it"
    - "A seed must survive -O2: non-static functions with an observable effect (an empty constructor is deleted by Apple clang)"
    - "Green-on-empty control: zero files scanned reports L1-ZERO-FILES and fails"

key-files:
  created:
    - tools/lint/core-lint.sh
    - tools/lint/lint-objects.sh
    - tools/lint/rules.txt
    - tools/lint/allowlist.txt
    - tools/lint/selftest/good.c
    - tools/lint/selftest/bad_asm.c
    - tools/lint/selftest/bad_atomic.c
    - tools/lint/selftest/bad_ctor.c
    - tools/lint/selftest/bad_entropy.c
    - tools/lint/selftest/bad_env.c
    - tools/lint/selftest/bad_extern_host.c
    - tools/lint/selftest/bad_file_io.c
    - tools/lint/selftest/bad_float.c
    - tools/lint/selftest/bad_include_stdio.c
    - tools/lint/selftest/bad_include_unlisted.c
    - tools/lint/selftest/bad_libm.c
    - tools/lint/selftest/bad_malloc.c
    - tools/lint/selftest/bad_mutable_global.c
    - tools/lint/selftest/bad_mutable_static.c
    - tools/lint/selftest/bad_runtime_helper.c
    - tools/lint/selftest/bad_thread.c
    - tools/lint/selftest/bad_time.c
    - tools/lint/selftest/bad_time_macro.c
    - tools/lint/selftest/bad_tls.c
  modified:
    - scripts/gate.sh

key-decisions:
  - "Layer-1 name rules skip tokens inside __attribute__/__declspec/_Alignas/_Static_assert parentheses, so format(printf, ...) is not a false positive; constructor/destructor inside an attribute is its own rule (L1-ATTR-CTOR)"
  - "The selftest also fails if a rule marked meta in rules.txt is not one of the three controls the script actually runs (L1-ZERO-FILES, L2-COMPILE, L2-W-COMMON)"
  - "core-lint.sh keeps the caller's cwd for --files and --root and locates its own resources from the script path; the default mode and --selftest cd to the repo root"
  - "A malformed allowlist row is a tool error (exit 2), not a silent skip"

patterns-established:
  - "Defined symbols for the undefined-symbol subtraction are uppercase nm types only, so a static function in one object cannot mask a missing symbol in another"
  - "Every nm/size/compile result goes through a temp file and a sorted comm, never a pipeline that masks a failure"

requirements-completed: [PROF-07]

coverage:
  - id: D1
    description: "The gate lints the stub core with both layers in every run; scanning zero files is a failure"
    requirement: "PROF-07"
    verification:
      - kind: integration
        ref: "./scripts/gate.sh (steps lint-selftest and core-lint; core-lint: files=2 violations=0); selftest control: empty root reports L1-ZERO-FILES"
        status: pass
    human_judgment: false
  - id: D2
    description: "Host time, files, threads, entropy, environment and mutable globals in core are each reported by both the Layer-1 source lint and the Layer-2 object check, by rule id"
    requirement: "PROF-07"
    verification:
      - kind: integration
        ref: "sh tools/lint/core-lint.sh --selftest (bad_time, bad_file_io, bad_thread, bad_entropy, bad_env, bad_mutable_static, bad_mutable_global each print an L1- and an L2- id)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Layer 2 fails on any non-empty writable section, common symbol, constructor section or undefined symbol not on the allowlist; a host symbol Layer 1 cannot see (bad_extern_host.c) is caught"
    requirement: "PROF-07"
    verification:
      - kind: integration
        ref: "sh tools/lint/core-lint.sh --files tools/lint/selftest/bad_extern_host.c (exit 1, names host_poll); selftest controls for L2-COMPILE and L2-W-COMMON"
        status: pass
    human_judgment: false
  - id: D4
    description: "The selftest fails if a rule id has no seeded violation, if good.c does not pass, or if a seed's EXPECT ids are not all printed"
    requirement: "PROF-07"
    verification:
      - kind: other
        ref: "mutation checks on a scratch copy: removed seed (SELFTEST-RULE-UNPROVEN L1-ASM), extra rule (UNPROVEN), unknown EXPECT id (UNKNOWN + missing id), dirty good.c (exit 1), id not printed (missing id); each exited 1"
        status: pass
    human_judgment: false

duration: 6min
completed: 2026-11-10
---

# Phase 1 Plan 03: Two-layer determinism lint Summary

**A POSIX sh + awk Layer-1 source scan and an authoritative Layer-2 compile-and-`nm`/`size` object check, proven by a 19-seed corpus, a 25-id rules table with a coverage gate, and wired into the gate before configure and after ctest.**

## Accomplishments

- Tracer slice (include rules, section, undefined-symbol, constructor, compile and common controls, both layers, in the gate) proven end to end, then expanded.
- `lint-objects.sh`: one object per `.c`, fixed flags (`-std=c17 -O2 -fno-common -fno-stack-protector -fno-pic -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0`), `size -m` on Darwin and `size -A` elsewhere, size-zero sections ignored, `nm -P` types C/c as a separate failure, Mach-O leading underscore stripped, allowlist filtered by `uname -s`, comparison through LC_ALL=C-sorted files and `comm -23`.
- `core-lint.sh`: awk tokenizer that strips comments (state carried across lines, `//` continuation), strings and character literals; pp-number scanning for float literals; brace-depth declaration tracker for the file-scope mutable heuristics; hints come from `rules.txt`.
- 25 rules (20 Layer 1, 5 Layer 2). Three are proven by dedicated controls (`L1-ZERO-FILES`, `L2-COMPILE`, `L2-W-COMMON`), the other 22 by EXPECT seeds.
- Every ROADMAP SC4 leak class (time, files, threads, entropy, mutable global) lists both an L1 and an L2 id.

## Measured results

| Item | Value |
|------|-------|
| `./scripts/gate.sh` (Darwin arm64, Apple clang 21.0.0, BWK awk 20200816, bash 3.2 as sh) | `[gate] PASS`, `total 4s` (sh-n 0s, lint-selftest 3s, configure 0s, build 0s, ctest 1s, core-lint 0s) |
| `sh tools/lint/core-lint.sh` | `core-lint: files=2 violations=0` |
| `sh tools/lint/core-lint.sh --selftest` | `seeds=20 seeds_ok=20 failures=0`, coverage gate over 25 rule ids |
| `grep -c '^L[12]-' tools/lint/rules.txt` | 25 (17 after Task 2) |
| `ls tools/lint/selftest/bad_*.c \| wc -l` | 19 |
| `grep -c '\|Darwin$' tools/lint/allowlist.txt` | 1 |
| Linux parity probe: `docker run rust:1.91` (aarch64, Debian gcc 14.2.0, dash, mawk 1.3.4) | `--selftest` `failures=0`; default scan `files=2 violations=0`; `good.c` clean |
| good.c on Darwin | no `___memcpy_chk` or `chkstk` in output |

The Linux probe ran on aarch64 only; the x64 ubuntu-24.04 runner (gcc 13.3, gawk) is first exercised by CI in plan 01-13.

## Task Commits

1. Task 1 (tracer): `c5ecaa9` - two-layer lint on the stub core, gate steps, include/section/symbol/ctor/compile/common controls
2. Task 2: `f3c946f` - Layer-1 name and state rules, SC4 leak classes seeded for both layers
3. Task 3: `0ee8432` - token rules, remaining seeds, rules.txt coverage gate

Tracer feedback gate (auto mode): `<verify>` re-run end to end after Task 1, passed, expanded.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] `rules.txt` created in Task 1, not Task 2**
- **Found during:** Task 1
- **Issue:** Both layers print a hint per violation, and the hints belong in `rules.txt` (single source of truth). Task 1's output could not carry hints without it.
- **Fix:** Task 1 created `rules.txt` with the 8 ids it implements; Tasks 2 and 3 appended (17, then 25). The plan's counts at each task boundary hold.
- **Files modified:** tools/lint/rules.txt
- **Commit:** c5ecaa9

**2. [Rule 1 - Bug] The planned `bad_ctor.c` seed passed clean at `-O2`**
- **Found during:** Task 1 first run
- **Issue:** Apple clang drops an empty `__attribute__((constructor))` function from `__mod_init_func`, so the seed produced no violation (RESEARCH Pitfall 5, here for a non-static function).
- **Fix:** the constructor calls an external `t64_seed_hook()`, giving it an observable effect. `L2-UNDEF-SYMBOL` is an extra id, which the seed format allows.
- **Files modified:** tools/lint/selftest/bad_ctor.c
- **Commit:** c5ecaa9

### Interpretation notes (no conflict with a measured fact)

- Layer 1 skips name checks for tokens inside `__attribute__`, `__declspec`, `_Alignas` and `_Static_assert` argument lists (a `format(printf, ...)` attribute is not a stdio use). `L1-ATTR-CTOR` is evaluated inside attributes only.
- Layer-1 name rules are whole-token as specified, so a struct member named `time` or `stat` is flagged. The hint says rename; Layer 2 stays authoritative for symbols. Watch this when real core code lands.
- `lint-objects.sh` and `core-lint.sh` take include paths and file lists that contain no spaces (documented in the script header). The repo path is quoted separately.
- Core compile warnings are not shown unless the compile fails (stderr goes to a temp file); the lint compile is for objects, not for warnings.
- `--files`/`--root` keep the caller's cwd. Plan 01-08's spike can call `sh tools/lint/core-lint.sh --files X.c` from the repo root.

## Known Stubs

None. `core/t64_core_stub.c` remains the intentional placeholder recorded in plan 01-01 and is what makes the lint non-vacuous.

## Threat Flags

None beyond the plan's register. Mitigations applied: T-01-08 (Layer 2 authoritative; `bad_extern_host.c` proves a leak Layer 1 cannot see), T-01-09 (coverage gate, zero-files control, `-O2`-surviving seeds, mutation checks on a scratch copy), T-01-10 (objects only, in a `mktemp` dir removed by trap, never linked or run), T-01-SC-03 (no installs; only the OS compiler, `size`, `nm`, awk, sh). A scan of `tools/lint/` and `scripts/gate.sh` for home or temp paths and email addresses found nothing.

## Notes for later plans

- Plan 01-04: `allowlist.txt` cites `ADR-0006 2026-11-09` as text; ADR-0006 (determinism-lint mechanism) should record the measured deltas: `-D_FORTIFY_SOURCE=0`, the `os` column, Mach-O underscore stripping, size-zero filtering and the `nm` C/c check.
- Plan 01-08 (Rust spike): the frozen leak corpus is `tools/lint/selftest/bad_*.c`; use `--files` to run a single seed through both layers.
- Plan 01-13: x64 Linux (gawk) and Apple clang on `macos-26` are first confirmed in CI; local Linux evidence is aarch64 mawk/dash only.
- When real core code lands, `L1-NAME-*` will need a decision about member-name false positives, and any compiler-runtime helper symbol (`__popcountdi2`, `__udivti3`) needs an ADR-backed per-arch allowlist row.

## Self-Check: PASSED

- FOUND: tools/lint/core-lint.sh, tools/lint/lint-objects.sh, tools/lint/rules.txt, tools/lint/allowlist.txt, all 20 files in tools/lint/selftest/, scripts/gate.sh (with `lint-selftest` and `core-lint` steps)
- FOUND commits: c5ecaa9, f3c946f, 0ee8432 (all ancestors of HEAD)
