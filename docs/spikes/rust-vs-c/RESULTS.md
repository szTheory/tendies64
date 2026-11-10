# Results: Rust-versus-C spike (CONTEXT D1)

Registered by `PREREG.md` in commit `9390f0b4bb001ff8b690c51758f4f156c2f39336`, which precedes this file.
Every number below is the output of `sh docs/spikes/rust-vs-c/run-spike.sh --registered`, run once. Rules
(a) and (b), the A-2 interpretation and the A-3 action rule are applied exactly as registered. The sections up
to "Calibration against the forecast" report run 1 with the original harness. "Addendum 1" at the end reports
run 2 with the harness of PREREG Amendment 1, which fixed a fail-open decision found by the phase code review.

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
| 65548f8c7ed2b3542d802574b8ca795b08eea6f41b00470fdd49746cf20b7f79 | original PREREG table | yes (run 1, the tables below) |
| 9154633fa7d4cb151017c2917d517c2a08a31601fda26c7b9e1b648e300a0794 | PREREG Amendment 1 | yes (run 2, Addendum 1) |

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

## Addendum 1: run 2 with the amended harness (2026-11-10)

Why: the phase code review (finding WR-21) showed that the original harness's decision failed open. It
printed `stay-c17` even when the fuzz had not finished, or when the lint or `rustc` was broken. Run 1 was
clean on every one of those points (fuzz exit 0, 2000040 inputs, `info-consistency: ok`, anomalies none, all
negative controls 1), so its recorded result was not wrong. But that harness could not have told an
incomplete run apart from a clean one. PREREG Amendment 1 makes the decision three-valued (`stay-c17`,
`switch-rust`, `inconclusive`), was committed first (commit `68e8c2c`), and was then run once with
`sh docs/spikes/rust-vs-c/run-spike.sh --registered`.

| item | run 2 value |
|------|-------------|
| harness | `run-spike.sh` SHA-256 `9154633fa7d4cb151017c2917d517c2a08a31601fda26c7b9e1b648e300a0794`, from the latest amendment |
| frozen inputs | ok, 42 files (41 against the original table, `run-spike.sh` against Amendment 1) |
| git commit as run | `68e8c2cae2566d6c78ddd09960632b489aa2f9ed` |
| started, ended | 2026-11-10T05:16:10Z, 2026-11-10T05:16:16Z (6 s) |
| host, image, container | Darwin arm64, Apple clang 21.0.0; `rust@sha256:867f1d1162913c401378a8504fb17fe2032c760dc316448766f150a130204aad`; aarch64, rustc 1.91.1, gcc 14.2.0 |
| per-class table | identical verdicts to run 1: all 17 frozen classes exit 1 on the host and in Linux; the Rust twins reject 13 and accept 4 (atomic static, allocation, floating point, build-time macro); std escape accepted |
| rule-a-triggers | none |
| anomalies | 0 |
| negative controls | host-c=1 linux-c=1 linux-rust=1 |
| sanitizer negative control | seeded overread caught, attributed to `admission.c` (`phase=boundary index=8 len=67108864`) |
| fuzz | host Apple clang, registered flags, exit 0, `mutations: 2000000`, `inputs: 2000040`, `info-consistency: ok` |
| boundary sweep | identical to run 1 |
| differential | match; C and Rust `outcomes: 83571 83673 83153 623372 249689 407044 469538`, `fnv1a64: a4750d6120f0629e`, both `info-consistency: ok` |
| rule-b-triggered | no |
| completeness-gaps | none |

Decision (run 2): stay-c17

Run 2 checks completeness that run 1's harness did not check. It does not strengthen what the rules
measure. Rule (b) still could hardly fire, because the parser it fuzzes reads four bytes (review finding
WR-23; see ADR-0002, "Evidence limits"). The lint that run 2 measured is a later version than run 1's: review fixes
CR-13 and WR-16 changed it in between. Both versions catch all 17 classes.

The negative controls for the new decision are `run-spike.sh --decision-selftest` (CTest
`repo.spike-decision-selftest`, 27 cases). Each non-clean condition yields `inconclusive`: fuzz exit 3, 4, 5,
125 or 137; a short or missing `inputs:` line; a missing or BAD `info-consistency`; a fuzz that did not run; a
failed Rust differential; a failed sanitizer control; a lint exit 2 on either OS; a missing row; a missing
`rustc`; a modified lint; a failed negative control; and a sanitizer report in `fuzz_admission.c` only.
Rules (a) and (b) yield `switch-rust`. Mutating the decision so it ignores the gaps fails 20 of the 27 cases.
Restoring the bare-text attribution fails the harness-only report case.

### Tool under test: disclosed gap in the original registration

Rule (a) measures the full C gate lint. PREREG froze the corpus, the Rust twins, the admission files and the
harness, but not the lint itself (review finding WR-22). So the lint could change between registration and a
run, or after it, and `--verify-frozen` would still report "frozen inputs: ok". Run 1 recorded no hash and no
commit for it. Amendment 1 cannot add rows to the frozen table, so the lint is now recorded, not frozen.
`--registered` prints the commit and these hashes, and it refuses `stay-c17` when the lint files differ from
that commit.

SHA-256 of the lint as run:

| file | run 1 (reconstructed) | run 2 (printed by the harness) |
|------|-----------------------|--------------------------------|
| `tools/lint/core-lint.sh` | `cbda76b47bda6fad758f0c9556ffa629fe66b5bcb669495861311cec5792612c` | `ca2eb8bd9c6efd02ef30d1cec8d1fe363e7f8ef9b1617ed5cbc29de8e2995980` |
| `tools/lint/lint-objects.sh` | `92d15101eaf31e5cf608130eda256e3c1049f3a035f8b5937d7791a26d1265c7` | `941878f7dbf421c3e24333c6f7d02e37c612f189445021459728f284d1c6be86` |
| `tools/lint/rules.txt` | `c9c8af2ef949a923424d44639c479a3624bf057905e577366784fda42a3cb6a8` | `07d375d782b427ed257a12fb64fe138bc0703961c65960057e564040b364d8f4` |
| `tools/lint/allowlist.txt` | `f6e0e19da65ab5ae45f53be34ee0b3aa68a888aacb21434d9aafcf8372f8a238` | `f6e0e19da65ab5ae45f53be34ee0b3aa68a888aacb21434d9aafcf8372f8a238` |
| git commit | `9390f0b4bb001ff8b690c51758f4f156c2f39336` (PREREG commit; `0063abd` has the same lint) | `68e8c2cae2566d6c78ddd09960632b489aa2f9ed` (harness checked the lint matched it) |

Run 1's column is reconstructed from git: the hashes of these files at the PREREG commit, after which run 1
ran and before RESULTS was committed. The lint is the same at `0063abd`. Run 1 did not record whether its
working tree matched that commit, so this column is an inference, not a measurement. Between the runs, review
fixes CR-13 (Layer 2 treats every section outside the read-only list as writable) and WR-16 (Layer 1 bans
host-state intrinsics and checks quoted includes) changed `core-lint.sh`, `lint-objects.sh` and `rules.txt`.
Both versions exit 1 on all 17 frozen classes on both operating systems.

What this means: neither run's rule (a) result can be reproduced from the pre-registration alone. Each is
reproducible from the commit named above. The result says nothing about later versions of the lint. CI's
lint selftest covers those separately.

Rule for future spikes: any future pre-registered spike must freeze the tool under test, not only its inputs.
The tool's files go into the frozen set and the hash table, so that `--verify-frozen` catches a change.
