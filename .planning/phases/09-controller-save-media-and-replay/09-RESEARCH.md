# Phase 9: Controller, Save Media and Replay - Research

**Researched:** 2026-11-19
**Domain:** deterministic C17 emulator core (SI/PIF/joybus, save media, state, scheduler) plus this repository's POSIX host runner and sh/python3 qualification harness
**Confidence:** HIGH for in-repo facts (read with the Read tool or executed against the clean tree at the start of this session); MEDIUM/LOW for community hardware facts, which are carried unchanged from the labels in `09-DISCUSSION-PROPOSAL.md` and were not re-fetched.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
`09-CONTEXT.md` `## Implementation Decisions` is authoritative and is not re-copied (about 30 KB; a second copy would be a second source that can drift). The planner MUST honor, by identifier: D-01 Profile family; D-02 Scheduler order stays `tick-sequence-fifo-v1`; D-03 Host input; D-04 Quantum; D-05 Canonical payload is the digest preimage (new profile only); D-06 Freezes (F0, FH, F1, FR, F2); D-07 Closed `(device, kind)` handler table; D-08 HLE PIF, no PIF ROM; D-09 SI registers and DMA; D-10 PIF command byte; D-11 Joybus parser; D-12 Channels and presence; D-13 Controller responses; D-14 Timing; D-15 Input schema; D-16 `pad-poll` diagnostic; D-44 Profile-gated CPU store preflight; D-17 Selection is the profile; D-18 EEPROM commands; D-19 Block rule; D-20 Busy window; D-21 SRAM; D-22 Channel 4 without EEPROM; D-23 Unknown, unsupported, deferred requests; D-24 API and formats; D-25 Replacement semantics; D-26 Persistence is host-owned; D-27 Save in digest/state; D-36 Deliverables; D-37 Quantum evidence; D-28 Container; D-29 Exclusions and observers; D-30 Import (validate then apply, no allocation); D-45 Generated field-use audit; D-31 Binding; D-32 Proof ladder; D-33 State privacy; D-34 Replay artifact; D-35 Runner surface; D-38 Harness mechanism; D-39 Fixtures; D-40 Cheap-first order and hosted budget; D-41 Importer security rules; D-42 Claims; D-43 Profile id scheme; D-46 Review before freeze; S-01..S-11 (eleven strictly sequential plans); the "Claim tiers and wording boundaries" block; the "Hard-won process rules" block.

Section "Corrections and Contradictions" below lists concrete verified facts that a plan must reconcile with a locked statement. They are inputs to the planner's reconciliation under the owner's "reopen only for a concrete contradiction or failure" rule; each carries a recommended resolution and an owning plan. None changes a decision's intent.

### Claude's Discretion
Internal representations, final profile-id and label text within the 63-character limit and the D-43 scheme, domain string text, section id numbering, exact test names and registration, container field order details, state-boundary builder choice per D-32 (subject to the "reached" assertion), flag names (provisional), and the number of plan tasks. Reopen a locked decision only for a concrete contradiction or failing witness.

### Deferred Ideas (OUT OF SCOPE)
- Class-rank scheduler order, Q > 1 RSP bursts (v2 performance milestone).
- FlashRAM (EXP-04), Controller/Rumble/Transfer Pak (EXP-05), RTC (EXP-06), 64DD (EXP-07).
- Ports 1-3, per-channel sequential joybus sampling, PIF challenge, direct PIF access (EXP-03 and later).
- Cross-build state import (labeled diagnostic mode), state-anchored replay, binary replay schema, rewind and save-state UI (PLAY-01).
- Optional hardware measurement of EEPROM/SI timing (an AUTONOMY section 4 stop when pursued), and a second-source check of button bit positions.
- Larger raw capture beyond what the artifact-dir mechanism incidentally allows.
- API/ABI shapes and the installed consumer (Phase 10); host player and playable homebrew (Phase 11).
- Docs-only CI lane (F-099) stays declined unless the owner asks.

### Non-goals (locked, verbatim)
- FlashRAM, Controller Pak, Rumble Pak, Transfer Pak, RTC, 64DD: refusal with defined postconditions only; no stubs that appear to work; kinds reserved.
- Banked or larger SRAM, SRAM mirroring, other-emulator save containers, header or database save detection, automatic persistence, any real save as a fixture.
- Ports 1-3 hardware behavior, hot-plug, stick shaping, RST/recentering, per-channel sequential sampling, direct CPU PIF access, WR4B/RD4B, CIC challenge, PIF ROM execution, other joybus peripherals, physical controller mapping (Phase 11).
- Class-rank scheduler policy and Q > 1 bursts; cross-build or cross-version state import; state-anchored replay; binary replay schema; compression; rewind and save-state UI.
- API/ABI freeze, installed consumer, host player, new dependency, new CI job, production thread; hardware, oracle or differential claims; VID-04; release (D6); naming (D7).
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| SAVE-01 | SI and the PIF state machine parse joybus and deliver the standard controller from timed logical input | Corrections K3, K4, K7, K16; patterns P2, P5, P6; plans 09-02/09-03; validation rows SAVE-01 |
| SAVE-02 | EEPROM (4 and 16 Kbit) with busy window and block rules, and SRAM, as explicitly selected save devices | Corrections K1, K2, K3; patterns P1, P3; plan 09-04; validation rows SAVE-02/04 |
| SAVE-03 | Save media imported and exported as bytes with endianness handling; refusal leaves the digest unchanged | Pattern P7; plan 09-05; validation rows SAVE-03 |
| SAVE-04 | Unknown or unsupported save device requests refused with defined postconditions | Plans 09-04/09-05; validation rows SAVE-02/04 |
| CORE-02 | Device activity ordered by the scheduler; CPU-to-RSP quantum in profile identity | Corrections K5, K10, K14, K16; pattern P2; plans 09-01/09-03/09-06 |
| CORE-04 | Versioned machine state export/import with canonical digest; round trip exact | Corrections K1, K8; pattern P4; plans 09-01/09-08/09-09 |
| CORE-05 | Timed input recorded and replayed as an artifact bound to profile, build and content hash | Corrections K6, K7; pattern P8; plan 09-07 |
</phase_requirements>

## Summary

The decision research is complete; this document is the planning-facing remainder. Every in-repo fact a plan depends on was re-read or executed. Most locked statements hold. Sixteen concrete facts do not match a locked statement or add a hidden dependency (section "Corrections and Contradictions"). The four that change plan content most: (K3) the real runner refuses any guest bound above 600,000 ticks and authored user code first retires at about tick 538,800, so a real-runner guest has about 61,000 ticks and can never observe the 375,000-tick EEPROM busy window expire; (K4) the runner's `pass` outcome and the strict result checker are hard-wired to one 30-byte transcript, which conflicts with D-16's per-poll ISViewer lines unless the pin becomes per-fixture; (K16) the scheduler API cannot reserve two events and then commit both, which D-09's "reserve all events before busy is visible" needs; (K1) the shipped Phase 8 AI profile has RDP depth inactive through a `descriptor_version == 10u` guard, which the v25 strip-and-compare differential must treat explicitly.

The membership sweep is smaller than feared once its roots are known: one predicate chain in `core/t64_internal.h` propagates to about twenty callers, plus eight storage-ladder sites in `core/t64_machine.c`, the AI-device gates (seven in `core/t64_ai.c`, one in `core/t64_bus.c`), the digest, and one site the proposal did not list, `t64_isviewer_active` in `core/t64_isviewer.c` (without it the new ids have no ISViewer window and pad-poll prints nothing).

**Primary recommendation:** Start 09-01 by capturing a golden predicate matrix for every registered profile id (the probe pattern in K1) before editing any ladder; add the new family through one root predicate plus an explicit `t64_has_ai_device` helper; resolve K3, K4 and K16 as the first tasks of 09-02/09-03 before any fixture is authored, because each changes the runner and scheduler seams every later plan builds on.

## Corrections and Contradictions to Locked Context

Each row: locked statement, verified fact, consequence, recommended handling. Tags follow the provenance rules.

| ID | Locked statement | Verified fact | Recommended handling (owner plan) |
|----|------------------|---------------|-----------------------------------|
| K1 | D-01/D-05: family inherits Phase 8 devices; v25 payload equals the v24 stream after stripping | `t64_has_rdp_depth_profile` is `return machine != NULL && machine->profile.descriptor_version == 10u &&` then the tuple list ending `t64_has_vi_filter_v9_profile(machine));` [VERIFIED: core/t64_internal.h:1028-1035]. A descriptor 11 machine therefore has depth inactive although create still allocates depth storage for it. Probe run this session against the Release core library printed `...v9-ai-v1  desc=11 model=19 state=24  has_ai=1 has_v9=1 has_rdp_vi=1 has_depth=0 has_scanout=1` while the three descriptor 10 ids probed (rdp-depth-v7, rdp-vi-v8, rdp-vi-filter-v9) printed `has_depth=1` [VERIFIED: executed probe]. The v24 digest consequently omits the depth block (`has_v21_state = t64_has_rdp_depth_profile(machine)`, [VERIFIED: core/t64_digest.c:228]) | Preserve the Phase 8 behavior for descriptor 12 (depth inactive) so the strip-and-compare differential stays exact; do not "fix" it inside Phase 9. Record the Phase 8 observation as a STATE follow-up; no Phase 9 requirement needs depth and no Phase 9 claim may mention it. Capture the golden predicate matrix first (09-01) |
| K2 | D-01 lists the membership sites as validator, writer, `t64_has_*` chain, create-time ladders, runner checks | `t64_isviewer_active` is a per-id plus per-tuple allow-list; its last arm is `ai_profile = machine->profile.descriptor_version == 11u &&` ... `memcmp(machine->profile.profile_id, ai_profile_id,` [VERIFIED: core/t64_isviewer.c:118-122] and the function body spans core/t64_isviewer.c:19-135. `core/t64_isviewer.c` holds 22 model/state and 12 descriptor comparisons [VERIFIED: grep counts, production code] | Add the four ids to `t64_isviewer_active` in 09-01 and test it (a pad-poll style authored ROM must be able to print through the window); fold into `unit.core.profile.v12-identity` |
| K3 | D-16/D-20/D-39: pad-poll takes several polls; `eeprom-rw` shows the busy window | The real runner refuses `--guest-ticks` above 600,000 in two places: `} else if (!bad && a->bound > UINT64_C(600000)) {` [VERIFIED: runner/t64_supervisor.c:606] and `guest_ticks <= UINT64_C(600000)) {` [VERIFIED: runner/t64_real.c:1591]. Measured: first user-code retirement at tick 538,831 for the audio ROM and 538,607 for hello under the AI profile, so a real-runner guest has about 61,169 ticks [VERIFIED: executed probe]. Arithmetic from D-14 [CITED: 09-CONTEXT.md D-14]: a 1-byte-tx/4-byte-rx channel is 2000*1+188+438+2000*4+250 = 10,876 ticks; EEPROM write handshake (tx 10, rx 1) 22,876; busy window 375,000 [VERIFIED: shell arithmetic]. Simulating 539k ticks costs about 0.06 s native (Release) [VERIFIED: executed probe] | Raise the real-mode cap to a named constant (recommended 4,000,000 ticks = 64 ms guest time) in both sites in 09-02, with a control that a value above the new cap is refused and the default bound (`REAL_GUEST_TICKS` 530,500) is unchanged. Alternative if the owner prefers a small blast radius: keep the cap, test the busy window only in-process (`unit.core.save.eeprom-busy-triple`) and word the SAVE-02 claim so it does not claim in-guest observation of window expiry |
| K4 | D-16: pad-poll prints one ASCII line per poll through ISViewer | The runner reports `pass` only when `fixture_identity->pass_eligible && profile_observations_valid(...) && ... output_length == sizeof expected_log - 1u && memcmp(output, expected_log, ...)` [VERIFIED: runner/t64_real.c:1371-1377]; the log is the compile-time 30-byte hello line (`T64_REAL_EXPECTED_LOG_BYTES 30u`) and the strict checker pins the same SHA-256 (`expected_log_sha256`) [VERIFIED: runner/t64_result_check.c:134-135, grep at 845-856 and 1182]. Any other transcript is `guest-fail` / `wrong-transcript` | Decide first in 09-02. Recommended: add `expected_log_bytes` and `expected_log_sha256` to `t64_real_fixture` (struct at runner/t64_real_fixture.h:11-19) with 0/NULL meaning the legacy hello pin; read the pin from the row in `t64_real.c` and `t64_result_check.c` (the checker already links `t64_real_fixture.c`). The harness authors the expected transcript literal independently (Python oracle over the scripted input and policy literals), so a wrong-value ROM variant yields `wrong-transcript` at the same comparator. Fallback (Phase 8 precedent): keep the hello transcript for `pass` and export readouts through a bounded observation record |
| K5 | D-36 required mutant: "handler reads `machine->tick` instead of `event->tick`" | In dispatch the machine tick is set from the event before the handler runs: `queue_pop_front(machine);` `machine->tick = event.tick;` ... `status = event.handler(machine, &event);` [VERIFIED: core/t64_scheduler.c:349-352]; `event.handler(` is called nowhere else in core [VERIFIED: grep]. The mutant is equivalent for every dispatched handler and cannot fail a test | Replace with non-equivalent mutants (09-06 `unit.core.sched.required-mutants`): input latch `visible_from` computed from `machine->tick` at the `t64_machine_queue_input` call instead of the stamped tick; EEPROM `busy_until` computed from the RD64B store tick instead of execute tick E; SI completion tick derived from the per-advance budget; sequence taken at commit instead of reserve; promote/sample order reversed. Each must build, reach its comparator and fail a named test |
| K6 | D-35: each plan adds its supervisor forwarding row | The child argv is a fixed array: `char *child_args[24];` [VERIFIED: runner/t64_supervisor.c:1498] filled with `child_args[child_argc++]` and no bound check. Present maximum is 20 slots with the test advance step; the 09-02 artifact pair brings it to 24 and every later flag overflows the stack array | In 09-02 enlarge the array, route every push through one checked helper, add a `_Static_assert` on the maximum option set, and add a control that exercises the maximum option set under ASan |
| K7 | D-12/D-15: "schema carries ports 0-3" | The shared `t64.input` v1 parser accepts only port 0: `!node_int(port_node, 0, 0, &value)) {` and `event->port = 0u;` [VERIFIED: runner/t64_input.c:101-107]; `t64_machine_queue_input` refuses `port != 0u` for every profile today [VERIFIED: core/t64_observe.c:103] | Keep `t64.input` v1 unchanged (it is the independently authored original-run script format for CORE-05). Carry ports 0-3 only in `t64.replay` and in the new-profile core API path (ports 1-3 refused by the connected mask, state unchanged); legacy profiles keep refusing `port != 0` |
| K8 | D-30: invariant summary "on the stack" (queue up to 256 entries) | Core builds with `-Wframe-larger-than=2048` [VERIFIED: core/CMakeLists.txt:113] and `T64_WERROR=ON` in the gate preset [VERIFIED: CMakePresets.json gate]; `sizeof(t64_event)` is 48 and `sizeof(t64_machine)` 78,456 [VERIFIED: executed probe]. A 256-entry (tick, sequence) summary is 4,096 bytes and breaks the frame limit | Keep per-function frames under 2,048 bytes: use bitsets (256 bits = 32 bytes), streaming checks, or a scratch field inside `t64_machine` sized at create time; do not rely on a large stack array |
| K9 | D-38: the synthetic flood child proves the artifact-pair FSIZE rule | The generic (non-real) child limit is `rl.rlim_cur = (rlim_t)(a.log_limit + FSIZE_SLACK);` [VERIFIED: runner/t64_supervisor.c:1880]; the real-mode child limit is `limit.rlim_cur = (rlim_t)(T64_REAL_RESULT_MAX_BYTES + FSIZE_SLACK);` [VERIFIED: runner/t64_supervisor.c:1534]. The synthetic child never runs under the real-mode line | Exercise the pair through the real-mode test twin (`t64-runner-test`, `T64_REAL_RUNNER_TESTING`) with a new test fault that writes N bytes to the artifact path: absent option 69,633 bytes dies (SIGXFSZ), present option the declared maximum succeeds and one more byte dies. Rename the test `harness.fsize-artifact-real-fault` |
| K10 | D-37: quantum "present in result identity records (check, add if missing)" | Result records carry `profile_id` and `profile_digest` only; no `quantum` key exists in `runner/` or `docs/spec/result-protocol.md` [VERIFIED: grep, zero matches]. The profile hash writer includes it: `profile_writer_u32(writer, profile->synchronization_quantum);` [VERIFIED: core/t64_machine.c:1649] | Do not add a key to the closed `t64.result` v2. State in the spec that the quantum is bound by `profile_digest`, and pin it with the hash-level control (existing contrast at core/tests/test_profile.c:450-470 plus a new explicit control) |
| K11 | D-42: claim skeleton checked in 09-02 | The claim checker requires `pass` plus the six other counts to equal `total` and a byte-exact generated `statement` [VERIFIED: docs/claims/TEMPLATE.md:52-56]; a `t64-claim` fence with placeholder counts fails `gov.claims.repo` | Check in the skeleton as prose plus `t64-claim-example` fences (documentation blocks are not checked); create real `t64-claim` blocks only in 09-11 |
| K12 | D-38/D-39: one real-runner admission and tick per new id; five ROMs | `t64_real_fixture_find_bytes` returns the first row whose size and SHA-256 match [VERIFIED: runner/t64_real_fixture.c, loop over `fixtures[]`], and each row binds one `profile_id` plus a literal `profile_sha256` that the supervisor recomputes via `t64_profile_lookup` and compares [VERIFIED: grep of runner/t64_supervisor.c:844-889]. So ROM bytes bind exactly one profile id, the four profile hashes must be final before the first row can pass the supervisor, and 09-02 has no D-39 ROM for the `-e4k`, `-e16k` or `-s32k` ids | Declare both new labels (`si_policy`, `save_policy`) at their final text in 09-01 so profile hashes are stable from then; add a small print helper (a test mode that prints the four hashes) and re-pin rows from it. In 09-02 add one tiny authored admit-and-tick ROM source built four times with a one-byte `--defsym` variant (`fixtures/phase09/admit.S`, rows `phase09-admit-*`); later plans add their real ROMs |
| K13 | D-40: about +25 s macOS from new always-on work | The inventory selftest builds a removal control per expected name for both presets; F-100 measured it at 34 s on macOS and 12 s on Linux [VERIFIED: docs/ci-reviews/LOG.md F-100 row]; the baseline is 181 names (counted from scripts/check-test-inventory.sh variables). The proposal's plan lists name 78 new tests, which scales that selftest by about 43 percent, roughly +15 s macOS [INF] | Prefer `--suite` groupings like Phase 8 (21 names) and target at most about 45 new CTest names; keep case-level granularity in the JSON case inventory |
| K14 | D-36: matrix includes "a span longer than one Count wrap" | `t64_cp0_synchronize_guest_time` (core/t64_cp0.c:558) computes `count_increments = elapsed_ticks / UINT64_C(2) +` ... by division [VERIFIED: core/t64_cp0.c:588], so a direct whole-versus-split test of a span above 2^33 ticks is O(1). Through the public `t64_machine_advance` a running CPU cannot cover 2^33 ticks cheaply, and a stopped CPU returns before time moves (`if (machine->cpu.stopped) {` ... in `advance_time_bound`, core/t64_scheduler.c:390-394) | Build the Count-wrap case with the direct-state pattern of `core/tests/test_cpu_time.c` (zeroed machine, set CP0 state, call the synchronize function whole versus split); do not try to reach it through `advance` |
| K15 | S-02/S-08: new runner modules and flags | `T64_RUNNER_BUILD_INPUTS` lists the runner sources that define the runner build id and the same sources appear in `t64-runner-real` and `t64-runner-test` [VERIFIED: runner/CMakeLists.txt:12-18, 293-304]; `t64_supervisor.c` is not in that list | Every new runner module must be added to `T64_RUNNER_BUILD_INPUTS` and to both runner targets (and the supervisor targets if linked there) or edits will not change the runner identity |
| K16 | D-09/D-44: all events reserved before busy is visible (RD64B has two events) | A reservation is invalidated by any commit: `reservation->expected_queue_count != machine->queue_count \|\|` and `reservation->event.sequence != machine->next_sequence \|\|` [VERIFIED: core/t64_scheduler.c:157-161]; `t64_schedule_cancel` removes an event but does not restore `next_sequence` [VERIFIED: core/t64_scheduler.c:192-224]. Two reservations cannot both be committed, and a failed second reserve after a committed first cannot be rolled back without changing the digest | Add an internal scheduler capacity check (queue room for n events, sequence headroom, tick overflow) used before the first reserve of a multi-event start, then reserve and commit sequentially (cannot fail after the check). Reuse its logic in `cpu_preflight_successor` (core/t64_cpu.c:1513-1550). Needed for host-initiated RD64B (`t64_machine_bus_write`) as well as the CPU path; test in `unit.core.si.atomic-queue-full` |

## Architectural Responsibility Map

| Capability | Primary tier | Secondary tier | Rationale |
|------------|--------------|----------------|-----------|
| Profile family, identity, digest sink and v25 payload | Core (`core/`) | Harness (pins) | Deterministic identity; the core owns no files, clocks or allocation outside create/admit/replace/destroy (AUTONOMY, AGENTS.md) |
| SI/PIF DMA, joybus parser, controller, input latch | Core | Runner (feeds timed input) | Guest-visible behavior on guest time only |
| EEPROM, SRAM, save import/export bytes | Core | Runner (files) | Core exchanges bytes through the API; D-26 persistence is host-owned |
| Scheduler order, quantum, resource-limit stops | Core | Tests | One guest-time scheduler orders all events |
| State export/import (container, readers) | Core | Runner (publication, fd input) | Core validates and applies bytes; runner owns files and no-clobber publication |
| Replay record/play, `t64.replay`, divergence report | Runner (`runner/`) | Core (queue_input only) | D-34: the core knows nothing of replays |
| Artifact publisher, FSIZE pair, supervisor forwarding | Runner/supervisor | Harness (controls) | Host files and limits |
| Fixture ROMs, manifest, build scripts | Harness (`fixtures/`, `scripts/`) | Runner (table pins) | Authored-only rights gate before guest use |
| Qualification, scanners, inventory, `ci.yml` | Harness/CI | Docs | Bound-source set; frozen at FH |
| Specs, claims, evidence | Docs (`docs/`) | Harness (claim checker) | Tiered claims per `docs/claims/TEMPLATE.md` |

## Standard Stack

No external package is added in this phase (AGENTS.md small flat dependency tree; D-39/Non-goals forbid a new dependency). The "stack" is the in-repo building blocks every plan must reuse.

### Core
| Component | Location | Purpose | Why standard |
|-----------|----------|---------|--------------|
| Scheduler | `core/t64_scheduler.c` | `(tick, sequence)` order, reserve/commit/cancel, advance loops | Locked D-02; every device uses it |
| PI DMA model | `core/t64_pi.c` | Template for SI and SRAM DMA (reserve-then-commit, token cancel, sticky error, atomic effect at the completion event) | D-09/D-21 mirror it |
| Digest walker | `core/t64_digest.c` | One function, per-version domains, `hash_u32`/`hash_u64`/`hash_event` | D-05 adds the sink seam here |
| SHA-256 | `tools/common/t64_sha256.[ch]` | Digest, container hashes, replay identity | Already core-lint admitted; never hand-roll |
| Test macros | `tests/t64_test.h` | `T64_CHECK`, `T64_CHECK_INT`, `T64_CHECK_STR`, `T64_TEST_FINISH` (zero checks is a failure) | Every owning test |

### Supporting
| Component | Location | Purpose | When to use |
|-----------|----------|---------|-------------|
| Strict JSON parser and canonical writer | `tools/common/t64_json.[ch]`, `t64_jsonw.[ch]`, `t64_json_canon.c` | Closed, bounded, duplicate-key-refusing; integers within 2^53; u64 travels as decimal string | `t64.replay` parse and canonical bytes. Default limits are 1 MiB input and 4,096 nodes (`T64_JSON_DEFAULT_MAX_NODES 4096u`, [VERIFIED: tools/common/t64_json.h:33]); replay needs caller-raised limits; node size is 72 bytes so 65,536 ops cost tens of MiB [INF] |
| Input schedule parser | `runner/t64_input.[ch]`, `t64_verified_input.[ch]` | 128 events / 65,536 bytes, verified snapshot fds | Original-run script; save/state/replay input snapshots |
| Raw-capture transaction | `runner/t64_raw_capture.c` | Parent-fd walk, 0700 staging, 0600 files, no-replace rename | Copy-with-provenance pattern for the second publisher; do not modify (CR-01 locus) |
| MIPS Binutils 2.45 | `build/tools/install` plus receipt `build/tools/qualified-host-tools.json` | Authored ROM assembly | New fixture scripts copy `scripts/build-audio-fixture.sh`; never touch `scripts/build-boot-fixtures.sh` (cache key, F-100) |
| Case-inventory checker | `scripts/check-audio-cases.py` + `docs/spec/audio-cases.json` | Exact `--list-cases` versus JSON inventory, suite runner | Copy to `scripts/check-phase09-cases.py` + `docs/spec/phase09-cases.json` (the Phase 8 copy is bound by `qualify-audio.py`; do not parameterize it) |
| GCC13 route | `_gcc13_closure_command` in `scripts/qualify-rdp-vi.py` (image `sha256:ad13696c...6786`, [VERIFIED: scripts/qualify-audio.py:1000]) | Read-only, network-less `-Werror` compile of changed C units | `qualify-phase09.py` imports it as `qualify-audio.py` does; core flags include `-Wframe-larger-than=2048` |

### Alternatives Considered
| Instead of | Could use | Tradeoff |
|------------|-----------|----------|
| New `scripts/check-phase09-cases.py` | Add `--inventory` to the audio checker | Cheaper, but edits a Phase 8 bound source and costs a requalification cycle |
| Per-fixture expected-transcript pin (K4) | Hello transcript plus an observation FIFO | Observation avoids touching `t64_result_check.c` but adds a supervisor flag and schema |

**Installation:** none. **Version verification (executed, host this session):** cc Apple clang 21.0.0, cmake 4.4.3, ninja 1.13.2, Python 3.14.4, jq 1.7.1, docker 29.5.2 with `gcc:13`, `gcc:14` and `gcc:15` images present, act 0.2.87, gh 2.101.0 [VERIFIED: executed commands].

## Package Legitimacy Audit

Not applicable: no external package is installed by this phase. The package-legitimacy seam was therefore not run. Packages removed due to SLOP: none. Packages flagged SUS: none.

## Architecture Patterns

### System Architecture Diagram

```
host script / t64.input v1 / t64.replay
        |  (runner: parse, verify snapshot, bound checks)
        v
  runner (t64_real.c) --queue_input(tick, port, buttons, x, y)--> core input latch (per port: current, next/visible_from)
        |                                                              |
        | advance(T64_STOP_GUEST_TICK, absolute)                       | sample at SI execute tick E
        v                                                              v
  scheduler (tick, sequence) --dispatch--> CPU step -- store to SI window --> SI start (capacity check, reserve+commit x2)
        |                                      |                                   |
        |                                      +--> PI start (SRAM both ways)      +--> SI execute @E: parse PIF RAM,
        |                                                                               controller/EEPROM responders,
        |                                                                               latch input, set busy_until
        +--> SI complete @C: RD64B copy to RDRAM, MI SI pending
        |
        v
  digest walker --(hash sink)--> canonical digest        --(buffer sink)--> state export container (sections + SHA-256)
        ^                                                                     |
        | state import: validate (no mutation) then one infallible apply      v
        +----------- runner: publisher (no-clobber), replay recorder/driver, save import via verified fd
```

### Recommended Project Structure (new or changed)
```
core/
  t64_si.c            # SI regs, DMA start/execute/complete, PIF RAM            (09-03)
  t64_joybus.c        # total 512-bit frame parser, controller + EEPROM responders (09-03/04)
  t64_save.c          # EEPROM/SRAM storage, save_info/export/import             (09-04/05)
  t64_state.c         # state container writer + per-section validate/apply      (09-08/09)
  t64_digest.c        # sink seam + v25 section walker                           (09-01)
  tests/test_digest_v25.c, test_si.c, test_joybus.c, test_save.c,
        test_sched_matrix.c, test_state_export.c, test_state_import.c, fuzz_*.c   # fuzz targets ASan-only
runner/
  t64_artifact_publisher.[ch]  # second small publisher                          (09-02)
  t64_replay.[ch]              # t64.replay parser/canonicalizer/recorder/driver (09-07)
  tests/test_artifact_publisher.c, test_pad_poll.c, test_save_fixtures.c, test_replay.c, test_state_runner.c
fixtures/{phase09,pad,save,replay}/*.S|*.ld   # authored; scripts/build-<family>-fixture.sh each
docs/spec/{si-pif-controller,save-media,state-container,replay,phase09-cases.json}
docs/claims/phase09.md, docs/evidence/phase09-*.md
scripts/{qualify-phase09.py,check-phase09-cases.py,tests/test_phase09_qualification.py}
```
Core files named `core/t64_*.c` are picked up by the glob (`file(GLOB T64_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/t64_*.c")`, [VERIFIED: core/CMakeLists.txt:9-10]) and therefore enter `T64_BUILD_ID` automatically; runner modules do not (K15).

### Pattern P1: explicit version-family membership
**What:** one root predicate plus explicit per-site decisions; never rely on fall-through. The current roots are `t64_has_ai_profile` (exact tuple 11/19/24) and `t64_has_vi_filter_v9_profile` which ORs it in [VERIFIED: core/t64_internal.h:927-939]; about twenty predicates chain through the latter.
**Recommended sites (script-generate the final list in 09-01):**
- `core/t64_internal.h`: add `t64_has_si_profile` (12/20/25); OR it into `t64_has_vi_filter_v9_profile` (propagates to the rdp/vi/cpu/rsp/t1/rdram chain; depth stays off by its `== 10u` guard, K1); add `t64_has_ai_device(machine)` = ai profile OR si profile.
- AI-device gates to switch to `t64_has_ai_device`: `core/t64_ai.c` lines 259, 303, 380, 403, 472, 632, 660 and `core/t64_bus.c:136` [VERIFIED: grep list]. Digest keeps exact predicates (`core/t64_digest.c:231` pattern) with the v25 branch first in the chain.
- `core/t64_machine.c`: `is_ai_profile` at 1189 and its eight create-time ladder uses (2229, 2250, 2265, 2308, 2332, 2385, 2398, 2410); `has_pinned_openipl3_profile_id`; the long id/descriptor chain and canonical-descriptor selection in `is_rdram_b_t1_profile` (1352 onward; the final `else` silently compares against the v2 base profile and fails closed); `profile_fields_are_valid` (`descriptor_version != 11u` style arms at 1533-1560); `write_profile` arms (1702-1741); `t64_profile_lookup` (starts 1978); four canonical const descriptors, one per id.
- `core/t64_isviewer.c` (K2); `runner/t64_real.c:759-768` (`rdp_vi_profile` is models 11-14, so model 20 falls through to "no observation required", which is correct for these ROMs).
**Test:** golden predicate matrix (every `t64_has_*` for all registered ids) captured before the first edit; new ids must equal the AI profile's row except the new predicate.

### Pattern P2: reserve-then-commit device with capacity pre-check
`t64_pi.c:88-100` is the template (one event). For SI add the K16 capacity check first. Verified PI start sequence: `t64_schedule_reserve_after(machine, delay, T64_DEVICE_PI, T64_PI_EVENT_COMPLETE, (uint64_t)length, token, t64_pi_complete, &reservation)` then `t64_schedule_commit`; a busy second start sets the sticky error and returns `T64_STATUS_BUS_DEVICE_REFUSED` [VERIFIED: core/t64_pi.c:57-100]. Closed `(device, kind)` ledger today [VERIFIED: grep of all reservation sites]: CPU 5/STEP 1; PI 3/COMPLETE 1; SP 4/COMPLETE 1 and RSP_ISSUE 2; RDP 7/WORK 1; VI 8/FIELD 1; AI 9/SAMPLE 1; input 6/SUBMITTED 1 (`T64_TRACE_DEVICE_INPUT`); synthetic test device 0xffff0001 kinds 1-5. New: SI 10 with execute and complete kinds (10 is unused in `core/t64_internal.h` and `core/include/tendies64/t64_machine.h`).

### Pattern P3: profile-keyed create-time storage
`t64_machine_create` computes one trailing region per profile (coverage offsets, then `total_size += sizeof(t64_machine)`) and allocates video/audio rings by profile (core/t64_machine.c:2215-2345). EEPROM (512 or 2,048 bytes) and SRAM (32,768) fit as trailing regions keyed by `save_kind`; no allocation callback is added at import (D-30).

### Pattern P4: digest sink and section walker
`t64_machine_digest` is one 580-line function streaming into `t64_sha256_ctx` (core/t64_digest.c:221-819); the legacy v5..v24 domains are an if/else chain at lines 251-287. Route `hash_u32`/`hash_u64`/`hash_event` and the 49 direct `t64_sha256_update` lines through a sink (hash or buffer). The v25 branch goes first in the chain. K1: v25 must reproduce the v24 field set (depth fields absent) for the strip-and-compare oracle.

### Pattern P5: device test layout
One test binary per area with `--list-cases`, `--case NAME`, `--suite NAME`, finishing with `T64_TEST_FINISH("unit.core.<area>.<suite>")`, registered one CTest name per case or suite, listed exactly in a JSON case inventory. Verified shape: `add_test(NAME unit.core.ai.profile-pins COMMAND test_profile --case old-profile-pins)` [VERIFIED: core/CMakeLists.txt:596-597]; `test_profile` already dispatches `--case` (core/tests/test_profile.c:938-958). Existing direct-binary run times: test_profile 0.03 s, test_scheduler 0.06 s, test_determinism 0.12 s, test_pi_dma 0.14 s, test_digest_trace 0.97 s [VERIFIED: executed].

### Pattern P6: fixture, manifest and runner-table chain
1. Source `fixtures/<family>/*.S` + `.ld`; build script copies `scripts/build-audio-fixture.sh` (pinned asset and payload hashes, qualified-tool receipt check, packer source pins, entry `0x80000400`, outputs under `build/fixtures`).
2. CMake custom command + target in `core/CMakeLists.txt` (pattern at the `T64_AUDIO_FIXTURE_OUTPUTS` block), consuming tests `add_dependencies` on it.
3. `fixtures/manifest.json` row: id (`^[a-z0-9][a-z0-9-]*$`, max 64), `source.revision` = 40-hex commit that first contains the source (so the row lands in a later commit than the `.S`), build recipe, `sha256`, `size`, `profile`, `distribution: ["build-only"]`, rights text, `expected.source/revision`. Source edits therefore cost a new revision and a re-pin: freeze fixture sources early.
4. `runner/t64_real_fixture.c` row: `{id, sha256, size, profile_id, profile_sha256, completion_pc, pass_eligible}`.
5. Real runner pass needs authored completion (BREAK at `completion_pc`), IPL3 entry and loader DMA milestones, plus K4.

### Pattern P7: refusal postconditions
Every refusal asserts bit-identical digest, bytes, counters and (where applicable) caller buffers; check order is fixed and tested row by row (D-24). Authored refusal matrices must prove an accepted baseline in the same setup and assert each case reaches its specific status (AUTONOMY).

### Pattern P8: replay driver at completed-tick boundaries
The real runner already advances in 16-tick slices (`#define T64_REAL_TRACE_STEP UINT64_C(16)` [VERIFIED: runner/t64_real.c:28], used as `uint64_t advance_step = T64_REAL_TRACE_STEP;` at :1558) with inclusive bounds and pre-queues all schedule events at tick 0 in source order [VERIFIED: runner/t64_real.c:1085-1155 read]; the recorder wraps `t64_machine_queue_input` and records `at = 0` for those. `T64_STOP_GUEST_TICK` targets are absolute.

### Anti-Patterns to Avoid
- Adding a version to a ladder by analogy without a predicate golden (K1 is the proof it hides).
- Treating a string match of a label as a membership test (`t64_isviewer_active` pairs ids with tuples).
- A helper that repeats the production transform as an "oracle".
- Editing a bound file after native qualification (qualifier, collector, `ci.yml`, `check-*.sh`, scanner, supervisor logic): about 15 minutes per cycle (F-096/F-098).
- Running `ctest` casually: it replaces `LastTest.log`; copy it first and use the preset (four jobs).

## Don't Hand-Roll

| Problem | Don't build | Use instead | Why |
|---------|-------------|-------------|-----|
| SHA-256 | a second implementation | `tools/common/t64_sha256.[ch]` | Core-lint admitted, already pinned in build identity |
| JSON for replay | a new parser | `tools/common/t64_json.[ch]` with raised `t64_json_limits`, `t64_jsonw`/`t64_json_canon.c` | Duplicate keys, UTF-8, integer range already refused |
| No-clobber publication | `open(O_TRUNC)` + `rename` | copy the `t64_raw_capture.c` transaction (parent-fd walk, 0700 stage, `RENAME_EXCL`/`RENAME_NOREPLACE`, identity-checked cleanup) | CR-01 was a publication-source race |
| Two-event atomic start | try/rollback with `t64_schedule_cancel` | capacity check then sequential reserve/commit (K16) | Cancel does not restore `next_sequence` |
| Verified input snapshots | `fopen` of a host path in the runner | `t64_verified_input_prepare` fd path | Replacement-race safe |
| Case inventory | ad-hoc case lists | `docs/spec/phase09-cases.json` + checker copy | Exact list equality is the guard that cases ran |
| GCC13 route | a new docker invocation | `_gcc13_closure_command` | Pinned image, read-only, no network |
| Fixture assembly | a different toolchain | pinned Binutils 2.45 scripts | Rights and identity gate |
| Test mutants | in-tree `#ifdef` mutants in core | out-of-tree patched copy that must build and fail the named test (Phase 8 practice) | Core is hashed into the build id; lint forbids host macros |

**Key insight:** this codebase's recurring defects were identity drift and wrong-denominator evidence, not algorithms. Reuse the pinned machinery and spend effort on oracles and controls.

## Common Pitfalls

### Pitfall 1: membership fall-through
**What goes wrong:** a new tuple is silently excluded (K1, K2) or silently included.
**Why:** chained predicates with an inner descriptor guard; per-id allow-lists in other files.
**Avoid:** golden predicate matrix before edits; `unit.core.profile.v12-identity` mutates every policy field through hash, serialization and known-id construction, tests empty and unterminated policy text, pins retained older bytes (AUTONOMY new-descriptor check).
**Warning signs:** pad-poll prints nothing (ISViewer inactive); a v25 differential fails only in the RDP section.

### Pitfall 2: the 61,000-tick real-runner window (K3)
**What goes wrong:** a fixture times out or can never see EEPROM busy expire.
**Avoid:** raise the cap first (09-02) or restrict claims; compute every guest schedule against entry tick about 538,800.

### Pitfall 3: pass rule and transcript (K4)
A new fixture that prints anything but the pinned line is `guest-fail`; a green process does not mean the case ran.

### Pitfall 4: atomicity of multi-event starts (K16)
Refusal must leave `next_sequence` untouched because it is in the digest (core/t64_digest.c:349-351).

### Pitfall 5: absolute ticks
`T64_STOP_GUEST_TICK` is absolute; sequence tests must advance from the current tick and assert the intended instruction retired.

### Pitfall 6: stack and frame limits (K8) and supervisor argv (K6)
Both fail late (Werror gate, stack overflow under ASan) and cost a full cycle.

### Pitfall 7: build-dependent goldens
`T64_BUILD_ID` hashes every core source, compiler flags and build type, so every core edit and every sanitizer build changes it; commit only build-independent goldens and recompose headers at test time (D-34).

### Pitfall 8: private paths and blobs in evidence
`scripts/pii-scan.sh` flags per-user home paths, per-user temp directories and `/tmp/claude-*` session directories anywhere in the tracked and untracked tree, including `.planning`; use `$PWD` or repo-relative paths and sanitize captures before the first commit. Saves, states and replays must never be tracked: `scripts/rights-scan.sh` already refuses `.sav`-class extensions with near-miss controls (extend the table, do not add a parallel scanner).

### Pitfall 9: fixture pin churn
Each `.S` or script edit changes the ROM SHA, the source-content hash and `source.revision`; the runner table, manifest and test literals must be re-pinned together. Author fixtures once per plan and pin by script output.

### Pitfall 10: the Phase 8 depth observation leaking into claims (K1)
Do not write Phase 9 text implying RDP depth coverage on the new family.

## Code Examples

Verified patterns (quotes are verbatim).

### Membership roots (core/t64_internal.h:927-938)
```c
static inline bool t64_has_ai_profile(const t64_machine *machine) {
  return machine != NULL && machine->profile.descriptor_version == 11u &&
         machine->profile.machine_model_version == 19u &&
         machine->profile.state_serialization_version == 24u;
}

static inline bool t64_has_vi_filter_v9_profile(
    const t64_machine *machine) {
  return (machine != NULL && machine->profile.descriptor_version == 10u &&
          machine->profile.machine_model_version == 18u &&
          machine->profile.state_serialization_version == 23u) ||
         t64_has_ai_profile(machine);
}
```
Plan shape [ASSUMED design suggestion]: add `t64_has_si_profile` for `12u/20u/25u` and OR it into the second function; add `t64_has_ai_device`.

### Dispatch sets machine time before the handler (core/t64_scheduler.c:349-352)
```c
  queue_pop_front(machine);
  machine->tick = event.tick;
  t64_trace_emit(machine, event.tick, event.device, event.kind, event.value);
  status = event.handler(machine, &event);
```

### Commit invalidation conditions (core/t64_scheduler.c:157-161)
```c
  if (machine == NULL || reservation == NULL || !reservation->valid ||
      reservation->machine != machine ||
      reservation->expected_tick != machine->tick ||
      reservation->expected_queue_count != machine->queue_count ||
      reservation->event.sequence != machine->next_sequence ||
```

### Test registration (core/CMakeLists.txt:596-599)
```cmake
add_test(NAME unit.core.ai.profile-pins
  COMMAND test_profile --case old-profile-pins)
add_test(NAME unit.core.ai.profile-mutations
  COMMAND test_profile --case descriptor-policy-mutations)
```

### Inventory registration (scripts/check-test-inventory.sh:297)
```sh
    printf '%s\n' "$BASELINE_CORE" "$BASELINE_CONTROLS" "$PHASE02_TESTS" "$PHASE03_TESTS" "$PHASE04_TESTS" "$PHASE05_TESTS" "$PHASE06_TESTS" "$PHASE07_TESTS" "$PHASE08_TESTS" "$_extra"
```
Add a `PHASE09_TESTS='...'` variable and its `"$PHASE09_TESTS"` argument (a data-row change); fuzz targets join `ASAN_ONLY`.

### Runner fixture row shape (runner/t64_real_fixture.h:11-19)
```c
typedef struct {
  const char *id;
  const char *sha256;
  size_t size;
  const char *profile_id;
  const char *profile_sha256;
  uint32_t completion_pc;
  bool pass_eligible;
} t64_real_fixture;
```

## Per-Plan File Lists

"M" = modify, "C" = create. Test registration is always in `core/CMakeLists.txt` or `runner/CMakeLists.txt` plus `scripts/check-test-inventory.sh` (`PHASE09_TESTS`). Names of new files are candidates (Claude's discretion).

**09-01 Core identity and walker (core only; freezes F0).**
- M `core/include/tendies64/t64_machine.h` (descriptor fields `si_policy`, `save_policy`; `T64_SAVE_*` kind constants 1..3; peripheral bit 0; SI trace device id), `core/t64_internal.h` (P1 predicates, `T64_SI_DEVICE`, section ids, sink types), `core/t64_machine.c` (P1 sites, four canonical descriptors, validation of `save_kind` 0..3 and `peripheral_flags` bit 0, create refusal of non-zero values on versions below 12, quantum and ratio validation), `core/t64_isviewer.c`, `core/t64_digest.c` (sink + v25), `core/t64_ai.c` and `core/t64_bus.c` (switch gates to `t64_has_ai_device`), `core/tests/test_profile.c` (new cases; offsetof table at 803-817; id-length case), `core/tests/test_scheduler.c` and `test_determinism.c` (ledger and FIFO characterization), `core/CMakeLists.txt`, `docs/spec/machine-contract.md` (replace the stale v5/v8 digest text at `:271-300`, add v25 framing, the ledger table, replacement-table rows), `scripts/check-test-inventory.sh`.
- C `core/tests/test_digest_v25.c`; a membership-sweep script or test mode producing the site list and the predicate golden matrix.
- Serialized profile size today is 1,319 bytes against `T64_PROFILE_SERIALIZED_MAX_BYTES` 2,048 [VERIFIED: executed probe, header]; two more labels fit. `sizeof(t64_profile_descriptor)` is 2,172 and grows by two 64-byte fields.

**09-02 Runner, supervisor, publisher, harness mechanism (publisher and artifact pair frozen after).**
- C `runner/t64_artifact_publisher.[ch]`, `runner/tests/test_artifact_publisher.c`, `fixtures/phase09/admit.S` + `.ld` + `scripts/build-phase09-admit-fixture.sh` (K12), `scripts/qualify-phase09.py` (fail-closed stub, zero cases fails), `scripts/tests/test_phase09_qualification.py`, `scripts/check-phase09-cases.py`, `docs/spec/phase09-cases.json`, `docs/claims/phase09.md` (skeleton, K11).
- M `runner/t64_supervisor.c` (artifact pair, exact `artifact_max_bytes + slack` limit only when present, K3 cap, K6 argv array), `runner/t64_real.c` (K3 cap, K4 pin generalization if chosen, artifact option, profile/observation branches for model 20 and the four ids), `runner/t64_real_fixture.[ch]` (rows, per-fixture pin fields), `runner/t64_result_check.c` (K4), `runner/CMakeLists.txt` (K15), `core/CMakeLists.txt` (admit fixture target), `scripts/rights-scan.sh` (denied extensions and magic for state/replay/save plus mutation controls), `docs/spec/raw-capture.md` (changed backstop), `fixtures/manifest.json`, `scripts/check-test-inventory.sh`.
- Controls: `harness.fsize-artifact-real-fault` (K9), `controls.artifact-substitution` (CR-01 style against the new leaf names).

**09-03 SI/PIF/controller tracer and first hosted data (freezes FH).**
- First task M `core/t64_cpu.c` (profile-gated budget in `cpu_successor_extra_event_budget`, 1483-1511; legacy budgets unchanged), `core/t64_scheduler.c` (K16 capacity check), `core/tests/test_si.c` case `cpu-store-budget`.
- C `core/t64_si.c`, `core/t64_joybus.c`, `core/tests/test_si.c`, `core/tests/test_joybus.c`, `core/tests/fuzz_joybus.c` (ASan-only), `fixtures/pad/poll.S` + `.ld` + `scripts/build-pad-fixture.sh`, `runner/tests/test_pad_poll.c`, `docs/spec/si-pif-controller.md` (source URLs and fetch dates; facts restated in authored words).
- M `core/t64_bus.c` (new SI region gated by the family predicate; the legacy `T64_SI_BASE` window stays `T64_BUS_REGION_WARM` for old profiles), `core/t64_warm_devices.c` (legacy SI_STATUS stub unchanged for old profiles), `core/t64_mi.c` or `t64_internal.h` (SI interrupt bit constant: `T64_MI_INTERRUPT_SI` exists only as a local define in `t64_warm_devices.c`), `core/t64_observe.c` (latch in `t64_input_event_handler`; port field in value bits 32-33 under the family), `core/t64_machine.c` (reset, create), `core/t64_digest.c` (SI/PIF and latch section), header (trace kinds), runner rows and real-runner handling, `runner/t64_supervisor.c` (any new observation fd forwarding), `scripts/qualify-phase09.py` (real collector/verifier), `.github/workflows/ci.yml` and `scripts/check-ci-yml.sh` (rule `CI-RUN-LINE`/`CI-UPLOAD-ARTIFACT` allowlists plus selftest controls), `docs/spec/machine-contract.md`, `docs/ci-reviews/LOG.md` (via `scripts/ci-review.sh --append`).

**09-04 Save devices (freezes save label and kind numbering).**
- C `core/t64_save.c`, `core/tests/test_save.c`, `fixtures/save/eeprom.S`, `fixtures/save/sram.S` (+ `.ld`), `scripts/build-save-fixtures.sh`, `runner/tests/test_save_fixtures.c`, `docs/spec/save-media.md`.
- M `core/t64_bus.c` (carve `[0x08000000, 0x08008000)` out of the domain-2 B unsupported arm before it is reached, the unsupported arm that lists `T64_DOMAIN2_B_BASE` is at core/t64_bus.c:104-108), `core/t64_pi.c` (read-length start for SRAM profiles, direction and target kind in the active descriptor; `case 0x08` currently always refuses), `core/t64_internal.h` (`t64_pi_state` fields, save state), `core/t64_machine.c` (trailing storage, reset), `core/t64_digest.c` (`save-media` section), `core/t64_joybus.c` (channel 4 responder), header (statuses `SAVE_*`, `save_info`), runner rows, manifest.

**09-05 Save import/export and persistence.**
- C `fixtures/save/roundtrip.S`, `runner/tests/test_save_roundtrip.c`, bounded mutation test in `core/tests/test_save.c`.
- M `core/t64_save.c`, header API, `core/t64_machine.c` (replacement restores blank save, clears deadline), `runner/t64_real.c` (save import via verified fd, export via publisher; result carries kind, length, format, flags only), `runner/t64_supervisor.c` (forwarding rows, with K6), `docs/spec/machine-contract.md` (replacement table), `scripts/rights-scan.sh` upload rules (data rows), `docs/spec/save-media.md`.

**09-06 CORE-02 ordering matrix (freezes F1: state content).**
- C `core/tests/test_sched_matrix.c`; out-of-tree mutant procedure documented in `docs/spec/phase09-cases.json` (K5 list).
- M `core/t64_mi.c`/`t64_cpu.c` only if a per-producer MI tie needs a hook (MI stays synchronous, sampled at step start: `t64_cp0_interrupt_pending` is checked first in `t64_cpu_step`, core/t64_cpu.c:2175-2180 [VERIFIED: sed read]); device specs (`docs/spec/audio-interface.md`, `docs/spec/vi.md`, `docs/spec/cpu-timing.md`) tie labels; full gate at the end.

**09-07 Replay.**
- C `runner/t64_replay.[ch]`, `runner/tests/test_replay.c`, `fixtures/replay/mix.S` + `.ld`, `scripts/build-replay-fixture.sh`, build-independent golden files (ops + checkpoint digests), `docs/spec/replay.md`.
- M `runner/t64_real.c` (record and play flags; separate `t64.replay-report` record, not `t64.result`), `runner/t64_supervisor.c` (forwarding rows), `runner/CMakeLists.txt` (K15), rows and manifest.

**09-08 State export and container.**
- C `core/t64_state.c`, `core/tests/test_state_export.c`, `runner/tests/test_state_runner.c`, `docs/spec/state-container.md`.
- M `core/t64_digest.c` (buffer sink and section markers), header API (size query then copy-out), `runner/t64_real.c` and `t64_supervisor.c` (export flag; real-runner state-size control under the raised FSIZE; about 9-11 MiB estimate [INF]: RDRAM 4 MiB, triangle coverage 2 MiB, memory coverage 2 MiB, video 614,400, audio 524,272 [VERIFIED: executed probe], depth block absent per K1).

**09-09 State import, invariants, boundary matrix.**
- C `core/tests/test_state_import.c`, `core/tests/fuzz_state_import.c` (ASan-only), `scripts/check-state-field-use.py` (`harness.state-field-use-audit`), import notes appended to `docs/spec/state-container.md`.
- M `core/t64_state.c` (readers with validate and apply modes), per-device helpers where a reader needs device internals, header API, runner import flags and forwarding rows, `scripts/check-test-inventory.sh` (`ASAN_ONLY`).

**09-10 Independent review, security audit and fix loop (freezes FR).**
- C `09-REVIEW.md`, `09-SECURITY.md`; fixes land here (logic) with regenerated goldens; run against real mutated blobs from the 09-09 sweep.

**09-11 Identity freeze and qualification (F2).**
- C `docs/evidence/phase09-*.md`; M `docs/claims/phase09.md` (real `t64-claim` blocks, one per profile id and fixture set because a block carries one `profile_id` and one `fixture_set`), test pins (replace symbolic accessors with literals), `scripts/qualify-phase09.py` data pins only, `docs/spec/*`; native normal, ASan/UBSan, gcc:13 and both hosted jobs on the exact SHA; no logic edits.

## State of the Art (precedent inside the repo)

| Old approach | Current approach | When | Impact |
|--------------|------------------|------|--------|
| Observation FIFOs for video/audio (Phase 8) | Per-fixture transcript pin (K4, recommended) | Phase 9 | Needs checker and runner change before fixtures |
| Single hello transcript pin | Per-row pin with legacy default | Phase 9 | Old rows unchanged |

## Assumptions Log

| # | Claim | Section | Risk if wrong |
|---|-------|---------|---------------|
| A1 | SWAP16 swaps the two bytes inside each 16-bit half-word; WORDREV32 reverses the four bytes of each 32-bit word (vectors in Validation) | Validation SAVE-03 | Wrong literal vectors; confirm and state the definitions in the 09-05 spec before writing vectors |
| A2 | Inventory selftest cost scales linearly with names (+15 s macOS) | K13 | Budget misjudged; measure in the first hosted pair |
| A3 | A 4,000,000-tick real-mode cap is enough and cheap (about 0.45 s native) | K3 | Pick another named constant; measured basis is 0.06 s per 539k ticks |
| A4 | `t64.replay` node memory for 65,536 ops is tens of MiB (72-byte nodes, about 8 nodes per op) | Standard Stack | Lower the op cap or stream-parse |
| A5 | A running CPU cannot reach a 2^33-tick idle span cheaply | K14 | Count-wrap case could also run through `advance` |
| A6 | State size 9-11 MiB | 09-08 | Artifact maximum choice (32 MiB ceiling is ample) |
| A7 | Hardware and protocol facts (register map, escapes, EEPROM commands, 6 ms ceiling, 4 Kbit mirror, SRAM base) remain single-source community facts with the proposal's labels | all SAVE rows | Wording must stay "named policy"; never a hardware claim |
| A8 | The proposed K5 replacement mutants are non-equivalent | K5 | A surviving mutant means a missing control; confirm each fails when built |

## Open Questions (RESOLVED)

All six questions are resolved by the 09-CONTEXT amendments and the plans: 1 by K4 (per-fixture pin, 09-02), 2 by K3 (per-fixture cap, 09-02), 3 by K1 (depth inactive on descriptor 12, STATE follow-up), 4 by 09-03 (exact Count assertions stay in process; the real runner prints Count as a two-tick-resolution witness), 5 by K12/K18 (one build script per family), 6 by 09-06 Task 4 (committed driver `scripts/k5-mutants.py`, list in `docs/spec/state-mutants.txt` for state and in the 09-06 mutant list for ordering).

1. **K4 transcript pin versus observation record**
   - Known: pass is hard-wired to one 30-byte transcript in the runner and the strict checker.
   - Unclear: owner preference between a per-fixture pin and an observation FIFO.
   - Recommendation: per-fixture pin, decided at the start of 09-02, before any fixture is authored.
2. **K3 cap value**
   - Recommendation: 4,000,000 ticks, two edit sites, default bound unchanged.
3. **K1 depth on descriptor 12**
   - Recommendation: preserve inactive; log a STATE follow-up; no Phase 9 text about depth.
4. **Count-witness exactness (D-16)**
   - The model advances Count once per two ticks (`count_increments = elapsed_ticks / UINT64_C(2) +` ..., core/t64_cp0.c:588); a printed Count spacing is only a two-tick-resolution witness. Recommendation: exact-tick assertions stay in-process (`unit.core.si.dma-timing` from trace records); the real-runner transcript asserts values and Count consistency.
5. **Fixture source layout** (D-39 says three sources for five ROMs; K12 adds the admit ROMs). Recommendation: one build script per family, defsym variants for wrong-value controls.
6. **Mutant harness** (K5). Recommendation: a small committed script that patches a temporary copy, builds it, runs the named test and requires failure; list lives in the JSON inventory.

## Environment Availability

| Dependency | Required by | Available | Version | Fallback |
|------------|-------------|-----------|---------|----------|
| C compiler | all builds | yes | Apple clang 21.0.0 | none needed |
| cmake / ninja | presets gate and asan | yes | 4.4.3 / 1.13.2 | none |
| python3 | harness scripts, `-I` for untrusted data | yes | 3.14.4 | none |
| jq | fixture build scripts | yes | 1.7.1 | none |
| GNU Binutils 2.45 MIPS tools | authored ROMs | yes | receipt `build/tools/qualified-host-tools.json`, tools under `build/tools/install/bin` | rebuild via the existing gate step (cache) |
| docker with gcc:13 image | read-only `-Werror` check | yes | gcc:13, 14, 15 images local | none |
| act | Linux job pre-flight | yes | 0.2.87 | none (never hosted evidence) |
| gh | hosted run inspection | yes | 2.101.0 | none |

Missing dependencies: none. Build tree `build/gate` exists (Release library, built from the current clean tree).

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | C17 executables on `tests/t64_test.h`, CTest (labels unit, controls, integration, harness, repo, gov), Python harness tests under `scripts/tests/` |
| Config file | `CMakePresets.json` (`gate`: Ninja, Release, `T64_WERROR=ON`, test jobs 4, `noTestsAction: error`; `asan`: Debug, ASan+UBSan) |
| Quick run command | `<built binary> --case <name>` (sub-second; existing core tests 0.03-0.97 s) then `python3 -B scripts/check-phase09-cases.py --build build/gate --suite <suite> --run --artifact-prefix <plan-tag>` (to be created in 09-02, copy of the audio checker) |
| Plan command | `cmake --build --preset gate` then `ctest --preset gate -R '<plan regex>'`; copy `LastTest.log` and use a unique `--output-junit` path per manual batch |
| Full suite command | `sh scripts/gate.sh` (only at the end of 09-03, 09-06, 09-11); ASan: `ctest --preset asan` |

### Phase Requirements to Test Map
Success criterion 1 = SAVE-01; 2 = SAVE-02/03/04; 3 = CORE-04/05; 4 = CORE-02. "Control" = wrong-value or mutant control that must build, run the same accepted setup and reach its specific comparator; the case list must equal `docs/spec/phase09-cases.json`.

| Req | Owning test (to create) | Behavior | Independent expected-value source | Wrong-value control | Plan |
|-----|-------------------------|----------|-----------------------------------|---------------------|------|
| SAVE-01 | `unit.core.cpu.si-store-budget` | SI RD64B store needs 2 extra events, WR64B 1; 8-byte store defined; atomic `QUEUE_FULL`/`SEQUENCE_EXHAUSTED`; legacy budgets untouched | Capacity rule restated by hand (needed = 1 + extra) on a small-queue, small-sequence-width authored profile; old-profile contrast | Mutant with budget 1 for RD64B must leave the queue over capacity or the digest changed | 09-03 |
| SAVE-01 | `unit.core.si.registers`, `unit.core.si.status`, `unit.core.si.pif-command` | Register map, RD64B (PIF to RDRAM) versus WR64B (RDRAM to PIF) both directions, STATUS clears MI SI, PIF address check, command byte bit 0x01 only; others refuse | Register offsets and bit lists from the SDK/community sources restated as literals [CITED: 09-CONTEXT.md D-09, D-10] | Swapped-direction test; command byte `0x02` must refuse with digest unchanged | 09-03 |
| SAVE-01 | `unit.core.si.dma-timing` | Execute tick E and completion C from literals | Hand arithmetic: n=1,m=4 gives 10,876; W 2,500; X 4,096 [CITED: D-14] | Shift by one tick; completion derived from the advance budget (K5) | 09-03 |
| SAVE-01 | `unit.core.joybus.parser`, `unit.core.joybus.fuzz-bounded` | Total over all 512-bit inputs; escapes; count masks; 16 single-button vectors; ports 1-3 and channel 4 without EEPROM flag `0x80`; pak/other commands timeout `0x40` | 64-byte PIF images authored from the field table before decoder breadth (AUTONOMY raw-command preflight) | A decoder that misreads the flag bits must fail the vectors | 09-03 |
| SAVE-01 | `unit.core.input.latch-partition`, `unit.core.si.refusals`, `unit.core.si.atomic-queue-full` | Strictly-after latch (X at 10 then Y at 10: sample at 10 sees old, 11 sees Y), whole versus split, later call wins at equal stamp | Authored trace from the adversarial determinism walk [CITED: 09-DISCUSSION-ADVERSARIAL.md] | Promote/sample order reversed mutant; refusal leaves digest identical | 09-03 |
| SAVE-01 | `runner.pad-poll`, `controls.real.pad-wrong-value` | Authored ROM reads scheduled buttons/axes through the real runner at inputs E-1, E, E+1 | Transcript literal authored by a Python oracle from the script and policy literals, held outside the ROM (K4) | ROM variant with one wrong byte and a tick-shifted script reach the same comparator and yield `wrong-transcript` | 09-03 |
| SAVE-01 | `harness.phase09-collect-real` | Collector/verifier on real hosted artifacts | Real two-platform retained artifacts | Controls built from the two real platform records | 09-03 |
| SAVE-02 | `unit.core.save.selection` | Selection is the profile; header byte `0x3F` nibble ignored; four ids | Authored ROM with a save-type nibble | Same ROM on the base id has no device | 09-04 |
| SAVE-02 | `unit.core.save.eeprom`, `unit.core.save.eeprom-mirror-and-16k` | Exact lengths info(1,3)/read(2,8)/write(10,1); `0xFF` on channel 4 times out; 4 Kbit `block & 0x3F`; 16 Kbit 256 blocks | Literal command vectors; block patterns embed the index (0x41 &0x3F = 1, [VERIFIED: shell arithmetic]) | Mask `0x7F` mutant fails on 4 Kbit; 16 Kbit blocks 0x41/0x01 must hold distinct data | 09-04 |
| SAVE-02 | `unit.core.save.eeprom-busy-triple` | Busy at `busy_until - 1`, idle at `busy_until`, idle after; write while busy replies `0x80`, window not extended; read while busy times out | 375,000 = 6 ms x 62,500,000 ticks/s [VERIFIED: shell arithmetic, core/t64_vi.c:23 clock] as policy | Deadline from the store tick instead of E (K5) must fail | 09-04 |
| SAVE-02 | `unit.core.save.sram` | `[0x08000000, 0x08008000)`; PI DMA both directions duration `1 + N/2`; effect atomic at completion; 32-bit aligned CPU access only; non-SRAM profiles still refuse read-length | PI duration equation [VERIFIED: core/t64_pi.c:79-87] with independent literals | Duration `N/2` mutant; mid-flight source mutation witness | 09-04 |
| SAVE-02/04 | `runner.eeprom-rw`, `runner.sram-rw`, `controls.real.save-wrong-byte` | Real-runner round through authored ROMs on `-e4k`, `-e16k`, `-s32k` (K3 limits apply) | Literal byte patterns in the harness | One-byte `--defsym` variant | 09-04 |
| SAVE-04 | `unit.core.save.refusals` | `SAVE_UNKNOWN_DEVICE`, `SAVE_UNSUPPORTED_DEVICE`, `SAVE_DEVICE_MISMATCH` and wire-level unsupported requests with defined postconditions | Status table from D-23 | A silent success (reading `0xFF`) must fail the specific-status assertion | 09-04 |
| SAVE-03 | `unit.core.save.import-export` | RAW/SWAP16/WORDREV32; busy cleared; `write_generation` increments; time and queue unchanged | 8-byte vectors authored by hand: RAW `00 01 02 03 04 05 06 07`; SWAP16 `01 00 03 02 05 04 07 06`; WORDREV32 `03 02 01 00 07 06 05 04` [ASSUMED definitions, A1] | Swapped-format vector fails | 09-05 |
| SAVE-03 | `unit.core.save.refusal-matrix`, `unit.save.mutation` | Check order table row by row; digest, bytes, deadline, counters, trace unchanged; too-small export reports required length | D-24 order table | Import while EEPROM busy refused with digest identical | 09-05 |
| SAVE-03 | `runner.save-roundtrip`, `controls.real.save-import-refused`, `harness.upload-allowlist-save` | Runner fd import then export via publisher; refused import | Verified-input path; no save bytes in result/log | Altered length/format refused before guest start | 09-05 |
| CORE-02 | `unit.core.sched.ledger`, `unit.core.sched.characterize-fifo` | Closed `(device, kind)` table equals the reservation sites plus SI; pairwise ties in both insertion orders on existing profiles | FIFO definition `(tick, sequence)` [VERIFIED: core/t64_scheduler.c:10-13]; literal expected order lists | Transposed-table mutation control | 09-01 |
| CORE-02 | `unit.core.profile.quantum-identity` | Q != 1 and ratio != 1:1 refused with allocator counters unchanged; Q=1 accepted; hash changes when only quantum changes | Independent reference hash | Contrast at core/tests/test_profile.c:450-470 plus an explicit one-field control | 09-01 |
| CORE-02 | `unit.core.sched.cross-device`, `unit.core.sched.partition`, `unit.core.sched.interleave`, `unit.core.sched.resource-limit-resume`, `unit.core.sched.mi-producers` | Whole versus splits at {T-1, T, T+1} and steps {1,3,7,4096,one-shot}; 2 and 3 interleaved machines (one other profile); trace on/off; mid-tick counter and resource stops then resume; Count-wrap via direct state (K14) | Regression pins plus self-comparison (limit stated in the claim) | Four to five mutants from K5, each failing a named test | 09-06 |
| CORE-04 | `unit.core.digest.sink-legacy-identity`, `unit.core.digest.v25-reference`, `unit.core.digest.v25-strip-differential` | Legacy digests byte-identical through the sink; v25 equals an independent reference encoder; stripped v25 stream equals v24 stream | Existing literal pins; the v24 stream is the oracle for the v25 port | Deliberately dropped-field v25 mutant must fail the differential | 09-01 |
| CORE-04 | `unit.core.state.export-layout`, `unit.core.state.idempotence-shape`, `unit.core.state.non-interference`, `runner.state-export`, `controls.real.state-export-fsize` | Container fields and sections decoded by an independent reference decoder with literal offsets; export does not change the digest | Reference decoder in the test | Altered section length/hash must be caught by the decoder | 09-08 |
| CORE-04 | `unit.core.state.roundtrip`, `unit.core.state.idempotence`, `unit.core.state.continuation`, `unit.core.state.boundary-matrix`, `unit.core.state.stalled-observer-stop`, `unit.core.state.refusal`, `unit.core.state.mutation`, `harness.state-field-use-audit`, `controls.real.state-import-refused` | T1-T4; per-boundary "reached" assertion; both drop-observers outcomes; structure-aware mutants that keep magic and checksums valid and each reach their specific refusal; refusals leave the live machine bit-identical | Continuation compared with replay checkpoint digests (an independent earlier run); generated field-use audit | One-field perturbation per boundary must be detected by T3 | 09-09 |
| CORE-05 | `unit.replay.format`, `unit.replay.mutation`, `unit.replay.partition`, `unit.replay.at-matters`, `unit.replay.independent-original-run`, `unit.replay.queue-full-divergence`, `runner.replay-mix` | Closed keys, caps, `at` ordering, identity binding, divergence report | Original run driven from an independently authored `t64.input` script; goldens = ops + checkpoint digests as build-independent literals, header composed at test time | Mutated single `at` must diverge; recorder omission control | 09-07 |
| CORE-05 | `controls.real.replay-mismatch-profile`, `controls.real.replay-mismatch-build`, `controls.real.replay-mismatch-content` | Each mismatch refused before guest execution with a named field | Altered header field | Each reaches its own comparator | 09-07 |
| Harness | `harness.fsize-artifact-real-fault`, `harness.fsize-report-capture-still-bounded`, `runner.artifact-publisher`, `harness.phase09-stub-fails-closed`, `harness.rights-scan-state-replay`, `harness.upload-allowlist-artifacts`, `runner.profile-table-id-length`, `harness.phase09-qualification` | Mechanism and gates | Literal limits; id lengths 38/42/43/43 against the 63-character limit [VERIFIED: shell length count; `T64_PROFILE_TEXT_CAPACITY 64u`, core/include/tendies64/t64_machine.h:15] | Oversize report and capture still refused with the option present; stub with zero cases fails | 09-02/11 |

### Cheap-first order (every plan)
1. Fixture identity and rights, manifest schema, `sh scripts/check-claims.sh --files docs/claims/phase09.md` (usage verified in the script header), `ast.parse` for Python (never `py_compile`), `sh scripts/pii-scan.sh --tree`, `sh scripts/rights-scan.sh --tree`.
2. gcc:13 `-Werror` on the changed C units through the existing image.
3. One accepted operation through the real runner with a cleared authored input (first-actual-consumer rule; for 09-02 use the admit ROMs, K12).
4. Direct binary `--case` runs, then `ctest --preset gate -R <plan regex>`, then the ASan selection, then old-profile pins and refusals.
5. `node scripts/verification-scope.mjs --base <last-tested-source>` before any broad retest (advisory).

### Sampling Rate
- **Per task commit:** the owning `--case` binaries (seconds), plus the inventory check for registered names.
- **Per plan:** focused CTest regex through the preset, ASan selection for core changes, old-profile regressions.
- **Per wave:** the complete affected Phase 9 group plus all old-profile pins; full gate only at the end of 09-03, 09-06 and 09-11.
- **Phase gate:** native normal, ASan/UBSan, gcc:13 and both hosted jobs green on one SHA (09-11).
- State-hash-heavy T3, mutation loops with sampled offsets, fuzz corpora, ASan and the replay soak stay native (D-40).

### Wave 0 Gaps
- [ ] `docs/spec/phase09-cases.json` and `scripts/check-phase09-cases.py` (copy of the audio checker) before the first new suite.
- [ ] `core/tests/test_digest_v25.c`, the predicate golden matrix and the membership-site list (09-01).
- [ ] `runner/tests/test_artifact_publisher.c`, the real-mode test fault for K9, `scripts/tests/test_phase09_qualification.py` (09-02).
- [ ] Admit ROM source and build script (09-02); `fixtures/pad`, `fixtures/save`, `fixtures/replay` sources and scripts when their plans start.
- [ ] A profile-hash print helper for re-pinning runner rows (K12).
- [ ] Framework: none new (CTest, T64 macros, python3 already present).

## Security Domain

ASVS level 1 (`security_enforcement` true, `security_block_on` high).

### Applicable ASVS Categories
| ASVS category | Applies | Standard control |
|---------------|---------|------------------|
| V2 Authentication / V3 Session / V4 Access control | no | Local single-user tool; no accounts |
| V5 Input validation | yes | Closed schemas and caps before allocation: joybus (total, bounded), save import (exact sizes), state import (validate then apply, checked arithmetic, structure-aware mutants), `t64.replay` and `t64.input` (strict bounded JSON, duplicate keys refused) |
| V6 Cryptography | yes (integrity only) | SHA-256 from `tools/common/t64_sha256.c`; hashes are integrity checks, not authentication (D-30); no new crypto |
| V8 Data protection | yes | Save/state/replay blobs private (0600, no-replace, no symlink parents), never uploaded or committed; no host paths, times or user names in headers |
| V12 Files and resources | yes | Parent-fd walk publisher, staging 0700, exclusive rename, identity-checked cleanup; FSIZE bound exactly `artifact_max_bytes + slack` only when requested |
| V14 Configuration | yes | No new dependency; `check-ci-yml.sh` exact run-line and upload allowlists |

### Known Threat Patterns
| Pattern | STRIDE | Standard mitigation |
|---------|--------|---------------------|
| Crafted state blob with valid hashes but unchecked index/length/loop bound | Tampering, DoS | Contract is memory safety plus liveness; generated field-use audit; per-device rules; audit in 09-10 on real mutated blobs |
| Source-substitution or rename race on publication (CR-01 class) | Tampering | Copy the transaction pattern; substitution controls against the new leaf names |
| Oversize output kills or loosens the backstop | DoS | Exact FSIZE rule, report and capture stay bounded, control that oversize is still refused |
| Integer overflow in size/offset arithmetic | Tampering | Checked arithmetic; caps before allocation; closed size sets |
| Replay JSON tricks (duplicate keys, huge numbers, deep nesting) | Tampering, DoS | Reuse strict parser with explicit limits; closed keys |
| Stack exhaustion from a fixed argv array (K6) or oversized frames (K8) | DoS | Checked push helper and `_Static_assert`; frame limit under Werror |
| Rights or privacy leak (save/state tracked or uploaded; workstation paths in evidence) | Information disclosure | Denied-extension table with mutation controls, upload allowlists, tree PII scan before evidence commits, repo-relative paths |
| Stale-build replay or state accepted | Spoofing | Exact profile/build/content binding with separate statuses |

## Sources

### Primary (HIGH confidence, read or executed this session)
- `.planning/phases/09-controller-save-media-and-replay/09-CONTEXT.md`, `09-DISCUSSION-PROPOSAL.md` (sections 1-10), `09-DISCUSSION-LOG.md`, selected lines of `09-DISCUSSION-ADVERSARIAL.md`; `AGENTS.md`, `.planning/AUTONOMY.md`, `.planning/REQUIREMENTS.md`, `.planning/STATE.md`.
- Code read: `core/t64_scheduler.c`, `t64_observe.c`, `t64_pi.c`, `t64_digest.c`, `t64_internal.h`, `t64_machine.c`, `t64_isviewer.c`, `t64_cpu.c` (1470-1570), `t64_bus.c`, `t64_warm_devices.c`, `t64_cp0.c`, `core/include/tendies64/t64_machine.h`, `core/CMakeLists.txt`; `runner/t64_supervisor.c`, `t64_real.c`, `t64_real_fixture.[ch]`, `t64_input.[ch]`, `t64_verified_input.h`, `t64_raw_capture.h`, `runner/CMakeLists.txt`; `scripts/check-test-inventory.sh`, `gate.sh`, `build-audio-fixture.sh`, `check-audio-cases.py`, `check-ci-yml.sh` (header), `.github/workflows/ci.yml`, `CMakePresets.json`; `fixtures/manifest.json`, `manifest.schema.json`; `docs/claims/TEMPLATE.md`, `phase08.md`, `docs/evidence/phase08-audio.md`, `docs/spec/machine-contract.md`, `raw-capture.md`, `docs/ci-reviews/LOG.md`; `tools/common/t64_json.h`; `tools/lint/rules.txt`.
- Executed this session against the clean tree and the Release build library: tool versions, per-profile predicate probe (K1), first-user-code tick probe (K3), struct-size and serialized-profile-size probes, direct runtimes of five core tests, id-length and timing arithmetic.

### Secondary (MEDIUM/LOW, not re-fetched)
- n64brew Joybus Protocol, PIF-NUS, Serial Interface, Peripheral Interface pages; Nintendo SDK `rcp.h`/EEPROM manual pages; libdragon identifiers: facts and labels exactly as recorded in `09-DISCUSSION-PROPOSAL.md` and `09-CONTEXT.md` ([COMM] single source unless stated). The external research seam (research-plan, Context7, web) was not used: the phase adds no library and the owner scoped web use to what a plan truly needs; none was needed.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH (in-repo, nothing external)
- Architecture and membership sites: HIGH (read and probed)
- Contradictions K1-K16: HIGH for the facts, MEDIUM for the recommended resolutions (planner's choice)
- Hardware protocol facts: LOW-MEDIUM (carried labels; named policy only)
- Pitfalls: HIGH (each tied to a verified site or a repo-recorded lesson)

**Research date:** 2026-11-19
**Valid until:** until the next edit of `core/t64_internal.h`, `runner/t64_supervisor.c`, `runner/t64_real.c` or `scripts/check-test-inventory.sh` (line numbers are cited against the commit at the start of this session; re-check them before editing)
