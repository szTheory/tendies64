---
phase: 08-audio-interface-and-raw-capture
verified: 2026-11-19T14:46:00Z
status: human_needed
score: 6/7 must-haves verified
covered_files:
  - ".planning/phases/08-audio-interface-and-raw-capture/08-01-PLAN.md"
  - ".planning/phases/08-audio-interface-and-raw-capture/08-01-SUMMARY.md"
  - ".planning/phases/08-audio-interface-and-raw-capture/08-02-PLAN.md"
  - ".planning/phases/08-audio-interface-and-raw-capture/08-02-SUMMARY.md"
  - ".planning/phases/08-audio-interface-and-raw-capture/08-03-PLAN.md"
  - ".planning/phases/08-audio-interface-and-raw-capture/08-03-SUMMARY.md"
  - ".planning/phases/08-audio-interface-and-raw-capture/08-04-PLAN.md"
  - ".planning/phases/08-audio-interface-and-raw-capture/08-04-SUMMARY.md"
  - ".planning/phases/08-audio-interface-and-raw-capture/08-05-PLAN.md"
  - "core/t64_ai.c"
  - "core/tests/test_ai.c"
  - "docs/claims/phase08.md"
  - "docs/evidence/phase08-audio.md"
  - "docs/spec/audio-cases.json"
  - "docs/spec/audio-interface.md"
  - "docs/spec/raw-capture.md"
  - "fixtures/audio/pcm.S"
  - "fixtures/manifest.json"
  - "runner/t64_audio_observation.h"
  - "runner/t64_raw_capture.c"
  - "runner/t64_real.c"
  - "runner/tests/test_audio_capture.c"
  - "scripts/qualify-audio.py"
covered_digest: "v4:sha256:6636015b5d74c8c6b9b66f291626814b7615fc0b28249ce1c43b724119580a7b"
behavior_unverified: 1
overrides_applied: 0
behavior_unverified_items:
  - truth: "The AI raises MI AI pending only on the 2-to-1 queue transition; completing a lone descriptor (1-to-0) does not synthesize an interrupt (docs/spec/audio-interface.md:35-38)"
    test: "Enable AI, submit ONE descriptor, advance to its completion; also submit two, let the first complete, acknowledge MI AI via an AI_STATUS write, then let the second (last) complete"
    expected: "MI AI pending (0x04) stays clear after the single-descriptor completion, and stays clear after the last completion that follows an acknowledged 2-to-1 interrupt"
    why_human: "The code is correct by reading (core/t64_ai.c:574-576) but no test can fail on it. Two mutants that raise the interrupt on every completion, or additionally on the final completion, passed every Phase 08 core suite and all four runner CTests, because the existing falling-edge test never acknowledges the interrupt before the last completion (core/tests/test_ai.c:715-752)."
human_verification:
  - test: "Prohibition 08-01-P1 (judgment): authored PCM success must not be presented as silicon accuracy, broad compatibility, VID-04 completion or oracle authority"
    expected: "No claim, evidence, spec or roadmap prose says or implies any of these"
    why_human: "Judgment-tier prohibition with no test descriptor; flagged unverified-prohibition, human review recommended. Non-authoritative verifier verdict: not violated (docs/claims/phase08.md not_claimed field and prose, docs/evidence/phase08-audio.md:95-113; the claim checker passes with the reserved words unused)."
  - test: "Prohibition 08-01-P2 (judgment): generated ROMs and raw captures must not be published or uploaded as Phase 08 evidence"
    expected: "No generated ROM, raw frame, or PCM file is tracked or uploaded"
    why_human: "Judgment-tier prohibition with no test descriptor; flagged unverified-prohibition, human review recommended. Non-authoritative verifier verdict: not violated (no tracked .z64/.rgba8/.s16be/.elf; the CI upload step lists only JSON projections; both hosted artifacts were downloaded and contain only JSON files, the largest 8.6 KB, with hashes and no PCM or video bytes)."
  - test: "Prohibition 08-05-P3 (judgment): Phase 08 must not bypass oracle authority, private-window, physical, paid, install, publication or release stops"
    expected: "No oracle execution, hardware testing, paid service, package install, or release action occurred"
    why_human: "Judgment-tier prohibition with no test descriptor; flagged unverified-prohibition, human review recommended. Non-authoritative verifier verdict: no violation observed (no new dependency or install in the Phase 08 diff; commit 15fd8d1 rebinds the Phase 07 oracle request hashes only and states local_decision stays blocked and publication_decision stays not-authorized). That commit's message cites an owner approval dated 2026-11-19; the verifier cannot confirm that approval and treats it as a claim."
---

# Phase 8: Audio Interface and Raw Capture Verification Report

**Phase Goal:** The AI turns guest DMA into raw audio buffers whose cadence comes only from guest time, and a headless run captures raw video and audio as files with metadata
**Verified:** 2026-11-19T14:46:00Z
**Status:** human_needed
**Re-verification:** No (initial verification)
**Verified source:** tested commit e15624cb; HEAD 08e403f adds only `docs/claims/phase08.md` and `docs/evidence/phase08-audio.md` (confirmed with `git show --stat`).

## Verdict

The goal is achieved for the authored scope the roadmap and claim define. Every roadmap success criterion has direct code and behavioral evidence, including a fresh end-to-end capture I ran myself and a mutation campaign that shows wrong samples, wrong cadence and wrong metadata are all rejected. No truth failed. The status is `human_needed` rather than `passed` for two reasons the decision tree does not allow me to absorb silently:

1. One spec-level negative invariant (no MI AI interrupt on a lone-descriptor completion) is present and correct in code but is not pinned by any test. Mutants that break it survive every Phase 08 suite.
2. Three judgment-tier prohibitions are still `status: unresolved` in the plans. I recorded a non-authoritative verdict for each (not violated) and flagged them for human review.

There are also non-blocking warnings (an undocumented-at-HEAD capture size limit, a spec wording mismatch, stale bookkeeping), listed below.

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
| --- | --- | --- | --- |
| 1 | SC1 (AUD-01, AUD-02): a cleared authored diagnostic produces raw sample buffers with sample index/rate metadata and exact values across DMA boundaries, two-deep queue behavior, and the interrupt at the documented `ai_full` 1-to-0 transition; submission or DMA start alone raises no interrupt | VERIFIED | See "SC1 evidence" below. |
| 2 | SC2 (AUD-03): cadence follows DACRATE and guest time, so differently paced host runs give identical sample streams | VERIFIED | See "SC2 evidence" below. |
| 3 | SC3 (RUN-06): a headless run writes raw video frames and raw audio buffers as files whose metadata covers sample index, rate, frame timing and profile/build identity, separate from host presentation, published without replacement | VERIFIED | See "SC3 evidence" below. |
| 4 | Expected PCM is authored independently of the code under test, and a wrong sample or a "non-silent" result is rejected (AUD-02) | VERIFIED | See "Independence and rejection" below. |
| 5 | Plan 08-05 qualification evidence is real and bound: exact suites reached, both hosted jobs executed accepted capture and no-replace finalization, published projection is bounded and cleared | VERIFIED | Hosted pair re-derived from the live run (below). Native ASan and pinned-GCC13 records were not re-run or opened (instruction), see note under Plan 08-05. |
| 6 | The claim and evidence are honest about scope per `docs/claims/TEMPLATE.md` | VERIFIED | `sh scripts/check-claims.sh` reports `files=62 blocks=4 errors=0`; scope review below; three wording or staleness warnings recorded. |
| 7 | Spec-level: the AI raises MI AI pending only at the 2-to-1 queue transition; a lone-descriptor completion does not (docs/spec/audio-interface.md:35-38) | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED | Code at `core/t64_ai.c:574-576` is correct. No test discriminates it: mutants M1b and M15 survive (table below). |

**Score:** 6/7 truths verified (1 present, behavior-unverified)

### SC1 evidence (AI DMA, two-deep queue, interrupt, metadata, exact values)

- Two-deep queue: `T64_AUDIO_QUEUE_CAPACITY 2u` (`core/t64_internal.h:75`). `submit_dma` refuses a third descriptor with `T64_STATUS_QUEUE_FULL` and leaves state unchanged (`core/t64_ai.c:205-207`); test checks status `0xc0000000`, a refused length write, and byte-identical AI state, events, MI and canonical digest (`core/tests/test_ai.c:666-713`).
- Interrupt edge: `count_before == T64_AUDIO_QUEUE_CAPACITY` then `t64_mi_set_pending(.., T64_MI_INTERRUPT_AI)` (`core/t64_ai.c:562,574-576`). Submission and DMA start never touch MI (`core/t64_ai.c:184-249,121-155`). Test: pending clear after two submissions (`core/tests/test_ai.c:734`), clear after the first sample (`:737`), set with the MI line high at the 2-to-1 completion (`:740-742`). Status write clears only bit `0x04` (`core/t64_ai.c:354-356`; test `core/tests/test_ai.c:754-775`).
- Metadata: packet info carries sequence, `first_sample_index`, first/last tick, frame count, rate numerator 48,681,812 and denominator `DACRATE+1`, channels, bits, encoding, profile id and build id (`core/t64_ai.c:532-548`; public struct `core/include/tendies64/t64_machine.h:277-292`). Observed in my end-to-end run: packet 0 sample index 0, packet 1 sample index 4, denominator 1104.
- Exact values across the DMA boundary: literal 32-byte PCM compared per packet in the unit tracer (`core/tests/test_ai.c:233-238,338,343-344`) and in the actual supervised runner (`runner/tests/test_audio_capture.c:42-46,1035`). The first packet's last sample and the second packet's first sample sit on the two-descriptor boundary.
- Boundary and rate-pair policies, address staging, `AI_LEN` readback, pause/resume, active-rate latch, overflow refusals: cases listed in `docs/spec/audio-cases.json` and all pass (`unit.core.ai.registers` 407 checks, `.cadence` 741, `.output` 478, `.determinism` 704, `.tracer` 40; 0 failures).
- Old profile keeps refusing AI writes: `unit.core.ai.old-profile-refusals`, `.profile-pins`, `.profile-mutations` pass. AI registers are routed only for the composite profile (`core/t64_bus.c:136`).

### SC2 evidence (guest-time-only cadence)

- Cadence arithmetic is integer-only on constants and DACRATE: `sample_delay()` computes `ceil((62,500,000 * (DACRATE+1) - remainder) / 48,681,812)` with carried remainder and overflow checks (`core/t64_ai.c:54-78`); events are scheduled with `t64_schedule_reserve_after`, relative to guest `machine->tick` (`core/t64_ai.c:89-109`).
- No host clock in the core: `tools/lint/core-lint.sh:257` bans `time clock clock_gettime gettimeofday mach_absolute_time ...` (and `getenv`, entropy sources) in core sources; `sh tools/lint/core-lint.sh` reports `files=94 violations=0`. A grep of `core/*.c` and `core/include` finds no time source.
- Runner audio path: the advance loop passes absolute guest-tick targets only (`runner/t64_real.c:1157-1172`). Host-clock uses elsewhere in `runner/` are not on the sample path: staging-directory name uniqueness (`runner/t64_raw_capture.c:331-342`, never appears in the bundle), a test-only commit barrier compiled under `T64_REAL_RUNNER_TESTING` (`:146-176`), and supervisor wall-time kill deadlines.
- Exact tick behavior: rational before/on/after at DACRATE 1103 and 1104 with an independently re-derived expected-tick formula (`core/tests/test_ai.c:209-218,923-972`); rate latch across descriptors (`:1037-1127`); pause/resume phase (`:1175-1225`).
- Pacing independence: whole-versus-split advance with an intermediate drain gives identical canonical digest, packet metadata and bytes (`core/tests/test_ai.c:2297-2367`); interleaved machines match isolated runs (`:2369-2456`); the actual runner at advance steps 16 and 32 reaches the same guest completion tick, canonical state digest, packet metadata and bytes (`runner/tests/test_audio_capture.c:3366-3417`, CTest `controls.real.audio-drain-cadence`). Output backpressure holds the same event and resumes exactly once without advancing guest time (`unit.core.ai.output`).
- Note on wording: `docs/evidence/phase08-audio.md:77-78` says no run compared different host throttle settings. That is an honest understatement. The varied levers are advance partition and drain schedule, which are the only host-controlled inputs the core can see. No wall-clock sleep variant exists because the core has no access to wall time (lint-enforced).

### SC3 evidence (raw A/V capture files with metadata, no-replace publication)

I ran the production supervisor against the retained authored fixture and inspected the result myself (output outside the repository tree):

- Bundle layout matches `docs/spec/raw-capture.md`: `manifest.json` (mode 0600), `video/frame-000000..000028.rgba8`, `audio/packet-000000..000001.s16be`, bundle dir mode 0700; no staging residue left beside the target.
- Manifest records profile id and SHA, descriptor 11 / model 19 / serialization 24, core build id (equal to the claim's native build `t64-build-v1-7ef62833...`), runner build id, source revision `e15624cbf435a4542d7d2dc1902f92f0ccf2de6e`, fixture and input hashes and sizes, terminal guest tick 551001, outcome `pass`, stop kind `authored-completion` (`runner/t64_raw_capture.c:690-783`).
- Audio entries: sample index (0 and 4), frame count 4, rate 48681812/1104, first/last guest tick, encoding, channels, bit depth. Concatenated packet files equal the independently typed 32-byte literal. The cadence is visible in the metadata: 540414..544666 then 546083..550335, a 1417-tick step across the DMA boundary (nominal 1417.37).
- Video entries: sequence, first/last guest tick, width/height/stride, `rgba8`, byte length 336 (12 x 7 x 4). Starts are strictly increasing.
- Accounting: stream bytes 9,776 + manifest 6,805 = `total_bytes` 16,581, matching `docs/evidence/phase08-audio.md:69`.
- Separate from presentation: the bundle is written from the general `t64_video_info/copy` and `t64_audio_info/copy` observer APIs (`runner/t64_real.c:479-657`); the runner has no presentation layer.
- No-replace: a second identical run against the same target returned report outcome `harness-error`, detail `capture-target-exists`, stop kind `host-error`; the first bundle's file hashes were byte-identical before and after. Publication is a retained-FD atomic rename (`renameatx_np RENAME_EXCL` / `renameat2 RENAME_NOREPLACE`, `runner/t64_raw_capture.c:313-329,1335-1339`) after file/dir fsyncs and a stage identity re-check (`:1308-1322`); the manifest is written last (`:1308-1309`). Race, I/O-fault, cap, truncated and interrupted-stage controls pass (CTests `controls.real.raw-capture-*`, `runner.raw-capture`).
- Finalize only on guest completion with no host error (`runner/t64_real.c:1497-1526`).

### Independence and rejection (AUD-02)

- The PCM literal exists as separately typed hex in three places: `runner/tests/test_audio_capture.c:42-46`, `core/tests/test_ai.c:233-238`, `scripts/qualify-audio.py:32-35`. I recomputed its SHA-256 from the typed bytes and it equals the claim's `9baa9bdf...fcd2ed`. The guest copies its own words from `fixtures/audio/pcm.S` into RDRAM; the machine under test never produces the expectation. No script generates expected values from the system under test. No test is skipped or disabled.
- `grep -i "non-silent"` over code, scripts and specs finds the phrase only in the evidence doc's rejecting sentence. No comparator accepts a non-silence criterion; all comparisons are `memcmp` against literals.
- Mutation campaign on a scratch copy of HEAD sources (never in the repository tree; baseline 9/9 green first):

| Mutant | Intent | Result |
| --- | --- | --- |
| M3 source copy shifted at sample 5 | wrong sample | caught: tracer, output, `runner.audio-pcm` and 3 controls (8 of 13 failed) |
| M10 flip one bit of the first left sample (exactly the wrong-sample build's delta) | wrong sample | caught: registers 3, cadence 3, output 3, determinism 1, tracer 1 failures, all 4 runner CTests |
| M4 floor instead of ceiling delay | cadence | caught: registers 65 failures, cadence aborts (hangs until killed), output 35, determinism 14, tracer 16, runner CTests |
| M9 denominator `DACRATE` instead of `DACRATE+1` | rate metadata | caught: cadence 4, output 1, all 4 runner CTests |
| M8 sample index reset per descriptor | sample index across DMA | caught: cadence 4, determinism 2, tracer 1, all 4 runner CTests |
| M2 interrupt on the wrong edge (`count_before == 1`) | interrupt | caught: registers |
| M14 interrupt raised on submission | "submission implies interrupt" | caught: registers 2 failures |
| M1b interrupt on every completion (`count_before >= 1`) | extra interrupt | SURVIVED all suites and runner CTests |
| M15 extra interrupt on the final completion | extra interrupt | SURVIVED all suites and runner CTests |

- The wrong-sample control (`controls.real.audio-wrong-sample`, `runner/tests/test_audio_capture.c:1030-1034,1103-1127`) accepts the wrong-sample ROM only by asserting the exact one-bit-different bytes; the strict comparator on the same bytes rejects it (`:1035`). M10 confirms the strict path is sensitive: the control and the literal run both fail when the machine changes that bit.
- Python side: `python3 -B scripts/tests/test_audio_qualification.py` ran 24 tests, OK, including changed-sample, truncated, forged and resealed observation rejection.

### Required Artifacts

| Artifact | Expected | Status | Details |
| --- | --- | --- | --- |
| `core/t64_ai.c` | AI registers, DMA queue, cadence, packets | VERIFIED | 680 lines of real logic; wired from `core/t64_bus.c:455,638` and the scheduler (`core/t64_scheduler.c:333-335`) |
| `runner/t64_raw_capture.c` | transactional capture bundle | VERIFIED | 1,352 lines; wired from `runner/t64_real.c:1088-1128,1497-1526` |
| `runner/t64_audio_observation.h` | pinned observation record | VERIFIED | static asserts pin layout (`:80-96`); used by runner and verifier |
| `runner/tests/test_audio_capture.c`, `core/tests/test_ai.c` | exact-value and behavior tests | VERIFIED (with WARNING W-1/truth 7) | all registered cases pass; one negative interrupt case not pinned |
| `docs/spec/audio-interface.md`, `raw-capture.md`, `audio-cases.json` | contracts and case inventory | VERIFIED (warnings W-2..W-4) | inventory has 75 distinct cases, 22 distinct CTests, as the evidence states |
| `scripts/qualify-audio.py` | bound evidence verifier | VERIFIED | self-tests pass; independent PCM literal at `:32-35` |
| `fixtures/audio/pcm.S`, `fixtures/manifest.json` | authored guest and identity | VERIFIED | manifest SHA-256 `f7c1953d...7931` equals the claim's `manifest_digest`; rebuilt ROM hash `cd955644...0b99` equals the recorded value |
| `docs/claims/phase08.md`, `docs/evidence/phase08-audio.md` | bounded claim and evidence | VERIFIED | see below |

### Key Link Verification

| From | To | Via | Status | Details |
| --- | --- | --- | --- | --- |
| `core/t64_bus.c` | `core/t64_ai.c` | register read/write routing for the composite profile only | WIRED | `core/t64_bus.c:136,455,638` |
| `core/t64_scheduler.c` | `core/t64_ai.c` | admission before popping a sample event, output-capacity wait | WIRED | `core/t64_scheduler.c:333-335` |
| `runner/t64_real.c` | public audio/video observer APIs | `t64_audio_info/copy`, `t64_video_info/copy` into the capture | WIRED | `runner/t64_real.c:479-657` |
| `runner/t64_supervisor.c` | `runner/t64_real.c` | `--raw-capture-dir` / `--raw-capture-max-bytes` forwarded | WIRED | `runner/t64_supervisor.c:474-475,1496-1527` |
| `.github/workflows/ci.yml` | `scripts/qualify-audio.py` | existing Linux and macOS jobs only | WIRED | hosted run succeeded on both |
| evidence doc | retained originals | hash bindings | WIRED for hosted; private native records not opened |

### Data-Flow Trace (Level 4)

| Artifact | Data variable | Source | Produces real data | Status |
| --- | --- | --- | --- | --- |
| audio packet bytes | `packet[]` | `memcpy` from guest RDRAM at the descriptor address per sample (`core/t64_ai.c:549-551`) | yes (e2e: bytes equal the literal; the in-flight source mutation case shows later guest stores are read, `core/tests/test_ai.c:1004-1035`) | FLOWING |
| manifest audio/video entries | `capture->audio[]/video[]` | packet info copied from the machine observer APIs (`runner/t64_raw_capture.c:1264-1272,1231-1239`) | yes | FLOWING |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
| --- | --- | --- | --- |
| Phase 08 CTests | `ctest --preset gate -R "ai\|audio\|raw-capture" -j4` | 27/27 passed | PASS |
| Qualification verifier tests | `python3 -B scripts/tests/test_audio_qualification.py` | 24 tests OK | PASS |
| Claims scan | `sh scripts/check-claims.sh` | files=62 blocks=4 errors=0 | PASS |
| Core lint (host clock ban) | `sh tools/lint/core-lint.sh` | files=94 violations=0 | PASS |
| Real capture end to end | production `t64-supervisor --real ... --raw-capture-dir <new>` | bundle as described under SC3; PCM equals literal | PASS |
| Refused re-publication | same command, same target | `capture-target-exists`; first bundle byte-identical | PASS |
| Hosted run record | `gh run view` and artifact API for run 44542244958 | success on both jobs, source e15624cb, durations 219 s and 429 s, artifact ids/sizes/digests match the evidence table exactly | PASS |
| Hosted projections | downloaded both artifacts, inspected | Linux and macOS agree on PCM SHA, 2 audio packets, 29 video packets, 16,581 bytes, source, fixture and input hashes; race outcomes `pass` and `harness-error:capture-no-replace-commit` on both; manifest SHA-256 values equal the evidence | PASS |
| Build identity reproducible | build HEAD in a scratch tree | core build id equals the claim's `7ef62833...`, fixture ROM hash equals `cd955644...` | PASS |

### Probe Execution

No `probe-*.sh` scripts are declared or exist for this phase. Not applicable.

### Requirements Coverage

| Requirement | Source plans | Description | Status | Evidence |
| --- | --- | --- | --- | --- |
| AUD-01 | 08-01, 08-02, 08-03, 08-05 | AI DMA, two-deep queue, DACRATE cadence and interrupt produce raw buffers with sample index and rate metadata | SATISFIED | SC1 and SC2 evidence; truth 7 carries one unpinned negative invariant (warning) |
| AUD-02 | 08-01, 08-03, 08-04, 08-05 | Authored diagnostic checks exact sample values across DMA boundaries; "non-silent" is never evidence | SATISFIED | Independence and rejection section; mutation results |
| AUD-03 | 08-01, 08-02, 08-03, 08-04, 08-05 | Cadence derived from guest time, never a host clock | SATISFIED | SC2 evidence; lint-enforced |
| RUN-06 | 08-04, 08-05 | Capture raw video frames and raw audio buffers from a headless run as files with metadata, separate from host presentation | SATISFIED within the authored-fixture runner | SC3 evidence; see warning W-1 for limits |

No orphaned requirements: REQUIREMENTS.md maps only AUD-01, AUD-02, AUD-03 and RUN-06 to Phase 8, and all four appear in plan frontmatter. Decision coverage gate: 9 of 9 CONTEXT decisions honored (non-blocking).

### Test Quality Audit

| Test file | Linked req | Active | Skipped | Circular | Assertion level | Verdict |
| --- | --- | --- | --- | --- | --- | --- |
| `core/tests/test_ai.c` | AUD-01/02/03 | 31 registered cases | 0 | none | value (byte and tick equality) | sufficient, except the one unpinned negative interrupt case |
| `runner/tests/test_audio_capture.c` | AUD-02/03, RUN-06 | all registered | 0 | none (typed literal, independent of SUT) | value and behavioral | sufficient |
| `scripts/tests/test_audio_qualification.py` | evidence binding | 24 | 0 | none | value | sufficient |

Disabled tests on requirements: 0. Circular patterns: 0. Insufficient assertions: 1 negative case (truth 7).

### Anti-Patterns Found

Debt-marker scan (`TBD`, `FIXME`, `XXX`) of the Phase 08 source, test, spec and script files found no markers; the only `XXX` hits are `mkdtemp` templates (`XXXXXX`) in `runner/tests/test_audio_capture.c`, which are not markers. No stubs, hollow props or placeholder returns on the audio path. Info only: unit CTests for `unit.core.ai.*` carry no timeout (ctest computed timeout 10,000,000 s), and mutant M4 made `unit.core.ai.cadence` loop instead of failing, so a regression of that kind would hang until the hosted job limit rather than fail fast.

### Plan 08-05 note

The hosted record, the verifier self-tests and the claim checker were verified independently. The native ASan/UBSan (181 cases), pinned GCC 13 (39 units) and full native gate results in `docs/evidence/phase08-audio.md` rest on privately retained records that I was told not to open or re-run. I confirmed only the consistent facts: 179 registered CTests (`ctest --preset gate -N`), and a green hosted `gate.sh` run on both operating systems at the tested commit. Those native-only numbers are therefore reported here as not independently re-derived, not as failures.

## Warnings (non-blocking)

- **W-1: capture size limit is real and undocumented at HEAD.** The supervisor starts the real runner with `RLIMIT_FSIZE` of `T64_REAL_RESULT_MAX_BYTES (4096) + FSIZE_SLACK (64 KiB)` = 69,632 bytes (`runner/t64_supervisor.c:44,1534-1536`). The runner writes every capture file, including `manifest.json`. So any single frame or PCM packet file over 69,632 bytes ends the run (fails closed, nothing published), and so does a manifest over that size. At about 174 bytes per video entry (measured: 6,805-byte manifest for 29 frames and 2 packets), the manifest alone caps a captured run at roughly 390 video entries, well below the documented 4,096-entry in-code bound (`runner/t64_raw_capture.c:34`) and the documented 64 MiB default and 256 MiB maximum (`docs/spec/raw-capture.md:13-17`). The accepted authored case (12 x 7, 29 frames, 336-byte frames, 16,581 bytes) is unaffected, and the runner currently accepts only registered authored fixtures (`runner/t64_real_fixture.c:8,49,54`), so RUN-06 holds for what the runner can run. But the committed spec reads as if realistic frames (a 320 x 240 RGBA frame is 307,200 bytes) and long runs are capturable. `general-frame-geometry` only exercises 4 x 3 and 12 x 7 frames. During this verification, another process left an uncommitted edit to `docs/spec/raw-capture.md` and `docs/evidence/phase08-audio.md` that documents the per-file limit (I confirmed the 69,632-byte arithmetic). That text does not mention the manifest bound, which binds first. Recommended: commit the disclosure, add the manifest bound, and track raising the limit for the capture directory before any larger fixture or capture consumer.
- **W-2: spec says "JSON integer fields are exact integers" but the manifest emits decimal strings.** `docs/spec/raw-capture.md:59-60` versus `runner/t64_raw_capture.c:663-688` (`json_u64_string`): every count, tick, sequence, size and version except `schema_version` is a quoted decimal string (for example `"descriptor_version":"11"`, `"first_tick":"540414"`). This matches the project's u64-safe result convention and the tests parse it that way (`t64_json_u64_string`), so the data is exact, but an external consumer following the spec would mis-parse. Doc-only fix.
- **W-3: stale statement in committed `docs/spec/raw-capture.md:115-117`.** At HEAD it says Linux primitive execution is pending the hosted job and no Linux runtime evidence is claimed, while the evidence doc and the hosted artifacts show the Linux job ran the capture and no-replace finalization. The uncommitted working-tree edit corrects this; it needs to be committed.
- **W-4: wording of the wrong-sample control.** `docs/evidence/phase08-audio.md:75-76` says the control "rejects a wrong sample". The control actually asserts the exact wrong bytes reach the comparator after accepted setup; the rejection is the strict literal comparison. The behavior is real (M10), the sentence is loose.
- **W-5 (planning bookkeeping, not code): Phase 08 closeout is not recorded.** There is no `08-05-SUMMARY.md`; `.planning/ROADMAP.md` still shows Phase 8 as `4/5 plans executed` with Plan 08-05 unchecked; `.planning/REQUIREMENTS.md` still lists AUD-01..03 and RUN-06 as `Pending`; `08-VALIDATION.md` frontmatter still says `status: draft`, `nyquist_compliant: false` and its prose says no Phase 08 test has run. These are expected to be updated by the orchestrator after verification.
- **Working-tree note.** When I first checked, `git status` showed two modified files unrelated to this phase's code (`.planning/AUTONOMY.md`, `docs/ci-reviews/LOG.md`). During my run another process also modified `docs/evidence/phase08-audio.md` and `docs/spec/raw-capture.md` and added `.planning/phases/08-audio-interface-and-raw-capture/08-SECURITY.md`. I did not make those changes and verified implementation and tests at HEAD. I read the diff only to assess W-1 and W-3.

## Deferred Items

None. Phase 9, the only later phase in the current roadmap, does not take over any Phase 08 item.

## Human Verification Required

### 1. Single-descriptor completion must not raise MI AI (behavior-unverified truth 7)

**Test:** Enable AI with a valid DACRATE/BITRATE pair, submit one 8-byte descriptor, advance to completion, and read MI interrupt. Then submit two, let the first complete (MI AI now set), acknowledge with an `AI_STATUS` write, and let the second (last) descriptor complete.
**Expected:** MI AI pending stays clear after the lone completion, and stays clear after the last completion that follows an acknowledged 2-to-1 interrupt.
**Why human:** The code is correct (`core/t64_ai.c:574-576`) but cannot be seen failing: mutants M1b and M15 passed every Phase 08 suite. The cheapest closure is a few lines added to `core/tests/test_ai.c` next to `case_mi_full_falling_edge` (`:715-752`), which then makes this an automatically verified truth.

### 2. Flagged judgment prohibitions (three items, non-authoritative verdict: not violated)

`unverified-prohibition: human review recommended` for each; none is silently passed.

- **08-01-P1:** authored PCM success must not be presented as silicon accuracy, broad compatibility, VID-04 completion or oracle authority. Verdict: not violated. `docs/claims/phase08.md` carries an eight-clause `not_claimed`, uses tier `unit-tested`, names no emulator or hardware oracle, and its prose says no VID-04, hardware, release or broader compatibility result is claimed; the reserved words are unused (checker clean).
- **08-01-P2:** generated ROMs and raw captures must not be published or uploaded. Verdict: not violated. No tracked ROM, ELF, raw frame or PCM file; CI uploads only JSON projections; the downloaded hosted artifacts are small archives (about 8.9 KB) holding only JSON files, the largest 8.6 KB, with hashes only and no private paths.
- **08-05-P3:** Phase 08 must not bypass oracle authority, private-window, physical, paid, install, publication or release stops. Verdict: no violation observed. No dependency, install or release change in the Phase 08 diff; the one Phase 07 oracle-record commit rebinds hashes only and states blocked and not-authorized remain. The owner approval cited in that commit message is a claim I cannot confirm.

## Gaps Summary

No gaps. No truth failed, no artifact is missing or a stub, and every key link is wired. The phase achieves its goal for the authored scope: the AI produces exact raw PCM across two queued DMAs on a guest-time-only cadence, and the real supervised runner writes a complete, metadata-bearing, no-replace-published raw A/V bundle that I reproduced and cross-checked against the independent literal and the live hosted record. The remaining work is closing the one unpinned negative-interrupt test (or accepting it), resolving the three flagged prohibitions, disclosing the capture size limit (W-1) in committed docs, and ordinary Phase 08 closeout bookkeeping (W-5).

---

_Verified: 2026-11-19T14:46:00Z_
_Verifier: Claude (gsd-verifier)_
