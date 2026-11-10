---
status: accepted
date: 2026-11-09
decision-makers: szTheory
consulted: none
informed: none
---

# Check core determinism with two lint layers, where the object check is authoritative

**Status:** accepted

## Context and Problem Statement

The core must not read the host clock, files, environment, threads or entropy, and must hold no hidden
mutable global state, because its output must be a pure function of its inputs (PROF-07). A source scan can be
fooled by macros, aliases and extern declarations. How should the gate check the core boundary so that a
leak is caught by the compiled artifact, not by a pattern that can be bypassed?

## Decision Drivers

- A leak that a source scan cannot see (an extern declaration of a host function) must still fail.
- The check must run in the gate on both operating systems with tools already present: a C compiler, `nm`,
  `size`, sh and awk.
- A failure must name a rule id, a location and a hint, so it is fixable without reading the script.
- A lint that scans nothing, or a rule that no seed can trigger, must itself fail.

## Considered Options

- A source-only token scan.
- An object-only check with no source scan.
- Two layers: a source scan that explains, and an object check that decides.

## Decision Outcome

Chosen option: "Two layers, with Layer 2 authoritative", because the object check cannot be bypassed by
spelling, and the source scan gives readable hints before a symbol name does.

Layer 1, `tools/lint/core-lint.sh` (POSIX sh and awk): an include allowlist (`stdint.h stddef.h stdbool.h
stdalign.h limits.h string.h` and project headers), banned headers, banned tokens (floating types,
`__DATE__`, `__TIME__`, `__TIMESTAMP__`, inline assembly, thread-local storage, atomics, constructor
attributes), banned time, entropy, file, stdio, allocation, thread and environment names, and heuristics for
mutable file-scope objects. Each violation prints a rule id, `file:line` and a hint. Rule ids, layers and
hints live in `tools/lint/rules.txt`, the single source of truth.

Layer 2, `tools/lint/lint-objects.sh` (authoritative): each core file is compiled to an object with exactly

```
-std=c17 -O2 -fno-common -fno-stack-protector -fno-pic -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0
```

The object fails on any non-empty section that is not on an explicit read-only list, any common symbol, any
constructor section (`.init_array`, `__DATA,__mod_init_func`) and any undefined symbol that the core does not
define and the allowlist does not name. The section check is allow-by-name, not deny-by-name (code-review
finding CR-13), so state placed in a custom-named section through `__attribute__((section(...)))` or a
section pragma fails:

- Mach-O read-only list: `__TEXT,{__text __const __cstring __literal4 __literal8 __literal16 __ustring
  __eh_frame __gcc_except_tab}`, `__DATA,__const`, `__DATA_CONST,__const`, `__LD,__compact_unwind` and
  `__DWARF,*`. Mach-O has no per-section write flag, so the name decides.
- ELF read-only list: `.text* .rodata* .data.rel.ro* .eh_frame .eh_frame_hdr .gcc_except_table* .comment
  .note* .group .symtab .strtab .shstrtab .symtab_shndx .rel* .rela* .llvm_addrsig .debug_*` and the ARM and
  RISC-V attribute and unwind sections. The flags from `readelf -S -W` are also read: any section with the
  write or TLS flag fails whatever its name, except `.data.rel.ro*`.

Constant tables in `__DATA,__const` and `.data.rel.ro*` stay allowed. Residual: a non-const object that is
explicitly placed in `__DATA,__const` (Mach-O) or `.data.rel.ro*` (ELF) passes the object check; the
Layer-1 rule `L1-ATTR-SECTION` reports every section attribute and section pragma in core sources.

Measured deltas that shape the exact mechanism (evidence label MEASURED unless stated):

- `-U_FORTIFY_SOURCE` alone leaves `___memcpy_chk` on Apple clang; adding `-D_FORTIFY_SOURCE=0` removes it.
- Apple clang rewrites `memset(p,0,n)` to `bzero`, so `bzero` is an allowlist row with the `os` column set to
  `Darwin`.
- Mach-O symbol names carry a leading underscore. One underscore is stripped on Darwin before comparison.
- GNU objects list empty `.data` and `.bss` sections, so size-zero sections are ignored. Apple clang can emit a
  common symbol and no data section, so the object is compiled with `-fno-common` and any `nm` type `C` or
  `c` also fails.
- Constructors land in `__DATA,__mod_init_func` or `.init_array`, and both are denied. A seed must keep an
  observable effect, because an empty constructor is deleted by the optimizer.
- The section tool differs: `size -m` on Darwin and `readelf -S -W` elsewhere (it shows the section flags,
  which `size -A` does not).
- Compiler runtime helpers can surface from innocent code: `__builtin_popcount*` becomes `__popcountdi2` on
  x86-64 gcc, `__int128` division becomes `__udivti3`, and a stack frame above 4 KiB becomes
  `___chkstk_darwin` on macOS. None is allowlisted by default.

Allowlist policy (`tools/lint/allowlist.txt`, rows `name|why|ADR/date|os`):

- It starts with `memcpy memmove memset memcmp` plus the Darwin-only `bzero`.
- `malloc` is never allowed: core memory comes from host callbacks.
- Every new row needs an ADR reference and the operating system it applies to.
- A compiler-runtime helper needs an ADR-backed per-architecture row, and the preferred fix is to remove the
  construct.

Runtime-helper avoidance: the Layer-1 rule `L1-RUNTIME-HELPER` flags `__int128` and `__builtin_popcount*`, and
the core target compiles with `-Wframe-larger-than=2048` and `-Wdouble-promotion`.

Coverage gate: `core-lint.sh --selftest` fails if a rule id in `rules.txt` has no seeded violation or dedicated
control, if `good.c` does not pass, if a seed's expected ids are not all printed, and if a scan covers zero
files. The selftest runs in every gate run before the build.

### Consequences

- Good: a host symbol that source scanning cannot see is still caught by the object check.
- Good: every rule is proven able to fail by a seed, and an empty scan is an error.
- Bad: Layer 1 name rules match whole tokens, so a struct member named `time` is flagged and must be renamed.
- Bad: results differ per architecture. The local Linux parity probe ran on aarch64 only, so the x64 runner
  and Apple clang on macos-26 are first exercised by CI. That is an unknown until the first CI run.

### Confirmation

`sh tools/lint/core-lint.sh --selftest` (gate step `lint-selftest`) and `sh tools/lint/core-lint.sh` (gate step
`core-lint`) run in every gate run. The seeds are `tools/lint/selftest/bad_*.c` and `good.c`. The Rust spike
(plan 01-08) reuses the seed corpus as its frozen leak list.

## Reversal evidence

This decision is wrong if a supported toolchain exists where the object facts do not hold, for example
LTO-only objects that carry no ordinary sections, or MSVC COFF objects whose section and symbol names differ.
It is also wrong if a determinism leak passes both layers, shown by a seeded or real case with an exit status
of 0 from the full gate. Either outcome needs a new or extended mechanism recorded here. Convenience of
the source scan is not evidence.

## Pros and Cons of the Options

### A source-only token scan

- Good: readable messages, no compiler needed.
- Bad: an extern declaration of a host function, or a macro that expands to one, passes.

### An object-only check

- Good: authoritative and hard to bypass.
- Bad: messages name symbols and sections, not the source line a contributor must fix.

### Two layers, with Layer 2 authoritative

- Good: hints from the source scan, a verdict from the object.
- Bad: two mechanisms to keep consistent, handled by the single rules table and the coverage gate.

## More Information

- Related: ADR-0002 (language and build; the Rust spike compares against this lint), `tools/lint/rules.txt`,
  `tools/lint/allowlist.txt`.
- Unknown: x64 Linux with gawk and Apple clang on macos-26 behave as the local probes did. CI confirms.
