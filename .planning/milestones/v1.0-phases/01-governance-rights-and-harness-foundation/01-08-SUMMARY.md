---
phase: 01-governance-rights-and-harness-foundation
plan: "08"
subsystem: governance
tags: [rust-spike, pre-registration, fuzz, asan, ubsan, differential, adr, gov-01]
status: complete

requires:
  - phase: 01-governance-rights-and-harness-foundation
    provides: "two-layer determinism lint and its seeded corpus (01-03), ADR-0002 proposed, check-docs.sh with PROPOSED_OK (01-04)"
provides:
  - "docs/spikes/rust-vs-c/PREREG.md: pre-registration (class list, 42 SHA-256 hashes, Rule (a), Rule (b), A-2, A-3, time box, forecast, amendment protocol), committed before RESULTS.md"
  - "docs/spikes/rust-vs-c/RESULTS.md: registered run, decision stay-c17"
  - "docs/spikes/rust-vs-c/run-spike.sh: --control, --print-hashes, --verify-frozen, --admission-check, --registered"
  - "19 Rust twins in corpus/rust/ and the spike-only admission parser in C and Rust (t64s_admit, t64s_admit_info, t64s_admit_result) with a fuzz and differential driver"
  - "ADR-0002 accepted; check-docs requires every ADR 0001-0007 accepted (PROPOSED_OK empty)"
affects: [01-13]

actuals:
  tokens: 19800
  tasks: 3
  commits: 4
plan_head_before: 93c18edcbf78f6dd126e32d75f43ca1189c72e0a
plan_head_after: 90a497b5bc5ab2769e3b8cbd1475a5fc5fac79e8

tech-stack:
  added: []
  patterns:
    - "Pre-registration: hash-freeze every input in a committed file, refuse to run unless the file is committed and every input matches, harness-only amendments by a dated section with a new hash"
    - "A fuzz harness proves its sanitizer is live with a seeded one-byte overread before the registered run"
    - "Differential fuzz: two languages generate the same inputs from one xorshift64 schedule and compare outcome counts plus an FNV-1a 64 over the outcome sequence"

key-files:
  created:
    - docs/spikes/rust-vs-c/PREREG.md
    - docs/spikes/rust-vs-c/RESULTS.md
    - docs/spikes/rust-vs-c/run-spike.sh
    - docs/spikes/rust-vs-c/admission/admission.h
    - docs/spikes/rust-vs-c/admission/admission.c
    - docs/spikes/rust-vs-c/admission/fuzz_admission.c
    - docs/spikes/rust-vs-c/admission/admission.rs
    - docs/spikes/rust-vs-c/corpus/rust/ (19 twins)
  modified:
    - docs/adr/0002-language-build-and-abi.md
    - docs/adr/0001-record-decisions-with-madr.md
    - scripts/check-docs.sh

key-decisions:
  - "D1 decided by registered evidence: stay with C17. Neither pre-registered rule triggered, so ADR-0002 is accepted (CONTEXT A-3: only this outcome may auto-accept)"
  - "Rule (a) reads 'both OSes' as a conjunction: a class triggers only when the C gate lint exits 0 on the host and in the Linux container, because the gate is green only if both are"
  - "PREREG records the image repo digest as a 32-hex prefix, and run-spike.sh checks the prefix, so the original hash table stays the only 64-hex content (42 values)"

patterns-established:
  - "The acceptance check for a decision record is mechanical: RESULTS.md Decision line, ADR status, check-docs and the gate all agree"

requirements-completed: [GOV-01]

coverage:
  - id: D1
    description: "PREREG.md is committed in an earlier commit than RESULTS.md and freezes every input by hash before any decision data exists"
    requirement: "GOV-01"
    verification:
      - kind: integration
        ref: "plan verify 1 (git log --diff-filter=A ancestry: 9390f0b is an ancestor of 0063abd); run-spike.sh --verify-frozen ok (42 files); mutation: an edited good.rs fails with 'frozen file changed'"
        status: pass
    human_judgment: false
  - id: D2
    description: "RESULTS.md shows per frozen class the C host, C Linux and Rust outcomes, the sanitizer fuzz, the differential, and applies Rule (a) and Rule (b) mechanically"
    requirement: "GOV-01"
    verification:
      - kind: integration
        ref: "plan verify 4 (two outcomes: lines, two fnv1a64: lines, one Differential: match, Sanitizers C and Rust lines); 19-row class table"
        status: pass
    human_judgment: false
  - id: D3
    description: "On stay-c17, ADR-0002 is accepted with a Confirmation citing RESULTS.md, and the gate requires every ADR 0001-0007 accepted"
    requirement: "GOV-01"
    verification:
      - kind: integration
        ref: "plan verify 3 (Decision: stay-c17, status: accepted, check-docs ok, gate PASS); grep -h '^status:' docs/adr/000[1-7]-*.md | grep -c accepted prints 7; check-docs --selftest cases=23 failures=0 including a proposed 0002 failing"
        status: pass
    human_judgment: false
  - id: D4
    description: "No spike code is built by CMake or lies inside core/"
    requirement: "GOV-01"
    verification:
      - kind: integration
        ref: "plan verify 2 under bash: 6 CMakeLists files, none mentions docs/spikes, core/ holds exactly 2 files"
        status: pass
    human_judgment: false

duration: 14min
completed: 2026-11-10
---

# Phase 1 Plan 08: Rust-versus-C spike and ADR-0002 acceptance Summary

**A pre-registered, hash-frozen Rust-versus-C spike ran once in 7 seconds and decided "stay with C17": the full C gate lint catches all 17 frozen leak classes on the host and in the Linux container, a 2,000,040-input ASan/UBSan fuzz of the C admission parser found nothing, and ADR-0002 is accepted.**

## Owner-visible decision (D1)

**Decision: stay with C17. No owner escalation is needed.** CONTEXT A-3 sends only a "switch" outcome to the owner, and this is not one. The evidence, from `docs/spikes/rust-vs-c/RESULTS.md`:

- Rule (a), a seeded leak class the C lint misses but Rust rejects at compile time: **no class triggers.** The full C lint (both layers) exits 1 on all 17 frozen classes on the macOS host and in the Linux container. The Rust twins reject 13 of the 17 (E0433 for std paths, E0133 for `static mut`, inline asm and extern calls, E0599 for libm, and a lint error for the `link_section` constructor). Four classes compile under `#![no_std]` + `#![forbid(unsafe_code)]`: atomic static, allocation, floating point, build-time macro. The std escape (`extern crate std;`) compiles too.
- Rule (b), a sanitizer finding in the C admission parser: **none.** 2,000,000 deterministic mutations plus 40 boundary inputs ran under ASan and UBSan with no report. A seeded one-byte overread in the same harness was caught and attributed to `admission.c`, so the clean result is not a vacuous pass.
- The Rust admission parser agrees with the C parser on all 2,000,040 inputs (differential match, same outcome counts, same FNV-1a 64).

Honest findings the owner may care about, none of which flips the decision:

- Three C classes (floating point, build-time macro, inline asm) are caught by the Layer-1 source scan only. Layer 2 does not back them up, so they rest on a heuristic token scan.
- Linux evidence is aarch64 only (the ubuntu-24.04 x64 runner is first exercised in plan 01-13). The Rust parser is not sanitizer-checked: `-Zsanitizer` is nightly-only, observed as `error: the option Z is only accepted on the nightly compiler` on the pinned stable image.
- The reversal window stays soft-open through Phase 2, as ADR-0002 records.

## Accomplishments

- Tracer: 19 Rust twins plus `run-spike.sh --control`, proven end to end on the control pair, with three harness negative controls (a seeded C violation on the host and in the container, and a Rust syntax error) each required to exit 1.
- Task 2: the admission parser in C and Rust with one contract, a C fuzz driver and a Rust `differential` test generating identical inputs from one xorshift64 schedule, and `PREREG.md` with 42 SHA-256 hashes, committed before any registered result.
- Task 3: one registered run, `RESULTS.md` (committed after PREREG), ADR-0002 accepted with a Confirmation that cites RESULTS.md per rule, and `check-docs.sh` now requires all seven ADRs accepted.
- The harness refuses to run unless PREREG.md is committed and every frozen file (and the image digest prefix) matches, and it checks `run-spike.sh` itself against the latest amendment's hash. No amendment was needed.

## Measured results

| Item | Value |
|------|-------|
| `sh docs/spikes/rust-vs-c/run-spike.sh --control` | `control: PASS`; row `0 0 0`; script-measured `control-duration-seconds: 1`, about 2 s wall including startup (first run, `date +%s` before and after) |
| `--registered` (one run, original harness hash `65548f8c...7f79`) | 7 s total: frozen check, 19 classes on host and container, sanitizer fuzz, differential |
| Registered table | rule-a-triggers: none; anomalies: 0; host and Linux never disagreed |
| Fuzz | 2,000,000 mutations + 40 boundary inputs; `outcomes: 83571 83673 83153 623372 249689 407044 469538`; `fnv1a64: a4750d6120f0629e` (C and Rust identical); `Sanitizers C: ASan+UBSan clean` |
| `./scripts/gate.sh` (Darwin arm64, final run after the ADR change) | `[gate] PASS`, `total 12s` (sh-n 0s, lint-selftest 2s, configure 0s, build 0s, ctest 9s, core-lint 0s) |
| `sh scripts/check-docs.sh --selftest` | `cases=23 failures=0` (the exemption case is skipped now; a proposed 0002 failing is a new case) |
| Acceptance counts | `ls docs/spikes/rust-vs-c/corpus/rust/*.rs \| wc -l` 19; PREREG 42 64-hex values; `grep -h '^status:' docs/adr/000[1-7]-*.md \| grep -c accepted` 7 |

## Task Commits

1. Task 1 (tracer): `b9ce2cb` - Rust twin corpus and `run-spike.sh --control`
2. Task 2: `9390f0b` - admission parser in C and Rust, fuzz and differential harness, PREREG.md
3. Task 3a: `0063abd` - RESULTS.md, decision stay-c17
4. Task 3b: `90a497b` - ADR-0002 accepted, check-docs requires all seven ADRs accepted

Tracer feedback gate (auto mode): the `<verify>` was re-run end to end after Task 1, passed, expanded.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] ADR-0001 still said ADR-0002 is exempt**
- **Found during:** Task 3
- **Issue:** ADR-0001 states that ADR-0002 is exempt from the acceptance rule until plan 01-08, which became false once `PROPOSED_OK` was emptied.
- **Fix:** one sentence now says the exemption existed until plan 01-08 accepted the record. ADR-0002's body (Chosen option, the "Until this record is accepted" bullet, Confirmation, More Information) was updated in the same way.
- **Files modified:** docs/adr/0001-record-decisions-with-madr.md, docs/adr/0002-language-build-and-abi.md
- **Commit:** 90a497b

### Interpretation notes (no conflict with a measured fact)

- **Image digest in PREREG.** The plan wants the image digest in PREREG and also exactly 42 64-hex values (18 + 19 + 4 + 1). A full repo digest is a 43rd. PREREG records a 32-hex prefix on an `Image digest prefix:` line, `--registered` refuses to run if the image digest does not start with it, and RESULTS.md carries the full digest.
- **Rule (a) wording.** "Both OSes" is registered as a conjunction (the gate is green only if both exit 0), with host/Linux disagreement flagged. Any C exit other than 0 or 1, or a Rust exit other than 0 or 1, is an anomaly. There were none.
- **Extra harness modes.** `--print-hashes`, `--verify-frozen` and `--admission-check` were added so the freeze and the sanitizer negative control could be tested without producing decision data. `--admission-check` runs the C contract table, the seeded-overread control and the Rust table only.
- **Pre-registration disclosure.** Before registering I compiled each Rust twin once to confirm it fails or compiles for the intended reason (the C side was already known from plan 01-03's selftest), and compared the two admission generators at 20,000 mutations on a scratch copy to catch a typing error. No sanitizer report appeared at that scale. PREREG.md states this under "Disclosure of work done before registration". No rule, class or parameter changed in response.
- **Rust test knob.** `admission.rs`'s `differential` test reads an optional `T64S_MUTATIONS` environment variable for harness controls. The registered run does not set it and RESULTS shows `mutations: 2000000`.
- **Cosmetic.** The seeded-overread control makes the shell print an `Abort trap: 6` job line on stderr. It is harmless and was not amended (the harness is frozen and the result is unaffected).
- **Plan verify 2 and zsh.** The plan's second `<automated>` command expands an unquoted multi-line variable, which only word-splits in sh or bash. In zsh it passes vacuously. I ran it under `bash -c` (6 CMake files, none mentions docs/spikes, core/ holds exactly 2 files) and with a direct per-file count.

## Known Stubs

None. The spike code is intentionally spike-only under `docs/spikes/` (text only, never built by CMake, never in `core/`).

## Threat Flags

None beyond the plan's register. Mitigations applied: T-01-29 (PREREG committed before RESULTS, hash-frozen corpus enforced by the harness, mechanical rules, no post-registration class able to flip the decision, the A-3 escalation path retained), T-01-30 (accepted: the container ran with `--network none` and a read-only mount, and the repo digest is recorded), T-01-31 (no CMake file references docs/spikes, `core/` holds only the two A-1 paths), T-01-SC-08 (no crates, no cargo, no installs; the image was already present and was never pulled). A scan of the spike files and edited docs for home paths, temp-dir paths from this host and email addresses found nothing.

## Notes for later plans

- Plan 01-13: the spike's Linux evidence is aarch64 and macOS arm64 only. The first CI run adds ubuntu-24.04 x64 and macos-26. The phase SUMMARY should repeat that this plan needed the local `rust:1.91` image (I-6), so the phase cannot close offline.
- `core/` may now hold files beyond the two A-1 paths (ADR-0002 is accepted, `DOC-CORE-BEFORE-ADR` no longer blocks). Phase 2 should treat the Layer-1-only classes (floating point, build-time macro, inline asm) as the lint's weakest point.
- Do not edit `tools/lint/selftest/` seeds that appear in the frozen list (18 C files), or anything under `docs/spikes/rust-vs-c/`: `run-spike.sh --verify-frozen` will fail, and only `run-spike.sh` is amendable (via a dated PREREG amendment).
- `scripts/check-docs.sh` selftest derives every ADR status from `PROPOSED_OK`, so adding a future proposed-but-allowed ADR again only needs that variable.

## Self-Check: PASSED

- FOUND: docs/spikes/rust-vs-c/PREREG.md, RESULTS.md, run-spike.sh, admission/{admission.h,admission.c,fuzz_admission.c,admission.rs}, 19 files in corpus/rust/, docs/adr/0002-language-build-and-abi.md (status accepted), scripts/check-docs.sh
- FOUND commits: b9ce2cb, 9390f0b, 0063abd, 90a497b (all ancestors of HEAD)
