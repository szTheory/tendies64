---
phase: "03"
slug: first-headless-rom-through-open-ipl3
status: planned
nyquist_compliant: true
wave_0_complete: false
created: "2026-11-10"
---

# Phase 3 — Validation Strategy

This is the finalized executable planning contract. No Phase 3 behavior has
passed yet. Each row below has exact task/wave/threat ownership and a named
automated check; every PLAN command has its immediate fails-when sibling.
New behavioral tests are written and registered with their owning feature.

## Test Infrastructure

| Property | Value |
|---|---|
| Framework | Existing CTest and authored C assertions; no test framework installation |
| Configuration | CMakeLists.txt, CMakePresets.json, scripts/gate.sh, exact test inventory and core lint |
| Quick check | Owning named CTest with `--no-tests=error --output-on-failure` after its target and runtime producer dependencies build |
| Full gate | `./scripts/gate.sh`, existing Linux/GCC and macOS/Clang jobs |
| Sanitizers | Full all-target ASan/UBSan build; current local host uses `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` |
| Runtime | Baseline hosted execution measured 38s Linux /58s macOS, excluding queue. New CPU/fixture cost unmeasured; keep early slices bounded and measure. |

## Sampling Rate

- Build and run the owning named behavioral test after relevant changes; compile
  all producer/checker dependencies of consumer tests.
- Run the full gate at meaningful integration/wave boundaries and before final
  verification; preserve all baseline registrations. Repeat only after relevant
  changes or unresolved failures warrant it.
- Exercise the second hosted compiler early in the first buildable slice; review
  every hosted run's measured queue/setup/build/test cost.
- Keep apps and tests in the background/headless by default. A timeout does not
  justify taking focus or removing an explicit resource/no-retry limit.

## Final Contract Verification Map

The 17 identities below are required and finalized by the ownership map added
below. Preserve the existing 73 gate /75 sanitizer registrations and append
all 17 for exact final inventories of 90 and 92. Removal selftests reject
every single missing name. No execution pass is claimed.

| CTest identity | Required behavior | Current status |
|---|---|---|
| `unit.core.boot-entry` | HLE profile/seed identity, artifact digest check, DMEM copy and exact CPU entry. | Pending; created with owning feature |
| `unit.core.cpu-path` | Qualified instruction decode/retirement, delay slots, virtual KSEG mapping and explicit unsupported stop. | Pending; created with owning feature |
| `unit.core.cp0-cache` | Reachable CP0 masks/state and CACHE semantics under the named software policy. | Pending; created with owning feature |
| `unit.core.warm-device-policy` | RI preset, absent-memory probe, zero-source DMA and reset-register subset. | Pending; created with owning feature |
| `unit.core.isviewer` | Magic/buffer/lane/count/partial/repeated/overflow semantics and bounded drain. | Pending; created with owning feature |
| `runner.result-v2` | Strict real v2, dual-read v1, unknown-field/version rejection, six outcomes and independent golden. | Pending; created with owning feature |
| `runner.fixture-descriptor` | Admitted bytes come from verified unlinked descriptor; source replacement cannot alter guest input/result digest. | Pending; created with owning feature |
| `integration.open-ipl3-hello` | Actual pinned IPL3 and authored hello execute; assert CPU milestones and exact ISViewer bytes. | Pending; created with owning feature |
| `integration.open-ipl3-determinism` | Same process with different advance partitions and trace on/off yields same guest output/state. | Pending; created with owning feature |
| `controls.real.empty-halt` | Empty halted guest is boot-fail, never pass. | Pending; created with owning feature |
| `controls.real.empty-bound` | Empty output at guest bound is boot-fail, not timeout/pass. | Pending; created with owning feature |
| `controls.real.wrong-transcript` | Wrong or incomplete non-empty output is guest-fail/timeout by contract, never pass. | Pending; created with owning feature |
| `controls.real.unsupported-opcode` | Unsupported instruction stops explicitly and cannot pass. | Pending; created with owning feature |
| `controls.real.output-overflow` | Truncated/lost capture is rejected as pass. | Pending; created with owning feature |
| `controls.real.source-replaced` | Replace original fixture after supervisor verification; descriptor bytes/digest remain the admitted original. | Pending; created with owning feature |
| `controls.real.identity-mismatch` | Altered IPL3/tool/fixture/profile identity fails before or during admission as specified. | Pending; created with owning feature |
| `controls.real.input-partition` | Same canonical timed schedule is traced at the same guest ticks under partition/trace toggles. | Pending; created with owning feature |

## Requirement Coverage

| Requirement | Automated proof |
|---|---|
| BOOT-01 | Actual owned IPL3 copy/CPU entry, verified instruction/deadline semantics, observed IPL3-to-hello milestones; explicit unsupported stop |
| BOOT-02 | Source/binary rights and maintenance record, exact IPL3 size/SHA256/MD5; altered/truncated/missing/mismatched asset refusal |
| BOOT-03 | Boot-limits/claim checks; prepared entry and source-build limits named; protected asset and overclaim controls |
| BOOT-04 | Original assembly/tool/packer provenance, exact captured hello bytes, actual execution and negative transcript/overflow cases |
| RUN-01 | Fixed reset, profile/input/content identity, bounded canonical timed schedule, guest-time stop, strict real v2 result |
| RUN-03 | Real empty output at halt AND bound yields boot-fail, never pass/timeout |
| RUN-05 | Magic, 512-byte buffer, word/halfword byte lanes, exact unpadded 0/1/3/4/512-byte flushes, oversize/repeated/error behavior |
| PROF-05 | Same fixture/profile/input/bound yields equal state/log evidence across both compilers/OSes, interleave, partition and trace variations |

## Critical Negative Controls and Ownership

- Source replacement synchronized after supervisor verification cannot change
  runner-admitted bytes or their digest. Copy/hash/open/descriptor/schedule errors
  refuse launch; every exit closes/unlinks/reaps owned resources.
- Wrong text, no output, unsupported instruction, output overflow, invalid v2,
  unknown fields/version/identity, host wall kill/crash and report/capture failures
  never pass. Preserve legacy v1 goldens and native-model/guest separation.
- Independent expected ISA/bus/device/output values cite primary hardware facts
  or labeled software/model contracts; matching this emulator's output is not an
  independent hardware oracle.
- Existing sanitizer child must still demonstrate active fault detection. New
  CPU/bus/output/descriptor code receives relevant sanitizer checks; retain the
  established full admission campaign and its existing bounds when affected.
- Rights/PII/private-path checks run before evidence publication. No manual bytes,
  protected assets, raw host captures or unmanifested binaries enter repo/CI.

## Wave 0 and Task Ownership

Existing CTest infrastructure is available; no setup-only Wave 0 is needed.
New tests are authored and registered in the same slice as their behavior,
with deliberate failing controls. The first tracer extends the already
registered unit.core.tracer with actual pinned-entry execution; tool/linked-byte
qualification and the numeric software contract precede CPU expansion.
wave_0_complete remains false because feature tests have not been implemented.
Every plan has two or three tasks and each task modifies at most five files.

## Manual-Only Verifications

No required physical, UI or owner-held-file check is planned. Broad judgment
prohibitions remain prominently flagged/non-authoritative under the autonomous
soft-gate policy. Any concrete rights, authority or missing-evidence stop is kept.

## Validation Sign-Off

- [ ] Every executable task has immediate automated verify and fails-when
- [ ] Every required behavior/control has task/wave/threat ownership
- [ ] No three consecutive tasks lack executable verification
- [ ] Exact baseline plus added test registrations are enforced
- [ ] First actual IPL3/hello execution and strict guest-result path pass
- [ ] Current-source sanitizers and negative control pass
- [ ] Existing hosted Linux/GCC and macOS/Clang gates pass same inputs
- [ ] Nyquist coverage audited after implementation; no planned test counted as passed

**Approval:** pending implementation and review; no routine owner acknowledgment.

## Exact task, wave, requirement, threat and command ownership

Commands run from the checkout root after the owning plan's configure/build. Each command's PLAN fails-when states the semantic failing direction; no missing-test success is permitted. The core tracer also retains its existing exact identity and adds actual-entry assertions in 03-01/1.

| CTest identity | Task / wave | Requirement | Threat ref | Exact command |
|---|---|---|---|---|
| `unit.core.boot-entry` | 03-01/3; wave 1 | BOOT-01,BOOT-02 | T-03-01 | `ctest --preset gate --no-tests=error -R '^unit\.core\.boot-entry$' --output-on-failure` |
| `unit.core.cpu-path` | 03-02/1; wave 2 | BOOT-01,PROF-05 | T-03-03,T-03-04 | `ctest --preset gate --no-tests=error -R '^unit\.core\.cpu-path$' --output-on-failure` |
| `unit.core.cp0-cache` | 03-02/2; wave 2 | BOOT-01,PROF-05 | T-03-03,T-03-04 | `ctest --preset gate --no-tests=error -R '^unit\.core\.cp0-cache$' --output-on-failure` |
| `unit.core.warm-device-policy` | 03-03/1-2; wave 3 | BOOT-01,BOOT-03 | T-03-05,T-03-06 | `ctest --preset gate --no-tests=error -R '^unit\.core\.warm-device-policy$' --output-on-failure` |
| `unit.core.isviewer` | 03-04/1-2; wave 4 | RUN-05,BOOT-04 | T-03-07 | `ctest --preset gate --no-tests=error -R '^unit\.core\.isviewer$' --output-on-failure` |
| `runner.result-v2` | 03-06/1; wave 6 | RUN-01,RUN-03 | T-03-11,T-03-12 | `ctest --preset gate --no-tests=error -R '^runner\.result-v2$' --output-on-failure` |
| `runner.fixture-descriptor` | 03-05/1-2; wave 5 | RUN-01,BOOT-02 | T-03-09,T-03-10 | `ctest --preset gate --no-tests=error -R '^runner\.fixture-descriptor$' --output-on-failure` |
| `integration.open-ipl3-hello` | 03-06/2-3; wave 6 | BOOT-01,BOOT-04,RUN-01,RUN-05 | T-03-11,T-03-12 | `ctest --preset gate --no-tests=error -R '^integration\.open-ipl3-hello$' --output-on-failure` |
| `integration.open-ipl3-determinism` | 03-07/2; wave 7 | PROF-05,RUN-01 | T-03-14 | `ctest --preset gate --no-tests=error -R '^integration\.open-ipl3-determinism$' --output-on-failure` |
| `controls.real.empty-halt` | 03-07/1; wave 7 | RUN-03 | T-03-13 | `ctest --preset gate --no-tests=error -R '^controls\.real\.empty-halt$' --output-on-failure` |
| `controls.real.empty-bound` | 03-07/1; wave 7 | RUN-03 | T-03-13 | `ctest --preset gate --no-tests=error -R '^controls\.real\.empty-bound$' --output-on-failure` |
| `controls.real.wrong-transcript` | 03-07/1; wave 7 | BOOT-04,RUN-03 | T-03-13 | `ctest --preset gate --no-tests=error -R '^controls\.real\.wrong-transcript$' --output-on-failure` |
| `controls.real.unsupported-opcode` | 03-07/1; wave 7 | BOOT-01,RUN-03 | T-03-03,T-03-13 | `ctest --preset gate --no-tests=error -R '^controls\.real\.unsupported-opcode$' --output-on-failure` |
| `controls.real.output-overflow` | 03-07/1; wave 7 | RUN-05 | T-03-07,T-03-13 | `ctest --preset gate --no-tests=error -R '^controls\.real\.output-overflow$' --output-on-failure` |
| `controls.real.source-replaced` | 03-05/1-2; real repetition 06/3; wave 5 | RUN-01 | T-03-09,T-03-10 | `ctest --preset gate --no-tests=error -R '^controls\.real\.source-replaced$' --output-on-failure` |
| `controls.real.identity-mismatch` | 03-07/1; wave 7 | BOOT-02,BOOT-04,RUN-01 | T-03-01,T-03-11,T-03-13 | `ctest --preset gate --no-tests=error -R '^controls\.real\.identity-mismatch$' --output-on-failure` |
| `controls.real.input-partition` | 03-04/3; wave 4 | RUN-01,PROF-05 | T-03-08,T-03-14 | `ctest --preset gate --no-tests=error -R '^controls\.real\.input-partition$' --output-on-failure` |

All named tests remain pending implementation/execution. controls.real.source-replaced is initially a descriptor handoff test in wave 5 and is repeated through actual CPU-backed admission in wave 6; helper-only evidence cannot close SC6. Every consumer declares dependencies on the producer/checker/fixture targets it invokes. Input partition is a real core-backed scheduling/trace proof, with no SI/PIF consumption claim.

## Exact success-criterion ownership

| Roadmap criterion | Owning plan/tasks | Required evidence |
|---|---|---|
| SC1 | 03-01/1,3; 03-08/2 | Existing primary index checked first; exact document hashes/source locators/sections and claim classifications; no manual bytes retained. |
| SC2 | 03-01/2; 03-04/1–3; 03-06/1–3 | Authored exact transcript, actual IPL3/DMA/hello milestones, fixed reset/profile/input/guest bound, scheduled trace and strict real v2. |
| SC3 | 03-01/1–3; 03-07/1; 03-08/1–2 | Exact IPL3/tool/fixture identities, separate rights/owner records and protected-byte scan over repo plus staged CI outputs. |
| SC4 | 03-01/3; 03-06/1; 03-07/1 | Actual empty halt/bound boot-fail and explicit PIF/CIC/cold/timeout limitations. |
| SC5 | 03-07/2; 03-08/1 | Current Linux/GCC and macOS/Clang gates with exact same guest identities/state, truthful different build IDs. |
| SC6 | 03-05/1–2; 03-06/2–3 | Barrier replacement, read-only inherited unlinked fd, digest of actual admitted buffer and original guest output; no caller-path reopen. |

## Sampling and inventories by completed plan

| After plan | Added identities total | Gate / ASan expected | Registration protection |
|---|---|---|---|
| 03-01 | 1 | 74 / 76 | boot-entry; existing tracer is extended without renaming. |
| 03-02 | 3 | 76 / 78 | cpu-path, cp0-cache. |
| 03-03 | 4 | 77 / 79 | warm-device-policy. |
| 03-04 | 6 | 79 / 81 | isviewer, input-partition. |
| 03-05 | 8 | 81 / 83 | fixture-descriptor, source-replaced. |
| 03-06 | 10 | 83 / 85 | result-v2, open-ipl3-hello. |
| 03-07/1 | 16 | 89 / 91 | Six real negative controls. |
| 03-07/2 and 03-08 | 17 | 90 / 92 | open-ipl3-determinism; per-name omission selftest over complete inventory. |

Do not run the full gate between a task registering a new test and the immediately following task adding its exact inventory entry. The registered owning CTest runs immediately; every completed plan restores the coherent exact gate inventory. The first plan wires verified fixture provisioning into the existing gate so early GCC evidence can run without the later CI-evidence enhancements. Core additions are included by existing source glob and source/object lint.

## Final current-source commands and failure directions

- `cmake --preset asan` — fails when active address/undefined instrumentation is absent or configuration rejects a target.
- `cmake --build --preset asan` — fails when any target/producer/checker fails; this is the full all-target build before test selection.
- `sh scripts/check-test-inventory.sh --preset asan` — fails when any of the exact 92 names differs, is disabled, duplicated or missing.
- The complete selected ASan/UBSan command in 03-08/2 uses `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, includes the real CPU/output/descriptor/report controls, admission campaign and `sanitizer.negative-control`, and fails on either production defects or an inactive intentional fault child.
- `./scripts/gate.sh` — fails for missing authorized fixtures, exact inventory drift, any semantic/protocol control, rights/PII/provenance or source/object core-lint failure.
- `sh scripts/compare-boot-evidence.sh build/evidence/gate-linux.json build/evidence/gate-macos.json` — fails for missing/nonpassing/stale hosted lane, unequal guest identities/state or missing exact transcript; truthful compiler build-ID difference is allowed.

## Spec-less probes and prominent flags

The fourteen surfaced edges are tracked without silent dismissal in 03-PROBE-LEDGER.md. Nine classified rows have explicit planned predicates; five originally unclassified rows retain prominent unresolved-classification flags alongside newly authored concrete acceptance predicates. They are not automatically green, and no execution evidence is fabricated. Three descriptor-less judgment prohibitions are in 03-08 must_haves.prohibitions: warm/hardware claim inflation, trace-as-controller claims and identity/rights/source-rebuild claim inflation. All remain non-authoritative, human review recommended, with no recorded human validation or accepted risk. AUTONOMY's soft gate applies unless an actual concrete rights/authority/evidence stop exists.

Full multi-source/decision/dependency coverage is in 03-SOURCE-AUDIT.md. No external runtime API/ORM/schema push applies; COVERAGE.md records the reasoned detector disposition. Source qualification and software numeric choices are implementation work, not owner permission checkpoints. No source/test/toolchain implementation or Phase 3 build/test was run while authoring these plans.
