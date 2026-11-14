---
gsd_state_version: "1.0"
milestone: v1.0
milestone_name: Verified Headless Core
current_phase: 5
current_phase_name: RSP Low-Level Emulation
current_plan: 9
status: executing
stopped_at: Plan05-08 complete with local, sanitizer, hosted Linux/macOS and first authored stress workflow green. Continuing Plan09 authored CPU/SP/RSP ROM integration.
last_updated: "2026-11-14T10:09:02.809Z"
last_activity: 2026-11-13
last_activity_desc: Phase 4 complete, transitioned to Phase 5
state_head: eac270ea1af9372544aa1b8a6040639b281c25f6
progress:
  total_phases: 6
  completed_phases: 1
  total_plans: 64
  completed_plans: 62
  percent: 17
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-11-13)

**Core value:** Every Tendies64 support claim is backed by evidence: a named profile, a pinned build and a reproducible, rights-cleared guest result.
**Current focus:** Phase 5 — RSP Low-Level Emulation

## Current Position

Phase: 5 (RSP Low-Level Emulation) — EXECUTING
Current Plan: 9
Total Plans in Phase: 10
Status: Ready to execute
Last activity: 2026-11-13 — Phase 4 complete, transitioned to Phase 5

Progress: [██░░░░░░░░] 17%

Verified roadmap ledger: Phases1–4 are complete, including all20 Phase4 plans.
Phase5 is planned with ten plans; Plans01–08 are implemented, locally verified and hosted on both platforms.
The projection's generated percentage is advisory
and does not override the checked roadmap ledger.

## Performance Metrics

**Velocity:**
- Total plans completed: 54
- Average duration: -
- Total execution time: 0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 1 | 13 | - | - |
| 2 | 8 | - | - |
| 3 | 13 | - | - |
| 4 | 20 | - | - |

**Recent Trend:**
- Last 5 plans: -
- Trend: -

*Updated after each plan completion*
**Per-Plan Metrics:**

| Plan | Duration | Tasks | Files |
|------|----------|-------|-------|
| Phase 01 P01 | 9min | 2 tasks | 15 files |
| Phase 01 P02 | 30min | 2 tasks | 16 files |
| Phase 1 P03 | 6min | 3 tasks | 24 files |
| Phase 1 P04 | 8min | 3 tasks | 14 files |
| Phase 1 P05 | 30min | 2 tasks | 17 files |
| Phase 1 P06 | 8 min | 3 tasks | 5 files |
| Phase 1 P07 | 4 min | 2 tasks | 24 files |
| Phase 1 P08 | 14min | 3 tasks | 33 files |
| Phase 1 P09 | 11min | 3 tasks | 47 files |
| Phase 1 P10 | 25 min | 2 tasks | 7 files |
| Phase 1 P11 | 6 min | 3 tasks | 40 files |
| Phase 1 P12 | 16min | 2 tasks | 21 files |
| Phase 1 P13 | 10 min | 2 tasks | 2 files |
| Phase 02 P01 | 31 min | 3 tasks | 9 files |
| Phase 02 P02 | 40 min | 2 tasks | 11 files |
| Phase 02 P03 | 47 min | 3 tasks | 14 files |
| Phase 02 P04 | 37 min | 2 tasks | 11 files |
| Phase 02 P05 | 40 min | 2 tasks | 15 files |
| Phase 02 P06 | 40 min | 2 tasks | 15 files |
| Phase 02 P07 | 54 min | 2 tasks | 11 files |
| Phase 02 P08 | 54 min | 3 tasks | 12 files |
| Phase 03 P01 | 64min | 2 tasks | 10 files |
| Phase 03 P02 | 42min | 1 tasks | 11 files |
| Phase 03 P03 | 52min | 1 tasks | 14 files |
| Phase 3 P04 | 73min | 2 tasks | 10 files |
| Phase 3 P5 | 138min | 2 tasks | 20 files |
| Phase 3 P6 | 45 min | 1 tasks | 16 files |
| Phase 03 P07 | 47min | 1 tasks | 11 files |
| Phase 03 P08 | 27min | 1 tasks | 8 files |
| Phase 3 P09 | 58min | 2 tasks | 9 files |
| Phase 03 P10 | 41min | 1 tasks | 7 files |
| Phase 03 P11 | 87m | 2 tasks | 25 files |
| Phase 03 P12 | 80min | 2 tasks | 28 files |
| Phase 03 P13 | 173min | 3 tasks | 26 files |
| Phase 03 P13 | 169min | 3 tasks | 32 files |
| Phase 04 P01 | 36min | 2 tasks | 10 files |
| Phase 04 P14 | 30min | 2 tasks | 7 files |
| Phase 04 P02 | 35min | 2 tasks | 12 files |
| Phase 04 P03 | 52min | 2 tasks | 8 files |
| Phase 04 P04 | 50min | 2 tasks | 13 files |
| Phase 04 P05 | 26min | 2 tasks | 12 files |
| Phase 04 P06 | 67min | 2 tasks | 13 files |
| Phase 04 P07 | 95min | 2 tasks | 12 files |
| Phase 04 P08 | 57min | 2 tasks | 13 files |
| Phase 04 P09 | 63min | 2 tasks | 12 files |
| Phase 04 P10 | 85min | 2 tasks | 15 files |
| Phase 04 P11 | 89min | 2 tasks | 72 files |
| Phase 04 P17 | 60min | 2 tasks | 2 files |
| Phase 04 P12 | 58min | 2 tasks | 9 files |
| Phase 04 P13 | 58min | 2 tasks | 9 files |
| Phase 04 P15 | 108min | 2 tasks | 17 files |
| Phase 04 P18 | 78min | 2 tasks | 7 files |

## Accumulated Context

### Decisions

Decisions are logged in the PROJECT.md Key Decisions table. Recent decisions that affect current work:

- [Roadmap]: Milestone v1.0 covers Phases 1-6, ending with the full `nemu64-test` base suite classified. GSD milestone IDs are not software versions
- [Roadmap]: D1 is accepted: C17, CMake 3.25+ and Ninja; the API/ABI remains unfrozen. ADR-0002 records spike limits; Phase 2 fuzzed real admission under ASan/UBSan.
- [Roadmap]: Primary-document verification (VR4300 manual and Nintendo docs pinned by SHA) opens Phase 3, and the CPU-area check is the Phase 4 entry gate
- [Owner, 2026-11-09]: Apache-2.0 license; public repository under szTheory. These are settled, not open gates
- [Phase 01]: 01-01: t64_core declared in root CMakeLists.txt; core/ holds only the two CONTEXT A-1 files
- [Phase 01]: 01-01: canonical JSON writer copies keys into per-depth 127-byte buffers; longer keys refused
- [Phase 01]: 01-02: fixture present but not a regular file, over 64 MiB or unreadable maps to harness-error/usage (no dedicated detail in the closed enum)
- [Phase 01]: 01-02: runner targets add _DARWIN_C_SOURCE on Apple because _POSIX_C_SOURCE=200809L hides O_NOFOLLOW and mkdtemp (core is untouched)
- [Phase 01]: 01-02: synthetic verdict grammar is PASS lines starting 'PASS ', FAIL lines starting 'FAIL ', and a verdict line exactly 'Done!' (synthetic-only)
- [Phase 1]: 01-03: Layer-1 lint skips name checks inside attribute parentheses; selftest rejects meta rules without a dedicated control; allowlist malformed row is a tool error
- [Phase 1]: 01-04: ADR-0002 stays proposed (PROPOSED_OK exemption in check-docs.sh, removed by plan 01-08); DOC-CORE-BEFORE-ADR is path-based with no content marker
- [Phase 1]: 01-04: check-ci-yml.sh adds a --gate option for the CI-GATE-EXEC mutation; CI-TIMEOUT accepts 1 to 10 minutes
- [Phase 1]: 01-05: JSON parser rejects object keys over 127 bytes (string-length) because the canonical writer cannot emit them; depth limit clamped to 64
- [Phase 1]: 01-05: run-expect.sh treats only --ext-matching files as seeds and matches rule_id ok as a whole word
- [Phase 1]: 01-06: allowlisted digests waive content classes only; R-DENIED-EXT is never waived, and zero-scanned results exit 2
- [Phase 1]: 01-06: history blob names come from rev-list --objects plus the raw log, since rev-list names a shared blob once; gitlinks read from raw-log mode 160000
- [Phase 1]: 01-06: added P-SHALLOW to pii-scan (unlisted rule id) so a shallow clone cannot pass a vacuous history scan; ROADMAP SC4 artifact leg covered by seeded staging-rom-magic and staging-nul cases
- [Phase 1]: 01-07: result consumer checks schema, then schema_version, then exact key sets, and checks canonical bytes last; S-DUPLICATE precedes S-ORDER and S-INVENTORY; only S-TRUNCATED and S-INVENTORY echo a harness-error detail line
- [Phase 1]: 01-07: identity prints nothing unless every line validated and compares no inventory (run suite first); profile_id/build_id restricted to [A-Za-z0-9._+-]{1,128}
- [Phase 1]: D1 decided by the pre-registered Rust spike: stay with C17 (no Rule (a) or Rule (b) trigger); ADR-0002 accepted, check-docs requires all of ADR 0001-0007 accepted
- [Phase 1]: Spike Rule (a) reads 'both OSes' as a conjunction: a class triggers only if the C gate lint exits 0 on host and Linux container and the Rust twin exits 1
- [Phase 1]: 01-09: manifest forbidden shape lists sha256/size/source/build/expected/path as accept-anything members so M-FORBIDDEN-DIGEST reports them; non-forbidden shape allows optional forbidden_reason and M-FORBIDDEN-REASON fires when either right is forbidden
- [Phase 1]: 01-09: a manifest with any finding makes rights-scan.sh exit 2 and emits no allowlist rows; --root defaults to the current directory; the CTest wrapper builds its argv label at run time because LastTest.log echoes the command line
- [Phase 1]: 01-10: MIT text for the MADR record comes from LICENSE.MIT at tag 4.0.0 (LICENSE holds only the expression); record pins the peeled commit 2475fe19..., maintenance_owner szTheory
- [Phase 1]: 01-10: provenance detector counts a tag only at line start (optional comment leader) within the first 20 lines; selftest asserts exact rule-id sets (76 cases, 16 ids)
- [Phase 1]: 01-11: host-misbehavior behaviours are end directives (hang, abort, segv, exit0-no-result, exit7); logrep stored as one repeated event; matrix columns runner_flags and log_limit appended after exit
- [Phase 1]: 01-11: no supervisor or classifier change was needed; all 19 controls matched the locked precedence table and goldens derived independently with printf and shasum
- [Phase 1]: 01-12: claims gate is POSIX sh + awk; evidence must pass t64-result-check identity AND end with a t64.suite record; synthetic evidence reports only C-EVIDENCE-SYNTHETIC
- [Phase 1]: 01-12: oracle_kind/oracle_revision empty is C-ORACLE; conditional keys on the wrong tier are C-FIELD-UNKNOWN; selftest (49 cases) proves rules the 17 seeds cannot reach
- [Phase 1]: 01-13: only the first push triggers a hosted run; log-only commits carry [skip ci], so every completed run has a Runs row
- [Phase 1]: 01-13: requiring gate-linux and gate-macos as status checks on main left as an owner-visible item (F-007); fork-PR approval set to all_external_contributors

- [Phase 2]: Machine skeleton verified on both hosted lanes; native model evidence only. Three non-authoritative judgment flags retained under autonomous soft-gate policy; no violation or accepted risk.
- [Phase 2]: CORE-02 remains partial; all-device integration is assigned to Phase 9.
- [Phase 03]: Admit only the exact byte-pinned Unlicense libdragon IPL3 asset and bind its embedded payload separately.
- [Phase 03]: Treat warm entry state as a deterministic software preset, with no authentic IPL2 handoff or hardware-accuracy claim.
- [Phase 03]: Decode only the verified IPL3 entry ANDI and stop explicitly on unsupported execution.
- [Phase 03]: Keep the pinned 4096-byte source asset, 4032-byte executable payload and authored ROM as distinct identities, with separate rights records.
- [Phase 03]: Set header offset 0x10 to the checked nonzero 184-byte flat app length expected by loader_compat; retain the one-MiB whole-cartridge ceiling.
- [Phase 03]: Pin authored source to a commit containing the files and exact content hash; keep per-host tool binary hashes in ignored local build metadata.
- [Phase 03]: Keep prepared-RDRAM descriptor 2 and state version 6 as explicit software policy, not hardware handoff evidence.
- [Phase 03]: Keep computed per-host tool hashes in ignored receipts; remove fixed Darwin hashes that vary by build path and toolchain.
- [Phase 03]: Install only zlib1g-dev in hosted Ubuntu CI behind explicit GitHub Actions/Linux guards; local gates do not provision packages.
- [Phase 3]: Count advances once per successful instruction retirement as deterministic software policy, not hardware cycle timing.
- [Phase 3]: CACHE behavior is a coherent-functional warm subset over pinned inventory candidates; no general timing or cache-fidelity claim.
- [Phase 3]: Update actively hashed CPU/cache policy values as supported behavior changes; the digest distinguishes identity without a profile ID or serialization version bump.
- [Phase 3]: Warm-device policy v3 binds only the enumerated DMEM and IMEM zero-source tuples, fixed HALT status, and exact reset commands as prepared software behavior.
- [Phase 3]: CACHE operations 0x08, 0x09, 0x11, and 0x15 use the coherent-functional warm subset without cache-line timing or RDRAM access.
- [Phase 3]: The authored hello first output store uses KUSEG 0x13ff0000; plan 03-06 owns the fixture-address contract, and virtual-address modes stay bounded.
- [Phase 3]: Model the pinned libdragon magic register as writable per-instance storage with readback, not as a fixed hardware value.
- [Phase 3]: Use warm state serialization version 7 for ISViewer state; preserve v6 state bytes and legacy synthetic identities.
- [Phase 3]: Preserve aligned CPU halfword loads and stores as one width-2 bus transaction.
- [Phase 03]: Keep observer drain cursor outside canonical v7 guest identity; pending input tuples are already represented in the serialized queue.
- [Phase 03]: Represent port-0 input as a scheduled trace-only event with signed 8-bit axes; do not update SI/controller state.
- [Phase 03]: Preserve the existing ISViewer inventory registration and baseline rather than duplicating it.
- [Phase 03]: Keep schema v1 closed and canonical, with uint64 ticks as decimal strings and equal-tick array order preserved.
- [Phase 03]: Queue validated input schedules only on fresh owned machines; traces establish scheduling time, not SI/controller consumption.
- [Phase 03]: Pin fixture source revision to the commit containing the exact four hashed inputs while preserving all seven ROM identities.
- [Phase 3]: Keep --real refused until the real producer is implemented in 03-11; this helper is preparation evidence, not actual CPU-runner proof.
- [Phase 3]: Keep raw schedule file length/SHA distinct from the canonical parsed schedule SHA.
- [Phase 3]: Limit legacy O_NONBLOCK hardening to avoiding FIFO open stalls; the synthetic original-path race remains open.
- [Phase 03]: 03-10: use strict real t64.result v2 while keeping v1 and native model observations isolated.
- [Phase 03]: 03-10: treat unit-authored reports as contract evidence, not proof of guest execution.
- [Phase 03]: 03-10: keep RUN-01, RUN-03 and BOOT-03 pending until phase-level guest evidence exists.
- [Phase 03]: Qualify guest completion only when the pinned fixture reaches its authored PC/opcode stop; arbitrary BREAK or transcript text is insufficient.
- [Phase 03]: On postlaunch untrusted harness errors, retain supervisor-known identities and independently verified input/capture facts; null state requires zero trusted progress and no milestones.
- [Phase 03]: Treat the full IPL3 asset SHA as pinned provenance, while runtime independently hashes only the embedded payload and normalized authored ROM present in the fixture.
- [Phase 03]: Only the seven generated rights-cleared named variants enter the real guest path; unknown fixture identities remain refused and only the original positive fixture can pass.
- [Phase 03]: Local determinism establishes repeatability for this pinned warm profile and authored inputs on this host; hosted Linux/GCC and macOS/Clang equality remains Plan 13 evidence.
- [Phase 03]: The v8 observer records exact attempted flush bytes with checked arithmetic separately from the retained 4096-byte capture; an inexact saturated value is never emitted as an exact result total.

### Pending Todos

None yet.

### Blockers/Concerns

- D2 (owner stop): `nemu64-test` rights are not documented. Until they are, it runs local-only with no uploads or public results (Phases 4-6). Owner decides after a per-asset rights audit
- D4: copy allowlist beyond ISC/MIT/BSD-3/Unlicense (owner). Default: keep the list as written
- D5: Angrylion-Plus oracle use (owner/legal). Blocks Phase 7 conformance claims only
- D6: release channel and versioning (owner). Blocks releases only
- D7: name not trademark-cleared. No branding
- Phase4 primary-document checks and native model tests are complete; physical hardware qualification remains absent

## Deferred Items

Items acknowledged and deferred at milestone close, most recent first:

| Category | Item | Status | Deferred At | Milestone |
|----------|------|--------|-------------|-----------|
| *(none)* | | | | |

## Session Continuity

Last session: 2026-11-14T10:09:02.555Z
Stopped at: Plan05-08 complete with local, sanitizer, hosted Linux/macOS and first authored stress workflow green. Continuing Plan09 authored CPU/SP/RSP ROM integration.
Resume file: .planning/phases/05-rsp-low-level-emulation/05-09-PLAN.md
