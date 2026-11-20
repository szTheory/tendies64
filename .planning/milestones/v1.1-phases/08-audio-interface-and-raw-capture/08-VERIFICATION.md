---
phase: 08-audio-interface-and-raw-capture
verified: 2026-11-19T15:22:00Z
status: passed
score: 7/7 must-haves verified
covered_files:
  - ".github/workflows/ci.yml"
  - ".planning/phases/08-audio-interface-and-raw-capture/08-01-PLAN.md"
  - ".planning/phases/08-audio-interface-and-raw-capture/08-01-SUMMARY.md"
  - ".planning/phases/08-audio-interface-and-raw-capture/08-02-PLAN.md"
  - ".planning/phases/08-audio-interface-and-raw-capture/08-02-SUMMARY.md"
  - ".planning/phases/08-audio-interface-and-raw-capture/08-03-PLAN.md"
  - ".planning/phases/08-audio-interface-and-raw-capture/08-03-SUMMARY.md"
  - ".planning/phases/08-audio-interface-and-raw-capture/08-04-PLAN.md"
  - ".planning/phases/08-audio-interface-and-raw-capture/08-04-SUMMARY.md"
  - ".planning/phases/08-audio-interface-and-raw-capture/08-05-PLAN.md"
  - ".planning/phases/08-audio-interface-and-raw-capture/08-05-SUMMARY.md"
  - "core/t64_ai.c"
  - "core/tests/test_ai.c"
  - "docs/claims/phase08.md"
  - "docs/evidence/phase07-oracle-authority.json"
  - "docs/evidence/phase08-audio.md"
  - "docs/spec/audio-cases.json"
  - "docs/spec/audio-interface.md"
  - "docs/spec/raw-capture.md"
  - "fixtures/audio/pcm.S"
  - "fixtures/manifest.json"
  - "runner/t64_audio_observation.h"
  - "runner/t64_raw_capture.c"
  - "runner/t64_real.c"
  - "runner/t64_supervisor.c"
  - "runner/tests/test_audio_capture.c"
  - "scripts/check-ci-yml.sh"
  - "scripts/check-rdp-oracle-authority.py"
  - "scripts/gate.sh"
  - "scripts/qualify-audio.py"
  - "scripts/tests/test_audio_qualification.py"
covered_digest: "v4:sha256:2879ad8d5efe13dfc0272834bba49c1a8d87d417c6177c31a327ae5bbee39259"
behavior_unverified: 0
overrides_applied: 0
re_verification:
  previous_status: human_needed
  previous_score: 6/7
  previous_verified: 2026-11-19T14:46:00Z
  previous_tested_commit: e15624cbf435a4542d7d2dc1902f92f0ccf2de6e
  gaps_closed:
    - "Truth 7 (lone-descriptor completion raises no MI AI) was present but behavior-unverified; core/tests/test_ai.c case_mi_full_falling_edge now acknowledges the 2-to-1 interrupt and asserts no MI AI on the final 1-to-0 completion; both surviving mutants (M1b, M15) now fail unit.core.ai.registers"
    - "W-1 capture size limit is now disclosed in docs/spec/raw-capture.md and docs/evidence/phase08-audio.md, including the manifest bound (the limit itself remains in code)"
    - "W-2 manifest decimal-string integers are now documented and match runner/t64_raw_capture.c"
    - "W-3 stale Linux statement replaced; hosted Linux evidence for run 44549490908 is real"
    - "W-4 wrong-sample control wording corrected"
  gaps_remaining: []
  regressions: []
flagged_prohibitions:
  - id: "08-01-P1"
    statement: "Authored PCM success must not be presented as silicon accuracy, broad compatibility, VID-04 completion or oracle authority"
    verification: judgment
    verdict: "not violated"
    authoritative: false
    flag: "unverified-prohibition: human review recommended (not an acknowledgment gate)"
  - id: "08-01-P2"
    statement: "Generated ROMs and raw captures must not be published or uploaded as Phase 08 evidence"
    verification: judgment
    verdict: "not violated"
    authoritative: false
    flag: "unverified-prohibition: human review recommended (not an acknowledgment gate)"
  - id: "08-05-P3"
    statement: "Phase 08 must not bypass oracle authority, private-window, physical, paid, install, publication or release stops"
    verification: judgment
    verdict: "not violated (the owner approval cited in two commit messages is unconfirmed and the verdict does not rely on it)"
    authoritative: false
    flag: "unverified-prohibition: human review recommended (not an acknowledgment gate)"
---

# Phase 8: Audio Interface and Raw Capture Verification Report

**Phase Goal:** The AI turns guest DMA into raw audio buffers whose cadence comes only from guest time, and a headless run captures raw video and audio as files with metadata
**Verified:** 2026-11-19T15:22:00Z
**Status:** passed (complete with 3 flagged judgment prohibitions, reported non-authoritatively; see below)
**Re-verification:** Yes. Previous report: status `human_needed`, 6/7, verified 2026-11-19T14:46:00Z against tested commit `e15624cb`.
**Verified source:** HEAD `7338803`. Native and hosted qualification ran on commit `3a41734` (hosted run 44549490908). `git diff 3a41734 HEAD --name-only` lists only `.planning/phases/08-audio-interface-and-raw-capture/08-05-SUMMARY.md` and `docs/evidence/phase08-audio.md`, neither of which is in the qualifier's bound source set. Between the initial tested commit `e15624cb` and HEAD, the only change under `core/`, `runner/`, `fixtures/`, `tools/`, the CMake files and `.github/` is the test file `core/tests/test_ai.c` (`git diff e15624c HEAD --name-only -- core runner fixtures tools CMakeLists.txt CMakePresets.json .github`), so production bytes are unchanged; the other source-side changes are `scripts/gate.sh` (one guard line) and `scripts/tests/test_audio_qualification.py` (one control).

## Verdict

The goal is achieved for the authored scope the roadmap and claim define, and the two reasons the initial report stopped at `human_needed` are resolved:

1. **Truth 7 is now behavior-verified.** The one spec-level negative invariant that no test pinned (a lone-descriptor 1-to-0 completion must not raise MI AI) is now pinned. I ran the mutants myself on a scratch clone of HEAD outside the working tree: unmutated `ctest --preset gate -R unit.core.ai` passes 9/9, and both formerly surviving mutants now fail it.
2. **The three judgment-tier prohibitions** were re-reviewed independently from the committed claim, evidence, CI upload step, downloaded hosted artifacts and Phase 08 diff. None is violated. They stay flagged and non-authoritative under the project's autonomous-mode soft gate (`.planning/AUTONOMY.md` section 2: complete with the flags when all automated requirements pass and no concrete stop is unresolved; a generic human-review recommendation is not an acknowledgment gate). No human validation or risk acceptance is recorded or implied by this report.

No truth failed, no artifact is missing or a stub, every key link is wired, and nothing regressed. Remaining items are non-blocking warnings (an engineering size limit that is now honestly disclosed, closeout bookkeeping, an informational test-strength note).

## What changed since the initial report

| Commit | Content | Effect on this report |
| --- | --- | --- |
| `7011a07` | `core/tests/test_ai.c` falling-edge case acknowledges the interrupt and asserts no re-raise at 1-to-0; `scripts/gate.sh` refuses `..` in `T64_GATE_JUNIT_OUT`; `scripts/tests/test_audio_qualification.py` runs the real case block as a control | closes truth 7; fixes security audit finding F-1 (`..` in the JUnit hook) |
| `c7bafa2` | `docs/spec/raw-capture.md` (limit incl. manifest bound, decimal-string integers, Linux evidence now present); `docs/evidence/phase08-audio.md` (wrong-sample wording, limitation bullet); CI review log and AUTONOMY lessons | closes W-2, W-3, W-4; discloses W-1 |
| `3a41734` | `08-SECURITY.md` (SECURED, 28/28) and the earlier `08-VERIFICATION.md`; this is the qualified commit | binding commit for native and hosted evidence |
| `7338803` | `docs/evidence/phase08-audio.md` re-bound to run 44549490908 on `3a41734` plus an "Independent review results" section; new `08-05-SUMMARY.md` | W-5 partly closed |

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
| --- | --- | --- | --- |
| 1 | SC1 (AUD-01, AUD-02): a cleared authored diagnostic produces raw sample buffers with sample index/rate metadata and exact values across DMA boundaries, two-deep queue behavior, and the interrupt at the documented `ai_full` 1-to-0 transition; submission or DMA start alone raises no interrupt | VERIFIED | "SC1 evidence" below. Production code unchanged since the initial pass; the Phase 08 selection passes 27/27 at HEAD. |
| 2 | SC2 (AUD-03): cadence follows DACRATE and guest time, so differently paced host runs give identical sample streams | VERIFIED | "SC2 evidence" below. Unchanged production code; cadence, determinism and drain controls pass. |
| 3 | SC3 (RUN-06): a headless run writes raw video frames and raw audio buffers as files whose metadata covers sample index, rate, frame timing and profile/build identity, separate from host presentation, published without replacement | VERIFIED | "SC3 evidence" below. `runner.raw-capture` and the `controls.real.raw-capture-*` CTests drive the production supervisor with `--raw-capture-dir` and pass at HEAD; both hosted jobs ran the same capture and no-replace finalization. |
| 4 | Expected PCM is authored independently of the code under test, and a wrong sample or a "non-silent" result is rejected (AUD-02) | VERIFIED | "Independence and rejection" below. I re-ran a one-bit wrong-sample mutant at HEAD: 15 of 27 tests fail. |
| 5 | Plan 08-05 qualification evidence is real and bound: exact suites reached, both hosted jobs executed accepted capture and no-replace finalization, published projection is bounded and cleared | VERIFIED (scope note) | Hosted run 44549490908 re-derived from the live GitHub record and both artifacts (below); the 216-file bound source snapshot digest recomputed at HEAD equals the evidence. Native ASan and GCC 13 results rest on private retained records that I did not open (instruction); see "Plan 08-05 note". |
| 6 | The claim and evidence are honest about scope per `docs/claims/TEMPLATE.md` | VERIFIED | `sh scripts/check-claims.sh`: `files=62 blocks=4 errors=0`; scope review below. |
| 7 | Spec-level: the AI raises MI AI pending only at the 2-to-1 queue transition; completing a lone descriptor (1-to-0) does not (`docs/spec/audio-interface.md:35-38`) | VERIFIED | Behavioral test now exists and discriminates: `case_mi_full_falling_edge` (`core/tests/test_ai.c:715-758`, run by `unit.core.ai.registers`) acknowledges the 2-to-1 interrupt at `:748-750`, then asserts MI AI clear after the 1-to-1 sample (`:753`) and after the final 1-to-0 completion (`:756`). Mutants below. |

**Score:** 7/7 truths verified (0 present, behavior-unverified)

### Truth 7: mutation evidence at HEAD (scratch clone outside the repository tree)

The clone was made with `git clone` of the local repository at HEAD; the build toolchain and fixtures were copied into it. Baseline first: `ctest --preset gate -R unit.core.ai` 9/9 pass, and the 27-test Phase 08 selection passes 27/27 there. Its restored core build id equals the claim's `t64-build-v1-7ef62833...`. Each mutant edits only the interrupt condition at `core/t64_ai.c:574` (`if (count_before == T64_AUDIO_QUEUE_CAPACITY)`), rebuilds, runs `ctest --preset gate -R unit.core.ai`, then the file is restored.

| Mutant | Condition | Result |
| --- | --- | --- |
| baseline | unmutated | 9/9 pass |
| M1b: interrupt on every completion | `count_before >= 1u` | FAIL: `unit.core.ai.registers`, 411 checks, 1 failure at `core/tests/test_ai.c:756` (`got 4 want 0`), case `mi-full-falling-edge` |
| M15: extra interrupt on the final completion | `count_before == CAP \|\| ai->descriptor_count == 0u` | FAIL: same single assertion at `:756` |
| M2: wrong edge | `count_before == 1u` | FAIL: `unit.core.ai.registers`, 3 failures: no interrupt at the 2-to-1 step (`core/tests/test_ai.c:741,742`) and a raise at the 1-to-0 step (`:756`) |
| Mx: contrived history-dependent extra | `count_before == CAP \|\| (count_before == 1u && ai->packet_count == 1u)` (raise only for a first-ever lone completion) | SURVIVES all 9 (informational, see below) |

Informational test-strength note (not a human item, not a gap): no test submits a single descriptor into an empty queue and checks MI AI, so a hypothetical mutant that keys on completion history (Mx) survives. The production decision is a pure function of `count_before` (`core/t64_ai.c:561,574-576`, values 1 or 2 only), and both values are now pinned, so this is residual hardening that I do not consider a goal gap. A short extra case (submit one descriptor from empty, complete, assert clear) would close it.

Other re-checks of the 7011a07 change: removing the `..` refusal line from a scratch copy of `scripts/gate.sh` makes `test_gate_junit_hook_is_private_and_opt_in` fail on `.../08-05-audio-private/../x.xml` (`0 != 2`); restored, 24 Python tests pass.

### SC1 evidence (AI DMA, two-deep queue, interrupt, metadata, exact values)

Production code is byte-identical to the initial pass, so the initial evidence stands; re-checked as a regression by running the cases.

- Two-deep queue: `T64_AUDIO_QUEUE_CAPACITY 2u` (`core/t64_internal.h:75`). `submit_dma` refuses a third descriptor with `T64_STATUS_QUEUE_FULL` and leaves state unchanged (`core/t64_ai.c:205-207`); the test checks status, a refused length write, and byte-identical AI state, events, MI and canonical digest (`core/tests/test_ai.c:666-713`).
- Interrupt edge: `count_before == T64_AUDIO_QUEUE_CAPACITY` then `t64_mi_set_pending(.., T64_MI_INTERRUPT_AI)` (`core/t64_ai.c:562,574-576`). Submission and DMA start never touch MI. Test: pending clear after two submissions (`core/tests/test_ai.c:734`), clear after the first sample (`:737`), set with the MI line high at the 2-to-1 completion (`:740-742`), cleared by a status write that preserves other sources (`:748-750`, `:760-781`), clear again at 1-to-0 (`:753,756`).
- Metadata: packet info carries sequence, `first_sample_index`, first/last tick, frame count, rate numerator 48,681,812 and denominator `DACRATE+1`, channels, bits, encoding, profile id and build id (`core/t64_ai.c:532-548`).
- Exact values across the DMA boundary: literal 32-byte PCM compared per packet in the unit tracer and in the supervised runner (`runner/tests/test_audio_capture.c:42-46,1035`).
- Old profile keeps refusing AI writes (`unit.core.ai.old-profile-refusals`, `.profile-pins`, `.profile-mutations` pass).

### SC2 evidence (guest-time-only cadence)

- Cadence arithmetic is integer-only on constants and DACRATE (`sample_delay()`, `core/t64_ai.c:54-78`); events are scheduled relative to guest `machine->tick` (`:89-109`). `tools/lint/core-lint.sh` bans host time sources in core sources (initial pass: `files=94 violations=0`; core sources unchanged).
- Whole-versus-split advance and the supervised runner at advance steps 16 and 32 give identical canonical digest, packet metadata and bytes (`core/tests/test_ai.c:2303-2373`; `runner/tests/test_audio_capture.c:3366-3417`; CTest `controls.real.audio-drain-cadence` passes at HEAD). The varied levers are advance partition and drain schedule, the only host-controlled inputs the core can see; `docs/evidence/phase08-audio.md` honestly says no run compared host throttle settings.

### SC3 evidence (raw A/V capture files with metadata, no-replace publication)

- The initial pass ran the production supervisor against the authored fixture and inspected the bundle (layout, modes 0600/0700, manifest last, 16,581 bytes, sample indexes 0 and 4, rate 48681812/1104, terminal tick 551001, no staging residue, second run refused with `capture-target-exists` and the first bundle byte-identical). Because `runner/` and `core/` are unchanged, I did not repeat the manual run; `runner.raw-capture` and all `controls.real.raw-capture-*` CTests exercise that same production path and pass at HEAD.
- Hosted: both jobs ran the actual accepted capture and no-replace finalization from source `3a41734` (re-derived below).
- Finalize only on guest completion with no host error (`runner/t64_real.c:1497-1526`).

### Independence and rejection (AUD-02)

- The PCM literal exists as separately typed hex in three places (`runner/tests/test_audio_capture.c:42-46`, `core/tests/test_ai.c:233-238`, `scripts/qualify-audio.py:32-35`); its SHA-256 equals the claim's `9baa9bdf...fcd2ed`. All comparisons are `memcmp` against literals; no comparator accepts a non-silence criterion; no test is skipped or disabled (grep: none).
- Initial mutation campaign (scratch copy at `e15624cb`; unchanged production code, history): M3 shifted sample, M10 one-bit first sample, M4 floor delay, M9 wrong denominator, M8 per-descriptor sample index, M2 wrong edge, M14 interrupt on submission were all caught; M1b and M15 survived (now killed, above).
- Re-run at HEAD: M10 (flip bit 0 of the second byte of the first left sample, exactly the wrong-sample ROM's delta, in `core/t64_ai.c`) fails 15 of 27 tests: `runner.audio-pcm`, `unit.core.ai.tracer/registers/cadence/output/determinism`, `controls.real.audio-drain-cadence`, `controls.real.audio-wrong-sample` and the `runner.raw-capture` and `controls.real.raw-capture-*` family. So a wrong sample stream is rejected by the strict literal comparison; the wrong-sample control itself accepts the wrong-ROM bytes only by asserting the exact one-bit-different pattern (`runner/tests/test_audio_capture.c:1030-1034,1103-1127`) to prove the comparator is reached after accepted setup.
- Python side: `python3 -B scripts/tests/test_audio_qualification.py` ran 24 tests, OK.

### Required Artifacts

| Artifact | Expected | Status | Details |
| --- | --- | --- | --- |
| `core/t64_ai.c` | AI registers, DMA queue, cadence, packets | VERIFIED | real logic; wired from `core/t64_bus.c` and the scheduler |
| `runner/t64_raw_capture.c` | transactional capture bundle | VERIFIED | wired from `runner/t64_real.c`; manifest writer matches the spec (W-2) |
| `runner/t64_audio_observation.h` | pinned observation record | VERIFIED | static asserts pin layout |
| `core/tests/test_ai.c`, `runner/tests/test_audio_capture.c` | exact-value and behavior tests | VERIFIED | all registered cases pass; the negative interrupt case is now pinned |
| `docs/spec/audio-interface.md`, `raw-capture.md`, `audio-cases.json` | contracts and case inventory | VERIFIED | W-2, W-3 fixed; limit disclosed |
| `scripts/qualify-audio.py`, `scripts/gate.sh` | bound evidence verifier and gate | VERIFIED | self-tests pass; `..` refusal control fails without the guard |
| `fixtures/audio/pcm.S`, `fixtures/manifest.json` | authored guest and identity | VERIFIED | manifest SHA-256 `f7c1953d...7931` equals the claim's `manifest_digest`; manifest diff over the whole phase only appends the two audio rows |
| `docs/claims/phase08.md`, `docs/evidence/phase08-audio.md` | bounded claim and evidence | VERIFIED | see "Claim and evidence honesty" |
| `.planning/phases/08-audio-interface-and-raw-capture/08-05-SUMMARY.md` | plan closeout | VERIFIED | claims checked below; one claim (native attempt count) rests on private records |

### Key Link Verification

| From | To | Via | Status | Details |
| --- | --- | --- | --- | --- |
| `core/t64_bus.c` | `core/t64_ai.c` | register routing for the composite profile only | WIRED | unchanged since the initial pass |
| `core/t64_scheduler.c` | `core/t64_ai.c` | admission before popping a sample event | WIRED | unchanged |
| `runner/t64_real.c` | public audio/video observer APIs | `t64_audio_info/copy`, `t64_video_info/copy` | WIRED | unchanged |
| `runner/t64_supervisor.c` | `runner/t64_real.c` | `--raw-capture-dir` / `--raw-capture-max-bytes` forwarded | WIRED | unchanged |
| `.github/workflows/ci.yml` | `scripts/qualify-audio.py` | existing Linux and macOS jobs only | WIRED | run 44549490908 succeeded on both jobs, every step success |
| evidence doc | retained originals | hash bindings | WIRED for hosted and for the source snapshot; private native records not opened |

### Data-Flow Trace (Level 4)

| Artifact | Data variable | Source | Produces real data | Status |
| --- | --- | --- | --- | --- |
| audio packet bytes | `packet[]` | `memcpy` from guest RDRAM at the descriptor address per sample (`core/t64_ai.c:549-551`) | yes: the M10 re-run shows a one-bit change at this copy fails the literal comparisons | FLOWING |
| manifest audio/video entries | `capture->audio[]/video[]` | packet info copied from the machine observer APIs | yes (hosted manifests record 2 audio and 29 video packets, 16,581 bytes) | FLOWING |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
| --- | --- | --- | --- |
| Phase 08 CTests | `ctest --preset gate -R "ai\|audio\|raw-capture" -j4` | 27/27 passed (repository build, rebuilt after `7011a07`) | PASS |
| Same selection on a clean scratch build of HEAD | same command in the scratch clone | 27/27 passed | PASS |
| Qualification verifier tests | `python3 -B scripts/tests/test_audio_qualification.py` | 24 tests OK | PASS |
| Workflow policy | `sh scripts/check-ci-yml.sh` | `check-ci-yml: ok` | PASS |
| Test inventory | `sh scripts/check-test-inventory.sh --preset gate` | `test inventory PASS preset=gate count=179` | PASS |
| Claims | `sh scripts/check-claims.sh` | `files=62 blocks=4 errors=0` | PASS |
| Phase 07 oracle authority checker | `ctest --preset gate -R "rdp-oracle\|oracle-authority"` | 1/1 passed | PASS |
| ASan inventory size | `ctest --preset asan -N` | `Total Tests: 181` (matches the evidence; not run) | consistent |
| Bound source snapshot | recomputed with the qualifier's own read-only `_source_snapshot()` at HEAD | 216 inputs, manifest digest `f9263789...c0e6` equals `docs/evidence/phase08-audio.md`, status clean | PASS |
| Hosted run record | `gh run view` / artifact API, run 44549490908 | success, attempt 1, source `3a41734`, `gate-linux` 188 s, `gate-macos` 507 s, no non-success step; artifact ids 14542207032 and 14542367221, sizes 8873 and 8864, digests equal the evidence table | PASS |
| Hosted artifacts | downloaded both by id; hashed; opened | zip SHA-256 values equal the evidence; audio `manifest.json` SHA-256 `606a4e0b...446e` (Linux) and `a4b40c75...5713` (macOS) equal the evidence; both agree on PCM SHA `9baa9bdf...fcd2ed`, 2 audio packets, 29 video packets, 16,581 bytes, fixture and input hashes, source `3a41734`, race outcomes `harness-error:capture-no-replace-commit` and `pass:positive-match`; core build ids equal the evidence table; no workstation path or email markers | PASS |
| Earlier hosted runs cited by the summary | `gh run view` 44538801603 (`1fdd663`), 44542244958 (`e15624c`) | both success | PASS |

### Probe Execution

No `probe-*.sh` scripts are declared or exist for this phase. Not applicable.

### Requirements Coverage

| Requirement | Source plans | Description | Status | Evidence |
| --- | --- | --- | --- | --- |
| AUD-01 | 08-01, 08-02, 08-03, 08-05 | AI DMA, two-deep queue, DACRATE cadence and interrupt produce raw buffers with sample index and rate metadata | SATISFIED | SC1, SC2, truth 7 now pinned |
| AUD-02 | 08-01, 08-03, 08-04, 08-05 | Authored diagnostic checks exact sample values across DMA boundaries; "non-silent" is never evidence | SATISFIED | Independence and rejection; M10 re-run |
| AUD-03 | 08-01, 08-02, 08-03, 08-04, 08-05 | Cadence derived from guest time, never a host clock | SATISFIED | SC2; lint-enforced |
| RUN-06 | 08-04, 08-05 | Capture raw video frames and raw audio buffers from a headless run as files with metadata, separate from host presentation | SATISFIED within the authored-fixture runner | SC3; limit disclosed (W-1) |

No orphaned requirements: REQUIREMENTS.md maps only AUD-01, AUD-02, AUD-03 and RUN-06 to Phase 8, and all four are in plan frontmatter. (The orchestrator's uncommitted `.planning/REQUIREMENTS.md` edit marking them complete was ignored, as instructed.)

### Decision Coverage

All trackable CONTEXT.md decisions are honored by shipped artifacts (9 of 9; `check.decision-coverage-verify`, non-blocking).

### Test Quality Audit

| Test file | Linked req | Active | Skipped | Circular | Assertion level | Verdict |
| --- | --- | --- | --- | --- | --- | --- |
| `core/tests/test_ai.c` | AUD-01/02/03 | 31 registered cases | 0 | none | value (byte and tick equality) | sufficient; the interrupt negative case is now pinned |
| `runner/tests/test_audio_capture.c` | AUD-02/03, RUN-06 | all registered | 0 | none (typed literal, independent of the system under test) | value and behavioral | sufficient |
| `scripts/tests/test_audio_qualification.py` | evidence binding | 24 | 0 | none | value; the `..` control runs the real gate case block | sufficient |

Disabled tests on requirements: 0. Circular patterns: 0. Insufficient assertions: 0 (one informational residual, mutant Mx above).

### Anti-Patterns Found

Debt-marker scan (`TBD`, `FIXME`, `XXX`) and `TODO`/`HACK`/`PLACEHOLDER` scan of the Phase 08 source, test, spec, claim, evidence and script files found no markers (the only `XXX` text is `mkdtemp` `XXXXXX` templates). No stubs, hollow props or placeholder returns on the audio path. Info (unchanged): `unit.core.ai.*` CTests carry no timeout, so a cadence regression would hang until the hosted job limit rather than fail fast (initial mutant M4).

### Advisory (New Scope, Unevidenced)

None.

### Claim and evidence honesty

- `docs/claims/phase08.md`: tier `unit-tested`, oracle kind `authored-pcm-literal-contract`, a long `not_claimed` field (other builds, profiles, fixtures; audio hardware timing, DAC, latency; host presentation; other rates, lengths, queue patterns; oracle agreement; commercial software), and prose saying no VID-04, hardware, release or broader compatibility result is claimed. The claim is byte-unchanged since the initial pass (`git diff 08e403f HEAD` does not list it).
- Claim build id `t64-build-v1-7ef62833...` is the native normal build; the scratch clone of HEAD reproduces it. The hosted Linux and macOS core build ids differ from it and from each other (toolchain differences, listed in the evidence). The claim prose says build identities "differ by platform"; strictly the native macOS and hosted macOS builds also differ. Info only, not an overclaim.
- `docs/evidence/phase08-audio.md` at HEAD is consistent with everything I could re-derive (hosted table, source snapshot, counts 179 and 181, 75 cases and 22 CTests inventory unchanged, 16,581 bytes, tick 551,001). Its "Independent review results" paragraph (seven mutants killed, one unpinned behavior, three prohibitions reviewed, none violated) matches the initial report and my re-review.

### Plan 08-05 note

Re-derived independently: hosted run record and both artifacts; the bound source snapshot digest; CTest counts (179 normal, 181 ASan listed); the summary's cited commits and runs (`a902d31`, `463d26f`, `1fdd663`, `e15624c`, `3a41734`; runs 44538801603, 44542244958, 44549490908) all exist and are as described; `08-SECURITY.md` finding F-1 (`..` in the JUnit hook) is fixed with a control that fails without the fix.

Not re-derived, by instruction: the native normal gate, ASan/UBSan results (181), pinned GCC 13 closure (39 units), the native record SHA-256 `fcec400b...33c2`, and the summary's "sixth native attempt" statement. These rest on privately retained records under `build/gate/08-05-audio-private` which I did not open, and `qualify-audio.py --native/--collect-hosted/--verify-evidence` was not run. The claim tier (`unit-tested`, authored scope) is supported by the unit and runner tests I did run plus the hosted gates on both operating systems.

## Flagged judgment prohibitions (non-authoritative; complete with 3 flags)

Each is `unverified-prohibition: human review recommended`; none is silently passed, and none is an acknowledgment gate under `.planning/AUTONOMY.md` section 2. My verdicts are non-authoritative LLM-judge verdicts from independent re-review.

### 08-01-P1: authored PCM success must not be presented as silicon accuracy, broad compatibility, VID-04 completion or oracle authority. Verdict: not violated.

- `docs/claims/phase08.md`: tier `unit-tested`, `not_claimed` covers hardware timing, DAC, other rates/lengths/queue patterns, oracle agreement, commercial software; prose "not a hardware or emulator-differential claim" and "No VID-04, hardware, release or broader compatibility result is claimed". The claim checker passes (`errors=0`).
- Every hit for accuracy, silicon, hardware, compatibility, VID-04 and oracle in the Phase 08 claim, evidence and specs is a disclaimer: `docs/spec/audio-interface.md:12-15` (profile choices "are not claims about undocumented silicon behavior"), `:109-111` ("not broad hardware compatibility"), `docs/spec/raw-capture.md:117-120`, `docs/evidence/phase08-audio.md:12-13,115-116,125,141-144`.
- The roadmap still states VID-04 and Plan 13 remain blocked; the evidence repeats it.

### 08-01-P2: generated ROMs and raw captures must not be published or uploaded as Phase 08 evidence. Verdict: not violated.

- `git ls-files` finds no tracked `.z64/.n64/.v64/.rgba8/.s16be/.elf/.bin/.pcm/.wav/.raw` file; `build/` is git-ignored. The two new `fixtures/manifest.json` rows are `distribution: ["build-only"]`. Files added across the whole Phase 08 range (outside `.planning/`) are only C/H sources, one assembly source and linker script, scripts, JSON and Markdown.
- CI: `.github/workflows/ci.yml` has two upload steps (one per job, same pinned action). Phase 08 narrowed each from a whole directory to an exact list of six JSON files per job, and `scripts/check-ci-yml.sh` enforces that exact list (`check-ci-yml: ok`). `rsp-stress.yml` uploads nothing.
- I downloaded both hosted artifacts by id: each holds exactly six JSON files (largest 8,564 bytes), the audio part is one `manifest.json` of hashes, counts and run identifiers (no PCM or frame bytes), and a search for workstation paths and email markers found none.

### 08-05-P3: Phase 08 must not bypass oracle authority, private-window, physical, paid, install, publication or release stops. Verdict: not violated.

- Installs and dependencies: the Phase 08 diff touches no package manager, download, `FetchContent`, `find_package` or install command (scan of CMake files, `scripts/gate.sh` and the other changed shell scripts; the workflow diff adds only run lines for the repository's own scripts and keeps the same pinned action).
- Oracle authority: at HEAD `docs/evidence/phase07-oracle-authority.json` still has `local_decision.status = blocked`, `publication_decision.status = not-authorized`, `execution_status = blocked-not-prepared`, `exported_corpus.status = missing`, and `scripts/check-rdp-oracle-authority.py` still has `TRUSTED_LOCAL_DECISION_SHA256` and `TRUSTED_PUBLICATION_DECISION_SHA256` unset (`None`). `harness.rdp-oracle-authority` passes. No oracle, ROM, firmware, hardware, paid service or release channel is touched anywhere in the diff.
- What commit `15fd8d1` (Phase 07 oracle-request re-pin) actually changed, from its diff: the derived `request_sha256`, the `fixtures/manifest.json` sha256 and git blob id, the `runner/tests/test_rdp_video.c` sha256 and blob id (in two places), and the checker's `PINNED_REQUEST_SHA256`. I recomputed the current files: `fixtures/manifest.json` SHA-256 `f7c1953d...7931` and blob `63097d16...`, `runner/tests/test_rdp_video.c` SHA-256 `e23557d9...6381` and blob `0f41e9d1...`, all equal to the new pins. The manifest changed only by appending the two audio rows, with no Phase 07 row touched. `PINNED_RIGHTS_AUDIT_SHA256` is unchanged. It is an identity reconciliation that grants and uses no authority.
- What the records show about the owner approval, stated precisely: `.planning/phases/08-audio-interface-and-raw-capture/08-RECHECK-FIX.md` does not mention commit `15fd8d1`, the Phase 07 oracle request, or any approval of that re-pin at all. Its scope is the Phase 08 recheck findings W1, W2, I3, I5, I6 plus an orchestrator follow-up about GCC 13 warning sites and the stale 9-versus-21 case inventory. The only places that state an owner approval are the commit messages of `15fd8d1` and `91628de` ("Owner approved this reconciliation on 2026-11-19") and one sentence in `08-05-SUMMARY.md` ("re-pinned only after owner approval, with decisions kept blocked"), all agent-authored. No owner-authored or project-owned decision record (`.planning/AUTONOMY.md`, `.planning/STATE.md`) names this approval, and a subagent cannot confirm it. I treat it as an unconfirmed claim. The verdict above does not depend on it, because the change grants nothing and no stop in `.planning/AUTONOMY.md` section 4 is crossed. If the approval was given in conversation and should be auditable, it needs a record in a project-owned file; this report does not supply one and records no human acceptance.
- Commit `91628de` (CI policy scanner admits the audio lane) carries the same unconfirmed approval statement. Its diff makes the scanner stricter (exact six-file upload list, fixed step order, unchanged action pins, permissions, triggers and timeouts), with 15 added mutation controls.

## Warnings (non-blocking) and disposition of the earlier W-1 to W-5

| ID | Initial finding | Disposition at HEAD |
| --- | --- | --- |
| W-1 | capture size limit real and undocumented | **Disclosed, limit remains (open as an engineering limit).** `docs/spec/raw-capture.md` ("Known limit") and `docs/evidence/phase08-audio.md` (Limitations) now state that the supervisor runs the real runner under `RLIMIT_FSIZE` of `T64_REAL_RESULT_MAX_BYTES (4096) + FSIZE_SLACK (64 KiB)` = 69,632 bytes (`runner/t64_supervisor.c:44,1534-1536`), that this binds a single frame or packet file and the manifest (a typical video entry is about 179 bytes by the writer's format, so roughly 380 to 390 frames), that a run over the limit ends with `SIGXFSZ`, fails closed and publishes nothing, and that larger geometries (a 320x240 RGBA frame is 307,200 bytes) need the limit raised. No code in `runner/` handles or ignores `SIGXFSZ`, so the default termination applies. The accepted authored case (12 x 7, 29 frames, 16,581 bytes) is unaffected and the runner accepts only registered authored fixtures. No later roadmap phase takes this over; it is recorded only as a disclosed limitation (also `08-05-SUMMARY.md`). Recommend a backlog item to raise the limit for the capture directory before any larger-frame capture consumer. |
| W-2 | spec said JSON integers are exact integers but the manifest emits decimal strings | **Closed.** `docs/spec/raw-capture.md:59-61` now says counters, sizes, ticks and sample indexes are exact decimal strings and only `schema_version` is a JSON number, which matches `runner/t64_raw_capture.c` (`json_u64_field` for every count, tick, sequence, size and version; `schema_version:1` literal). |
| W-3 | stale "Linux primitive execution is pending" text | **Closed.** The statement now points to the hosted Linux job of this qualification; the Linux artifact records the accepted capture (2 audio and 29 video packets, 16,581 bytes) and the race outcomes `pass:positive-match` and `harness-error:capture-no-replace-commit`. |
| W-4 | loose wording of the wrong-sample control | **Closed.** The evidence now says the control proves wrong sample bytes reach the strict byte comparator, which refuses them; my M10 re-run shows the strict comparison does fail on a one-bit-different stream. |
| W-5 | closeout bookkeeping not recorded | **Partly closed; remaining items are for the orchestrator's `phase complete` step and are not goal evidence.** `08-05-SUMMARY.md` now exists. Still open at verification time: `.planning/ROADMAP.md` shows Phase 8 as `4/5 plans executed`, Plan 08-05 unchecked and "Ready to execute"; `.planning/STATE.md` is stale (`current_plan: 05`, `status: executing`); `08-VALIDATION.md` frontmatter is `status: draft`, `nyquist_compliant: false` and its prose still says "No Phase08 test has run yet"; the `.planning/REQUIREMENTS.md` completion edit is uncommitted. |

Informational, carried: unit CTests without a timeout (hang rather than fail fast); mutant Mx residual (above); the hosted Linux and macOS evidence agree only on platform-invariant results by design.

## Deferred Items

None. Phase 9, the only later phase in the current roadmap, does not take over any Phase 08 item (W-1 is not planned there).

## Human Verification Required

None. The previously required item (truth 7) is closed by automated evidence. The three judgment prohibitions above are reported under the autonomous-mode soft gate with non-authoritative verdicts; no human verification is recorded as having occurred.

## Gaps Summary

No gaps. No truth failed, no artifact is missing or a stub, every key link is wired, and nothing regressed: production sources are byte-identical to the initial pass and the Phase 08 test selection, Python verifier tests, workflow policy, test inventory, claims and oracle-authority checks all pass at HEAD. The phase achieves its goal for the authored scope: the AI produces exact raw PCM across two queued DMAs on a guest-time-only cadence, raises the interrupt only at the documented 2-to-1 edge (now test-pinned), and the real supervised runner writes a complete, metadata-bearing, no-replace-published raw A/V bundle, reproduced on both hosted operating systems.

---

_Verified: 2026-11-19T15:22:00Z_
_Verifier: Claude (gsd-verifier)_
