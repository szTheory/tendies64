# Phase 10: Carry-in Closure Before the Freeze - Research

**Researched:** 2026-11-23
**Domain:** deterministic N64 emulator core (C17, CMake, CTest), identity/state-format change, authored guest fixtures, CI gate and authority-checker tooling. No web, framework or package ecosystem is involved.
**Confidence:** HIGH for in-repo mechanics (every discrete value below was read at HEAD `1371c57`; `core/`, `runner/`, `scripts/`, `.github/`, `fixtures/`, `CMakeLists.txt`, `tests/` are unchanged since `d3211e9`, verified with `git diff --stat d3211e9 HEAD`, which listed only `docs/` files). MEDIUM for hosted-runner timing (one noisy sample set). The primary RDP manual was not re-downloaded (protected content, instruction); the depth table was re-checked for internal consistency and independently recomputed instead.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
Locked by ROADMAP.md, STATE.md and the v1.2 fan-out synthesis (not re-asked):
- Depth (VID-07) is enabled only under new registered profile ids; the shipped descriptor 11 and 12 ids and the 17 pre-Phase-9 ids keep digests, serialized bytes and Z-refusal behavior byte-identical — **Reversibility:** one-way — registered ids and digest pins feed every later claim, replay binding and the installed consumer
- VID-07 and the CORE-06 version marker are ONE identity/state bump (state v26 domain, container format v2); v24 and v25 stay byte-identical — **Reversibility:** one-way — a second bump would re-pin every qualified artifact again
- CORE-06 premise is corrected: the combiner block and coverage table already travel in section 8 (plan 09-10); remaining work is the version marker, depth members and a depth-inclusive mid-primitive and mid-scan-out round trip; the completeness flip-every-member test is written first (RED)
- State blobs are documented as build-identity-bound and outside the ABI promise (state-container docs and ABI notes)
- Wave order: 0 scripted preflight, pin-impact map, GATE-02 then GATE-01 (ci.yml, check-ci-yml.sh and gate.sh are bound source); 1 VER-06 (VG-01 on descriptor 10 before depth is enabled elsewhere); 2 VID-07 plus CORE-06 as one identity and state bump; 3 RUN-07 (the one manifest-moving commit); 4 independent review per wave and one qualification
- GATE-02: per-row oracle pins replace the whole-file hash; until it lands, identity-only rebinds are owner-approved for v1.2 manifest rows (reproduce the old pinned hash first, change only manifest identities, the derived request hash and PINNED_REQUEST_SHA256, keep local_decision blocked, orchestrator task only, never an executor); grants no oracle, rights or publication authority — **Reversibility:** costly — changes the authority checker's binding contract
- GATE-01: explicit CTest COST properties; remove only the redundant rsp-no-hle guard inside the audio-records step (the step itself stays because the emit step consumes its records); `timeout-minutes` 15 with the CI-TIMEOUT scanner rule and its self-test changed together; before/after step timings recorded; F-106, F-111, F-114 outer limits recorded or declined with reason; success is a measured cold-cache hosted run, not a projection
- RUN-07: one composite guest, run through the real supervised runner twice (raw-capture run, then save-out run) with equal final digest; the Phase 9 raw-capture exclusion is NOT relaxed; fixture must fit the ~68 KiB per-file capture limit (small VI window); guest is authored (no oracle, no protected material)
- Phase 9 items tagged for this phase are triaged as done, deferred (to Phase 11) or dropped, each with a recorded reason: splitting unit.core.state.import, replay-driver duplication, dead t64_artifact_publisher.c link
- Provisional-name sweep: relabel or settle the "provisional until Phase 10" names in the eight docs listed in lens-phase10 R8 (the freeze is now a Phase 11 candidate with a final bind at end of Phase 12), so Phase 11 inherits no stale provisional labels
- Plans are capped at 4 tasks and about 45 minutes; no decimal plans; a defect fix is a new numbered plan; independent review per wave in scratch clones (git archive); parallelization stays false

Decided autonomously (CONTEXT "Decided autonomously"):
- [Test layout] "core unit tests driving the machine directly (test_rdp_depth.c, test_vi.c, test_rdp_*.c), no manifest row" — cheap to move or rename, no manifest or identity effect (**see Correction C1: this premise is wrong for the oracle pin; the file placement must change, the decision's intent is preserved**)
- [Preflight] "one scripted local preflight under about 90 s cited by every plan"
- [Qualifier] "thin delta binding only changed ids, copy-paste of reusable parts, no new dependency"
- [Fuzz] "extend fuzz_state_import.c and field-use audit rows for each new member"
- [Review] "per wave in scratch clones plus one final independent audit before qualification"
- [Docs] "update rdp.md, vi.md, machine-contract K1 text, state-container sections and PROJECT.md in the same wave as the identity bump"

### Claude's Discretion
- Exact new profile id values and family tuple naming, chosen by the planner against the registered-id conventions (the public id must be stable enough for the Phase 11 consumer to name)
- Plan decomposition within the wave order (estimate 12-16 plans; wave 2 is the ballooning risk)
- Whether the optional `make -j2` boot-fixtures change is worth doing (low value; measure first)

### Deferred Ideas (OUT OF SCOPE)
- VID-04 differential oracle evidence stays unmapped (owner D5 decision required)
- Public API/ABI work, installed consumer and freeze candidate: Phase 11
- Host, playable title, LibFuturo evaluation and the final ABI bind: Phase 12
- Relaxing the raw-capture exclusion (rejected as a pre-freeze contract change)
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description (REQUIREMENTS.md) | Research Support |
|----|-------------------------------|------------------|
| GATE-02 | The Phase 7 oracle request pins each fixture-manifest row by its own hash, so adding an authored guest-ROM row no longer forces an identity-only rebind; `local_decision` stays blocked and no oracle, rights or publication authority is granted | Checker mechanics, two whole-file hash sites, 14 bound inputs (not only the manifest), design below (Section "GATE-02") |
| GATE-01 | The hosted macOS gate has a measured cold-cache run inside its job limit. Route: explicit CTest COST properties, the redundant rsp-no-hle guard in the audio-records step removed, `timeout-minutes` raised to 15 with the CI-TIMEOUT scanner rule and its self-test changed together, before/after step timings recorded, outer limits of F-106, F-111 and F-114 recorded | Scanner lines, awk-pattern trap, COST property semantics, guard-removal blast radius, cold-run procedure |
| VER-06 | The Phase 7 test-strength gaps VG-01 to VG-05 and the standard-NTSC VI clock vector are closed with independently derived expected values and mutants that fail when the behavior is wrong; VG-01 lands on descriptor 10 before depth is enabled elsewhere | VG definitions, independent literals, mutant driver pattern, NTSC vector arithmetic |
| VID-07 | RDP depth (Z) is active on composite profiles under new registered profile ids (shipped ids keep identities, digests and Z-refusal byte-identical), with independently calculated expected values and a wrong-value control | Predicate site and sites list, registration mechanics, depth semantics recheck and literals |
| CORE-06 | One state/container version marker covers the plan 09-10 content change and the depth members; a mid-primitive and mid-scan-out round trip that includes depth is exact, a flip-every-member test sees each member, and state blobs are documented as build-identity-bound and outside the ABI promise | v26 walker, container v2, importer design hazard (depth members must precede the triangle proof), flip-test pattern |
| RUN-07 | One composite guest (video, audio, input, save) runs through the real supervised runner twice (raw-capture run, then save-out run with the same final digest), each output compared with independent expectations; the raw-capture exclusion is not relaxed | Runner observation constants that constrain the guest, fixture admission rows, option matrix, manifest-moving commit steps |
</phase_requirements>

## Summary

The fan-out lens (`lens-phase10.md`) holds on its main findings and its file/line references (predicate at `core/t64_internal.h:1290`, sidecar already allocated for the AI family, CORE-06 premise corrected, GATE-01 scanner rule, per-row authority feasibility). This research found **six places where HEAD contradicts a premise the planner would otherwise rely on** (Section "Corrections", C1-C6). The three that change the plan shape: (C1) editing any of 14 bound test/fixture files, not only `fixtures/manifest.json`, moves the oracle-request pin, and adding cases to a Phase 7 test binary also breaks the exact `--list-cases` inventory, so VER-06 and VID-07 tests must live in **new** files or the standing rebind covers far more than "manifest rows"; (C2) the non-raw runner path hard-codes video (4x3, 48-byte packet) and audio (two 16-byte packets) observation shapes for every non-AI profile, so the RUN-07 composite guest must produce exactly those shapes in both runs or the runner changes; (C3) the locked "remove the redundant rsp-no-hle guard in the audio-records step" is not local to `ci.yml`: the guard is the `repo.rsp-no-hle` CTest inside the Phase 8 `qualification` case suite, which `scripts/qualify-audio.py` requires, and it is not needed to fit a 15-minute limit (the cold projection 673 s fits 900 s with 227 s margin).

Depth semantics recheck (LIGHT): the eight-row Z exponent table that `docs/spec/rdp.md` V7 says it "applies exactly" is **not printed in `rdp.md`**; it lives in the archived `07-08-DEPTH-FIELDS.md` (citing Nintendo 64 Programming Manual NU6-06-0030-001G p. 324). I re-derived the table's internal consistency (the eight ranges tile `0..0x3ffff` exactly) and independently recomputed the worked example in `07-08-RAW-CONTRACT.md` (PrimDepth `0x1357`, dZ 9) with a scratch calculator written from the documents and not from `core/t64_rdp_depth.c`; it matched, and the same calculator produced the VG-01 and exponent-boundary literals below. Independent literals for VG-01 and the depth fixture are tabulated below.

The identity bump is best done as **descriptor 13 / model 21 / state 26** with four ids mirroring the SI family, by widening `t64_has_si_profile` to mean "SI device family" (so no sealed body text changes and no re-pin churn like 09-01's eight bodies) and introducing exact-tuple predicates only where the digest domain, importer registry and exporter need them. The depth scalars must be emitted **inside the RDP block** (like v21-v23), not in a trailing tail, because the importer's render-in-progress proof (`state_check_triangle_render`, `core/t64_state.c:1686-1688`) needs them before the tail is read.

**Primary recommendation:** Run Wave 0 as (1) a committed preflight plus a committed pin-impact map, (2) GATE-02 widened to also stop whole-file binding of mutable authored test sources (or accept one orchestrator rebind per wave), (3) GATE-01 with `ci.yml` final in Wave 0 (decide the hosted footprint of the Phase 10 qualification now, default none new), then write every new VER-06/VID-07/CORE-06 test in new files outside the oracle-bound set, and keep the identity bump to one new descriptor-13 family with `t64_has_si_profile` widened rather than re-sealed.

## Corrections to lens-phase10.md and CONTEXT.md (what no longer holds, or was incomplete)

| # | Claim in lens/CONTEXT | What HEAD shows | Consequence for the plan |
|---|----------------------|-----------------|--------------------------|
| C1 | CONTEXT "[Test layout]": VG tests in `test_rdp_depth.c`, `test_vi.c`, `test_rdp_*.c` have "no manifest or identity effect"; lens R6: VER-06 "need no manifest row" | `scripts/check-rdp-oracle-authority.py:37-46` `EXPECTED_CORPUS_PATHS` binds 14 files by whole-file SHA-256 and git blob id, including `core/tests/test_rdp.c`, `test_rdp_copy.c`, `test_rdp_triangles.c`, `test_rdp_texture.c`, `test_rdp_combiner.c`, `test_rdp_blender.c`, `test_rdp_depth.c`, `core/tests/test_vi.c`, `test_video_output.c`, `runner/tests/test_rdp_video.c`, `scripts/check-rdp-vi-cases.py`, `docs/spec/rdp-vi-cases.json`, `fixtures/rdp/fill.S`, `fixtures/manifest.json`; `_verify_corpus` refuses with `authored corpus SHA-256 changed`. Editing `docs/spec/rdp-vi-cases.json` (needed to list new cases of a Phase 7 binary) additionally trips `scripts/qualify-rdp-vi.py:28` `CASE_INVENTORY_SHA256 = "c915cc38ce78aeccc0d6718351f4968368f35126366e4b353a90037abc98385b"`, enforced at `:561` (`source-derived case inventory differs from its reviewed pin`) by the hosted "Emit authored raw RDP VI evidence" step. `scripts/check-rdp-vi-cases.py` compares each binary's `--list-cases` with that inventory exactly (`source-derived cases differ from inventory`) but no gate CTest, `ci.yml` step or `gate.sh` step invokes it (grep found none), so unlisted extra cases would not fail the gate; the oracle pin would | New VG, depth-composite and v26 tests go in **new test files/binaries** (new CTest names), not in the Phase 7 bound files. Otherwise every wave that touches them needs an oracle rebind (and, if the inventory is edited, a `qualify-rdp-vi.py` pin edit in a closed Phase 7 qualifier), outside the standing approval's literal text ("manifest identities") |
| C2 | Lens R5: two runner runs "needs no CLI change" and is cheapest | `runner/t64_real.c:594-603` (`drain_video`, non-raw path) requires, for any profile that is not `T64_REAL_AUDIO_PROFILE_ID`, width 4, height 3, packet 48 bytes, source 24 bytes; `drain_audio` (`:742-748`) requires `info.byte_length == T64_AUDIO_OBSERVATION_PACKET_BYTES` (16) and at most `T64_AUDIO_OBSERVATION_PACKET_COUNT` (2) packets. The save-out run is necessarily non-raw (RAW is refused with every producer) | The composite guest must emit exactly one 4x3 video window class and two 16-byte audio packets (as `fixtures/rdp/fill.S` and `fixtures/audio/pcm.S` do), or `runner/t64_real.c` changes. Also a new row in `runner/t64_real_fixture.c` (hash, size, profile id, profile hash, completion PC, log pin) is required |
| C3 | Lens R7/CONTEXT: the guard removal is a local cost saving | The ~25-32 s is the CTest `repo.rsp-no-hle` listed in `docs/spec/audio-cases.json` suite `qualification` (`"repo.rsp-no-hle"` between `controls.real.raw-capture-race` and `harness.audio-qualification`). `scripts/check-audio-cases.py:244-262` additionally runs the guard directly and writes `source-body-check-*.log`, which `scripts/qualify-audio.py` requires (`PASS: rsp-no-hle`, lines 1367, 1510, 2201; native plan lines 979-990). A 15-minute limit alone already fits the 673 s cold projection | Treat guard removal as the last, measured, stop-loss task of GATE-01: it touches the Phase 8 closed qualifier (F-106 says that file stays untouched). If it needs a `qualify-audio.py` edit, record "declined with reason" (CONTEXT allows declining only the F-106/F-111/F-114 outer-limit records, so surface this to the orchestrator) |
| C4 | CONTEXT: "Phase 9 510-digest audit harness" is a reusable asset | The 510 digests (17 ids x 3 ROMs x 10 checkpoints) were an ad hoc two-tree comparison (`build/gate/09-10-regress/*-17ids.txt`, `09-REVIEW-FIX.md:367`), not a committed tool. The build id is a fingerprint over core sources (`core/CMakeLists.txt:20-66`), so any core edit changes digests that include packet identity (the control: "differs in 30 of 792 lines") | Wave 0 must write the sweep as a committed tool (two `git archive` trees, one forced build identity, same driver). Only idle-state literals (`unit.core.digest.sink-legacy-identity`) are build-independent |
| C5 | Lens R2/R3: new ids cost "another descriptor-membership/sealed-body churn like 09-01" | 09-01 re-sealed eight bodies only because it replaced `t64_has_ai_profile` by `t64_has_ai_device` **inside** `.c` bodies. If the new family is added by widening the header predicate (`t64_has_si_profile`) the sealed `.c` bodies' text does not change. Sealed rows by file (counted from `REVIEWED_BODY_DIGESTS`): `t64_rdp_depth.c` 14, `t64_rdp.c` 24, `t64_rdp_pipeline.c` 20, `t64_rdp_raster.c` 26, `t64_ai.c` 19, `t64_bus.c` 4, `t64_si.c` 1, `t64_save.c` 1, `t64_pi.c` 1 | Prefer the widening design; verify cheaply with `python3 -B scripts/check-rsp-no-hle.py --build build/gate` after the predicate edit (an unchanged sealed body needs no re-pin) |
| C6 | Lens R8: "eight docs" say provisional until Phase 10 | At HEAD: 7 live spec docs (`replay.md:11`, `result-protocol.md:546,554`, `raw-capture.md:132`, `state-container.md:15,258,626`, `si-pif-controller.md:250`, `save-media.md:30`, `machine-contract.md:487,529,609,701,829`), 2 closed-phase records (`docs/claims/phase09.md:32`, `docs/evidence/phase09-qualification.md:276`; leave untouched), and ~25 source/header comments (`core/include/tendies64/t64_machine.h` 12, `core/t64_state.c` 2, `core/t64_internal.h`, `core/t64_machine.c`, `runner/t64_replay.h`, `runner/t64_artifact_publisher.h`, `runner/t64_save_flags.h`, two test files). ROADMAP Phase 11 "Order" already assigns "20 stale 'Phase 10 freeze' mentions relabeled" to Phase 11 | Phase 10 relabels the 7 spec docs; leave code/header comments to Phase 11 (editing them also moves the build id) |

Other lens items verified and still valid: predicate site and the five sites it controls; sidecar allocation for the AI family (`core/t64_machine.c:2672-2681`); CORE-06 premise (tail of 2,097,244 bytes in section 8, `state-container.md:89-92`); CI-TIMEOUT rule (`scripts/check-ci-yml.sh:286-288`); zero COST properties; cache entries fresh (`gh cache list`: both Binutils entries last used 2026-11-20, so GitHub's 7-day eviction would land about 2026-11-27 if nothing touches them).

## Architectural Responsibility Map

Tiers are this project's layers, not web tiers.

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Depth (Z) behavior, sidecar, digest members | `core/` deterministic machine (`t64_rdp_depth.c`, `t64_digest.c`) | `docs/spec/rdp.md` contract | Core owns all guest-visible state; host never computes Z |
| Profile id registry, family tuple, predicate chain | `core/t64_machine.c`, `core/t64_internal.h` | `docs/spec/profile-membership-sites.txt` (generated), `unit.core.profile*` goldens | New semantics get a new registered identity (rdp.md, cpu-compatibility.md) |
| State container v2 export/import, version marker | `core/t64_state.c` (validate-then-apply importer) | `fixtures/state/decode_container.py`, field-use audit, fuzz | Importer is the attack surface for new members |
| Raw-capture and save-out runs, observation shapes | `runner/` (supervisor + real runner) | `runner/t64_real_fixture.c` admission table | Host files and processes stay out of core |
| Authored guests and their ROM builds | `fixtures/*/*.S` + `scripts/build-*-fixture.sh` | `fixtures/manifest.json`, rights/PII scans | Authored here only (D2/D5) |
| Gate, scanners, cost ordering, timeouts | `.github/workflows/ci.yml`, `scripts/check-ci-yml.sh`, `core/CMakeLists.txt` COST | `scripts/gate.sh`, `docs/ci-reviews/LOG.md` | CI is bound source; changes first (Wave 0) |
| Oracle-request binding | `scripts/check-rdp-oracle-authority.py` + `docs/evidence/phase07-oracle-authority.json` | `scripts/tests/test_rdp_oracle_authority.py` | Orchestrator-only edit; grants no authority |

## Standard Stack

No external package is added. Everything is the existing toolchain (versions read this session).

### Core
| Tool | Version | Purpose | Why standard here |
|------|---------|---------|-------------------|
| C17 + CMake/Ninja/CTest | cmake 4.4.3, ninja 1.13.2 | Core, runner, tests, `COST` ordering | D1 ADR; `CMakePresets.json` gate preset runs `jobs: 4` |
| Python stdlib (`python3 -B`) | 3.14.4 | scanners, mutant drivers, qualifiers | existing scripts are stdlib-only |
| pinned `mips64-elf` Binutils (cached) | 2.45 (build/tools/install) | authored guest ROM builds | the only sanctioned way to build guests |
| `gh`, `act` + Docker | gh 2.101.0, act 0.2.87, Docker server 29.5.2 | hosted-run timings, cache deletion, local Linux compile | AUTONOMY section 2 local-first rule |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| New descriptor 13 family | Flip depth on descriptors 11/12 in place | Rejected (locked): moves every Phase 8/9 pin and digest |
| One AI-only depth id plus SI family | SI family only | AI-only adds a second family and the runner's audio checks are hard-wired to the AI id; recommend SI family only (see A2) |
| Mutants as ad hoc reviewer edits | Committed driver modeled on `scripts/k5-mutants.py` | Driver costs one script copy; makes "mutant fails" re-runnable (see Validation) |

**Installation:** none. **Version verification:** the toolchain versions above come from `--version` probes this session. Package Legitimacy Audit: not applicable (no external package is installed or recommended; `gsd_run query package-legitimacy` not run).

## Architecture Patterns

### System Architecture Diagram (data and gate flow for Phase 10)

```
 authored guest .S/.ld ──build-*-fixture.sh──▶ ROM (build/fixtures)
        │                                           │
        │ manifest row + runner admission row       ▼
        ▼                                   supervisor ──▶ real runner ──▶ core machine
 fixtures/manifest.json                      (profile id)   (observe/drain)   (descriptor 13 tuple 13/21/26)
        │  GATE-02: per-row pin                  │ RAW run: video+audio files ──┐
        ▼                                        │ SAVE-OUT run: PAIR + .sav    │
 check-rdp-oracle-authority.py                   ▼                              ▼
 (blocked; no authority)                 result record: final state_digest   independent literals
                                                  └───────── equal in both runs ◀───┘ compare
 core unit tests (new files) ──▶ digest walker v26 ──▶ container v2 export ──▶ importer (validate, then apply)
        │   flip-every-member RED first      depth scalars in RDP block          rules per new member, fuzz, field-use rows
        ▼
 gate: configure ▶ build ▶ inventory ▶ ctest(-j4, COST order) ▶ core-lint ▶ post-Gate steps ▶ upload
       timeout-minutes 15 on macOS; CI-TIMEOUT scanner + selftest changed together
```

### Recommended project structure (new/changed files by wave)

```
Wave 0  scripts/preflight-phase10.sh            NEW  (<= ~90 s; replays Phase 9 lessons, see Validation)
        .planning/phases/10-.../PIN-IMPACT.md   NEW  (table in "Pin-impact map" below, committed)
        scripts/check-rdp-oracle-authority.py   EDIT per-row binding (orchestrator task; pin recomputed last)
        docs/evidence/phase07-oracle-authority.{json,md}  EDIT identity-only
        scripts/tests/test_rdp_oracle_authority.py        EDIT BOUND_FILES + new cases
        .github/workflows/ci.yml                EDIT gate-macos timeout-minutes: 15 (final; no later ci.yml edit)
        scripts/check-ci-yml.sh                 EDIT rule, comment line 27, selftest mutations
        core/CMakeLists.txt                     EDIT COST properties (near line 843 and the sanitizer block ~1100)
        scripts/gate.sh                         EDIT only if a Phase 10 private JUnit root is needed (lines 102-103)
        docs/ci-reviews/LOG.md                  EDIT F-106/F-111/F-114 dispositions, new timings
        core/tests/test_k1_refusal.c (name free) NEW  characterization of Z refusal on 11/12 BEFORE any predicate edit
Wave 1  core/tests/test_vg_gaps.c (name free)   NEW  VG-01..05 + NTSC vector (new binary, new CTest names)
        scripts/p10-mutants.py                  NEW  modeled on scripts/k5-mutants.py (--check-anchors in gate, --run out of gate)
        scripts/check-test-inventory.sh         EDIT PHASE10_TESTS block (+ selftest cost ~0.2 s/name native)
Wave 2  core/t64_internal.h, core/t64_machine.c, core/include/tendies64/t64_machine.h (descriptor/ids), core/t64_digest.c,
        core/t64_state.c, core/t64_rdp*.c (only if a sealed body must change), docs/spec/profile-membership-sites.txt (regenerate),
        docs/spec/state-field-rules.txt, docs/spec/state-mutants.txt, core/tests/fuzz_state_import.c,
        core/tests/test_digest_v26.c NEW, core/tests/test_rdp_depth_composite.c NEW, core/tests/test_state_import_v26.c NEW,
        fixtures/state/decode_container.py (container v2 decode), docs/spec/{rdp,vi,machine-contract,state-container}.md, .planning/PROJECT.md
Wave 3  fixtures/composite/*.S,*.ld NEW; scripts/build-composite-fixture.sh NEW (copy of build-save-fixtures.sh);
        fixtures/manifest.json (+ rows), runner/t64_real_fixture.c (+ rows), runner/tests/test_composite_two_run.c NEW,
        CMake targets, scripts/check-test-inventory.sh
Wave 4  review per wave in `git archive` scratch clones; thin-delta qualifier; one cold hosted run on the final tree
```

### Pattern 1: new registered family, widened predicate (recommended)
**What:** descriptor 13 / model 21 / state 26 with four ids mirroring the SI family (`save_kind` 0..3), reusing `T64_SI_FAMILY_COMMON_FIELDS` style body. `t64_has_si_profile` (today the exact tuple `descriptor_version == 12u`, `machine_model_version == 20u`, `state_serialization_version == 25u`, `core/t64_internal.h:1170-1174`) is widened to "SI device family" (tuple 12/20/25 or 13/21/26); new exact predicates `t64_has_si_v25_profile` / `t64_has_si_v26_profile` are used only at the places that must stay exact: digest domain selection (`core/t64_digest.c:311` `const bool has_v25_state = t64_has_si_profile(machine);` and the domain chain starting `:339`), `state_version_registered` (`core/t64_state.c:2721-2725`), the exporter's version refusal, `T64_STATE_CONTAINER_VERSION` choice, and ring-storage selection by registered id (09-01 deviation 2: an unregistered family id enables AI gates with no ring storage, so create must refuse descriptor 13 under unregistered ids).
`t64_has_rdp_depth_profile` (`core/t64_internal.h:1290-1297`, today `machine->profile.descriptor_version == 10u && (...)`) gains `|| t64_has_si_v26_profile(machine)`.
**When to use:** always for this phase. **Verify first (cheap):** after the header edit, the sealed-body guard must pass unchanged.

Membership sites to wire (the generated list `docs/spec/profile-membership-sites.txt`, 82 lines, is regenerated by `python3 -B scripts/list-membership-sites.py --print` and checked by CTest `repo.si-membership-sites`): `t64_machine.c`: `profile_fields_are_valid`, `write_profile`, `t64_machine_create` ladders (the depth sidecar allocation at `:2672-2681` uses `is_ai_family_profile`), `is_si_profile`, `has_si_family_profile_id`, `has_pinned_openipl3_profile_id`, `is_rdram_b_t1_profile`, `t64_profile_lookup`; `t64_isviewer.c: t64_isviewer_active`; `t64_state.c: state_version_registered`, `write_header`, `state_section_identity`, `t64_machine_state_export`; `t64_digest.c: t64_digest_stream`; `t64_bus.c: decode_region`, `t64_cpu.c: cpu_successor_extra_event_budget`, `t64_observe.c: t64_input_event_handler`, `t64_si.c` x3 (these call `t64_has_si_profile`, so widening covers them). Registered-id validation: `T64_PROFILE_TEXT_CAPACITY` is 64 (`core/include/tendies64/t64_machine.h:15`); longest planned id (`...-si-v2-e16k`, 43 chars) fits.

### Pattern 2: v26 digest walker and container v2
**What:** a new domain constant next to `"t64.state.si.25"` (e.g. `"t64.state.si.26"`, `core/t64_digest.c:278`), `has_v26_state` first in the domain chain, framed sections 1-9 (+10, +11 for save ids) as v25. Depth members stream in the **new-span class** so strip-and-compare against v25 stays available (`digest_bytes_class(... T64_DIGEST_CLASS_NEW)`; the sink API is `digest_bytes_class` at `core/t64_digest.c:19-50`, `hash_u32_new`/`hash_u64_new` at `:84`/`:88`; the existing depth block is `if (has_v21_state) {` at `:705`). Container: header `container version` word at offset `0x008` (documented as `1`, `state-container.md:36`; constant `T64_STATE_CONTAINER_VERSION UINT32_C(1)`, `core/t64_internal.h:1873`) becomes a per-state-version value (2 for state 26; 1 stays for 25), header state word at `0x00C`. The public string `T64_STATE_CONTAINER_FORMAT "t64.state-container.v1"` (`core/include/tendies64/t64_machine.h:410`) and the runner flag `--state-export-format` need a matching decision (recommend `t64.state-container.v2` accepted only with state-26 ids; see A4).
**Depth block placement (design hazard, VERIFIED):** `state_read_rdp` builds `triangle.view.depth_address = 0u; ... depth_valid = false; ... primitive_depth_valid = false;` (`core/t64_state.c:1686-1688`) and `state_check_triangle_render` rejects any opcode `t64_rdp_triangle_opcode_accepted` refuses. A Z triangle in flight can therefore only be accepted if the depth scalars (`depth_image_address`, `depth_image_valid`, `primitive_z`, `primitive_dz`, `primitive_depth_valid`) are read **before** that check. Emit them inside the RDP block at the position the v21 block uses (`core/t64_digest.c` `if (has_v21_state) { hash_u32(sink, rdp->depth_image_address); ...` immediately before `work_policy_version`), keep only the hidden table (`T64_RDP_DEPTH_HIDDEN_BYTES`, 2,097,152 bytes for 4 MiB RDRAM; macro at `core/t64_internal.h:130-131`) in the tail after the coverage table. The commit currently zeroes `rdp.depth_image_address` (`core/t64_state.c:3022`) and the hidden table (`:3002-3004`); both must become blob-driven for v26 and stay zeroed for v25.
**Size accounting:** v25 idle stem blob 8,448,795 bytes (`state-container.md:164-170`); v26 adds 24 bytes of scalars plus 2,097,152 hidden bytes, about 10,545,971 idle. The tests assert `required < 11534336u` (`core/tests/test_state_export.c:720`, `runner/tests/test_state_runner.c:855`); idle v26 still fits, but the documented "arithmetic bound below 11,534,336" (`state-container.md:171-176`) is wrong for v26 with retained packets (+614,400 video +524,272 audio => 11,684,643). Re-derive and state a new upper bound (e.g. 12,582,912) for the v26 text and tests.

### Pattern 3: completeness flip test written first (RED)
`core/tests/test_digest_v25.c:1060-1140` has the model: an `rdp_member` table that is "the complete member list of the structure: a size assertion fails the build when a member is added without a row, and every byte that no row covers (padding) is flipped as well" with `RDP_DEPTH_REASON` exclusion text for descriptor 12. For v26 copy the file (copy-paste is accepted by the dependency philosophy), move the five depth members from excluded (`MX`) to in-stream (`M`), add the hidden table, and make it fail first on a build with no walker change.

### Pattern 4: per-row oracle pin (GATE-02)
`PINNED_REQUEST_SHA256` is line 23; `_verify_corpus` (`scripts/check-rdp-oracle-authority.py:297-396`) hashes the manifest as a whole twice: `corpus.fixture_manifest.sha256` (`:308-314`) and `bound_authored_inputs` containing `fixtures/manifest.json` (`:39`, enforced equal to `EXPECTED_CORPUS_PATHS` at `:332`). Per-row rights/identity is already checked per row (`row.get("sha256") != expected.get("built_fixture_sha256")`, `:394`) for `authored_fixture_ids == ["rdp-fill", "rdp-fill-payload"]` (`:384`). Design:
1. Replace `fixture_manifest` `{path, sha256, git_blob_id_sha1}` with `{path, bound_rows: [{id, row_sha256}]}` where `row_sha256` is the existing canonical-JSON hash (`_canonical_sha256`) of each bound manifest row (the same two ids); delete `fixtures/manifest.json` from `EXPECTED_CORPUS_PATHS` and from `bound_authored_inputs`.
2. Add checker tests (in `scripts/tests/test_rdp_oracle_authority.py`): (a) appending an unrelated authored row to a scratch manifest copy leaves the checker valid and blocked; (b) changing a bound row's `sha256`, `size` or rights refuses; (c) the old whole-file mutation no longer refuses; (d) `local_decision` stays blocked and `--require-approved` still exits 2.
3. One last identity-only rebind inside the GATE-02 commit (reproduce old pin first, orchestrator task). Wave 3's manifest edit must then need no rebind: assert it with `ctest -R '^harness\.rdp-oracle-authority$'` (0.2 s).
4. Decide with the orchestrator, **before** Wave 1, whether to also stop binding the other 13 bound files (tests, scripts, case inventory, `fixtures/rdp/fill.S`) by whole-file hash (C1). Default recommendation: keep file hashes but put all new tests in new files, so no further rebind occurs after GATE-02; if any edit to a bound file is unavoidable (for example a defect fix in `test_rdp_depth.c`), batch it into one orchestrator rebind at the end of that wave.

### Pattern 5: GATE-01 mechanics
- `COST`: CMake doc: "When parallel testing is enabled, tests in the test set will be run in descending order of cost ... When the cost of a test is not defined by the project, ctest will initially use a default cost of 0" [CITED: cmake.org/cmake/help/latest/prop_test/COST.html]. A fresh hosted runner has no `Testing/Temporary/CTestCostData.txt` (a local copy exists at `build/gate/Testing/Temporary/CTestCostData.txt`). Set `COST` on `unit.core.state.import` (hosted macOS about 210 s), then `harness.state-field-use-audit` (about 71 s), `harness.phase09-qualification` (about 35 s) and `repo.rsp-no-hle` (about 48 s on Linux); `jobs: 4` is already the preset (`CMakePresets.json`). The sanitizer block already overrides TIMEOUT for `unit.core.state.import` (`core/CMakeLists.txt:~1106`); add `COST` once, outside the sanitizer conditional.
- Scanner: rule text `CI-TIMEOUT required gate jobs set timeout-minutes from 1 to 10` (line 27) and the check `if (val ~ /^[0-9]+$/ && val + 0 >= 1 && val + 0 <= 10) hastimeout = 1` with error `timeout-minutes must be an integer from 1 to 10` (lines 286-288).
- **Trap:** two selftest mutations inject into the first line matching `/timeout-minutes: 10/` (`scripts/check-ci-yml.sh:538` and `:681`, both `awk '/timeout-minutes: 10/ && !done ...`). `ci.yml` has `timeout-minutes: 10` at lines 20 (gate-linux) and 74 (gate-macos). If only gate-macos becomes 15, the first match is still gate-linux and both awk lines keep working; if both change, the injected file equals the clean file and the `expect CI-SECRETS` cases fail. Recommended: gate-linux stays 10, gate-macos 15, scanner enforces a per-job bound (linux <= 10, macos <= 15) with two new mutations (macos 16 and linux 11 must fail `CI-TIMEOUT`; the existing stress-workflow mutation at line 470 is unrelated and stays).
- Cold-run procedure: `gh cache list` shows exactly two entries (key `binutils-macOS-ARM64-69d0bfd4...` 5.03 MiB and `binutils-Linux-X64-69d0bfd4...` 31.10 MiB, last used 2026-11-20); delete the macOS one with `gh cache delete <id>`, dispatch `ci.yml` (it has `workflow_dispatch`), then read step timings with `gh run view <id> --json jobs`. A failed or timed-out job does not save the cache: actions/cache README: "a new cache is automatically created provided the job completes successfully" [CITED: github.com/actions/cache README]; that a timed-out job saves nothing is an inference [ASSUMED] (A5). Measure the cold run on the **final** tree in Wave 4 (Phase 10 tests eat the margin); take warm before/after step timings in Wave 0.

### Anti-patterns to avoid
- Editing any of the 14 oracle-bound files for VG/VID/CORE tests (Correction C1).
- Editing `ci.yml` twice: decide the hosted footprint of the Phase 10 qualification in Wave 0 (any new emit/upload step also needs `CI-UPLOAD-ARTIFACT`/`CI-REQUIRED-STEP` scanner edits). Default: no new hosted emit step; a native thin-delta record plus the existing hosted pair.
- Capturing digest literals for non-idle states from one tree (build id leaks into packet-bearing digests).
- Adding state members without a field-use row (`scripts/check-state-field-use.py` fails an unmapped member) and without fuzz coverage.

## Depth semantics recheck (primary sources) and independent literals

**Sources.** `docs/spec/rdp.md:443-507` (V7) states the software policy but prints no Z table and no URL. The primary citations live in `.planning/milestones/v1.1-phases/07-rdp-reference-renderer-and-vi-scanout/07-08-DEPTH-FIELDS.md` and `07-08-RAW-CONTRACT.md`: Nintendo 64 Programming Manual NU6-06-0030-001G (pp. 318-336: Z image format Figure 16-12/16-13, `z_format[8]`, DeltaZ, blender modes), SGI RDP Command Summary v2.0 (Tables 11, 15, 20, 26), SDK `gbi.h` rev 1.138. The SDK page for `gDPSetPrimDepth` independently says the Z value is converted to a 14-bit value and deltaZ to a 4-bit value for storage [CITED: ultra64.ca/files/documentation/online-manuals/man/n64man/gdp/gDPSetPrimDepth.htm via search result]. Phase 10 should add the table and URLs to `rdp.md` so the repo doc is self-contained (the archive path moved).

**Table (verbatim from `07-08-DEPTH-FIELDS.md:60-69`, citing manual printed p. 324):**

| Exponent | Shift | Add | Decode from stored 11-bit mantissa `m` |
|---:|---:|---:|---|
| 0 | 6 | `0x00000` | `(m << 6) + 0x00000` |
| 1 | 5 | `0x20000` | `(m << 5) + 0x20000` |
| 2 | 4 | `0x30000` | `(m << 4) + 0x30000` |
| 3 | 3 | `0x38000` | `(m << 3) + 0x38000` |
| 4 | 2 | `0x3c000` | `(m << 2) + 0x3c000` |
| 5 | 1 | `0x3e000` | `(m << 1) + 0x3e000` |
| 6 | 0 | `0x3f000` | `(m << 0) + 0x3f000` |
| 7 | 0 | `0x3f800` | `(m << 0) + 0x3f800` |

**Internal consistency [VERIFIED: scratch script, this session]:** with `edge += 2048 << shift` each row's Add equals the previous row's end and the last end is `0x40000`, so the rows tile the 18-bit range exactly (`0x0000..0x3ffff`). Visible 16-bit word = exponent(3) | mantissa(11) | high two dZ-code bits; low two bits go to the sidecar (`rdp.md:473-477`).

**Independent literals (computed from the documents, not from `core/t64_rdp_depth.c`; scratch calculator, not committed):**

| Case | Inputs | Expected |
|---|---|---|
| Far/uninitialized word | stored `0xfffc` | decodes to `0x3ffff` (exponent 7, mantissa `0x7ff`) |
| `07-08-RAW-CONTRACT.md` example | PrimDepth Z `0x1357`, dZ 9 | linear `0x9ab8`, exponent 0, mantissa `0x26a`, decoded `0x9a80`, delta code 3 (decoded 8), visible `0x09a8`, hidden low bits 3 (matches the contract text; recomputed independently) |
| VG-01 (b) primitive Z saturation | raw `0x7ffe` / `0x7fff` / `0x8000` / `0xffff`, dz 0 | linear `0x3fff0` / `0x3fff8` / `0x3fff8` / `0x3fff8`; visible (dz code 0) `0xffc0` / `0xffe0` / `0xffe0` / `0xffe0` (mutant `cap 0x7fff -> 0x7ffe` changes the second and third) |
| VG-01 (c) delta ceiling | `abs(dzdx)+abs(dzdy)` = 8193 Q16 units | ceiling gives delta 2, priority code 1; floor gives 1, code 0; 8192 gives 1; 8191 gives 1; 0 gives 0; max `0x7fffffff` twice clamps to 65535 |
| VG-01 (a) decal at equality | old: primitive Z raw `0x2000` (linear `0x10000`), dz 9 -> stored visible `0x1000`, decoded `0x10000`, stored delta decoded 8; new: primitive dz 2 | tolerance = max(2, 8) = 8. New raw `0x2001` (linear `0x10008`, difference 8) must **pass** decal; raw `0x2002` (difference 16) must fail. Mutant `<=` to `<` fails the first |
| Exponent boundaries (dz code 0) | linear `0x3f`/`0x40`/`0x1ffff`/`0x20000`/`0x2ffff`/`0x30000`/`0x37fff`/`0x38000`/`0x3bfff`/`0x3c000`/`0x3dfff`/`0x3e000`/`0x3efff`/`0x3f000`/`0x3f7ff`/`0x3f800`/`0x3ffff` | visible `0x0000`/`0x0004`/`0x1ffc`/`0x2000`/`0x3ffc`/`0x4000`/`0x5ffc`/`0x6000`/`0x7ffc`/`0x8000`/`0x9ffc`/`0xa000`/`0xbffc`/`0xc000`/`0xdffc`/`0xe000`/`0xfffc`; decoded back `0x00000`/`0x00040`/`0x1ffc0`/`0x20000`/`0x2ffe0`/`0x30000`/`0x37ff0`/`0x38000`/`0x3bff8`/`0x3c000`/`0x3dffc`/`0x3e000`/`0x3effe`/`0x3f000`/`0x3f7ff`/`0x3f800`/`0x3ffff` |

Recipe to keep independence: write the expected values as hex literals in the test from this table and `docs`, never by calling `z_compress`/`z_decompress`; add a wrong-value control in the existing style (`same-setup-wrong-pixel`: change one expected literal after an accepted setup and require divergence at the intended byte). Policy facts that remain software policy, not silicon (state it in evidence): primitive Z `min(raw16, 0x7fff) << 3`, signed floor division by 8192 with clamp for triangle Z, `z_update_en` suppressed by transparent/decal (rdp.md:462-493; 07-08-DEPTH-FIELDS.md "Explicit gaps": the precedence is unproven hardware behavior). Nothing here is oracle material (D5): the table comes from the Nintendo/SGI documents already cited by the project, and the arithmetic is derivable from them.

VG-02..05 independent derivations (from the verifier's prescriptions, `07-VERIFICATION.md:122-137`): VG-02 v9 serrate packet with STATUS bit 6 set, field 1, rows of different content: expected bytes built from source rows `2n+1` and `2n+3` (not `2n`, `2n+2`, or `2n+1`/`2n+2`); VG-03 an RGBA5551 source pixel with bit 0 clear through the v8 and v9 paths: output alpha byte 0; VG-04 negative S (and T) with nonzero remainder, e.g. S = -131073 and W = 131072 (Q16): exact S*65536/W = -65536.5, floor -65537 (texel -2) vs truncation -65536 (texel -1) (my example; confirm the surrounding tile shift/mask path in `docs/spec/rdp.md:197-211` before use); VG-05 a shaded triangle with negative `dx*(x-major)` not a multiple of 65536, and, for Bayer, choose per-position channel values at the threshold `c = 8m - bias(x&3, y&3)` from the printed matrix (`rdp.md:417-423`: rows `0 4 1 5`, `6 2 7 3`, `1 5 0 4`, `7 3 6 2`) so any bias perturbation changes the 5-bit result (the surviving mutant used a pixel where `(3,2)` is insensitive).

NTSC VI clock vector [VERIFIED: recomputed with `fractions`]: field = `(H_SYNC.low12 + 1) * V_SYNC.low10 / 2` source cycles (`vi.md` "Clock and boundary policy"). Standard NTSC `(3093 + 1) * 525 / 2 = 812,175` cycles; at 62,500,000 ticks/s over 48,681,812 Hz the exact field is 1,042,708.4657... ticks, first deadline `ceil` = **1,042,709** (matches `07-VERIFICATION.md` Observation 3). The vector kills the surviving clock mutant 48,681,812 to 48,600,000 (about 1,755 ticks of difference). The 1 Hz change is immaterial per field (about 0.02 tick) and is not required; if wanted, check the k-th boundary for large k with exact `Fraction` arithmetic.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| SHA-256, canonical hashing | a second hash routine | `tools/common/t64_sha256.c`, `_canonical_sha256` in the checker | already pinned and reviewed |
| Test ordering by cost | a custom scheduler or sleeps | CTest `COST` property | native CTest behavior with `jobs: 4` |
| Mutant drivers | ad hoc reviewer edits | copy `scripts/k5-mutants.py` pattern (anchors, throwaway worktree, KILLED/SURVIVED/INVALID, two controls) | re-runnable evidence; `--check-anchors` can be a cheap CTest |
| Two-tree digest regression | eyeballing digests | a committed driver run on two `git archive` trees with one forced build identity | build id leaks into packet digests (C4) |
| Fixture ROM build | a new toolchain flow | copy `scripts/build-save-fixtures.sh` (same pinned asset, payload, Binutils hashes) | precedent of copy-paste builders (pad, audio, admit, save all copy each other) |
| Importer bounds for new members | ad hoc `if`s | `F32_LE`/`FBOOL`/`STATE_CHECK` readers with named `T64_STATE_RULE_*` and a row in `docs/spec/state-field-rules.txt` | the field-use audit fails an unmapped member |
| Composite guest transcript | a new log format | the ISViewer print idiom already in `fixtures/replay/mix.S` and a log pin in the runner row | admission is by pinned length+SHA |

**Key insight:** every piece has a precedent in the repo; the risk is in the pins that bind them (oracle request, sealed bodies, F2 identity pins), not in new algorithms.

## Runtime State Inventory

Phase 10 is not a rename, but it changes identities and pins. Explicit answers:

| Category | Items Found | Action Required |
|----------|-------------|-----------------|
| Stored data | No persisted state blobs are tracked in git; blobs are private runner artifacts (`.sta`, mode 0600) and are build-identity-bound | None; document "build-bound, outside the ABI promise" |
| Live service config | GitHub Actions cache: two Binutils entries (ids `10797050444` macOS, `10796791825` Linux, last used 2026-11-20); no secrets (`permissions: {}`) | Deleting the macOS entry is the cold-run procedure (note it in the plan; not an owner stop) |
| OS-registered state | None | None |
| Secrets/env vars | `T64_TEST_IPL3_PATH`, `T64_BOOT_FIXTURES_DIR`, `T64_STATE_MUTANT_INVENTORY`, `T64_GATE_JUNIT_OUT` (allow-listed roots in `scripts/gate.sh:102-103`) | Add a Phase 10 private JUnit root only if a Phase 10 qualifier uses it |
| Build artifacts / installed packages | `build/` is 26 GB with about 5,647 retained run directories under `build/gate`; local tags `phase09-*` not pushed; `build/gate/Testing/Temporary/CTestCostData.txt` local only | Prune closed-phase run dirs before native runs (AUTONOMY v1.2 rules); rebuild fixtures after any manifest/guest change |

## Common Pitfalls

### Pitfall 1: edits to oracle-bound sources silently require a rebind (C1)
**What goes wrong:** `harness.rdp-oracle-authority` fails after touching `core/tests/test_rdp_depth.c`, `core/tests/test_vi.c` or `docs/spec/rdp-vi-cases.json`; editing the case inventory also fails the hosted raw RDP/VI emit step (`qualify-rdp-vi.py:561`; whether the hermetic `harness.rdp-vi-qualification` reaches that check was not verified).
**Avoid:** new files only; run `ctest -R '^harness\.rdp-(oracle-authority|vi-qualification)$'` after every wave (about 1 s) and replay `python3 -B scripts/qualify-rdp-vi.py --emit-hosted ...` locally (F-107 lesson) when any bound input moves.

### Pitfall 2: runner observation shapes reject the composite guest (C2)
**What goes wrong:** the save-out run ends `harness-error` with detail `video-drain-error` or `audio-drain-error` because the VI window is not 4x3 or the audio packets are not exactly two of 16 bytes.
**Avoid:** author the guest to the legacy shapes; check admission with a one-off supervised run before writing expectations.

### Pitfall 3: importer cannot accept a Z triangle in flight (depth block after the render proof)
**What goes wrong:** blobs exported mid Z-triangle are refused with `FIELD_RANGE`/`ENUM_CLOSED` because the importer saw `depth_valid = false` when it ran the proof.
**Avoid:** emit and read the five depth scalars inside the RDP block (Pattern 2). Test with a B17 analogue (`core/tests/test_state_import.c:3104-3275` shows B17) using opcodes `0x09/0x0b/0x0d/0x0f`.

### Pitfall 4: widening `t64_has_si_profile` changes the digest domain or exporter by accident
**What goes wrong:** v26 machines hash under the v25 domain, or the exporter accepts v26 into the v25 path. **Avoid:** exact predicates at digest, registry and export; golden-first: capture the 17 idle literals and the family literals before the header edit (09-01 practice), and add the predicate-matrix rows for the four new ids with depth expected active (`core/tests/test_si_profile.c` matrix uses `P_DEPTH` flags).

### Pitfall 5: K1 refusal behavior has no characterization test today
**What goes wrong:** SC1 requires the shipped ids keep Z refusal byte-identical, but no test pins it. [VERIFIED: a grep of every test that names `si-v1` or `filter-v9-ai-v1` for Z opcodes found zero hits]. **Avoid:** Wave 0 test capturing status, refusal reason and digest on descriptors 11 and 12, before the predicate edit. Read from source: SetZImage and SetPrimDepth are refused at dispatch by `supported_opcode` (`core/t64_rdp.c:863-869`: `increment_counter(&rdp->dpc_clock_counter); increment_counter(&rdp->dpc_buffer_busy_counter); record_command(...); set_refusal(machine, T64_RDP_REFUSAL_UNSUPPORTED_OPCODE);`), while the four Z triangles pass dispatch (a triangle profile accepts any opcode with a nonzero word count) and are refused at apply with `T64_RDP_REFUSAL_UNSUPPORTED_OPCODE` (`core/t64_rdp.c:444-457`) after the packet length checks. Pin both paths, including the DPC counters.

### Pitfall 6: state-import test cost explodes
**What goes wrong:** unit.core.state.import is 48 s native, 210 s hosted macOS, 1,149-1,441 s under ASan (97% of ASan wall); the v26 blob is about 25% larger and a second matrix doubles it, pushing macOS past budget and ASan toward the 2,400 s TIMEOUT.
**Avoid:** v26 import coverage in a separate CTest with a sampled matrix and the depth-specific rows (B17 analogue, mid-scan-out, hostile rules per new member); do the Phase 9 "split" now only if measured need remains (see triage). Compare registered TIMEOUT against max(native, ASan, hosted) before the first push (F-108).

### Pitfall 7: F2 identity pins (live-tree comparators in `harness.phase09-qualification`)
`scripts/tests/test_phase09_qualification.py:516-672` pins the four si ids, their profile hashes, `phase09-*` manifest rows, transcript pins, `docs/spec/phase09-cases.json` bytes (`inventory_sha256`) and baseline + `PHASE09_TESTS` names. New rows named `phase10-*` and a new `PHASE10_TESTS` block do not touch them. Never change an existing descriptor field (that would move a pinned profile hash).

### Pitfall 8: manifest edits and the oracle pin until GATE-02 lands
Only ordering protects you: GATE-02 first. After any `fixtures/manifest.json` edit run `ctest -R '^harness\.rdp-oracle-authority$'` at once (AUTONOMY lesson); the rebind is an orchestrator task.

### Pitfall 9: hosted-time drift
Spread on identical code: macOS 349/420/433/435/487 s (F-114). New tests add `test-inventory-selftest` cost (about 0.2 s per name natively; hosted macOS selftest already 47 s). Measure; do not project.

## Code Examples

Sketches only (design proposals, not discovered values):

```c
/* core/t64_internal.h: family predicates (proposal; tuple 13/21/26 is A1) */
static inline bool t64_has_si_v25_profile(const t64_machine *m) { /* exact 12/20/25 */ }
static inline bool t64_has_si_v26_profile(const t64_machine *m) { /* exact 13/21/26 */ }
static inline bool t64_has_si_profile(const t64_machine *m) {
  return t64_has_si_v25_profile(m) || t64_has_si_v26_profile(m); /* device family */
}
/* t64_has_rdp_depth_profile(): ... existing terms ... || t64_has_si_v26_profile(machine) */
```

```cmake
# core/CMakeLists.txt after the add_test of unit.core.state.import (COST is a plain double;
# order only, no registration or inventory effect)
set_tests_properties(unit.core.state.import PROPERTIES COST 1000)
set_tests_properties(harness.state-field-use-audit PROPERTIES COST 400)
```

```sh
# GATE-01 cold-run measurement (orchestrator; read-only except the cache delete)
gh cache list
gh cache delete <macOS-binutils-id>
gh workflow run ci.yml        # workflow_dispatch exists
gh run view <run-id> --json jobs
```

Two-run RUN-07 flow (shapes from `docs/spec/state-container.md:539-626` option matrix): run 1 = fixture + profile + `--raw-capture-dir` (+ default or custom `--input`); run 2 = same fixture + profile + `--input` + `--artifact-dir`/`--artifact-max-bytes` + `--save-export-leaf`. RAW with PAIR or SAVE-OUT is refused by design; two separate supervisor invocations, then compare `state_digest` of the two result records and each output with its own literals.

## RUN-07 mechanics

**Guest shape.** Use the closed building blocks: RDP fill + VI 4x3 window from `fixtures/rdp/fill.S` (accepted authored case 16,581 bytes total capture; `raw-capture.md:122-130`), two AI DMAs of 16 bytes each from `fixtures/audio/pcm.S` (DACRATE 1103; packet rate 48,681,812/1,104 as pinned in `audio_observation_valid`), SI poll from `fixtures/pad/poll.S` or `fixtures/replay/mix.S` (six polls, input script `fixtures/replay/mix.input.json`), SRAM write/read from `fixtures/save/sram.S`/`roundtrip.S` (`phase09-save-roundtrip-s32k` precedent). Run it on one new id of the depth family (recommend the `-s32k` suffix: roundtrip precedent). Cap: `RLIMIT_FSIZE` child limit 69,632 bytes without the artifact pair (`raw-capture.md:122-130`); the pair raises it to the declared maximum plus 65,536. Expected sizes are tiny (4x3).
**Runner edits (production, unavoidable):** a row in `runner/t64_real_fixture.c` (columns seen in the table: id, fixture SHA-256, size, profile id, profile hash, completion PC, long-cap flag, transcript length, transcript SHA-256; `T64_REAL_GUEST_TICKS_LONG_CAP` for long guests), and, if the guest exceeds the legacy observation shapes, `runner/t64_real.c` changes (avoid). The profile hash literal for each new id comes from `t64_profile_hash` after registration (so RUN-07 follows Wave 2).
**Manifest-moving commit steps** (precedent: five prior rebinds `1b0a1ae`..`cec0446`): (1) add `fixtures/composite/*.S|.ld` and `scripts/build-composite-fixture.sh` (copy of the save builder), CMake fixture target; (2) manifest rows (`authored-here`, `cleared`, `build-only`, sizes/hashes from the build; include a `wrong-` control row); (3) runner admission rows; (4) `scripts/check-test-inventory.sh` names; (5) run rights/PII scans; (6) after GATE-02 no oracle rebind should be needed: confirm with `ctest -R '^harness\.rdp-oracle-authority$'`; if GATE-02 slipped, orchestrator-only identity rebind (reproduce old hash, change only manifest identities, request hash and `PINNED_REQUEST_SHA256`, keep `local_decision` blocked). `gh`-side nothing.
**Independent expectations:** video bytes (the 4x3 packet, 48 bytes), audio bytes (two 16-byte S16BE packets), save bytes (the exact SRAM bytes the guest writes), each as literals in the test with a wrong-value control; the final `state_digest` equality across the two runs is the only cross-run claim. The runner report never "passes" a resumed run (09-09 note), unrelated here.

## Don't repeat: scope/triage of Phase 9 items (recommendation, each with reason)

| Item | Recommendation | Reason |
|---|---|---|
| Split `unit.core.state.import` | Defer unless the v26 coverage is added to the same binary; if added, do it as a new CTest with sampled counts | 97% of ASan wall; Phase 9's 45-of-45 name bound no longer binds Phase 10 (new `PHASE10_TESTS`) |
| Replay-driver duplication (L3 IN-02) | Defer to Phase 11 | touches runner/replay loop; no state/digest effect; Phase 11 reshapes the API |
| Dead `t64_artifact_publisher.c` link in `test_replay` | Drop or do as a one-line CMake edit in Wave 4 | `runner/CMakeLists.txt:340,350,649,715` list the file; harmless; any runner CMake edit moves no pin but costs a build |

## State of the Art

| Old approach | Current approach | Impact |
|---|---|---|
| Whole-file oracle pin | per-row binding | adding authored rows stops forcing rebinds |
| Depth guarded by descriptor version 10 (K1) | depth on a new registered family | shipped ids keep Z refusal; consumers name the new id |
| Review at phase end | per-wave scratch-clone review | Phase 9's blocking defect was found only late |

**Deprecated/outdated:** "510-digest harness" (not committed, C4); "eight docs" count (C6); lens R7 guard-removal framing (C3).

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | New family tuple is descriptor 13 / model 21 / state 26 with ids `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2` and suffixes `-e4k`, `-e16k`, `-s32k` | Pattern 1 | Different numbers/names are cheap before the bump; one-way after. Owner/planner naming call (CONTEXT leaves it to the planner) |
| A2 | No AI-only (descriptor 11 analogue) depth id is added; only the SI family gets depth | Standard Stack | If the roadmap's "composite profiles" meant both, a second family doubles wave 2 |
| A3 | Depth scalars belong inside the RDP block (v21-style position), hidden table in the tail, all in the new-span class | Pattern 2 | Alternative (tail + deferred render proof) is feasible but needs a reader restructure; planner may choose after a spike |
| A4 | Container v2 means header version word 2 plus format string `t64.state-container.v2` accepted only with state-26 ids; v1 string only with state-25 | Pattern 2 | The runner flag contract changes; Phase 11 may rename; reversible until the freeze candidate |
| A5 | A timed-out job saves no Binutils cache (documented only for "completes successfully") | Pattern 5 | Cold-run recovery assumptions; verify on the first cold run |
| A6 | A mechanism to force one build identity for the two-tree sweep (edit `core/CMakeLists.txt:66` in each scratch clone) exists as 09-10 did; the exact mechanism was not found committed | Corrections C4 | Sweep script design differs; low impact |
| A7 | The VG-04 example (S = -131073, W = 131072) selects different texels under the tile policy | Depth recheck | Choose another pair after reading the exact sampling path |
| A8 | Hosted macOS step costs after Phase 10 stay inside 900 s cold (673 s cold projection today plus new tests, minus COST saving about 40 s) | Pattern 5 | If the cold run exceeds, trim tests or move work to non-gating lanes (G4) |
| A9 | New CTest names outside Phase 9's 45-name bound are acceptable now | Pitfall 7 | CONTEXT says plans may add names; Phase 9's bound was frozen to its own phase |

## Open Questions (RESOLVED)

1. **Does the owner/orchestrator widen GATE-02 to stop whole-file binding of the 12 mutable bound test/script sources (C1), or keep them and rely on new-file placement?**
   - Known: new-file placement avoids all rebinds after GATE-02. Unknown: whether a defect fix in an old test file will be needed (VG vectors may expose a real defect; budget one fix per VG per CONTEXT). A production fix in core source does not move the oracle pin (the 14 bound files are tests, scripts, the inventory, the fill guest and the manifest) but does move the build id.
   - Recommendation: keep file hashes, new files only, and pre-agree that a forced edit of a bound file is a batched orchestrator rebind (the standing approval's text names "manifest identities"; treat this as the same identity-only class but record it).
   - RESOLVED: keep file hashes, new files only; a forced edit of a bound file is a batched orchestrator rebind (10-CONTEXT 'GATE-02 scope').
2. **Guard removal (C3) versus Phase 8 qualifier freeze.** Recommendation: schedule last in GATE-01, measure first, and decline with a recorded reason if it requires editing `qualify-audio.py`.
   - RESOLVED: 10-CONTEXT 'GATE-01 guard'.
3. **Hosted footprint of the Phase 10 qualification.** Recommendation: none new in `ci.yml`; decide in Wave 0 so `ci.yml` is final.
   - RESOLVED: 10-CONTEXT 'Hosted footprint' (no new hosted step).
4. **Does RUN-07 need descriptor-13 `-s32k` only, or all four ids?** Recommendation: one save id for the fixture, all four ids registered.
   - RESOLVED: 10-CONTEXT 'Identity naming'; RUN-07 runs on `-s32k`.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| cmake / ninja / CTest | build, gate | yes | 4.4.3 / 1.13.2 | none |
| python3 | scanners, drivers | yes | 3.14.4 | none |
| pinned mips64-elf Binutils | guest builds | yes (`build/tools/install/bin`) | 2.45 (cached) | `scripts/build-boot-fixtures.sh --qualify` rebuild (about 3 min macOS) |
| gh CLI | cold run, timings | yes | 2.101.0 | none |
| act + Docker daemon | Linux compile pre-flight (F-102) | yes | act 0.2.87; Docker server 29.5.2 | hosted run |
| git, shasum, node | scratch clones, hashes | yes | git 2.41.0; node 26.10.0 | none |
| host load | native timing | load average about 6.2 at probe | 4+ jobs share | wait or prune `build/gate` run dirs |

**Missing dependencies with no fallback:** none. **Note:** `build/` holds about 5,647 retained run directories; prune before native runs.

## Validation Architecture

(`workflow.nyquist_validation` is `true` in `.planning/config.json`.)

### Test Framework
| Property | Value |
|----------|-------|
| Framework | CTest over C test binaries (`tests/t64_test.h`) plus Python harness tests |
| Config file | `CMakePresets.json` (gate, asan), `core/CMakeLists.txt`, `runner/CMakeLists.txt` |
| Quick run command | `ctest --test-dir build/gate -R '<regex>' --output-on-failure` after `cmake --build --preset gate` |
| Full suite command | `ctest --preset gate` (224 names at HEAD per `ctest -N`; ASan preset 226) and `sh scripts/gate.sh` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| GATE-02 | adding an unrelated manifest row leaves the checker valid/blocked; changing a bound row refuses; `local_decision` stays blocked | harness (python) | `ctest -R '^harness\.rdp-oracle-authority$'` and `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` | extend `scripts/tests/test_rdp_oracle_authority.py` (exists) |
| GATE-01 | scanner accepts macOS 15 / Linux 10, refuses 16 / 11; awk selftest cases still reach their rules; COST visible in JSON | scanner selftest + preflight | `ctest -R '^repo\.ci-yml(-selftest)?$'`; `ctest --test-dir build/gate --show-only=json-v1` (COST present on the named tests) | edit `scripts/check-ci-yml.sh` (exists) |
| GATE-01 (measured) | cold hosted macOS run inside the limit; before/after step timings | manual-hosted | `gh cache delete`, `gh workflow run ci.yml`, `gh run view --json jobs` | n/a (orchestrator, Wave 4) |
| VER-06 | VG-01..05 + NTSC vector pass with literals; each named mutant fails | unit + mutant driver | `ctest -R 'unit\.core\.(vg|vi\.clock)'` (names free) ; `python3 -B scripts/p10-mutants.py --run` | Wave 0/1: new test file + driver |
| VID-07 | depth active on the four new ids; K1 refusal unchanged on shipped ids; fixture pixels/depth equal literals; wrong-depth control fails; 17-id and Phase 8/9 digests unchanged | unit | `ctest -R 'unit\.core\.(profile|digest|rdp\.depth)'`; characterization test run before and after the predicate edit; two-tree sweep script | new tests |
| CORE-06 | flip-every-member sees each member (RED first); export container v2/state 26; v25 export bytes unchanged; import round trip mid Z-triangle and mid scan-out equal | unit | `ctest -R 'unit\.core\.(digest\.v26|state\.(export|import))'`; `harness.state-field-use-audit` | new tests; extend `fuzz_state_import.c` |
| CORE-06 (docs) | build-bound/ABI statement present in state-container.md and machine-contract notes | grep in preflight | `grep -n "build-identity-bound" docs/spec/state-container.md` | doc edit |
| RUN-07 | two supervised runs; each output equals its literal; equal `state_digest`; wrong-value controls fail | runner integration | `ctest -R 'runner\.composite'` (names free) | new test + fixture |

### Mutants (named, from `07-VERIFICATION.md:243-261` and this research)
VG-01: decal tolerance `<=` to `<`; primitive Z cap `0x7fff` to `0x7ffe`; delta ceiling to floor (the `+ UINT64_C(8191)` term in `t64_rdp_depth_evaluate_pixel`). VG-02: selected parity ignored in the filter path; same-parity next row replaced by next row. VG-03: alpha forced to 255 in the v9 read function and in the v8 renderer. VG-04: floor correction disabled for the S lane and for the T lane. VG-05: major-displacement product truncates; Bayer bias shifted. VI clock: source clock to 48,600,000. VID-07/CORE-06 add: depth sidecar omitted from the digest; depth scalar omitted; hidden table omitted from import commit; sidecar invalidation loop removed (`core/t64_rdp_pipeline.c:269`); predicate widened to descriptor 11/12 (must be caught by the K1 characterization). Run in a throwaway worktree with a `--check-anchors` mode in the gate and `--run` per wave; two controls (an equivalent edit must survive, a non-compiling edit must be INVALID) as `scripts/k5-mutants.py` does.

### Sampling Rate
- **Per task commit:** the task's named CTest regex plus `ctest -R '^harness\.rdp-(oracle-authority|vi-qualification)$'` and `repo.rsp-no-hle` guard (`python3 -B scripts/check-rsp-no-hle.py --build build/gate`), each seconds long.
- **Per wave merge:** `ctest --preset gate` (about 140-220 s native), `sh scripts/preflight-phase10.sh` (target <= 90 s), act Linux compile for any C change, replay of the post-Gate hosted steps locally (F-107), TIMEOUT audit (F-108), `ctest -N -R` regex count check before accepting a verify command.
- **Phase gate:** full gate green, ASan selection of the new names, hosted pair green on the final head (`gh run list`), one forced cold macOS run on the final tree, independent review in scratch clones per wave.

### Wave 0 Gaps
- [ ] `scripts/preflight-phase10.sh`: act-image compile, post-Gate replay, `ctest -N -R` counts, TIMEOUT audit, oracle-authority check, PII scan, head CI status (`gh run list`), host load.
- [ ] two-tree digest sweep tool (17 ids x authored ROMs x checkpoints; forced build identity) replacing the unrecorded 09-10 harness.
- [ ] `core/tests/test_k1_refusal.c` characterization (status, refusal reason, digest for SetZImage, SetPrimDepth, four Z triangles on ids 11 and the four 12s) captured before any edit of `core/t64_internal.h`.
- [ ] `scripts/p10-mutants.py` (modeled on `scripts/k5-mutants.py`) with anchors for the VG and VID mutants.
- [ ] `core/tests/test_digest_v26.c` RED completeness test (before the walker change).
- [ ] pin-impact map committed (table below).

## Pin-impact map (Wave 0 deliverable seed)

| Edit | Pin or check it moves | Cheap check |
|---|---|---|
| any of 14 files in `EXPECTED_CORPUS_PATHS` | oracle request hash (`PINNED_REQUEST_SHA256`, line 23) | `ctest -R '^harness\.rdp-oracle-authority$'` |
| `docs/spec/rdp-vi-cases.json` (needed to list new cases of a Phase 7 binary) | `qualify-rdp-vi.py:28` `CASE_INVENTORY_SHA256` (hosted emit step) plus the oracle request | replay the hosted emit step locally; `check-rdp-vi-cases.py --build build/gate --suite <name>` (not in the gate) |
| `fixtures/manifest.json` row | oracle request until GATE-02; `phase09-*` rows are F2-pinned (do not touch) | oracle test; `harness.phase09-qualification` |
| any core `.c`/`.h` text | build id (fingerprint); packet-bearing digests of pre-v25 ids; sealed bodies if a sealed function body changes | sweep tool; `check-rsp-no-hle.py` |
| `docs/spec/phase09-cases.json`, existing `phase09-*` rows, existing profile fields | F2 pins in `harness.phase09-qualification` | that CTest |
| `ci.yml`, `check-ci-yml.sh`, `gate.sh` | bound source of Phase 9's closed record (tuples at own commits; no re-verification of old record) | `repo.ci-yml`, `repo.ci-yml-selftest` |
| membership sites | `docs/spec/profile-membership-sites.txt` | `repo.si-membership-sites` |
| new used state members | `docs/spec/state-field-rules.txt`, `state-mutants.txt` | `harness.state-field-use-audit` |
| new CTest names | `scripts/check-test-inventory.sh` lists; selftest cost | `check-test-inventory.sh --preset gate` and `--selftest` |

## Security Domain

`security_enforcement` is enabled (absent = enabled); ASVS level 1.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication / V3 Session | no | no accounts or sessions |
| V4 Access Control | no (artifact files are 0600, owner-only directory) | existing artifact publisher |
| V5 Input Validation | yes | validate-then-apply importer with named rules for every new member; fuzz (`core/tests/fuzz_state_import.c`) and boundary matrix; field-use audit |
| V6 Cryptography | limited | existing `t64_sha256`; never hand-roll; hashes are integrity, not authentication (documented) |
| V10 Malicious code / V14 Configuration | yes | CI action pins by SHA, `permissions: {}`, no secrets, scanner rules; no new dependency |

### Known Threat Patterns
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Crafted state blob with out-of-range depth address/products | Tampering, DoS | per-member rules (`ADDR_RANGE`, `ENUM_CLOSED`, `BOOL_01`), re-run the triangle extent/preflight proof from blob values before commit |
| Oversized or truncated container | DoS | existing 64 MiB ceiling (`T64_STATE_MAX_BYTES`), runner artifact max 32 MiB, exact length checks |
| Protected material in a guest or evidence | Info disclosure | authored guests only (D2/D5), `rights-scan.sh`, `pii-scan.sh`, manifest rights rows |
| CI weakening during timeout change | Tampering | scanner selftest mutations for the new bound, SHA pins unchanged |
| Silent authority escalation via rebind | Elevation of privilege | `local_decision` stays blocked; both trusted decision pins remain `None`; orchestrator-only edits |

## Project Constraints (from CLAUDE.md and AGENTS.md)
- Authored guests only; no protected ROM/IPL/PIF/64DD/save/firmware use in public CI; a hash proves identity, not rights.
- No oracle material (D5): do not execute, fetch, inspect or copy Angrylion-Plus/parallel-rdp; expected values are authored from primary documents.
- Support statements tie to exact profile, build identity and evidence tier; this phase's claims stay "unit-tested, authored scope"; agreement or a passing test is not hardware accuracy.
- CI/CD local-first (AUTONOMY section 2): reproduce locally including `act`, run cheap consistency checks before hosted waits; take low-risk speed wins without weakening evidence; no routine approval requests.
- `git.create_tag` false; releases are D6 stops; no branding (D7).
- Distinguish owner decisions, documents, code observations, measurements, inferences, unknowns in evidence.
- Plans: at most 4 tasks, about 45 minutes, no decimal plans, defect fix = new numbered plan, review per wave in `git archive` scratch clones, `parallelization` false; reviewers never run `ninja -t clean` or fixture rebuilds in the shared tree.
- Executors never edit `scripts/check-rdp-oracle-authority.py` or `PINNED_REQUEST_SHA256`.

## Sources

### Primary (HIGH confidence, read this session)
- Source at HEAD: `core/t64_internal.h`, `core/t64_machine.c`, `core/t64_digest.c`, `core/t64_state.c`, `core/t64_rdp.c`, `core/t64_rdp_depth.c`, `core/t64_rdp_pipeline.c`, `core/tests/test_digest_v25.c`, `core/tests/test_si_profile.c`, `core/tests/test_rdp_depth.c`, `runner/t64_real.c`, `runner/t64_real_fixture.c`, `runner/t64_supervisor.c`, `core/CMakeLists.txt`, `CMakePresets.json`
- Scripts: `scripts/check-rdp-oracle-authority.py`, `scripts/check-ci-yml.sh`, `scripts/gate.sh`, `scripts/check-rsp-no-hle.py`, `scripts/check-audio-cases.py`, `scripts/check-rdp-vi-cases.py`, `scripts/qualify-audio.py`, `scripts/qualify-phase09.py`, `scripts/tests/test_phase09_qualification.py`, `scripts/k5-mutants.py`, `scripts/list-membership-sites.py`
- Docs: `docs/spec/rdp.md`, `vi.md`, `state-container.md`, `machine-contract.md`, `raw-capture.md`, `profile-membership-sites.txt`, `docs/ci-reviews/LOG.md` (F-096..F-114), `docs/evidence/phase07-oracle-authority.{json,md}`
- Planning records: `10-CONTEXT.md`, `lens-phase10.md`, `SYNTHESIS.md`, `.planning/STATE.md`, `.planning/ROADMAP.md`, `.planning/REQUIREMENTS.md`, `.planning/AUTONOMY.md`, `07-VERIFICATION.md`, `07-08-DEPTH-FIELDS.md`, `07-08-RAW-CONTRACT.md`, `09-01-SUMMARY.md`, `09-REVIEW-FIX.md`
- Commands: `git diff --stat d3211e9 HEAD`, `ctest -N` (224 tests), `gh cache list`, `gh run list`, `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` (exit 0: "authority record valid; blocked: no oracle operation is authorized")

### Secondary (MEDIUM confidence)
- [CITED] cmake.org/cmake/help/latest/prop_test/COST.html (COST semantics); github.com/actions/cache README ("a new cache is automatically created provided the job completes successfully"; it does not mention `save-always`)
- [CITED] ultra64.ca `gDPSetPrimDepth` reference (14-bit Z and 4-bit deltaZ stored; via search result snippet, not fetched in full)
- The Nintendo 64 Programming Manual PDF itself was not downloaded (protected content); the project's own citation and my table-consistency check stand in.

### Tertiary (LOW confidence)
- None used for decisions. A web search for a public secondary Z-table source found none; emulator sources (MAME/Glide64) were deliberately not opened (oracle-adjacent, D5).

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH, existing toolchain only.
- Architecture (identity/state bump): HIGH for sites and hazards (read in source); MEDIUM for the exact family/format naming (A1, A4).
- Pitfalls: HIGH for C1-C5 (read in scripts/code); MEDIUM for hosted timing.
- Depth literals: HIGH for arithmetic (independently recomputed), policy items remain software policy.

**Research date:** 2026-11-23
**Valid until:** 2026-12-07 (hosted cache eviction about 2026-11-27 may change the "cold" baseline; source is stable until Phase 10 edits begin)
