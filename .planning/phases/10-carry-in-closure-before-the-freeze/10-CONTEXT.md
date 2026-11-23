# Phase 10: Carry-in Closure Before the Freeze - Context

**Gathered:** 2026-11-23
**Status:** Ready for planning

<domain>
## Phase Boundary

Everything that changes state, descriptors or digests lands once, before the API is shaped. Requirements: GATE-02, GATE-01, VER-06, VID-07, CORE-06, RUN-07. The composite profiles render depth under NEW registered ids, the state container carries one version marker, the accepted Phase 7 gaps (VG-01..05 and the standard-NTSC VI clock vector) have independent tests with failing mutants, one composite guest is verified through two runner runs with an equal final digest, and the hosted macOS gate fits a 15-minute job limit with a measured cold run. No oracle material (D5). No public API work (Phase 11).

</domain>

<decisions>
## Implementation Decisions

### Locked by ROADMAP.md, STATE.md and the v1.2 fan-out synthesis (not re-asked)
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

### Decided autonomously
- [Test layout] Q: "Where do VG-01..05, the NTSC clock vector and depth tests live?" → Selected: "NEW test files with new CTest names only; never edit the 14 files bound by scripts/check-rdp-oracle-authority.py (test_rdp*.c, test_vi.c, test_video_output.c, runner/tests/test_rdp_video.c, rdp-vi-cases.json, fixtures/rdp/fill.S, manifest) or the case inventory pinned in qualify-rdp-vi.py (research correction C1)" — new files are cheap to move; editing bound files would force an oracle-request rebind''
- [GATE-02 scope] Q: "Does GATE-02 also drop whole-file hashes of the other bound files?" → Selected: "No: new-file placement; if an old bound file must change, a batched orchestrator rebind (orchestrator only)" — default can be revisited in the GATE-02 plan
- [GATE-01 guard] Q: "Remove the redundant rsp-no-hle guard?" → Selected: "schedule last, measure first; the guard is a CTest in the Phase 8 qualification suite required by qualify-audio.py (F-106 frozen); if removal needs a qualifier edit, decline with a recorded reason — the 15-minute limit already fits the 673 s cold projection" — measurement-driven and reversible
- [Hosted footprint] Q: "New hosted CI step for Phase 10 qualification?" → Selected: "none by default; decide in Wave 0 so ci.yml is edited once" — ci.yml edits are reversible
- [Identity naming] Q: "Which registered ids?" → Selected: "descriptor 13 / model 21 / state 26 mirroring the four SI ids (research A1), widening t64_has_si_profile in the header instead of editing sealed .c bodies; depth scalars inside the RDP block, hidden table in the tail" — planner may refine; ids are one-way once pinned, so the plan must record them before first digest pin
- [Preflight] Q: "How is the Phase 9 lesson replay packaged?" → Selected: "one scripted local preflight under about 90 s cited by every plan" — script is repo-local and rewritable
- [Qualifier] Q: "New qualifier or delta on Phase 9?" → Selected: "thin delta binding only changed ids, copy-paste of reusable parts, no new dependency" — tooling only, replaceable before qualification
- [Fuzz] Q: "Which importer bounds coverage for new members?" → Selected: "extend fuzz_state_import.c and field-use audit rows for each new member" — test code only
- [Review] Q: "Review cadence?" → Selected: "per wave in scratch clones plus one final independent audit before qualification" — process choice recorded in AUTONOMY.md
- [Docs] Q: "How are K1 and depth-silent docs updated?" → Selected: "update rdp.md, vi.md, machine-contract K1 text, state-container sections and PROJECT.md in the same wave as the identity bump" — documentation edits

### Claude's Discretion
- Exact new profile id values and family tuple naming, chosen by the planner against the registered-id conventions (the public id must be stable enough for the Phase 11 consumer to name)
- Plan decomposition within the wave order (estimate 12-16 plans; wave 2 is the ballooning risk)
- Whether the optional `make -j2` boot-fixtures change is worth doing (low value; measure first)

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `core/tests/test_digest_v25.c`, the Phase 9 510-digest audit harness (SC1 byte-identity check), `fuzz_state_import.c`, `scripts/check-rdp-oracle-authority.py` (already verifies each row's `built_fixture_sha256` individually), `scripts/verification-scope.mjs`
- The 2 MiB RDP sidecar is already allocated for the AI family (t64_machine.c), so memory layout does not move

### Established Patterns
- New semantics get a new registered profile identity (rdp.md, cpu-compatibility.md, machine-contract.md); descriptor membership sites are sealed bodies re-pinned in an isolated derived refresh (09-01)
- Authored guests only; independent literals from the primary RDP documentation (rdp.md V7 table); wrong-value controls; unit-tested, authored-scope evidence tier

### Integration Points
- `t64_has_rdp_depth_profile` (core/t64_internal.h, guarded by descriptor version 10) also gates Z opcode acceptance, triangle preflight, sidecar invalidation, reset, importer zeroing (t64_state.c) and the digest `has_v21_state` block (t64_digest.c)
- `.github/workflows/ci.yml`, `scripts/check-ci-yml.sh` (CI-TIMEOUT 1..10 minutes rule and self-test), `scripts/gate.sh` (hard-coded Phase 8/9 private JUnit roots), `scripts/qualify-phase09.py`
- Fixture manifest `fixtures/manifest.json` and `PINNED_REQUEST_SHA256` (RUN-07's single manifest-moving commit)

</code_context>

<specifics>
## Specific Ideas

Full per-requirement findings, risks and file references are in `.planning/research/v1.2-fanout/lens-phase10.md` (R1-R8, adversarial pass) and `SYNTHESIS.md`; the planner must read both. Unverified items to check during planning: whether lens plan-count estimates hold once VG vectors expose real defects (budget one fix per VG); cold-cache timeout failure mode in actions/cache save [VERIFY].

</specifics>

<deferred>
## Deferred Ideas

- VID-04 differential oracle evidence stays unmapped (owner D5 decision required)
- Public API/ABI work, installed consumer and freeze candidate: Phase 11
- Host, playable title, LibFuturo evaluation and the final ABI bind: Phase 12
- Relaxing the raw-capture exclusion (rejected as a pre-freeze contract change)

</deferred>
