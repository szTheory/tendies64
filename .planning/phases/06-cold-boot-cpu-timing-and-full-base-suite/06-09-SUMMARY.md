---
phase: 06-cold-boot-cpu-timing-and-full-base-suite
plan: "09"
subsystem: authored-cold-startup
status: complete
completed: 2026-11-15
requires:
  - phase: 06-01
    provides: Cold A/B profiles and T1 timing
provides:
  - Input-pinned production cold probe and process controls
  - One retained Tier B zero-RI_SELECT startup observation
affects: [06-02, 06-03, 06-05]
requirements-completed: [BUS-03, CPU-08]
actuals:
  tokens: 8083
  tasks: 2
  commits: 3
key-files:
  created: [runner/t64_cold_boot.c, runner/tests/test_cold_boot.c, docs/spec/rdram-boot.md]
  modified: [runner/CMakeLists.txt, scripts/check-test-inventory.sh, docs/spec/cpu-profile.md]
---

# Plan 09 — First cleared cold startup

The production runner admits the exact cleared authored hello/IPL3 and two
cold/T1 profile IDs, executes the canonical core and observes bus/retirement
traces and output. Comparison mode remains refused until completion support.
Process controls reject changed inputs, profiles and setup failures.

After qualification and native controls, one Tier B integration invocation
observed a successful 4-byte RI_SELECT read with value0 at tick53, PC
0xa4000094, instruction0x8c42000c. The run later stopped CPU_UNSUPPORTED at
tick75, with no output or trace drops. Its terminal instruction and refused
bus address/status were not captured, so the later stop cause is unknown.
This establishes startup and the serviced selector read, not cold branch,
RDRAM protocol or hello completion. [Boot specification](../../../docs/spec/rdram-boot.md)
records the selected software seeds and evidence limits.

## Checks and correction

Fixture source/tool/boot/manifest qualification, all-target normal and ASan
builds, focused process controls in both presets and exact inventories pass.
The inventories are inherited127/129 plus two registrations:129/131.
The single actual integration and scoped documentation check pass.

Task1 did not follow its planned pre-implementation TDD ordering. The first
control assertion incorrectly expected Tier A's seed to be1; it was corrected
to the selected policy5. Neither that mismatch nor compiler warnings are
presented as a meaningful pre-implementation RED.

Parent spot-check found an unused terminal-memory reconstruction fallback
that could mislabel an unsupported attempt and misstate some instruction
widths. The fallback was removed; observed operations now require successful
bus trace plus retirement. The original result used that trace path and is
unaffected. Final normal/instrumented builds and controls pass after this
correction; no Tier B probe was repeated.

Original executable, command and outcome were archived together with a
hash-checked binding before the correction rebuild under ignored
`build/gate/06-09-original-probe-archive/`. Original probe source is055d763;
final control source is6f47359. Final control logs and hashes are retained in
`build/gate/06-09-final-controls/`; detailed commands and deviations are in
`build/gate/06-09-execution-checkpoint.md`. These records retain separate
identities rather than relabeling the old run. Three task/correction commits
contain only the six planned paths. Owner instruction/config edits remain
unchanged and unstaged.

## Next action

Execute Plan02's independently qualified full-BASE inventory/context and first
local member before protocol expansion. Preserve the original probe and its
unknown terminal cause; no replay is required for this closeout. Phase-level
requirements remain pending. No user action is needed.
