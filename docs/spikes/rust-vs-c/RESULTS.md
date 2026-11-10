# Results: Rust-versus-C spike (CONTEXT D1)

Registered by `PREREG.md` in commit `9390f0b4bb001ff8b690c51758f4f156c2f39336`, which precedes this file.
Every number below is the output of `sh docs/spikes/rust-vs-c/run-spike.sh --registered`, run once. Rules
(a) and (b), the A-2 interpretation and the A-3 action rule are applied exactly as registered.

## Run metadata

| item | value |
|------|-------|
| run | 1 of 1, started 2026-11-10T02:38:40Z, ended 2026-11-10T02:38:47Z (7 s), inside the time box |
| frozen inputs | verified before the run: 42 files against the PREREG table |
| host | Darwin arm64, Apple clang 21.0.0 (clang-2100.1.1.101) |
| container image | `rust:1.91`, repo digest `rust@sha256:867f1d1162913c401378a8504fb17fe2032c760dc316448766f150a130204aad` |
| container architecture | aarch64 (Debian, gcc 14.2.0) |
| rustc | 1.91.1 (ed61e7d7e 2025-11-07) |
| container isolation | `--network none`, repo mounted read-only |
| fuzz build tool | host cc (Apple clang 21.0.0), no container fallback needed |
| fuzz flags | `-std=c17 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all` |

Linux evidence is aarch64 only. The ubuntu-24.04 x64 runner is first exercised by CI in plan 01-13.

## Harness hashes used

| run-spike.sh SHA-256 | source | produced the reported table |
|----------------------|--------|-----------------------------|
| 65548f8c7ed2b3542d802574b8ca795b08eea6f41b00470fdd49746cf20b7f79 | original PREREG table (no amendment exists) | yes (run 1) |

## Per-class table

Exit codes of the full C gate lint (`core-lint.sh --files <seed>`, both layers) on the macOS host and in the
Linux container, and of `rustc --edition 2021 --crate-type=lib --emit=metadata` on the Rust twin.
Rule (a) is "yes" only when both C exits are 0 and the Rust twin exits 1.

| class | C host | C Linux | Rust | Rule (a) |
|-------|--------|---------|------|----------|
| control | 0 | 0 | 0 (accepted) | no (control) |
| host time | 1 | 1 | 1 (rejected) | no |
| file I/O | 1 | 1 | 1 (rejected) | no |
| thread | 1 | 1 | 1 (rejected) | no |
| entropy | 1 | 1 | 1 (rejected) | no |
| environment | 1 | 1 | 1 (rejected) | no |
| stdio | 1 | 1 | 1 (rejected) | no |
| mutable static | 1 | 1 | 1 (rejected) | no |
| mutable global | 1 | 1 | 1 (rejected) | no |
| atomic / interior-mutable static | 1 | 1 | 0 (accepted) | no |
| TLS | 1 | 1 | 1 (rejected) | no |
| allocation | 1 | 1 | 0 (accepted) | no |
| libm | 1 | 1 | 1 (rejected) | no |
| floating point | 1 | 1 | 0 (accepted) | no |
| build-time macro | 1 | 1 | 0 (accepted) | no |
| inline asm | 1 | 1 | 1 (rejected) | no |
| constructor | 1 | 1 | 1 (rejected) | no |
| extern host call | 1 | 1 | 1 (rejected) | no |
| std escape (Rust-only probe) | n/a (Rust-only probe) | n/a (Rust-only probe) | 0 (accepted) | n/a (probe) |

Rule (a) triggers: none. Anomalies (a C exit other than 0 or 1, or a Rust exit other than 0 or 1): none.
Host and Linux never disagreed. Harness negative controls (a seeded C violation on the host and in the
container, and a Rust syntax error) each exited 1, as required.

### What fired

| class | C rule ids (host; Linux identical unless noted) | Rust result |
|-------|--------------------------------------------------|-------------|
| host time | L1-INCLUDE-BANNED, L1-NAME-TIME, L2-UNDEF-SYMBOL | E0433 |
| file I/O | L1-INCLUDE-BANNED, L1-NAME-FILE, L2-UNDEF-SYMBOL | E0433 |
| thread | L1-INCLUDE-BANNED, L1-NAME-THREAD, L2-UNDEF-SYMBOL | E0433 |
| entropy | L1-INCLUDE-BANNED, L1-NAME-ENTROPY, L2-UNDEF-SYMBOL | E0433 |
| environment | L1-INCLUDE-BANNED, L1-NAME-ENV, L2-UNDEF-SYMBOL | E0433 |
| stdio | L1-INCLUDE-BANNED, L1-NAME-STDIO, L2-UNDEF-SYMBOL | E0433 |
| mutable static | L1-STATIC-MUTABLE, L2-W-SECTION | E0133 |
| mutable global | L1-GLOBAL-MUTABLE, L2-W-SECTION | E0133 |
| atomic static | L1-STATIC-MUTABLE, L1-TOKEN-ATOMIC, L2-W-SECTION (Linux adds L2-UNDEF-SYMBOL) | accepted |
| TLS | L1-STATIC-MUTABLE, L1-TLS, L2-W-SECTION (host adds L2-UNDEF-SYMBOL) | E0433 |
| allocation | L1-INCLUDE-BANNED, L1-NAME-ALLOC, L2-UNDEF-SYMBOL | accepted |
| libm | L1-INCLUDE-BANNED, L1-TOKEN-FLOAT, L2-UNDEF-SYMBOL | E0599 |
| floating point | L1-FLOAT-LITERAL, L1-TOKEN-FLOAT (Layer 1 only) | accepted |
| build-time macro | L1-BUILD-MACRO (Layer 1 only) | accepted |
| inline asm | L1-ASM (Layer 1 only) | E0133 |
| constructor | L1-ATTR-CTOR, L2-CTOR, L2-UNDEF-SYMBOL | lint error: declaration of a static with `link_section` (no error code) |
| extern host call | L2-UNDEF-SYMBOL (Layer 2 only) | E0133 |

## Admission fuzz (C, under sanitizers)

- Mutations: 2000000, in four kinds, from the fixed seed. Boundary inputs: 40 (ten sizes, four header variants
  each). Inputs run in total: 2000040, all in memory.
- Fuzz tool: host cc (Apple clang 21.0.0) with the registered flags. The sanitizer negative control ran first:
  a build with a seeded one-byte overread was caught and attributed to `admission.c`
  (`FAIL: input phase=boundary index=8 len=67108864 fnv1a64=6df56c954a5ca4da`).
- The registered run exited 0 with no sanitizer report and `info-consistency: ok`.

Boundary sweep, outcome codes for the four header variants z64, v64, n64, none (3 too-small, 4 too-large,
5 unknown-order, 6 misaligned):

| size | codes |
|------|-------|
| 0 | 3333 |
| 1 | 3333 |
| 3 | 3333 |
| 4 | 3333 |
| 4031 | 3333 |
| 4032 | 3333 |
| 4033 | 3333 |
| 67108863 (limit-1) | 6666 |
| 67108864 (limit) | 0125 |
| 67108865 (limit+1) | 4444 |

Rule (b): not triggered. No ASan or UBSan report exists, so none is attributable to `admission.c`.

## Differential (C against Rust, identical inputs)

The C driver and the Rust `differential` test generated the same 2000040 inputs. The Rust test ran in the
container after the no_std metadata build and the contract table both passed.

C driver, then Rust:

```
outcomes: 83571 83673 83153 623372 249689 407044 469538
fnv1a64: a4750d6120f0629e
outcomes: 83571 83673 83153 623372 249689 407044 469538
fnv1a64: a4750d6120f0629e
```

Differential: match

All seven outcome codes occurred. The boundary lines, `mutations:` and `inputs:` also matched line for line.

## Sanitizers

Sanitizers C: ASan+UBSan clean
Sanitizers Rust: not run (stable rustc; -Zsanitizer is nightly-only, per PREREG)

## Decision

Decision: stay-c17

Neither registered rule triggered. Rule (a): for all 17 frozen leak classes the full C gate lint exits 1 on
both the macOS host and the Linux container, so no class is one the C gate misses while the Rust twin fails
to compile. Rule (b): the 2000040-input sanitizer fuzz of the C admission parser produced no ASan or UBSan
report. Under CONTEXT A-3 only this outcome may auto-accept ADR-0002, so ADR-0002 is accepted with a
Confirmation that cites this file. The decision was reached by the registered rules alone, not by style,
speed, ecosystem or preference.

## Post-registration notes

Informational only. No class was added after registration, and nothing in this table can flip the decision.

| observation | what it shows |
|-------------|----------------|
| Rust accepts floating point (`f32` multiply) under no_std | Rust does not forbid floats. The C lint catches them (L1 only) |
| Rust accepts `extern crate alloc` with `Box` | allocation stays possible in no_std Rust. The C lint catches it (L1 and L2) |
| Rust accepts a static `AtomicU32` | interior-mutable globals stay possible. The C lint catches them (L1 and L2) |
| Rust accepts `env!("HOME")` | a build-time environment read stays possible. The C lint catches `__TIME__` (L1 only) |
| Rust accepts `extern crate std;` then `SystemTime::now()` | the one-line std escape defeats `#![no_std]`, so a Rust core would need its own source rule against it |
| Floating point, build-time macro and inline asm are caught by Layer 1 only | those three C classes have no Layer 2 backstop. They are caught today because the Layer-1 token scan is a heuristic over the seeds, so a future idiom that evades it would not be stopped by Layer 2 |
| The Rust `constructor` twin is rejected by the `unsafe_code` lint (a `link_section` static), not by a numbered error | rejection comes from the forbid attribute, so it depends on the twin carrying `#![forbid(unsafe_code)]` |

## Calibration against the forecast

- Rule (a) was forecast not to trigger (Layer 2 catches atomics, constructors and commons, Layer 1 catches
  the banned tokens). Confirmed: no class triggered, and the Rust pilot's accepted set (floats, atomics,
  allocation, build-time macros, the std escape) matched the registered run.
- Rule (b) was forecast unknown. The registered fuzz found nothing. The sanitizer negative control shows the
  harness would have caught and attributed a seeded overread, so the clean result is not a vacuous pass.
- Estimate of effort: the registered run took 7 seconds of wall time. The cost of the spike was in writing
  the corpus, the two parsers and the harness, not in running them.
- Limits: Linux evidence is aarch64 only, the Rust parser is not sanitizer-checked (its safety rests on
  `forbid(unsafe_code)` and the matching differential), and the corpus is the frozen seeds, not arbitrary C.
