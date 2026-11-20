---
phase: 08-audio-interface-and-raw-capture
plan: "05"
subsystem: qualification
status: complete
tags: [qualification, hosted-evidence, audio, raw-capture]
requires:
  - phase: 08-04
    provides: Actual raw A/V capture with no-replace publication
provides:
  - Authored audio denominator and retained-original qualification wrapper
  - Native normal, ASan/UBSan and GCC13 qualification of the reviewed final source
  - Authenticated hosted Linux and macOS capture and no-replace finalization records
  - Bounded unit-tested claim and evidence for the authored AI and raw-capture scope
affects: [09]
requirements-completed: [AUD-01, AUD-02, AUD-03, RUN-06]
requirements-contributed: [AUD-01, AUD-02, AUD-03, RUN-06]
key-decisions:
  - The qualified claim is tier unit-tested and authored-scope only; no hardware, oracle or commercial-software claim.
  - Hosted comparison covers only platform-invariant results (PCM literal, packet counts, byte total); build-identity hashes differ per platform by design.
  - Verifier tails are replayed against real retained artifacts before each push.
coverage:
  - id: D1
    description: Native normal gate, ASan/UBSan full CTest and GCC13 closure on the final source with retained originals
    requirement: AUD-01
    verification:
      - kind: integration
        ref: python3 -B scripts/qualify-audio.py --native (commit 3a41734; 179 normal, 181 ASan, 39 GCC13 units)
        status: pass
    human_judgment: false
  - id: D2
    description: Both hosted jobs execute actual accepted A/V capture and no-replace finalization; artifacts authenticated by ID, digest, run, attempt and job
    requirement: RUN-06
    verification:
      - kind: integration
        ref: python3 -B scripts/qualify-audio.py --collect-hosted and --verify-evidence (run 44549490908, attempt 1)
        status: pass
    human_judgment: false
  - id: D3
    description: Exact authored sample values and guest-time-only cadence across DMA boundaries; wrong sample and single-descriptor interrupt mutants rejected
    requirement: AUD-02
    verification:
      - kind: unit
        ref: ctest --preset gate -R unit.core.ai (9 tests) and controls.real.audio-*
        status: pass
    human_judgment: false
  - id: D4
    description: Cadence derived from DACRATE and guest ticks only; no host clock on the sample path
    requirement: AUD-03
    verification:
      - kind: unit
        ref: unit.core.ai.cadence, unit.core.ai.determinism, controls.real.audio-drain-cadence; tools/lint/core-lint.sh
        status: pass
    human_judgment: false
completed: 2026-11-19
plan_head_before: 463d26f77b62d6c1d4469a7411c6e04492cf49a6
plan_head_after: 3a41734c8cc09c0e5cbcf8d32e77a4f53f756ffd
---

# Phase8 Plan05: authored audio and raw-capture qualification

The reviewed final source `3a41734` passed the complete normal gate (179 CTests),
the full ASan/UBSan inventory (181) and the pinned GCC13 `-Werror` route (39 C
units) natively, and both existing hosted jobs passed it in run 44549490908
(`gate-linux` 188 s, `gate-macos` 507 s). The exact-binding verifier accepted the
native record, both original artifact archives and the combined platform pair.
The bounded claim is `docs/claims/phase08.md` (tier unit-tested, authored scope);
the exact scope, bindings and limitations are in `docs/evidence/phase08-audio.md`.

## What ran

Task1 (`a902d31`, earlier) bound the exact authored denominator and adversarial
evidence controls. Task2 ran the independent source review (`08-REVIEW.md`:
CR-01 publication-source race and WR-01 partial observer evidence), fixed both and
the recheck items W1, W2, I3 and I6, then qualified. Native qualification passed
on its sixth attempt; every earlier attempt is retained privately and none was
replayed. Hosted runs on `1fdd663`, `e15624c` and finally `3a41734` all passed
their jobs; only the last is the binding because later commits changed bound
source.

## Corrections found on the way

All fixed with controls built from the real format, in this order of discovery:
guard drift (test inventory, workflow policy scanner, oracle-request pins; the
last re-pinned only after owner approval, with decisions kept blocked), GCC13
`-Werror` and missing include path, stale 9-case versus 21-case inventory,
native verifier tails on real CTest output (discovery-only LastTest replacement,
several retained capture records per suite, ASan original containment), the
hosted collector's repeated record-path line, and a Linux/macOS comparison that
wrongly required build-identity hashes to match. The independent goal
verification then found that the lone-descriptor no-interrupt rule was not pinned
by any test; it is now, and both surviving mutants fail it. The independent
security audit closed 28 of 28 threats (`08-SECURITY.md`) and its low finding, a
`..` component accepted by the gate's private JUnit path check, is refused with a
control that runs the real case block.

## Limits and stops

Authored single case only; no audio hardware, DAC, latency, host presentation,
other rates or lengths. A per-file supervisor `RLIMIT_FSIZE` (about 68 KiB) means
larger geometries need a raised limit before they can be captured; it fails
closed and is documented in the raw-capture spec. Phase7 VID-04 and Plan13 remain
blocked on oracle authority and the parallel-rdp rights audit; no oracle, ROM,
firmware, hardware, paid service, release channel or name decision was touched.

## CI/CD efficiency findings

`docs/ci-reviews/LOG.md` F-096 to F-098: the pinned Binutils source build is about
46% of the macOS gate and is uncached (deferred until after this phase because
`ci.yml` is inside the bound source), `repo.rsp-no-hle` runs twice per job, and
verifier tails must be replayed on real artifacts before pushing.

## Self-Check: PASSED
