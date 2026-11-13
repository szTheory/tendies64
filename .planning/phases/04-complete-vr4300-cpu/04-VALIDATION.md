---
phase: "04"
slug: complete-vr4300-cpu
status: validated
nyquist_compliant: true
wave_0_complete: true
created: "2026-11-11"
---

# Phase 4 validation contract

Execution is complete. All ten phase requirements have automated behavioral
owners and passing evidence, including the separately qualified local
completeness gate. Earlier planning statements below remain historical where
superseded by the final audit. Wave0 is complete by non-applicability: existing
infrastructure suffices; new tests arrived with their behavior.

## Infrastructure and inherited limits

| Property | Contract |
|---|---|
| Framework | C17, CMake/Ninja, CTest, existing shared assertion/canonical JSON/SHA helpers |
| Existing inventory | Preserve exactly91 gate/93 ASan names, including every old warm/hello/fault/fuzz control |
| Final planned inventory |111 gate/113 ASan;20 specific additions listed below, no arbitrary padding |
| Configure/build | cmake --preset gate; cmake --build --preset gate (all targets) |
| Focused verification | ctest --preset gate --no-tests=error -R '^exact-owning-name-or-alternation$' --output-on-failure, following successful build |
| Whole local gate | sh scripts/gate.sh |
| Exact inventory | sh scripts/check-test-inventory.sh --preset gate and --preset asan; --selftest rejects every missing name plus malformed/duplicate/disabled/unexpected sets |
| Instrumentation | cmake --preset asan; full all-target cmake --build --preset asan before focused execution |
| Sanitizer environment | ASAN_OPTIONS=detect_leaks=0:halt_on_error=1; UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1, inherited from Phase03 and scripts/expect-sanitizer.sh |
| Existing resource controls | Keep admission fuzz120-second timeout and current corpus/allocation/iteration/resource bounds; keep intentional sanitizer fault child; no new timeout/CI/concurrency/cache lane |
| External diagnostic | Local-only separate adapter; no public CI/input/capture/result/upload; all private files under ignored build/private-cpu-diagnostic |
| Duration | New runtimes unknown; measure actual batch/test cost and diagnose bounds; no assumed feedback estimate |

Each automated command has an adjacent fails_when clause. Empty test selection fails via --no-tests=error. New script commands are implemented in the owning behavioral slice before first use and have offline authored refusal controls. Tests/apps stay headless/background. A timeout alone does not authorize foreground focus or a larger resource bound.

## Exact additions and cumulative inventories

Rows follow execution order. In wave1,01 and14 touch disjoint paths;01 uses already registered tests,14 owns the runner registrations/inventory additions. All later shared registration updates serialize.

| Plan | Newly protected CTest names | Cumulative gate / ASan |
|---|---|---:|
| 04-01 | No new names; existing owning tests expanded | 91 / 93 |
| 04-14 | runner.cpu-diagnostic-protocol, runner.cpu-diagnostic-inventory | 93 / 95 |
| 04-02 | unit.core.cpu-exceptions | 94 / 96 |
| 04-03 | unit.core.cpu-integer, unit.core.cpu-control | 96 / 98 |
| 04-04 | unit.core.cpu-cp0 | 97 / 99 |
| 04-05 | unit.core.cpu-time | 98 / 100 |
| 04-06 | unit.core.cpu-mmu | 99 / 101 |
| 04-07 | unit.core.cpu-memory | 100 / 102 |
| 04-08 | unit.core.cpu-link | 101 / 103 |
| 04-09 | unit.core.cpu-icache | 102 / 104 |
| 04-10 | unit.core.cpu-dcache, unit.core.cpu-cache-ops | 104 / 106 |
| 04-11 | unit.core.softfloat32, unit.core.softfloat64, unit.core.softfloat-isolation | 107 / 109 |
| 04-17 | No new names; existing raw arithmetic edge cases expanded before consumers | 107 / 109 |
| 04-12 | unit.core.cop1-state | 108 / 110 |
| 04-13 | unit.core.cop1-arithmetic, unit.core.cop1-exceptions | 110 / 112 |
| 04-15 | runner.cpu-diagnostic | 111 / 113 |
| 04-18 | No new names; public native verification/evidence closeout | 111 / 113 |
| 04-16 | No new names; existing owning tests expanded | 111 / 113 |

No old baseline set is renamed or removed. PHASE04_TESTS is appended to expected_names for both presets; ASAN_ONLY remains the original two controls. Register all names and preserve --selftest removal controls. No external ROM membership count is placed in CTest; it has a separate exact private manifest.

## Task / requirement / test / threat ownership

The following is the exact plan-task mapping. Tracer rows extend immediately registered existing tests where a new file would otherwise require a setup-only registration pass. Expansion rows add their own exact names in the same task.

| Task | Requirements | Immediate automated behavioral owners | Threat refs |
|---|---|---|---|
| 04-01.1 | CPU-01, CPU-03, CPU-09 | unit.core.boot-entry, unit.core.profile | T-04-01, T-04-02, T-04-SC |
| 04-01.2 | CPU-01, CPU-03, CPU-09 | unit.core.boot-entry, unit.core.profile, unit.core.digest-trace | T-04-01, T-04-02, T-04-SC |
| 04-02.1 | CPU-02, CPU-03, CPU-09 | unit.core.cpu-path, unit.core.boot-entry | T-04-03, T-04-04, T-04-SC |
| 04-02.2 | CPU-02, CPU-03, CPU-09 | unit.core.cpu-exceptions, unit.core.cpu-path | T-04-03, T-04-04, T-04-SC |
| 04-03.1 | CPU-01, CPU-02, CPU-09 | unit.core.cpu-path, unit.core.cpu-exceptions | T-04-05, T-04-06, T-04-SC |
| 04-03.2 | CPU-01, CPU-02, CPU-09 | unit.core.cpu-integer, unit.core.cpu-control, unit.core.cpu-path | T-04-05, T-04-06, T-04-SC |
| 04-04.1 | CPU-03, CPU-04, CPU-09 | unit.core.cpu-path, unit.core.cpu-exceptions | T-04-07, T-04-08, T-04-SC |
| 04-04.2 | CPU-03, CPU-04, CPU-09 | unit.core.cpu-cp0, unit.core.cpu-path | T-04-07, T-04-08, T-04-SC |
| 04-05.1 | CPU-03, CPU-09 | unit.core.cpu-path, unit.core.cpu-cp0, unit.core.scheduler | T-04-09, T-04-10, T-04-SC |
| 04-05.2 | CPU-03, CPU-09 | unit.core.cpu-time, unit.core.cpu-cp0, unit.core.scheduler | T-04-09, T-04-10, T-04-SC |
| 04-06.1 | CPU-03, CPU-04, CPU-09 | unit.core.cpu-path, unit.core.cpu-exceptions | T-04-11, T-04-12, T-04-SC |
| 04-06.2 | CPU-03, CPU-04, CPU-09 | unit.core.cpu-mmu, unit.core.cpu-cp0, unit.core.cpu-exceptions | T-04-11, T-04-12, T-04-SC |
| 04-07.1 | CPU-04, CPU-05, CPU-09 | unit.core.cpu-path, unit.core.cpu-mmu, unit.core.bus | T-04-13, T-04-14, T-04-SC |
| 04-07.2 | CPU-04, CPU-05, CPU-09 | unit.core.cpu-memory, unit.core.cpu-path, unit.core.bus | T-04-13, T-04-14, T-04-SC |
| 04-08.1 | CPU-05, CPU-09 | unit.core.cpu-path, unit.core.pi-dma, unit.core.sp-dma | T-04-15, T-04-16, T-04-SC |
| 04-08.2 | CPU-05, CPU-09 | unit.core.cpu-link, unit.core.cpu-path, unit.core.digest-trace | T-04-15, T-04-16, T-04-SC |
| 04-09.1 | CPU-06, CPU-09 | unit.core.cpu-path, unit.core.cpu-mmu | T-04-17, T-04-18, T-04-SC |
| 04-09.2 | CPU-06, CPU-09 | unit.core.cpu-icache, unit.core.cpu-path, unit.core.cp0-cache | T-04-17, T-04-18, T-04-SC |
| 04-10.1 | CPU-05, CPU-06, CPU-09 | unit.core.cpu-path, unit.core.cpu-link, unit.core.pi-dma, unit.core.sp-dma | T-04-19, T-04-20, T-04-SC |
| 04-10.2 | CPU-05, CPU-06, CPU-09 | unit.core.cpu-icache, unit.core.cpu-dcache, unit.core.cpu-cache-ops, unit.core.cpu-link | T-04-19, T-04-20, T-04-SC |
| 04-11.1 | CPU-07, CPU-09 | unit.core.cpu-path | T-04-21, T-04-22, T-04-SC |
| 04-11.2 | CPU-07, CPU-09 | unit.core.softfloat32, unit.core.softfloat64, unit.core.softfloat-isolation, gov.provenance.repo | T-04-21, T-04-22, T-04-SC |
| 04-17.1 | CPU-07, CPU-09 | unit.core.softfloat32, unit.core.softfloat64, unit.core.softfloat-isolation, unit.core.cpu-path | T-04-33, T-04-34, T-04-SC |
| 04-17.2 | CPU-07, CPU-09 | unit.core.softfloat32, unit.core.softfloat64, unit.core.softfloat-isolation, unit.core.cpu-path | T-04-33, T-04-34, T-04-SC |
| 04-12.1 | CPU-02, CPU-07, CPU-09 | unit.core.cpu-path, unit.core.softfloat-isolation | T-04-23, T-04-24, T-04-SC |
| 04-12.2 | CPU-02, CPU-07, CPU-09 | unit.core.cop1-state, unit.core.cpu-control, unit.core.cpu-memory | T-04-23, T-04-24, T-04-SC |
| 04-13.1 | CPU-07, CPU-09 | unit.core.cpu-path, unit.core.cop1-state, unit.core.softfloat32 | T-04-25, T-04-26, T-04-SC |
| 04-13.2 | CPU-07, CPU-09 | unit.core.cop1-arithmetic, unit.core.cop1-exceptions, unit.core.cop1-state, unit.core.softfloat32, unit.core.softfloat64, unit.core.softfloat-isolation | T-04-25, T-04-26, T-04-SC |
| 04-14.1 | VER-01, VER-03, CPU-09 | runner.cpu-diagnostic-protocol | T-04-27, T-04-28, T-04-SC |
| 04-14.2 | VER-01, VER-03, CPU-09 | runner.cpu-diagnostic-protocol, runner.cpu-diagnostic-inventory | T-04-27, T-04-28, T-04-SC |
| 04-15.1 | VER-01, VER-03, CPU-09 | runner.cpu-diagnostic, runner.cpu-diagnostic-protocol, runner.cpu-diagnostic-inventory | T-04-29, T-04-30, T-04-SC |
| 04-15.2 | VER-01, VER-03, CPU-09 | runner.cpu-diagnostic, unit.core.boot-entry, unit.core.profile, unit.core.rom-admission | T-04-29, T-04-30, T-04-SC |
| 04-18.1 | CPU-09, VER-03 | Full native gate; full ASan build then exact owning CPU/legacy/protocol/adapter/fuzz/fault selection;111/113 inventory/selftest | T-04-35, T-04-36, T-04-SC |
| 04-18.2 | CPU-09, VER-03 | rights/PII/provenance tree/history checks; configured reviews and actual exact-source gate-linux/gate-macos run records | T-04-35, T-04-36, T-04-SC |
| 04-16.1 | VER-01, VER-03, CPU-09 | gate owners plus all-target ASan rebuild and all three authored diagnostic controls; qualified private script step as specified in plan | T-04-31, T-04-32, T-04-SC |
| 04-16.2 | VER-01, VER-03, CPU-09 | gate owners plus all-target ASan rebuild and all three authored diagnostic controls; qualified private script step as specified in plan | T-04-31, T-04-32, T-04-SC |
| 04-16.3 | VER-01, VER-03, CPU-09 | gate owners plus all-target ASan rebuild and all three authored diagnostic controls; production --run-all/--verify-complete canonical batch aggregation | T-04-31, T-04-32, T-04-SC |

There are36 unique numbered threats (two per plan) plus one identical shared supply-chain row. Each plan's trust boundary/register has severity and disposition and names specific implementation and tests; ASVS1/high findings block affected use. All37 tasks have immediate automated failure directions; Plan18 is verification-only, may start green and carries no inapplicable TDD marker. All feature owners retain immediate behavioral evidence.

## Requirement acceptance and independent expectations

| Requirement | Positive / edge / negative obligation | Owners |
|---|---|---|
| CPU-01 | Complete MIPSIII integer matrix,32-bit extension,64-bit arithmetic/shift/HI-LO, divide/extrema/overflow/RI and zero-register effects |01,03; cpu-integer |
| CPU-02 | Full-width branch/jump/link,likely annulment,delay faults,nested oldEXL preserves both EPC/BD,ERET and COP1 branches |02,03,12; cpu-control/cpu-exceptions/cop1-state |
| CPU-03 | Every CP0 field/transfer width/side effect; Status defined0xFF57FFFF→RP-fixed0xF757FFFF,TS preserved; Count phase/equality/interrupt/write order independent retirement |01,02,04,05; cpu-cp0/cpu-time |
| CPU-04 | All32 paired entries/seven masks/ASID/both-G/V/D,32/64 segments/privileges,refill/invalid/modified/XTLB/address faults,duplicate-match TS and staged TLB ops |06,07; cpu-mmu/cpu-memory |
| CPU-05 | Aligned widths/all merge offsets,RE/endian/page boundaries,LL/LLD/SC/SCD physical/width/one-shot/fault priority,ERET/overlap/cache invalidation |07,08,10; cpu-memory/cpu-link/cache-ops |
| CPU-06 |16KiB/32-byte I +8KiB/16-byte D,virtual index/physical tag,cacheability/stale aliases/dirty/writeback/tags/all documented primary CACHE ops,undefined atomic refusal and written scope |09,10; cpu-icache/cpu-dcache/cache-ops |
| CPU-07 | Exact permitted explicit-context binary32/64 integer closure,4RM,FR/FCR31/FS/E/NaN/subnormal/conversion/trap/destination suppression,source/object lint and inter-instance isolation |11,17,12–13; softfloat/copy/lint/cop1 tests |
| CPU-09 | Every expected native value has primary citation,permitted hardware vector or independently recorded integer/bit/rational derivation; every row has edge/negative coverage; never same implementation or another-emulator goldens | Every owner; opcode/vector coverage tables and probe ledger |
| VER-01 | Qualified pinned local source/tool/assets/IPL3/actual startup; preserve all required CPU registrations/values; execute complete bounded fresh batches and report every group honestly, including executed fails |14–16; strict adapter and private all-batch verification |
| VER-03 | Exact111/113 native sets plus separate complete source/name/value/group manifest; reject zero/missing/reduced/unknown/duplicate/disabled/truncated/unreached |02–16; inventory selftest and diagnostic-inventory |

Raw edge detector18 predicates and five unclassified flags are retained unchanged. The probe ledger maps each predicate to explicit must_haves.truths and exact automated owners. Four non-authoritative bespoke claim constraints route to judgment review; do not mint unsupported wired descriptors or claim human validation. AUTONOMY permits completion with those judgment flags only after all automated requirements and real stops are clear.

## Private diagnostic completeness contract

Pin nemu64-test9a8b9f7d94ee2f6f57d7feed70c98c22cdc30e6c. Required CPU module map is address_error_exception,arithmetic,cache,cop0,cop1,cop_unusable,endian_re,exception_instructions,jumps,overflow_exception,privilege,tlb,tlb64,traps. Seal every BasicFunctionality/RarelyUsed CPU test's exact name/value denominator; separately account for all other levels and non-CPU registrations. Constructor758 and README262 do not define runtime denominators. Actual exact runtime denominator is derived from the pinned source/Test.values implementations and executor convention before first build, then frozen in private required-inventory.json; it is not known or fabricated by this planning-only pass.

One required value per fresh machine is a deterministic batch. Keep all test code linked, qualify upstream lifecycle/setup equivalence, patch only a verified overlay selector word and bind base/selector/variant ROM hashes. Compact start/value/done≤4096 preserves complete lifetime capture without raising caps, consuming canonical state or losing bytes. Completion requires exact source/tool/overlay/inventory/profile/build/ROM identities, valid output/traced park-loop milestones and the complete cross-batch union. Adjacent batches may not omit/duplicate a value. Group status records expected/reached/pass/fail and explicit nonrequired skips. All required values must be reached; an executed fail is honest complete diagnostic status, a blocked/missing/unreached batch leaves SC4 unmet.

Before any affected use: qualify source/lock/checksum/source correspondence/build hooks, official Rust channel/components and every actual pinned test Cargo dependency/resolved graph; nust64 is excluded and must not be installed/invoked; remove Geneva/font/framebuffer and non-CPU startup; explicitly select the already cleared IPL3 and verify final image assets/format/startup. All ROM/capture/derived metadata/status stay ignored/private. No public hello allowlist or collector widening occurs.

## Sampling and final verification

After each behavior/register change, build then run the owning exact selection; run inventory selftest/current preset after registration updates. Coherent boundaries01,05,10,13,15 and native close18 run the full native gate once on final source. Reuse that evidence for metadata-only changes. After review fixes run targeted owning retests plus necessary regression checks; no broad repetition solely for bookkeeping.

Plan18 runs the full native gate, configures/builds the entire ASan tree, runs the focused instrumented CPU/legacy warm/new adapter/fuzz/fault set and exact inventory selftest, then configured reviews/fixes and the existing two hosted jobs on the exact eligible public source. It precedes private Plan16, so a private tool/asset stop does not block native evidence. Plan16 runs private all-batch/complete verification through the shared production protocol/aggregator and refreshes affected native checks/evidence after any later source fixes; no broad check is repeated solely for metadata. Public native evidence is sanitized before commit; raw logs, derived external counts/statuses, ROMs and captures are excluded. Parent owns normal summary/state/roadmap closeout. Rights/PII/provenance checks and configured code/security reviews remain required before public push.

## Planning sign-off

- [x] Every task has immediately paired automated/fails_when and measurable acceptance.
- [x] All ten requirements, four success criteria and D-01–D-13 have implementation/test owners.
- [x] Exact native inventory additions, private manifest semantics, waves and file conflicts are fixed.
- [x] Every security threat has mitigation/test ownership; private/public boundaries remain explicit.
- [x] No setup-only Wave0, no deferred umbrella tests, no new public CI or host-FP path.
- [x] Actual implementation, per-use qualifications, complete private reporting and applicable checks/reviews pass.

No human approval or risk acceptance is claimed. A concrete qualification failure blocks its affected use; native tests cannot substitute for SC4.

## Revision dependency and acceptance boundary

Execution order is wave1:01+14; waves2–11:02–11; wave12:17 raw arithmetic edge expansion; wave13:12; wave14:13; wave15:15; wave16:18 public native closeout; wave17:16 private qualification/build/complete run. Shared modified paths serialize. Plan11 retains the full per-file audited copy and all immediate production positive/negative/context/lint assertions; Plan17 expands the broad independent matrix before consumers. No registrations change in17/18.

One shared CMake t64_cpu_diag_protocol target implements t64.cpu-inventory v1 and t64.cpu-batch v1. The production runner and public authored controls link it. Plan15 controls invoke the actual adapter for malformed records, missing done, wrong identity, duplicate adjacent batches and omitted required values, with a complete positive set. Plan16 --run-all preserves those exact production records, and --verify-complete calls the same production CLI aggregator against the sealed canonical inventory. Its routing controls reach that executable, not a substitute parser or source grep; only the shared aggregator computes complete group status. Neither lookalike JSON nor independent script sums may establish SC4.

Plan16's10 paths are four implementation/input paths and six ignored generated private outputs; the three sequential qualification/build/run tasks are a bounded57k unit. Plan11's59k atomic closure plus Plan17's25k expansion replaces the former76k combined scope with explicit separate-executor overhead; Plan18's19k owns public native closeout. Calibration remains factor1, zero samples, low confidence. Actual implementation/evidence remains pending.

## Iteration 2 sanitizer evidence correction

Plan16 follows Plan18 and modifies tracked adapter C/tests. Every Plan16 task now explicitly configures and builds ALL ASan targets after its owning C/CTest changes, then runs exactly runner.cpu-diagnostic, runner.cpu-diagnostic-protocol and runner.cpu-diagnostic-inventory using the inherited sanitizer environment. Each automated command has an immediate fails_when covering build failure, failed instrumented assertions and missing/empty controls. The exact focused command is:

`cmake --preset asan && cmake --build --preset asan && ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^(runner\.cpu-diagnostic|runner\.cpu-diagnostic-protocol|runner\.cpu-diagnostic-inventory)$' --output-on-failure`

These are public authored controls through the verified production CLI/shared protocol, never private ROM/results. Any changed code/test invalidates that portion of Plan18's earlier evidence; record the new owning run rather than relabeling old evidence. Core or other subsystem fixes additionally require affected subsystem checks/review and the already specified conditional broader regression/public-native evidence refresh. Reuse unaffected broad/hosted evidence and do not repeat checks solely for metadata. Plan18 remains before private qualification; 18 plans,17 waves,37 tasks and111/113 inventories are unchanged. No implementation, build, test, CI or package use was performed during this planning correction.

## Compatibility validation extension

Plans19/20 add no CTest names or dependencies. They extend the existing
profile/boot/digest, integer/control/link, CP0/MMU/memory/cache, COP1 and three
diagnostic owners. The exact native inventories remain111/113. Their portable
RED/GREEN, review-fix and validation records distinguish actual behavior
failures from fixture setup mistakes. Selected undefined-operation policies
are software contracts, not independently measured hardware expectations.

| Task cluster | Requirements | Existing automated owners | Registered threats |
|---|---|---|---|
| 04-19 profile/branch/CP0/link policies | CPU-09, VER-01, VER-03 | profile, boot-entry, digest-trace, cpu-integer/control/cp0/link, cop1-state and all three diagnostic controls | T-04-38, T-04-39 |
| 04-20 cache/COP2/FR0/CP0/address policies | CPU-09, VER-01, VER-03 | profile, boot-entry, cpu-path/integer/cp0/mmu/memory/cache, cop1-arithmetic/state/exceptions and all three diagnostic controls | T-04-40, T-04-41, T-04-42 |

The current diagnostic contract supersedes planning-era version references:
inventory v3, batch v2 and qualified seal v2 select the immutable v2 profile;
legacy seal v1 is strict and --compat retains the published v1 profile.
The source-faithful default-base predicate includes BasicFunctionality,
RarelyUsed and Weird. Raw undefined PageMask fields remain untranslated;
TLBR projects them under v2 only. All execution limits and publication stops
are retained. Historical planning counts/projections above are not current
execution results.

## Final validation coverage audit — 2026-11-13

The task map above covers all original39 task rows and the two subsequent
compatibility clusters. Every requirement has a registered behavioral owner,
negative/edge controls and executed evidence. No missing automated coverage
was found, so no test-only agent or duplicate gate was dispatched.

| Requirement | Final evidence | Coverage |
|---|---|---|
| CPU-01 | cpu-integer/path; full final-source Linux/macOS native gate | COVERED |
| CPU-02 | cpu-control/exceptions and COP1 branches; explicit nested annul side-effect/retirement regression | COVERED |
| CPU-03 | cpu-cp0/time with independent masks, clock and fault-order assertions | COVERED |
| CPU-04 | cpu-mmu/memory and fault/privilege/TLB matrices | COVERED |
| CPU-05 | cpu-memory/link with width/merge/reservation/refused-effect controls | COVERED |
| CPU-06 | cpu-icache/dcache/cache-ops plus written scope and v2 isolation | COVERED |
| CPU-07 | softfloat32/64/isolation, cop1-state/arithmetic/exceptions, host-FP source/object lint | COVERED |
| CPU-09 | Cited/derived native matrices, independent profile/state reference and explicit setup/RED provenance | COVERED |
| VER-01 | Qualified pinned local build/startup and unchanged shared C complete-set verification | COVERED |
| VER-03 | Exact111/113 native inventories/removal controls and separately sealed private exact membership/aggregation | COVERED |

Final-source hosted run43476881467 passes both complete native gates and
publication tails. Local final all-target normal/instrumented builds and six
owning COP1/diagnostic checks pass after the conversion correction; earlier
broader sanitizer/fuzz evidence is reused within its stated unchanged scope.
Plan16's private authoritative gate succeeds with preserved limits. Its
complete reporting is not an all-passing or hardware-accuracy claim.

The four judgment-tier transparency prohibitions remain non-authoritative
unverified flags, and the five unclassified probe flags remain visible in the
probe ledger. They do not erase any automated obligation. No physical review,
public diagnostic rights or human acceptance is invented. The explicit
AUTONOMY soft gate governs routing with those disclosed flags.

## Validation Audit 2026-11-13

| Metric | Count |
|---|---|
| Gaps found | 0 |
| Resolved | 0 |
| Escalated | 0 |
