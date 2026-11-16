---
phase: 06-cold-boot-cpu-timing-and-full-base-suite
plan: "03"
subsystem: native-rdram-protocol
status: complete
completed: 2026-11-15
requires:
  - phase: 06-02
    provides: Completed bounded integration attempt with explicit evidence limits
provides:
  - Tier-B per-machine RI/RDRAM/MI repeat software protocol
  - Capability-scoped bus routing and complete state-v13 binding
  - Independent native command, clock, refusal and access-owner controls
affects: [06-04, 06-05, 06-08]
requirements-completed: [BUS-03]
actuals:
  tokens: 23837
  tasks: 3
  commits: 9
key-files:
  created: [core/t64_rdram.c, core/tests/test_rdram.c]
  modified: [core/t64_internal.h, core/t64_bus.c, core/CMakeLists.txt, core/t64_warm_devices.c, core/t64_mi.c, core/t64_digest.c, core/tests/test_warm_devices.c, core/tests/test_bus.c, docs/spec/rdram-boot.md, scripts/check-test-inventory.sh]
---

# Plan 03 — Native cold protocol and state ownership

The exact Tier B capability now owns the RI configuration/current-load/
selection/reset/standard sequence, logical device IDs, broadcast operations,
MI repeat mode, model calibration readback and capacity scan. Complete spans
route through the canonical bus. Every new future-affecting scalar and device
field is included in the new state-v13 digest, with old domains unchanged.

[The register specification](../../../docs/spec/rdram-boot.md) separates the
primary register map, permitted open-IPL3 algorithm and versioned software
choices. Two logical slots, the selected geometry branch, unknown manufacturer
sentinel and opaque delay/refresh payloads do not establish physical topology
or analog calibration. RI waits use checked absolute guest-time deadlines;
the source's loop count is provenance for that software interval, not a measured
clock duration. A retirement-count intermediate was replaced before closeout.

## Verification

Authored raw CPU instructions exercise bus writes and readbacks with literal
PC/opcode/retirement and trace assertions. Independent sequences cover the
remaining protocol and MI state; reached refusals compare actual before/after
latches. Tests cover idle and variable-cost time, advance partitions, separate
instances, one-field digest sensitivity and RI reset during enumeration.

CPU data-read policy, instruction fetch, stores, PI/SP DMA and register accesses
keep distinct owners. Installed backing stays4MiB with no alias above it.
The selected compat-v2 zero-read windows/exclusions and legacy special warm
word probe remain intact. One first-pass generic span check incorrectly
rejected that word probe; the fix limits it to the new register window.
Fixture NOT_READY and direct-step queue-exhaustion setup failures were retained
as invalid setup, never relabeled as protocol RED evidence.

All-target normal and ASan/UBSan builds pass. Final focused selections pass
boot-entry, RDRAM protocol, warm-device policy, bus, profile, digest, PI DMA and
SP DMA owners in both configurations. MI behavior is owned by the bus and
warm-device tests; no standalone MI CTest exists. The exact inventories pass
at133/135. The new test and inventory entry were added atomically rather than
deferring the inventory edit to Plan05; that authorized bookkeeping deviation
must not cause a duplicate addition later.

Original final logs and their hashes are retained under ignored
`build/gate/06-03-retained-checks/`; detailed commands, meaningful RED reports,
invalid setup attempts and nine commits are in
`build/gate/06-03-execution-checkpoint.md`. Final native source is3644db6.
Actuals are realized diff characters divided by four, not model billing.
Protected owner instruction/config edits remain unchanged and unstaged.

## Remaining work

This is native authored software-model evidence. No full ROM boot, external
diagnostic, historical probe replay, hosted run or hardware qualification was
performed. The earlier failed external attempt and missing historical binary
remain stopped/disclosed. BUS-03 remains phase-level pending until the planned
Tier A/B hello comparison. Continue Plan04's CP0/timing/scheduler integration;
no user action is needed.
