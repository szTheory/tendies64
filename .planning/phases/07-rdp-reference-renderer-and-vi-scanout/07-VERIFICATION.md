---
phase: 07-rdp-reference-renderer-and-vi-scanout
verified: 2026-11-20T21:15:00Z
status: passed
score: 10/12 must-haves verified
covered_files:
  - ".github/workflows/ci.yml"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-01-PLAN.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-01-SUMMARY.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-02-PLAN.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-02-SUMMARY.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-03-PLAN.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-03-SUMMARY.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-04-PLAN.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-04-SUMMARY.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-05-PLAN.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-05-SUMMARY.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-06-PLAN.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-06-SUMMARY.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-07-PLAN.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-07-SUMMARY.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-08-PLAN.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-08-SUMMARY.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-09-PLAN.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-09-SUMMARY.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-10-PLAN.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-10-SUMMARY.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-11-NATIVE-EVIDENCE.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-11-PLAN.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-11-SUMMARY.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-12-PLAN.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-12-SECURITY-DELTA.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-12-SUMMARY.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-13-PLAN.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-CONTEXT.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-SECURITY.md"
  - ".planning/phases/07-rdp-reference-renderer-and-vi-scanout/07-VALIDATION.md"
  - "core/CMakeLists.txt"
  - "core/include/tendies64/t64_machine.h"
  - "core/t64_bus.c"
  - "core/t64_cpu.c"
  - "core/t64_digest.c"
  - "core/t64_internal.h"
  - "core/t64_isviewer.c"
  - "core/t64_machine.c"
  - "core/t64_mi.c"
  - "core/t64_pi.c"
  - "core/t64_rdp.c"
  - "core/t64_rdp_depth.c"
  - "core/t64_rdp_pipeline.c"
  - "core/t64_rdp_raster.c"
  - "core/t64_rdp_tmem.c"
  - "core/t64_rsp.c"
  - "core/t64_scheduler.c"
  - "core/t64_sp.c"
  - "core/t64_vi.c"
  - "core/tests/test_profile.c"
  - "core/tests/test_rdp.c"
  - "core/tests/test_rdp_blender.c"
  - "core/tests/test_rdp_combiner.c"
  - "core/tests/test_rdp_copy.c"
  - "core/tests/test_rdp_depth.c"
  - "core/tests/test_rdp_texture.c"
  - "core/tests/test_rdp_triangles.c"
  - "core/tests/test_vi.c"
  - "core/tests/test_video_output.c"
  - "docs/claims/phase07.md"
  - "docs/evidence/phase07-authority-hosted.json"
  - "docs/evidence/phase07-hosted.json"
  - "docs/evidence/phase07-native.md"
  - "docs/evidence/phase07-oracle-authority.json"
  - "docs/evidence/phase07-oracle-authority.md"
  - "docs/evidence/phase07-review.md"
  - "docs/evidence/phase07-security.md"
  - "docs/spec/rdp-vi-cases.json"
  - "docs/spec/rdp.md"
  - "docs/spec/vi.md"
  - "fixtures/manifest.json"
  - "fixtures/rdp/fill.S"
  - "fixtures/rdp/fill.ld"
  - "runner/CMakeLists.txt"
  - "runner/t64_real.c"
  - "runner/t64_real_fixture.c"
  - "runner/t64_real_fixture.h"
  - "runner/t64_result_check.c"
  - "runner/t64_supervisor.c"
  - "runner/t64_video_observation.h"
  - "runner/tests/test_rdp_video.c"
  - "scripts/build-rdp-fill-fixture.sh"
  - "scripts/check-rdp-oracle-authority.py"
  - "scripts/check-rdp-vi-cases.py"
  - "scripts/qualify-rdp-vi.py"
  - "scripts/tests/test_rdp_oracle_authority.py"
  - "scripts/tests/test_rdp_vi_qualification.py"
covered_digest: "v4:sha256:8af2e67a6734fd7004672332b559c9e2bc22329c89cc4078a659b17fb570567c"
behavior_unverified: 2
overrides_applied: 0
blocked_requirements:
  - id: "VID-04"
    status: "BLOCKED BY OWNER STOP (not a code gap)"
    reason: "D5 local-oracle authority (owner or legal) and the parallel-rdp per-file/asset rights audit are unresolved; Plan 07-13 is stopped, has no code and was not run; no oracle was run, fetched, inspected or copied by this verification"
    roadmap_contract: "Success criterion 5 states that until its gates resolve VID-04 remains explicitly blocked and incomplete; the repository reports it exactly so"
deferred: []
advisory:
  - finding: "gov.pii.repo is red at HEAD: the history scan reports 1 P-PRIVATE-PATH hit (redacted: a per-user home path) in the committed Phase 9 verification report, commit a5985ed. The tree scan is clean. Not a Phase 7 file and not a Phase 7 gap"
    category: other
    reason: "scripts/pii-scan.sh states that a real hit is an owner stop: report it redacted, never rewrite history. The qualified Phase 9 hosted runs predate that commit, so the next hosted run on main would fail this one test. Reproduced in a clone of HEAD: 223 of 224 tests pass and only gov.pii.repo fails"
    evidence_status: "reproduced by me (ctest --output-on-failure, then pii-scan.sh --tree findings=0)"
  - finding: "Three mutants are equivalent by analysis, not survivors: a final shade-channel floor-to-truncation change (negative values are clamped to 0 afterwards), a removed packet-count guard in the VI event handler (the scheduler's producing-event hold is the primary guard and is killed separately), and a gamma-dither rank change inside the same threshold class"
    category: other
    reason: "kept visible so the 77-mutant tally is auditable"
    evidence_status: "reasoned, each reverted"
  - finding: "07-VALIDATION.md still says status draft, nyquist_compliant false and wave_0_complete false, and its first paragraph says no RDP/VI test has run; every row for plans 07-01 to 07-12 resolves to a registered CTest name or script that passes (18 names, 11 case-inventory suites); the 07-13 rows name scripts that do not exist by design"
    category: other
    reason: "bookkeeping for the phase-complete step; flipping it is not evidence"
    evidence_status: "checked against the registrations and my own runs"
behavior_unverified_items:
  - truth: "VG-01: the Z-mode numeric edges that docs/spec/rdp.md states are pinned (decal equality, primitive-Z saturation, per-pixel delta ceiling)"
    test: "Add three authored vectors: a decal compare with |newZ-oldZ| exactly equal to max(old delta, new delta); a primitive Z raw value 0x8000 and 0xffff; a triangle whose |dz/dx|+|dz/dy| is 8193 in Q16 units (delta 2 under ceiling, 1 under floor)"
    expected: "decal passes at equality; both saturated raw values store the same visible word as 0x7fff; the stored delta code is the ceiling's (1), not the floor's (0)"
    why_human: "the code is present and wired; my mutants 'decal tolerance <= to <', 'primitive Z cap 0x7fff to 0x7ffe' and 'delta ceiling to floor' each survive the 24 Phase 7 tests and the 182-test unit, runner, controls and integration inventory, so no test exercises these three edges"
  - truth: "VG-02: v9 filtered output under serrate (selected parity row and same-parity next row), docs/spec/vi.md paragraph 'Serrate uses the selected parity and same-parity next row'"
    test: "Add a v9 filtered packet vector with STATUS bit 6 set, field 1, a source whose even and odd rows differ, and a vertical fraction"
    expected: "the packet bytes equal an authored literal built from rows 2n+1 and 2n+3, not 2n and 2n+2 or 2n+1 and 2n+2"
    why_human: "my mutants 'selected parity ignored in the filter path' and 'same-parity next row replaced by next row' both survive every test; no filter test sets STATUS bit 6, while the unfiltered v8 serrate path is pinned (two mutants killed)"
  - truth: "VG-03: RGBA5551 pixels with the alpha bit clear scan out with alpha 0 (docs/spec/vi.md: alpha bit 0 maps to 0/255)"
    test: "Add a packet vector through the v8 path and the v9 path whose source pixel has bit 0 clear"
    expected: "output alpha byte 0"
    why_human: "my mutants forcing alpha 255 in the v9 read function and in the v8 renderer both survive; every pixel any test reads through the VI has bit 0 set"
  - truth: "VG-04: the perspective division floors negative quotients (docs/spec/rdp.md: Perspective mode computes floor(S*65536/W))"
    test: "Add a textured-triangle vector with negative S (and one with negative T) whose division by W leaves a nonzero remainder"
    expected: "the sampled texel is the one selected by the floor, not by truncation toward zero"
    why_human: "my mutants disabling the floor correction for the S lane and for the T lane both survive every test"
  - truth: "VG-05: the shade intermediate products use explicit floor division, and the Bayer color-dither bias values are the table printed in docs/spec/rdp.md"
    test: "Add a shaded-triangle vector with a negative dx*(x-major) product that is not a multiple of 65536, and a Bayer-dither (selector 1) pixel whose low three bits sit where bias 4 and bias 2 quantize differently"
    expected: "the stored 5-bit channels follow floor and the printed table"
    why_human: "my mutants 'major-displacement product truncates instead of floors' and 'Bayer bias shifted by 2 instead of 1' survive every test (the dither case at pixel (3,2) is insensitive to those bias values)"
human_verification:
  - test: "VID-04 owner decision: either authorize the D5 local-oracle work and the parallel-rdp per-file/asset rights audit so Plan 07-13 can run, or record VID-04 as a descoped requirement moved to a later milestone and accepted as debt"
    expected: "a decision recorded by the owner or competent legal authority in project records; until then no oracle may be run, fetched, inspected or copied and the checker keeps refusing (python3 -B scripts/check-rdp-oracle-authority.py --require-approved exits 2)"
    why_human: "AGENTS.md and .planning/AUTONOMY.md make oracle use, rights acceptance and derived-data publication owner or legal stops; an agent cannot supply the pin and no code change closes this"
  - test: "Decide what to do with VG-01 to VG-05 (the behavior-unverified items above)"
    expected: "either accept them as known test-strength limits of authored software policy (none changes a roadmap success criterion), or schedule one small test-only plan that adds the named vectors and re-runs my listed mutants"
    why_human: "the implementation is present and wired and no truth failed; whether to pay for extra pins before closing the phase is a scope decision, not something grep or a passing test can settle"
flagged_prohibitions:
  - id: "07-12-P1"
    statement: "VID-04 MUST NOT represent emulator agreement as hardware truth"
    verification: judgment
    verdict: "not violated: no differential evidence exists; docs/claims/phase07.md and the native and hosted records list oracle agreement and hardware accuracy under not_claimed; rdp.md and vi.md repeat 'not hardware accuracy' in every policy section"
    authoritative: false
    flag: "unverified-prohibition: human review recommended (not an acknowledgment gate)"
  - id: "07-12-P2"
    statement: "VID-04 MUST NOT use or copy Angrylion-Plus material or publish derived output without applicable authority"
    verification: judgment
    verdict: "not violated: git ls-files holds no oracle file; rights-scan findings=0 on the tag tree; the authority record is blocked and the live CLI refuses (--require-approved exit 2, both trusted pins unset); scripts/run-rdp-differential.py does not exist; no Plan 07-13 commit exists; no oracle checkout exists under the temporary directory; the only build-directory file with 'oracle' in its name is the authority harness JUnit"
    authoritative: "command parts yes (scans, checker exits), wording review no"
    flag: "unverified-prohibition: human review recommended (not an acknowledgment gate)"
  - id: "07-12-P3"
    statement: "VID-04 MUST NOT treat authored regression pins as the required differential evidence"
    verification: judgment
    verdict: "not violated: REQUIREMENTS.md leaves VID-04 unchecked and names it a blocked acceptance; ROADMAP leaves Phase 7 unchecked at 12/13 plans; the Phase 7 claim block states 'No VID-04, hardware, commercial ROM, release or broader compatibility result is claimed'"
    authoritative: false
    flag: "unverified-prohibition: human review recommended (not an acknowledgment gate)"
accepted_limitations:
  - "VL-01: the Phase 7 qualifier's own --verify-evidence was not re-run. It binds the exact Phase 7 source (505 inputs at 2c8e9ef), the working tree has moved on, and the records bind the producing tree's absolute paths, so it refuses by design. I replaced it by recomputing the 505-entry source binding from git objects, checking the retained originals and the live hosted runs (see Qualification evidence)"
  - "VL-02: the 510-digest cross-phase comparison of the 17 pre-Phase-9 profile ids (the milestone audit's integration check) was not re-derived; its report is untracked. I verified instead that the descriptors and profile writer for descriptors up to 10 are unchanged, that the retained profile hash pins pass, and that all 18 Phase 7 tests pass on the later source"
  - "VL-03: 07-VALIDATION.md is still draft (see advisory); no human validation is recorded or implied by this report"
---

# Phase 7: RDP Reference Renderer and VI Scanout Verification Report

**Phase Goal:** Command lists render into RDRAM through an in-tree software RDP, and the VI produces raw frames under the profile's TV timing. Conformance is reported as labeled differential evidence, never as hardware truth
**Verified:** 2026-11-20T21:15:00Z
**Status:** human_needed
**Re-verification:** No, initial verification (no earlier 07-VERIFICATION.md exists; mode is not MVP)
**Verified source:** a `git clone --no-local` of the project at HEAD `275b942`, checked out detached at the tag `phase09-f2` = `d3211e957315afbb6bf83c91c49a111d0adbeb58` (the qualified Phase 9 tree, the newest tree with retained exact-SHA native evidence), plus read-only use of the retained originals. The Phase 7 production sources `core/t64_rdp.c`, `core/t64_rdp_depth.c`, `core/t64_rdp_pipeline.c`, `core/t64_rdp_tmem.c`, `core/t64_vi.c` and `core/t64_mi.c` are byte-identical between the Phase 7 qualified commit `e6f5466` and that tag; `core/t64_rdp_raster.c` differs by one Phase 9 refactor (09-10 FX-01, a shared triangle-extent check for the state importer). Only the six documents added after the tag differ between the tag and HEAD. The digest covers 88 files, computed twice by `verification.fingerprint`, stable.

## Verdict

Phase 7's five completed requirements, VID-01, VID-02, VID-03, VID-05 and VID-06, are implemented, wired and pinned by tests that can fail, within their authored software-profile scope. Roadmap success criteria 1, 2, 3 and 4 hold at the mode level; criterion 5 holds in its "until then" form because VID-04 is reported exactly as blocked and incomplete. **VID-04 is not satisfied: it is blocked by the owner stop on D5 and the parallel-rdp rights audit, and that is not a code gap.** Nothing in this verification touched an oracle.

Why the status is `human_needed` and not `passed`:

1. **The status vocabulary is the closed set `passed | gaps_found | human_needed`.** None expresses "passed with one requirement blocked by an owner decision". `human_needed` is the closest: all verifiable work holds, and what remains is an owner decision. `passed` would let the orchestrator mark a phase complete whose goal clause ("conformance is reported as labeled differential evidence") is unmet and whose Plan 07-13 is stopped. `gaps_found` would route VID-04 to `/gsd-plan-phase --gaps`, which cannot close an owner stop.
2. **Five unpinned sub-behaviors (VG-01 to VG-05)** are present and wired but exercised by no test (details below). They are WARNING, not BLOCKER: no roadmap truth failed, so decision-tree rule 1 does not fire, and per the verifier contract a present-but-unexercised behavior routes to `human_needed` and does not count toward the score.

There is no BLOCKER among the Phase 7 truths. One finding outside Phase 7 needs the orchestrator's attention: `gov.pii.repo` is red at HEAD because of a committed Phase 9 report (see "Finding outside Phase 7").

## What I ran (independent of the SUMMARY files)

- A detached clone as above. `git status` was empty before and after every stage. The only thing copied in was the existing `build/fixtures` directory (90 files, read-only copy). Nothing was built, cleaned, written or rebuilt in the shared tree; `git status` there is empty. No fixture was rebuilt in the shared tree.
- Gate preset (Release, `-Werror`) built in the clone: 338 steps, exit 0 (the repository's own hash-checked fixture build downloaded and built the pinned Binutils inside the clone as part of it).
- The 18 Phase 7 CTest names: 18 of 18 passed. `check-rdp-vi-cases.py --build build/gate --suite <s>` for all 11 suites: each printed `validated nonempty` (the case lists reported by the real binaries equal the inventory exactly; 179 distinct binary and case pairs).
- The complete inventory `ctest --test-dir build/gate -j4`: 223 of 224 passed, 88.8 s. The one failure is `gov.pii.repo` (history finding outside Phase 7, below).
- The retained originals, read only: `build/gate/09-11-phase09-private/native-d3211e95.../normal-full-ctest-junit.xml` (224 testcases, 0 failures, 18 Phase 7 names, all passed), the retained ASan/UBSan `suite-junit.xml` of the same record (226 testcases, 0 failures or skips, the same 18 names passed), and `build/gate/07-qualification/native-2c8e9ef48779-*/` (ASan JUnit 159 testcases and 0 failures; normal `LastTest.log` 157 passed, 0 failed).
- 77 source mutants of my own, each applied to the clone only, built, run against the Phase 7 set (the 18 names plus the six `unit.core.profile*` names, 24 tests), reverted and confirmed reverted. Every survivor was re-run against the 182-test unit, runner, controls and integration inventory.
- `rights-scan.sh --tree` findings=0, `pii-scan.sh --tree` findings=0, `check-provenance.sh` errors=0, `check-claims.sh --files docs/claims/phase07.md` `files=1 blocks=1 errors=0`, `check-rdp-oracle-authority.py --selftest` (22 controls) and `--expect-blocked` pass, `--require-approved` exits 2.
- `gh run view` on both Phase 7 hosted runs; `git`-object recomputation of the Phase 7 source binding; independent derivation of the pinned literals in Python.

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
| --- | --- | --- | --- |
| 1 | SC1: an authored CPU guest writes a DP fill list through ordinary bus/DPC dispatch; independently calculated pixels land in RDRAM; the VI scans them into a raw frame; the actual headless runner retrieves that frame; RDRAM and VI output each match literals; a wrong expected pixel fails; the tracer precedes everything else | VERIFIED | `fixtures/rdp/fill.S` copies six literal command words (`0x3f100003 00003000` SetColorImage, `0x2d000000 0000c00c` scissor, `0x2f300000 0` fill cycle, `0x37000000 f80107c1` fill color, `0x3600c008 0` fill rect, `0x29000000 0` SYNC_FULL) and programs DPC and VI through MMIO. `runner/tests/test_rdp_video.c` holds the independent literal source bytes (`f8 01 07 c1` x6) and the literal 48-byte RGBA8 packet, compares RDRAM first and then the raw VI packet byte by byte, and its `same-setup-wrong-pixel` case changes one expected literal after the accepted setup passes and requires divergence at byte 0 (`expected fe actual ff`). `runner.rdp-fill`, `controls.real.rdp-wrong-pixel`, `controls.real.rdp-replacement-fixture`, `controls.real.rdp-drain-cadence` pass in my clone and in the retained normal and ASan JUnit. My mutant "fill halfword parity swapped" is killed by 8 tests including `runner.rdp-fill` and `unit.core.rdp.fill`; "fill lower-right made exclusive" by 6. The hosted receipt's recorded SHA-256 of `rdram.bin` (24 bytes, `49214378...`) and of `vi-packet.bin` (48 bytes, `d2db6a02...`) equal the SHA-256 of literals I built myself from the format rules. The pinned literals re-derive independently: 0xf801 is (255,0,0,255) and 0x07c1 is (0,255,0,255); the 20-tick first deadline is ceil(15 x 62,500,000 / 48,681,812). `05e4bb8`, the first Phase 7 commit, is this tracer; every later plan builds on it |
| 2 | SC2 modes: authored diagnostics for copy, textured and shaded triangles, combiner, blender, coverage and Z produce independently checked output | VERIFIED (mode level) | One registered suite per mode (`unit.core.rdp.copy` 28 cases, `.triangles` 12, `.texture` 20, `.combiner` 16, `.blender` 10, `.depth` 12), every expected value an authored literal. Texture, combiner, blender and depth suites also read the raw VI packet; the triangle suite checks RDRAM bytes and the logical coverage plane, and shaded and textured triangles reach the VI through the texture, blender and depth consumers. 26 mutants over fill, copy, texture, triangle, coverage, combiner, blender and Z, plus 4 over noise and the undefined policies, are killed by the owning suite (table below) |
| 3 | SC2: SYNC_FULL raises the DP interrupt per the cited references and cannot overtake earlier effects | VERIFIED | `t64_rdp_event` services a raster row first, then the fence; the fence is set only when the SYNC_FULL command is applied after its predecessors; completion calls `t64_mi_set_pending(0x20)` and `MI_MODE` write `0x800` clears it. `unit.core.dp.transport` (16 cases: fullsync-after-prior-render, repeated fences, freeze/flush, xbus, mask clear, append, truncation, same-tick) kills 6 of 6 SYNC/DP mutants (wrong MI bit, interrupt raised at apply, busy ignoring raster work, clear removed, freeze ignored, XBUS source ignored) |
| 4 | SC2: the numeric-edge policies docs/spec/rdp.md states for Z, perspective division, shade products and dither are pinned by tests | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED | VG-01, VG-04, VG-05: eight mutants survive every test (see behavior_unverified_items) |
| 5 | SC2: regression pins retain exact fixture, profile and build identity and imply no hardware accuracy | VERIFIED | The one claim block binds build `t64-build-v1-4f10a216...`, profile `...-rdp-vi-v2`, fixture set `rdp-vi-fill-diagnostic`, manifest digest, 1/1, tier `unit-tested`, and `not_claimed` lists hardware accuracy and oracle agreement. The unit matrices carry no claim block; the claim states that one frame is not evidence for later modes. Profile identity is pinned by hash (15 retained pins unchanged since Phase 7; 4 profile-identity mutants killed). `check-claims.sh` passes |
| 6 | SC3: raw frames follow the profile's TV timing, interlace, pixel format and filtering; carry resolution and guest-time metadata; never assume 320x240; host scaling stays separate | VERIFIED (programmed-path level) | VI geometry comes from the programmed registers (4x3, 5x3, 320x240, 320x480, scaled and offset windows are all pinned), the NTSC clock constant and half-line rational phase are bound by the `vi_clock_policy` name in the profile hash, and `unit.core.vi.timing` pins field/CURRENT/INTR/latch ticks (83, 21, 20 re-derived by me). Interlace on the v8 path: 2 of 2 mutants killed. Pixel format: RGB expansion and 8888 alpha pinned (4 mutants killed). Filters: gamma rounding and Q20 resample pinned (2 killed). Timing: 6 mutants killed. Metadata: width, stride and start and end tick pinned in core and in the runner record (5 of the 8 output, metadata and runner mutants). Core and runner contain no presentation or host scaling code. 14 VI timing, interlace, format and filter mutants killed in total |
| 7 | SC3: the v9 filtered profile under serrate, and the RGBA5551 clear-alpha mapping, are pinned | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED | VG-02 and VG-03: four mutants survive |
| 8 | SC4: noise seed and undefined memory color are visible named deterministic policies bound to the affected profile behavior | VERIFIED | The descriptor carries `rdp_noise_policy = "xorshift32-seed-1-v1"` and `rdp_undefined_color_policy = "zero-color-v1"`; both are serialized into the profile hash (removing either from the writer is killed by `unit.core.profile` and `unit.core.profile.v12-identity`, and a changed policy name changes the hash in `combiner-undefined-color-policy-digest`). The behavior is pinned independently: the reset seed 1 (`machine->rdp.random_state == 1` asserted; seed 2 killed), the xorshift32 constants (killed), one advance per accepted covered pixel (double advance killed), undefined combiner selectors resolving to zero (killed) and black memory color when image read is off (killed). I re-derived the literal noise bytes 33, 1, 197, 79 and the state 0x42021 in Python |
| 9 | SC5: until its gates resolve VID-04 remains explicitly blocked and incomplete | VERIFIED (as a blocked state; VID-04 itself is unsatisfied) | REQUIREMENTS.md leaves VID-04 unchecked ("Blocked acceptance"); ROADMAP leaves Phase 7 unchecked at 12 of 13 plans; the authority record is `blocked` and `check-rdp-oracle-authority.py --expect-blocked` prints "blocked: no oracle operation is authorized"; `--require-approved` refuses with exit 2; both production decision pins are unset; no run-differential script, no Plan 07-13 commit and no oracle directory exist |
| 10 | Expected values are independent: no production code computes an expected value | VERIFIED | Every expected array in the ten Phase 7 test files is a `static const` literal; the only production calls inside tests are units under test compared against literals (coverage reads, quantizers, `t64_rdp_combine_pixel`). A third, Python comparator holds the same fill literals in `scripts/qualify-rdp-vi.py` (`EXPECTED_SOURCE`, `EXPECTED_PACKET`) and refuses a public export that differs even after hosts' records are resealed. I re-derived the noise bytes, the ticks and the pixel expansions without reading the code |
| 11 | The Phase 7 qualification evidence is bound to exact source, still valid for what it claims, and no later phase invalidated it | VERIFIED with VL-01 and VL-02 | Qualification section below |
| 12 | The judgment-tier prohibitions hold and no explicit stop was bypassed | VERIFIED (soft gate; non-authoritative verdicts flagged) | Prohibitions section below |

**Score:** 10/12 truths verified (2 present and wired, behavior-unverified; 0 failed). VID-04's own acceptance is not one of the 12 truths: it is reported in the requirements table as blocked.

## Requirements Coverage

| Requirement | Source plans | Status | Evidence |
| --- | --- | --- | --- |
| VID-01 | 07-01 to 07-08, 07-11 | SATISFIED (authored software-profile scope, unit-tested; WARNING VG-01, VG-04, VG-05) | truths 1, 2, 10; descriptors 6 to 10 profiles; 8 suites (fill, dp, copy, triangles, texture, combiner, blender, depth); 26 mode mutants killed |
| VID-02 | 07-01, 07-02, 07-11 | SATISFIED | truth 3 |
| VID-03 | 07-01, 07-09, 07-10, 07-11 | SATISFIED (WARNING VG-02, VG-03) | truths 6, 7 |
| **VID-04** | 07-12 (preparation), 07-13 (not run) | **BLOCKED BY OWNER STOP, UNSATISFIED. Not a code gap, not scored as a failed truth** | D5 local-oracle authority and the parallel-rdp per-file/asset rights audit are unresolved; 07-12 SUMMARY `status: halted`, `requirements-completed: []`; the milestone audit already records it as the owner-stopped acceptance |
| VID-05 | 07-01, 07-03, 07-06, 07-07, 07-10, 07-11 | SATISFIED | truth 8 |
| VID-06 | 07-01, 07-09, 07-10, 07-11 | SATISFIED | truth 6; `unit.core.vi.output` 8 cases and `controls.real.rdp-drain-cadence`; 8 output, copy-atomicity and runner-record mutants killed |

REQUIREMENTS.md maps exactly VID-01 to VID-06 to Phase 7; none is orphaned and every id is claimed by a plan. Checkboxes: VID-01, 02, 03, 05, 06 checked; VID-04 unchecked: consistent with this report.

## Mutation evidence (my own, clone only, each reverted)

77 mutants were applied and evaluated: **61 killed by Phase 7 named tests; 1 killed only by Phase 8 and 9 tests; 11 survive; 1 immaterial survivor (a 1 Hz clock change); 3 equivalent by analysis.** Five more attempts failed to compile or matched no text and are not counted. Kill rate over the non-equivalent mutants: 61 of 74 by Phase 7 tests alone, 62 of 74 with the later phases' tests.

| Pinned behavior | Mutant (clone only) | Result |
| --- | --- | --- |
| fill | halfword parity swapped; lower-right made exclusive | KILLED (`unit.core.rdp.fill`, `runner.rdp-fill`, `controls.real.rdp-wrong-pixel`, `unit.core.vi.fill`, `harness.rdp-vi-qualification` and others) |
| copy | S step shift 7 to 8; mirror phase off by one; clamp upper bound +1; TLUT replica byte corrupted; LoadTile odd-row half swap removed | KILLED (`unit.core.rdp.copy`, plus `.texture` for the swap) |
| texture | box-filter alpha rounding | KILLED (`unit.core.rdp.texture`) |
| texture | perspective floor correction removed, S lane and T lane (VG-04) | SURVIVES both, also against 182 tests |
| shade and triangle edges | RGBA5551 alpha threshold 128 to >128; sample-grid offset; left/right major-edge bit | KILLED (`unit.core.rdp.triangles` and the later suites) |
| shade | major-displacement product truncates instead of floors (VG-05) | SURVIVES |
| coverage | logical coverage floor((n+1)/2) to floor(n/2) | KILLED (`unit.core.rdp.depth`, `.blender`) |
| combiner | floor to no-floor; upper clamp removed; cycle 1 fed zero instead of cycle 0 | KILLED (`unit.core.rdp.combiner`, `.blender`) |
| blender | normalizing term; alpha factor shift; coverage-times-alpha rounding; wrap destination; alpha-compare equality; black memory color | KILLED (`unit.core.rdp.blender`) |
| noise seed and undefined policies | reset seed 1 to 2; xorshift shift 13 to 12; double advance per pixel; undefined RGB source 0 to 1; black memory color to white | KILLED (`unit.core.rdp.combiner`, `.blender`) |
| named policy identity | noise, undefined-color, output and clock policies each dropped from the profile hash | KILLED (`unit.core.profile`, `unit.core.profile.v12-identity`, `runner.rdp-fill`) |
| Z | opaque compare `<=` to `<`; update allowed in decal mode; interpenetrating coverage `>` to `>=` | KILLED (`unit.core.rdp.depth`) |
| Z | decal equality `<=` to `<`; primitive Z cap 0x7fff to 0x7ffe; delta ceiling to floor (VG-01) | SURVIVES all three, also against 182 tests |
| SYNC_FULL, DPC | wrong MI bit; interrupt raised at apply; busy ignoring raster work; MI clear removed; freeze ignored; XBUS source ignored | KILLED (`unit.core.dp.transport`, `unit.core.rdp.fill`, `unit.core.vi.fill`, runner tests) |
| VI timing | deadline ceil to floor; INTR compare `==` to `>=`; field period V_SYNC+1; CURRENT write no longer clears MI; progressive parity ignored; source clock 48,681,812 to 48,000,000 | KILLED (`unit.core.vi.timing`, `.output`, `.filters`) |
| VI clock sensitivity | source clock to 48,600,000 | SURVIVES the 24 Phase 7 tests, KILLED by 18 Phase 8 and 9 tests (`unit.core.sched.matrix`, `unit.core.ai.cadence`, `runner.raw-capture` and others). The witness vectors (64 cycles, 83 ticks) cannot see a clock between 48.2 and 48.68 MHz. See Observation 3 |
| VI interlace (v8) | serrate parity ignored in the row map; serrate source span without parity | KILLED (`unit.core.vi.timing`) |
| VI interlace (v9) | selected parity ignored in the filtered path; same-parity next row replaced (VG-02) | SURVIVES both, also against 182 tests |
| VI pixel format | 5-bit expansion; green shift; 8888 alpha expansion in the v8 and v9 paths | KILLED (`unit.core.vi.timing`, `.output`, `.filters`, `.depth`, `.blender`) |
| VI pixel format | RGBA5551 alpha bit clear forced to 255, v9 path and v8 path (VG-03) | SURVIVES both, also against 182 tests |
| VI filters | gamma square root rounding; Q20 resample shift | KILLED (`unit.core.vi.filters`, `.output`) |
| dither | Bayer bias shifted by 2 instead of 1 (VG-05) | SURVIVES |
| raw output | scheduler producing-event hold disabled; copy cursor not advanced; undersized buffer partially copied; width hardcoded to 320; start tick off by one; packet copy offset; runner end tick; runner FullSync count | KILLED (`unit.core.vi.output`, `.timing`, `.fill`, `controls.real.rdp-drain-cadence`, `runner.rdp-fill`) |
| write ownership | CPU store no longer invalidates the coverage sidecar | KILLED (`unit.core.rdp.depth`, `.blender`) |
| equivalent (3) | final shade floor to truncation (clamped to 0 afterwards); packet-count guard removed in the VI handler (the scheduler hold is primary and is killed above); a gamma-dither rank inside one threshold class | not counted |

## Verification Gaps (WARNING, none a BLOCKER)

| ID | Severity | Truth affected | What is unpinned | Closure |
| --- | --- | --- | --- | --- |
| VG-01 | WARNING | 4 (SC2 Z) | decal compare at equality; primitive Z above 0x7fff; per-pixel delta ceiling | three authored vectors (see behavior_unverified_items) |
| VG-02 | WARNING | 7 (SC3 interlace x filtering) | v9 filtered output with serrate | one v9 serrate packet vector |
| VG-03 | WARNING | 7 (SC3 pixel format) | RGBA5551 clear-alpha scans out as 0 | one vector through each path |
| VG-04 | WARNING | 4 (SC2 texture) | perspective floor for negative S and T | two texture vectors |
| VG-05 | WARNING | 4 (SC2 shade, dither) | shade product floor; Bayer bias values | two vectors |

None of these makes a pinned expected value wrong. Each is a spec sentence in `docs/spec/rdp.md` or `vi.md` that a one-line production change can contradict without turning any test red. They are the reason truths 4 and 7 are not counted as verified.

## Qualification evidence and whether the Phase 7 claim stays valid

**What the claim binds.** `docs/claims/phase07.md` holds one block: build `0.0.0+t64-build-v1-4f10a216...`, profile `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-v2`, fixture set `rdp-vi-fill-diagnostic`, 1/1, tier `unit-tested`, oracle `authored-fill-byte-contract` with revision `source-content-sha256:1d49ee49...`. That revision is the SHA-256 of `runner/tests/test_rdp_video.c` at the Phase 7 commit (I recomputed it: `1d49ee49594481e3` at `2c8e9ef` and `e6f5466`).

**Checks I made.**
- **Source binding.** I recomputed all 505 entries (path, git blob id, mode, SHA-256) of `source-binding.json` from the git objects of `2c8e9ef`: 0 mismatches; the canonical manifest digest equals the recorded `2bc31c3e...c4eee`.
- **Native originals.** Retained ASan/UBSan JUnit: 159 cases, 0 failures, 0 errors, 0 skips; normal `LastTest.log`: 157 passed, 0 failed. The retained final-binary binding names 93 normal and 95 sanitizer artifacts; I did not re-hash the 188 copies and rely on the executor's recorded parent recheck for that part.
- **Hosted.** `gh run view 44228987686`: `success`, head `e6f54668afec23e603f4ce61fc44fd9b91d216da`, both jobs success. `gh run view 44257072608` (authority batch): `success`, head `eea64b6fae3bde504cbe4658fd8e9213b5bcd3d6`. The hosted receipt's profile hash `5612696...` equals the literal pinned in `runner/tests/test_rdp_video.c`, and its source, packet and policy fields equal the independent Python literals.
- `check-claims.sh` accepts the block; the claim, receipt, spec and case inventory files are identical at the tag and at HEAD.

**What later phases changed and what that does and does not mean.** Between `e6f5466` and the qualified Phase 9 tag, 126 files changed under core, runner, fixtures, scripts, tools, .github and docs/spec. For the Phase 7 sources: `t64_rdp.c`, `t64_rdp_depth.c`, `t64_rdp_pipeline.c`, `t64_rdp_tmem.c`, `t64_vi.c`, `t64_mi.c`, `fixtures/rdp/fill.S` and `fill.ld` are byte-identical; `t64_rdp_raster.c` has one refactor whose commit message and diff show a shared bound check for the importer and no change to the row arithmetic; `t64_scheduler.c`, `t64_bus.c`, `t64_digest.c`, `t64_machine.c`, `t64_internal.h`, `t64_state.c` and the runner changed for Phase 8 and 9 scope. In `write_profile` and the profile table, descriptors up to 10 are untouched; only the conditions were extended for the new descriptors 11 and 12. The 15 retained profile hash pins in `core/tests/test_profile.c` are unchanged (zero pins removed or altered; two pins for the Phase 8 and 9 profiles were added), and `unit.core.profile` passes on the later tree. All 18 Phase 7 tests pass on the later tree, normal and sanitized.

Therefore, precisely:
1. **The Phase 7 claim remains valid as a statement about its bound build and oracle revision.** It was never a claim about later builds, and it says so (`not_claimed: other builds, profiles, fixtures or command combinations`).
2. **No Phase 7 claim is re-issued for the Phase 8 or 9 builds.** The claim's build id and oracle revision no longer name the current tree: `runner/tests/test_rdp_video.c` changed after Phase 7 (commit `fb14108`, a pipe-reading helper; the expected arrays are untouched), so its hash is now different. That is by design for per-source claims, but nothing in the repository tells a reader that the Phase 7 behavior is unchanged on the newer builds except the passing tests. I recommend leaving the claim as is and not widening it.
3. **The Phase 7 behavior these claims depend on is unchanged on the newer builds** (evidence above). The Phase 9 integration finding (17 pre-Phase-9 profile ids byte-identical to the Phase 8 source, no Phase 7 claim invalidated) is consistent with everything I could check; I did not re-derive its 510 digests (VL-02).
4. **Z/depth is inactive on descriptors 11 and 12 (decision K1)** and does not affect the Phase 7 profile: `t64_has_rdp_depth_profile` requires descriptor version 10, which covers the v7, v8 and v9 filter profiles that Phase 7 qualified. `rdp.md`, `vi.md` and the claim are silent on K1 but claim nothing about the later descriptors.

## Prohibitions and stops

The three judgment-tier prohibitions of plan 07-12 are in the frontmatter with verdicts; all three are not violated, and all are non-authoritative and flagged per the soft-gate rule. Stops: no oracle was run, fetched, inspected or copied; no oracle checkout exists; the authority record is blocked and the live gate refuses; no protected ROM, boot image, save, firmware or 64DD media is tracked (`git ls-files` finds no ROM, save or binary extension; rights scan findings=0; the only inputs are authored `.S` and `.ld` sources and the cleared open IPL3 asset); no new dependency, workflow action or download was added during plans 07-01 to 07-12 (checked in the CMake and workflow diffs); no release or naming decision; AGENTS.md and `.planning/config.json` untouched by Phase 7. The security audit's own tally (39 of 41 rows closed, T-07-27 and T-07-28 open as future Plan 07-13 routes) is consistent with the code: no oracle runner exists to be unsafe.

## Required Artifacts

| Artifact | Expected | Status | Details |
| --- | --- | --- | --- |
| `core/t64_rdp.c`, `t64_rdp_tmem.c`, `t64_rdp_raster.c`, `t64_rdp_pipeline.c`, `t64_rdp_depth.c` | DPC transport, TMEM and copy, triangles, combiner and blender, depth | VERIFIED | substantive (about 3,800 lines), wired through `t64_bus.c` MMIO, `t64_scheduler.c` events, `t64_machine.c` profiles, `t64_digest.c` and `t64_state.c` |
| `core/t64_vi.c` | timed scanout, filters, bounded lossless raw queue | VERIFIED | 1,325 lines; `t64_video_info` and `t64_video_copy` are the observer seam; the scheduler holds a full-queue event before dispatch |
| `fixtures/rdp/fill.S`, `runner/tests/test_rdp_video.c` | authored guest and independent literal comparator | VERIFIED | CPU guest programs DPC and VI itself; no SDK encoder is copied |
| `docs/spec/rdp-vi-cases.json`, `scripts/check-rdp-vi-cases.py` | exact case denominator | VERIFIED | 11 suites, 179 distinct pairs; real binaries list exactly the inventory. This is a membership check; assertion strength is what the mutants test |
| `scripts/qualify-rdp-vi.py`, `scripts/check-rdp-oracle-authority.py` | qualification and authority gate | VERIFIED | `--selftest` and the registered harness pass; the authority checker has 22 reached controls |
| `docs/claims/phase07.md`, `docs/evidence/phase07-*` | bounded claim, native and hosted records | VERIFIED | see Qualification |
| Plan 07-13 artifacts (`scripts/run-rdp-differential.py`) | the differential harness | ABSENT BY DESIGN | the plan is stopped; not scored |

## Key Link Verification

| From | To | Via | Status | Details |
| --- | --- | --- | --- | --- |
| CPU guest store to DPC START/END | `t64_rdp_write_register` and scheduled `t64_rdp_event` | `t64_bus.c` MMIO decode, `t64_schedule_reserve_after` | WIRED | fill, DPC and fetched-RSP paths all pass |
| FullSync fence | MI pending bit 0x20 | `t64_mi_set_pending`, clear through MI_MODE 0x800 | WIRED | the wrong-bit, raised-at-apply and clear-removed mutants are killed |
| RDRAM pixels | VI raw packet | `render_packet` or `render_filter_packet` into the packet ring | WIRED | fill, copy, texture, combiner, blender, depth suites all read a raw packet |
| VI packet ring | observer and runner | `t64_video_info`, `t64_video_copy`, `runner/t64_real.c` observation record | WIRED | runner mutants on end tick, FullSync count and copy offset killed |
| profile descriptor | profile hash | `write_profile` text fields for the noise, undefined-color, clock and output policies | WIRED | all four removals killed |
| qualifier | independent expected bytes | `EXPECTED_SOURCE`, `EXPECTED_PACKET` | WIRED | refuses a forged public export |

## Data-Flow Trace (Level 4)

| Artifact | Data variable | Source | Produces real data | Status |
| --- | --- | --- | --- | --- |
| raw frame | VI packet bytes | RDRAM bytes written by a real DPC fill and read back through the real VI scanout | yes (the hosted receipt's 24 RDRAM bytes and 48 packet bytes equal the independent literals on both hosts) | FLOWING |
| claim counts | `pass` and `total` | `runner.rdp-fill` result and the retained LastTest | yes | FLOWING |
| hosted receipt | decoded RDRAM and packet | real runner on Linux and macOS (runs 44228987686 and 44257072608) | yes | FLOWING |

## Behavioral Spot-Checks

| Behavior | Command | Result | Status |
| --- | --- | --- | --- |
| Phase 7 test set | `ctest --test-dir build/gate -R '^(unit\.core\.(rdp\|dp\|vi)\.\|runner\.rdp-fill\|controls\.real\.rdp\|harness\.rdp)'` in the clone | 18 of 18 | PASS |
| case inventory | `check-rdp-vi-cases.py --build build/gate --suite <each of 11>` | `validated nonempty` x 11 | PASS |
| full inventory | `ctest --test-dir build/gate -j4` | 223 of 224; only `gov.pii.repo` fails (history finding outside Phase 7) | PASS for Phase 7, one repo-level FAIL |
| claims | `sh scripts/check-claims.sh --files docs/claims/phase07.md` | `blocks=1 errors=0` | PASS |
| authority | `check-rdp-oracle-authority.py --selftest`, `--expect-blocked`, `--require-approved` | 22 controls pass; blocked; exit 2 | PASS (blocked, as required) |
| scans | `rights-scan.sh --tree`, `pii-scan.sh --tree`, `check-provenance.sh` | 0, 0, 0 errors | PASS |

## Probe Execution

No `probe-*.sh` is declared by any Phase 7 plan and none exists. Not applicable.

## Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
| --- | --- | --- | --- | --- |
| `runner/tests/test_rdp_video.c` | 226 | `XXX` in the mkdtemp template `/tmp/t64-rdp-video.XXXXXX` | none | a template, not a debt marker; the only TBD, FIXME or XXX hit over 26 Phase 7 source, test, script and doc files, and no TODO, HACK or PLACEHOLDER |

Stubs: none on a data or rendering path. Hardcoded empty data and static returns: none found on the paths above; the mutants prove the data paths are live.

## Human Verification Required

1. **VID-04 owner decision.**
   **Test:** either authorize the D5 local-oracle work and the parallel-rdp per-file/asset rights audit so Plan 07-13 can run, or record VID-04 as a descoped requirement moved to a later milestone and accepted as debt.
   **Expected:** a decision recorded by the owner or competent legal authority in project records; until then no oracle may be run, fetched, inspected or copied and the checker keeps refusing (`python3 -B scripts/check-rdp-oracle-authority.py --require-approved` exits 2).
   **Why human:** AGENTS.md and `.planning/AUTONOMY.md` make oracle use, rights acceptance and derived-data publication owner or legal stops; an agent cannot supply the pin and no code change closes this.

2. **VG-01 to VG-05: accept or pin.**
   **Test:** decide what to do with the five behavior-unverified items (Z numeric edges, v9 filtered serrate, RGBA5551 clear alpha, perspective floor on negative quotients, shade and dither tables).
   **Expected:** either accept them as known test-strength limits of authored software policy (none changes a roadmap success criterion), or schedule one small test-only plan that adds the named vectors and re-runs my listed mutants.
   **Why human:** the implementation is present and wired and no truth failed; whether to pay for extra pins before closing the phase is a scope decision, not something grep or a passing test can settle.

Related options already on the table in `.planning/v1.1-MILESTONE-AUDIT.md`: authorize the oracle work and then close, or verify Phase 7 for the five completed requirements, descope VID-04 and close v1.1 with VID-04 as accepted debt. This report supports either: it is the missing Phase 7 verification report for the five completed requirements.

## Finding outside Phase 7 (report to the orchestrator)

`gov.pii.repo` fails at HEAD. In my clone of HEAD, `scripts/pii-scan.sh` (tree plus history) reports `tree_files=1227 commits=993 findings=1`: one `P-PRIVATE-PATH` hit in the added lines of commit `a5985ed` in `.planning/phases/09-controller-save-media-and-replay/09-VERIFICATION.md` (a per-user home path in the "What I ran" section; value redacted here). The tree scan is clean, so only the history-wide scan sees it. The scanner's own contract: "A real hit is an owner stop: report it redacted, never rewrite history." The hosted runs that qualified Phase 9 predate that commit, so the next hosted run on main would turn red on this one test. Phase 7 and Phase 8 files are not involved. I left the shared tree untouched and wrote this report with no absolute path.

## Verification limits

**VL-01: the Phase 7 qualifier's own `--verify-evidence` was not re-run.** It re-binds the exact Phase 7 source and the producing tree's absolute paths; both have moved, so it refuses by design (as the Phase 9 verification found for its own qualifier). I recomputed the part that does not depend on the path (the 505-entry source binding and its digest), read the retained originals, and checked the live hosted runs. Not replayed: the live re-hash of the 188 retained binary copies and the regeneration of the suite summaries from the live build directories.

**VL-02: the 510-digest cross-phase equality check was not re-derived.** Its report is retained locally and untracked. My substitute: unchanged descriptors and profile writer for descriptors up to 10, unchanged profile hash pins, and all 18 Phase 7 tests passing on the later tree.

**VL-03: mutant coverage is a sample.** 77 mutants over the pinned behaviors the task named (fill, copy, texture, shade, combiner, blender, coverage, Z, SYNC_FULL, VI timing, filter, interlace, noise seed, undefined policies, raw output). Each filter, dither and Z table entry was not mutated individually; the five survivor groups show where the tables are only partly pinned.

## Observations (evidence-backed, none a gap)

1. **The wrong-pixel control targets the VI packet comparator only.** The RDRAM comparator is exercised by the accepted path and the fill mutants, not by a separate negative control. SC1's wording ("a deliberately wrong expected pixel fails") is met.
2. **The claim registry holds one Phase 7 block (fill only).** The other modes are covered by authored unit matrices that the native evidence describes and that I exercised, but they carry no claim blocks. The claim says so.
3. **The VI clock is pinned strongly only by later phases.** The Phase 7 timing tests use one small witness vector, so they alone cannot see a clock error below about 1 percent, and no Phase 7 test uses a standard NTSC register set (`vi-diagnostic-vs-standard-timing` programs H_SYNC 4 and V_SYNC 2). The 0.17 percent mutant is killed by 18 Phase 8 and 9 tests. A Phase 7 vector with the standard NTSC field (3094 x 525 / 2 source cycles, 1,042,709 ticks) would make the phase self-sufficient.
4. **TV timing is selected by profile model version, not read from the `tv_standard` field.** Only NTSC profiles exist and the spec states PAL and MPAL are not supported; the clock constant is bound by name into the profile hash. A future PAL profile would need explicit VI work.
5. **Triangle Z, state serialization and the Phase 9 composite family:** Z is active only on descriptor 10 (K1). Not a Phase 7 issue.
6. **Claim scope:** the Phase 7 claim names the earlier v2 fill profile only; the later renderer profiles (v3 to v9) are covered by the authored unit matrices, as the claim states.

## Gaps Summary

No roadmap success criterion failed. VID-01, VID-02, VID-03, VID-05 and VID-06 hold within their authored scope and pass every check I could run or recompute. Two truths are present and wired but not behaviorally pinned at five sub-behaviors (VG-01 to VG-05, WARNING). VID-04 is blocked by an owner stop on D5 and the parallel-rdp rights audit and is not a code gap. The status is `human_needed` because of the VID-04 owner decision and the VG scope decision; there is no BLOCKER to fix. One repo-level finding (the history PII hit in the Phase 9 report) needs the orchestrator, outside Phase 7.

---

_Verified: 2026-11-20T21:15:00Z_
_Verifier: Claude (gsd-verifier)_


## Orchestrator resolution of the human items (2026-11-20)

1. **VID-04 owner decision.** The owner chose to descope VID-04 to v1.2 and close v1.1 (explicit answer in conversation on 2026-11-20). REQUIREMENTS.md and ROADMAP.md record the descoping and Plan 07-13 as deferred. No oracle authority, acquisition, execution or publication was granted; `docs/evidence/phase07-oracle-authority.json` still reads blocked and `--require-approved` still exits 2.
2. **VG-01..VG-05.** Accepted as known test-strength limits under the owner's instruction to follow the orchestrator's recommendations; carried as v1.2 tech debt for one small test-only plan that adds the named vectors (Z decal-at-equality and primitive-Z saturation, v9 serrate rows, RGBA5551 clear-alpha, perspective floor for negative S and T, shade product floor and Bayer bias values, and a standard-NTSC VI clock vector).
The status above therefore reads passed for the five completed requirements (VID-01, VID-02, VID-03, VID-05, VID-06) within their authored scope.
