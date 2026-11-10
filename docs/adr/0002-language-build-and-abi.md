---
status: accepted
date: 2026-11-10
decision-makers: szTheory
consulted: none
informed: none
---

# C17 core and runner, CMake and Ninja build, C ABI behind opaque instances

**Status:** accepted

## Context and Problem Statement

The emulator core, the runner and the gate tooling need one language, one build system and one boundary
that later phases can consume. The choice is cheap to reverse until real core code merges and expensive after
the C API is frozen and consumed. This decision is [owner-visible]: it is surfaced to the owner and does not
block work. Which language and ABI should the core use, and what evidence would change the choice?

## Decision Drivers

- A native C ABI that other languages and hosts can bind without a runtime.
- A flat dependency tree, and the ability to copy small permissive C helpers with recorded provenance
  (ISC, MIT and BSD-3 C references exist for N64 hardware).
- Determinism: the core is checkable for host leaks by compiling objects and inspecting their sections and
  symbols.
- Safety evidence over preference: the sibling-repository convention and personal taste are not evidence
  (AGENTS.md).
- The gate must run unchanged on ubuntu-24.04 and macos-26 with tools those images already carry.

## Considered Options

- C17 core and runner.
- Rust with a no_std core (the runner-up).
- C++17.

## Decision Outcome

Chosen option: "C17 core and runner". The pre-registered spike below decided "stay with C17"
(`docs/spikes/rust-vs-c/RESULTS.md`), which accepts this record under amendment A-3.

Language and build (D1):

- C17 with `CMAKE_C_EXTENSIONS OFF`, `-Werror` through the cache option `T64_WERROR`, a CMake 3.25 floor,
  presets version 6, the Ninja generator and CTest.
- Gate glue is POSIX sh and awk. Structured checks (manifest validation, inventory, outcome classification,
  blob scanning) are small C tools. There is no Python dependency.
- Layout: `core/` (public headers in `core/include/tendies64/`), `runner/`, `tools/`, `tests/`, `fixtures/`
  (manifest only), `third_party/` (per-copy provenance), `docs/adr/`, `scripts/`. The `core/` directory is the
  determinism-lint boundary.

ABI:

- A C ABI behind opaque instances and ops tables. The facade header is provisional (CONTEXT amendment A-1);
  the API freeze is Phase 10.
- Memory comes from host callbacks. The core holds no file-scope mutable state.
- Digest rules, fixed now so digests stay portable across hosts: fixed-width integer types, explicit endian
  serialization, no hashing of struct padding, no host floating point in the core.

Proof matrix for PROF-05 (later): ubuntu-24.04 with gcc and clang, and macos-26 with Apple clang. Windows and
MSVC are deferred as an advisory canary.

Spike and acceptance rules:

- The time-boxed Rust spike (plan 01-08) is pre-registered before any result exists. It implements the
  determinism lint equivalent and a ROM-admission size and header parser in Rust, with no crates, and compares
  them with the C seeded-violation results.
- Switch criteria, quoted from the phase context: "switch D1 to Rust only if (a) the C lint cannot catch a
  seeded leak class that Rust rejects at compile time, or (b) fuzz/ASan finds a memory-safety bug in the C
  admission parser during the phase. Otherwise D1 stays C17."
- Interpretation of "lint equivalent" (amendment A-2): the frozen leak corpus is re-expressed as Rust twins
  and each twin is checked against compile-time rejection under `#![no_std]` and `#![forbid(unsafe_code)]`
  with single-file `rustc --emit=metadata`, plus the Rust admission parser fuzzed under ASan and UBSan. The
  interpretation is recorded in the pre-registration file before results exist.
- Amendment A-3: a "switch" outcome stops the accepting plan and the workflow must escalate to the owner,
  because it rewrites the C stub, the lint and the CMake files. Only a "stay with C17" outcome may
  auto-accept this record.
- Outcome: the registered run (`docs/spikes/rust-vs-c/RESULTS.md`, Decision: stay-c17) triggered neither
  criterion, so this record is accepted. Before acceptance, `DOC-CORE-BEFORE-ADR` in `scripts/check-docs.sh`
  failed any file under `core/` other than `core/t64_core_stub.c` and `core/include/tendies64/t64_facade.h`.
  With this record accepted, other core files are legal.

### Consequences

- Good: native C ABI, copy-with-provenance from permissive C references, a small flat tree.
- Good: the determinism boundary is checkable by object inspection on both operating systems.
- Bad: no compile-time memory safety. Mitigation: sanitizer fuzzing of the admission parser during the phase,
  and the switch criterion (b).
- Bad: the facade is provisional, so consumers must not treat it as an ABI promise before Phase 10.

### Confirmation

The pre-registered spike decided this record. `docs/spikes/rust-vs-c/PREREG.md` froze the class list, the file
hashes, both rules and the time box in a commit that precedes the results; `docs/spikes/rust-vs-c/RESULTS.md`
applies them mechanically. Per rule:

- Rule (a), the C lint cannot catch a seeded leak class that Rust rejects at compile time: not triggered. The
  full C gate lint (both layers) exits 1 on all 17 frozen classes on the macOS host and in the Linux
  container, so no class is both missed by the C gate and rejected by the Rust twin. The Rust twins reject 13 of
  the 17 classes. Four (atomic static, allocation, floating point, build-time macro) compile under `#![no_std]`
  and `#![forbid(unsafe_code)]`, and the std escape compiles too. Three C classes (floating point, build-time
  macro, inline asm) are caught by the Layer-1 source scan only.
- Rule (b), a sanitizer finding in the C admission parser: not triggered. A fuzz of 2000040 in-memory inputs
  (a ten-size boundary sweep and 2,000,000 deterministic mutations) under ASan and UBSan produced no report.
  A seeded overread was caught and attributed in the same harness, so the clean result is not vacuous.
- The Rust admission parser agrees with the C parser on all 2000040 inputs (differential match). It is not
  run under sanitizers: `-Zsanitizer` is nightly-only and the pinned image is stable.

Evidence limits: Linux evidence is the aarch64 container only, and the C corpus is the frozen seeds. The gate
rule `DOC-CORE-BEFORE-ADR` no longer blocks other core files. The Layer-2 object lint (ADR-0006) remains the
determinism check, and plan 01-13's CI run adds ubuntu-24.04 x64.

## Reversal evidence

Two pre-registered criteria, and only these, reverse this decision to Rust:

1. The C lint cannot catch a seeded leak class that Rust rejects at compile time. It is a trigger only when
   the full C gate (both lint layers, both operating systems) exits 0 on a class in the frozen leak list
   while the Rust twin fails to compile under the stated flags. Classes added after registration go in a
   separate table and cannot flip the decision.
2. Fuzzing under ASan or UBSan finds a memory-safety bug in the C admission parser, attributable to the
   parser and not to the harness, in the registered run (fixed seed, deterministic mutations).

Reversal window: nearly free until real core code merges, soft-open through Phase 2 (test vectors, digests
and manifests are language-neutral), and effectively closed once the C API is frozen and consumed in v1.2.
Style, speed, ecosystem and preference do not count.

## Pros and Cons of the Options

### C17 core and runner

- Good: native C ABI, small tree, permissive C references to copy from with provenance.
- Good: the object-level lint is POSIX tools only.
- Bad: memory safety rests on tests, sanitizers and review.

### Rust with a no_std core (runner-up)

- Good: compile-time rejection of several leak classes and of unsafe memory use.
- Bad: pilot measurement showed floats, atomic globals, allocation and build-time macros still compile under
  `#![no_std]`, and an `extern crate std` escape hatch needs its own source rule.
- Bad: a C ABI still has to be exported, and a Rust toolchain is not part of the gate's tool set.

### C++17

- Good: stronger type features than C.
- Bad: a larger ABI surface and runtime, constructors that run at load time, and a heavier determinism
  boundary to check, with no evidence it removes a leak class that C17 cannot catch.

## More Information

- Related: ADR-0006 (determinism lint mechanism), `docs/spikes/rust-vs-c/` (`PREREG.md` and `RESULTS.md`),
  `tools/lint/`.
- The phase context records D1 and amendments A-1 to A-3 in `.planning/phases/`.
