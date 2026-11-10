---
phase: "02"
slug: "deterministic-machine-skeleton"
status: validated
nyquist_compliant: true
wave_0_complete: true
created: "2026-11-10"
---

# Phase 2 — Validation Strategy

> Local implementation, independent source review and corrected sanitizer
> evidence are complete. Hosted CI remains pending; phase completion has not
> been claimed.

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest + project-authored C assertions in `tests/t64_test.h`; no third-party framework |
| **Config file** | `CMakeLists.txt`, `CMakePresets.json`, `scripts/gate.sh`, `scripts/check-test-inventory.sh`, core lint |
| **Quick run command** | `ctest --preset gate --no-tests=error -R '^unit\.core\.determinism$' --output-on-failure` (single registered seam; full inventory is enforced by the full gate) |
| **Full suite command** | `./scripts/gate.sh` |
| **Inventory control** | `sh scripts/check-test-inventory.sh --selftest` checks 73 gate and 75 ASan names plus 148 single-removal and five malformed-inventory controls. |
| **Sanitizer command** | `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^(unit\.core\..*|unit\.model-result|fuzz\.rom-admission|sanitizer\.negative-control)$' --output-on-failure` |
| **Measured runtime** | Final local gate: 51s total, including 13s inventory-removal selftest. Final portability correction: full ASan build and all 14 selected tests passed in 44.58s; complete production ROM mutation campaign 44.57s. `02-FINAL-LOCAL-EVIDENCE.md` records final source/blob identity and supersedes earlier split/stale records. |

The repository's `asan` preset enables AddressSanitizer and
UndefinedBehaviorSanitizer. LeakSanitizer is unsupported in this host
environment, so the completed sanitizer run used `detect_leaks=0`; both address
and undefined-behavior instrumentation remained enabled. The existing gate
checks the exact inventory before CTest and still runs core lint without adding
a hosted job.

## Sampling Rate

- **After each task:** run the targeted CTest command below.
- **After each plan wave:** run `./scripts/gate.sh` (the final run passed).
- **Before phase verification:** require the full gate on the existing Linux and
  macOS CI lanes and complete the configured review/verification steps.
- **Feedback target:** the final gate took 51 seconds; exact-inventory mutation
  selftests took 13 seconds within that run.

## Per-Contract Verification Map

| Contract / requirement | Test type | Automated command | File exists? | Status |
|------------------------|-----------|-------------------|--------------|--------|
| Allocation callbacks, alignment/alias checks, allocation denial and transactional replacement (PROF-06, BUS-04) | unit | final `./scripts/gate.sh`, `unit.core.lifecycle` | ✅ yes | ✅ pass |
| Profile/build identity, profile limits, header-country independence and target-vs-boot distinction (PROF-01–04) | unit + result seam | final `./scripts/gate.sh`, `unit.core.profile` | ✅ yes | ✅ pass |
| Queue order/capacity/sequence exhaustion, inclusive idle bound, same-tick counter stop, halt and quiescence (CORE-01–03) | unit | final `./scripts/gate.sh`, `unit.core.scheduler` | ✅ yes | ✅ pass |
| Physical map, RDRAM last-byte boundary, no-alias unsupported results and MI pending/mask behavior (BUS-01) | unit | final `./scripts/gate.sh`, `unit.core.bus` | ✅ yes | ✅ pass |
| PI register masks/readback, busy/error/IRQ, reset cancellation token, exact model deadline and destination visibility (BUS-01–02) | unit | final `./scripts/gate.sh`, `unit.core.pi-dma` | ✅ yes | ✅ pass |
| SP row/line/skip decode, low-bit alignment, completion-time source read, bank wrap, active+pending queue and third-request policy (BUS-02) | unit | final `./scripts/gate.sh`, `unit.core.sp-dma` | ✅ yes | ✅ pass |
| Canonical digest field inclusion/exclusion, interleaved instances, trace-on/off and visible overflow (CORE-06, PROF-06) | unit | final gate and ASan run, `unit.core.digest-trace` | ✅ yes | ✅ pass |
| Raw ROM size/order normalization, field reads, refusal atomicity, max boundary and production admission path (BUS-04) | unit | final gate and ASan run, `unit.core.rom-admission` | ✅ yes | ✅ pass |
| Seeded mutation campaign through production admission + normalization; all three orders, short prefixes and every exposed header field under ASan/UBSan (BUS-04) | deterministic fuzz | ASan run: `fuzz.rom-admission` (seed `0x7a64c0de`; 250,000 mutations + 148 fixed cases) | ✅ yes | ✅ pass, 44.57s |
| Separate seeded memory fault is actually caught by sanitizer without failing the parent CTest process | negative control | ASan run: `sanitizer.negative-control` | ✅ yes | ✅ pass |
| Closed native model schema, scope and assertion counts (PROF-02/04) | consumer | final gate and ASan run, `unit.model-result` | ✅ yes | ✅ pass |
| Real model probe identity; legacy result/identity/seal/suite/claims rejection (PROF-01–04) | producer/consumer seam | final gate and ASan run, `unit.core.profile-result` | ✅ yes | ✅ pass |
| Cross-device isolated/interleaved/partition/observer equality (CORE-01–03/06, PROF-06) | seam | final gate and ASan run, `unit.core.determinism` | ✅ yes | ✅ pass; 1,933 assertions |
| Every required registration present; every single-name removal fails with all others remaining (D-14) | inventory | public gate selftest and exact inventory: 73 gate / 75 ASan | ✅ yes | ✅ pass; 148 removal and five malformed controls |

Expected values are independently calculated from the selected model and
authored test data. These native checks are not hardware-derived evidence. No
ROM, PIF, IPL, firmware or commercial fixture was added to this corpus.

## Required Negative Controls

All controls below were exercised by the final local gate, the ASan/UBSan
component run, or the inventory selftest as shown in the verification map.

- **Admission refusal:** start from a valid synthetic image and non-default
  machine state; attempt null, short, oversized, non-word-aligned and unknown-
  order spans plus allocator denial; assert digest, image ownership, registers,
  queues and observable outputs stay unchanged.
- **Production fuzz path:** each deterministic mutation calls the same core
  admission/normalization function used by callers. Exercise all three valid
  order signatures; keep the signature valid in field mutations so the parser
  and every field-dependent read execute. Separately mutate signature and
  length/order boundaries to cover early refusal paths. Use runtime-authored
  byte arrays, pinned seed `0x7a64c0de`, and a bounded campaign of 250,000
  mutations plus fixed boundary cases from Decision C. Record case inventory and
  fields/branches reached per the locked D-13 requirement.
- **Completion-time visibility:** change a synthetic SP source region between
  DMA acceptance and completion; destination must receive the completion-time
  bytes and remain unchanged before that event.
- **Scheduler resource faults:** force queue-full and sequence-exhaustion through
  a test seam; request must refuse with a stable result before mutation. Verify
  same-tick count stopping leaves later events queued.
- **PI reset:** cancel an active transfer, assert no destination copy at its
  completion, busy clears and one PI interrupt is raised; repeated reset or a
  stale token cannot duplicate the interrupt or mutate a newer request.
- **Trace observer:** overflow the ring and assert a visible drop count and the
  same final digest as tracing disabled.
- **Model/guest boundary:** actual t64.model-result v1 probe bytes must be
  accepted by the strict model checker and rejected by unchanged result,
  identity, seal, suite and claims tooling, including a valid suite-footer
  wrapper. Require a positive legacy harness control and unchanged v1 bytes.
- **Exact registration presence:** before CTest the public gate compares the
  complete explicit 73-name set; asan requires 75. The same production checker
  must reject each single-name deletion in isolated CTest trees while all other
  registrations remain. Include duplicate/unknown/disabled/empty/enumeration-
  error controls; grouped filters and total-only counts are insufficient.
- **Sanitizer control:** run an intentionally faulty child executable under the
  sanitizer and have the test harness require both a nonzero exit and a matching
  sanitizer report; do not let the parent CTest process itself crash.

## Wave 0 Requirements

- [x] Register native component tests for lifecycle, profile, scheduler, bus,
  PI DMA, SP DMA, digest/trace and ROM admission.
- [x] Add the bounded production admission/normalization mutation driver and its
  field/branch/case inventory under the asan preset.
- [x] Add a separate sanitizer negative-control child and parent test.
- [x] Make the exact SHA-256 implementation linked into the core part of the
  linted core source set.
- [x] Preserve current `./scripts/gate.sh` as the single full local/CI command.
- [x] Add model checker/probe tests and legacy evidence rejection.
- [x] Add exact complete inventory and single-registration removal controls
  before CTest in the public gate.

## Manual-Only Verifications

None for Phase 2. Physical hardware measurements are out of scope; unknown
timing and edge behavior remains explicitly policy-labeled until such evidence is
authorized and available.

## Validation Sign-Off

- [x] All tasks have an automated verify or Wave 0 dependency
- [x] No three consecutive tasks lack automated verification
- [x] Wave 0 covers all missing test references
- [x] No watch-mode flags
- [x] `nyquist_compliant: true` after planner/executor verify coverage
- [ ] Full existing gate green on both current hosted lanes before phase verify

**Handoff:** The final hosted run [42923015327](https://github.com/szTheory/tendies64/actions/runs/42923015327) tested exact HEAD `32de4f7b664c5e8083ff22a44bda8a46f67d88c6`. macOS arm64/Apple Clang passed all 73 tests. Linux x86_64/GCC 13.3 failed during test compilation: `runner/tests/test_skeleton_probe.c` path-building `snprintf` calls at lines 541–547 trigger `-Werror=format-truncation`; CTest did not start on Linux. The run was not retried. The hosted-gate sign-off remains open until this bounded test-path issue is fixed and the Linux lane passes. Cost review F-023 records 82 s wall time, an 80 s macOS critical path, and why both existing OS/toolchain lanes remain without CI changes. Local validation and independent source/security reviews passed after fixes. See `02-REVIEW-FIX-2.md` for the corrected sanitizer record and `02-RECHECK-FINAL.md` for its retained-log limits.

## Executable Task Ownership and Waves

Each owning task registers its test before running its exact filter. There is no
separate scaffold-only plan: tests start red in the behavior-adding slice.
Every executable PLAN verification has an immediate `fails_when` sibling.
All commands below use the existing presets and add `--no-tests=error`.

| Task | Wave | Production capability / registration created | Required evidence |
|---|---|---|---|
| 02-01 Task 1 | 1 | Owned instance/admission/idle bound; `unit.core.tracer` | Public-entry authored image → digest path |
| 02-01 Task 2 | 1 | One pure SHA target; default source/object lint set | Existing `unit.common` vectors and full gate |
| 02-01 Task 3 | 1 | Exact descriptor/limits; `unit.core.profile` | Canonical golden, changed fields, two descriptors/country independence |
| 02-02 Task 1 | 2 | Full production normalize/parse/commit; `unit.core.rom-admission` | All orders, every exposed field, exact length boundaries |
| 02-02 Task 2 | 2 | Allocator/refusal lifecycle; `unit.core.lifecycle` | Denial, reported capacity/alignment, overlap, release order |
| 02-03 Task 1 | 3 | Bounded scheduler; `unit.core.scheduler` | Ties/bounds/halt/quiescence/resource and counter exhaustion |
| 02-03 Task 2 | 3 | Canonical future state; `unit.core.digest-trace` | Independent encoding golden, inclusion/exclusion, partitions |
| 02-03 Task 3 | 3 | Bounded trace/drain | Off/on/drain/overflow equality and visible drops |
| 02-04 Task 1 | 4 | Physical RAM/SP/ROM bus; `unit.core.bus` | Every map endpoint, unsupported no-alias, byte/word rules |
| 02-04 Task 2 | 4 | MI masks/pending/IRQ | Six paired commands, set-wins, pending not acknowledged |
| 02-05 Task 1 | 5 | PI scheduled copy; `unit.core.pi-dma` | Source/policy-derived exact bytes, latches and deadlines |
| 02-05 Task 2 | 5 | PI cancel/token/digest | No stale copy/duplicate IRQ; same-tick clear/set |
| 02-06 Task 1 | 6 | SP checked rows; `unit.core.sp-dma` | Both directions, source-at-completion, skip/wrap/overflow |
| 02-06 Task 2 | 6 | SP active/pending/full/reset | Reserved promotion, third request, token/stale negatives |
| 02-07 Task 1 | 7 | `fuzz.rom-admission`, `sanitizer.negative-control` (asan only) | Seed 0x7a64c0de, 250000 mutations + fixed inventory; caught child |
| 02-07 Task 2 | 7 | Strict separate model checker; `unit.model-result` | Closed t64.model-result v1 schema and native scope |
| 02-08 Task 1 | 8 | Host probe; `unit.core.profile-result` | Real identity and result/identity/seal/suite/claim rejection; unchanged v1 bytes |
| 02-08 Task 2 | 8 | `unit.core.determinism`, profile limits | Expected bytes/counters and interleave/partition/trace equality |
| 02-08 Task 3 | 8 | Exact inventory in public gate, final measured evidence | Any single registration removal fails; final source/CI/review evidence |

Waves are deliberately sequential: each plan expands the same private state,
build registrations and digest/reset contract. Parallel writes would collide.
There are 11 required gate `unit.core.*` registrations plus `unit.model-result`.
The implemented checker pins all 61 Phase 1 names plus these 12 Phase 2 names
(73 gate tests); ASan adds both campaign/control registrations (75 tests).
The final checker selftest individually removed all 148 expected names across
isolated CTest trees and rejected each one; duplicate, unexpected, disabled,
empty and enumeration-error controls also failed closed. The final gate measured
13 seconds for these inventory controls. The full bounded sanitizer campaign
completed in 75.83 seconds, under its 120-second limit. No dependency or CI job
was added.

## Multi-Source Coverage Audit

| Source | Item | Plan coverage | Status |
|---|---|---|---|
| GOAL | Named deterministic instance over memory/MI/PI/SP, isolated/interleaved equality | 01–08 | COVERED |
| GOAL | SC1 profile/build/hash and limits | 01 T3, 08 T1 | COVERED |
| GOAL | SC2 exact three bounds and interleaved digests | 03 T1/T2, 08 T2/T3 | COVERED |
| GOAL | SC3 sourced scheduled PI/SP and separate MI masks/pending | 04–06 | COVERED |
| GOAL | SC4 admission refusal and two-profile country contrast | 01 T3, 02, 08 T2/T3 | COVERED |
| GOAL | SC5 trace-on/off equality | 03 T3, 08 T2/T3 | COVERED |
| GOAL | SC6 full production parser ASan/UBSan fuzz | 07 T1 | COVERED |
| REQ | CORE-01 | 01, 03, 05–08 | COVERED |
| REQ | CORE-02 | 03–08 | COVERED |
| REQ | CORE-03 | 03, 07, 08 | COVERED |
| REQ | CORE-06 | 03–08 | COVERED |
| REQ | PROF-01 | 01, 07, 08 | COVERED |
| REQ | PROF-02 | 01, 07, 08 | COVERED |
| REQ | PROF-03 | 01, 02, 07, 08 | COVERED |
| REQ | PROF-04 | 01, 07, 08 | COVERED |
| REQ | PROF-06 | 02, 03, 07, 08 | COVERED |
| REQ | BUS-01 | 04, 05, 07, 08 | COVERED |
| REQ | BUS-02 | 05, 06, 07, 08 | COVERED |
| REQ | BUS-04 | 01, 02, 07, 08 | COVERED |
| RESEARCH | Callback storage preconditions, reported-size/alignment and alias checks | 01 T1, 02 T2 | COVERED |
| RESEARCH | Candidate normalization/hash/one commit and complete refusal state | 02, 07 T1, 08 T2 | COVERED |
| RESEARCH | Profile target vs support, exact tick conversion/quantum/capacity identity | 01 T3, 08 T1 | COVERED |
| RESEARCH | Inclusive idle bounds, halt/counter/quiescence, queue/sequence budget | 03 T1 | COVERED |
| RESEARCH | One authored linted SHA and explicit canonical state audit | 01 T2, 03 T2, 04–06 | COVERED |
| RESEARCH | Bounded copy-only trace and visible overflow | 03 T3, 08 T2/T3 | COVERED |
| RESEARCH | Half-open physical map and source-labeled MI commands | 04 | COVERED |
| RESEARCH | PI masks/readback, atomic source-at-completion, reset tokens | 05 | COVERED |
| RESEARCH | SP row/skip/bank/queue/full/source-at-completion | 06 | COVERED |
| RESEARCH | PI/SP timing gaps, latch/full/wrap/error inferences | 01 T3, 05–08 docs | COVERED |
| RESEARCH | Full production admission fuzz + separate caught sanitizer fault | 07 T1 | COVERED |
| RESEARCH | Existing gate/two lanes/no packages, rights/core boundaries and independent reviews | 01 T2, 08 T3 | COVERED |
| RESEARCH | Separate model schema; legacy result/identity/seal/suite/claims rejection | 07 T2, 08 T1 | COVERED |
| RESEARCH | Exact test inventory and every single-name removal negative | 08 T3 | COVERED |
| CONTEXT | D-01 ownership/callback boundary | 01 T1, 02 T2 | COVERED |
| CONTEXT | D-02 candidate commit/reset/refusal | 01 T1, 02, 08 T2/T3 | COVERED |
| CONTEXT | D-03 named descriptor/build/result identity | 01 T3, 08 T1 | COVERED |
| CONTEXT | D-04 explicit profiles/country/limits | 01 T3, 02 T1, 08 T1 | COVERED |
| CONTEXT | D-05 queue order/resource reservation | 03 T1, 05, 06 | COVERED |
| CONTEXT | D-06 exact stop modes/interleaving | 03 T1/T2, 08 T2/T3 | COVERED |
| CONTEXT | D-07 canonical state and one SHA | 01 T2, 03 T2, 04–06 | COVERED |
| CONTEXT | D-08 observational trace | 03 T3, 05, 06, 08 T2/T3 | COVERED |
| CONTEXT | D-09 source-backed map/MI | 04–06 | COVERED |
| CONTEXT | D-10 delayed immutable PI/SP and source visibility | 05, 06 | COVERED |
| CONTEXT | D-11 explicit timing/inference policies | 01 T3, 05, 06 | COVERED |
| CONTEXT | D-12 bounded raw three-order admission | 02 | COVERED |
| CONTEXT | D-13 full production bounded sanitizer fuzz/control | 07 T1 | COVERED |
| CONTEXT | D-14 gate/review/CI/source evidence | 08 T3 | COVERED |

Deferred CPU/RSP instruction execution, real boot/VI/audio, ABI freeze, export/
replay, arbitration/cycle timing, rights decisions and physical hardware are
excluded by CONTEXT and the roadmap; none is silently removed from Phase 2.

## Edge Probe No-Silent-Drop Ledger

Input: `/tmp/tendies64-phase2-edge-coverage.json`; 20 surfaced rows = 13 explicit
must-have predicates + 7 flagged unresolved unclassified assumptions. No row is
dismissed or falsely resolved. The temporary path is a planning input only;
this table is its durable disposition record.

| Requirement | Category | Status / verification | Must-have / testable disposition |
|---|---|---|---|
| CORE-01 | unclassified | unresolved / flagged | Assumption: one scheduler owns all progress; 03 T1 and 08 T2/T3 test named paths, verifier must inspect remaining call paths |
| CORE-02 | unclassified | unresolved / flagged | Assumption: only present devices have behavior; 03/05/06 and profile limits must never imply CPU/RDP/VI/AI/SI execution |
| CORE-03 | boundary | resolved / explicit | 03 truth: lower/current/bound-1/bound/bound+1 and halt/count precedence |
| CORE-03 | precision | resolved / explicit | 03 truth: integer checked tick/counter/sequence arithmetic, resource-stop without wrapping |
| CORE-06 | unclassified | unresolved / flagged | Assumption: all meaningful implemented boundaries use copy-only observer; 03 T3/08 T2/T3 plus review audit |
| PROF-01 | adjacency | resolved / explicit | 01 truth: equal descriptors equal hash, different fields distinct; adjacent spans nonoverlap in 02 |
| PROF-01 | empty | resolved / explicit | 01 truth: null/empty/unknown selector refuses |
| PROF-01 | ordering | resolved / explicit | 01 truth: fixed canonical descriptor order and independent golden |
| PROF-02 | adjacency | resolved / explicit | 08 truth: separate profile/build identities never fuse even if text equal/touching |
| PROF-02 | empty | resolved / explicit | 08 truth: no fabricated profile identity for unknown selector; all actual results consumer-validated |
| PROF-02 | ordering | resolved / explicit | 08 truth: canonical result order and unchanged v1 bytes |
| PROF-03 | unclassified | unresolved / flagged | Assumption: every region consumer uses descriptor; country contrasts test production selection, verifier audits future call paths |
| PROF-04 | adjacency | resolved / explicit | 01 truth: one current disposition per feature, no overlapping implied support |
| PROF-04 | empty | resolved / explicit | 01 truth: each limitation has nonempty promotion evidence; no empty category implies support |
| PROF-04 | ordering | resolved / explicit | 01 truth: stable limitations table ordering |
| PROF-06 | unclassified | unresolved / flagged | Assumption: all writable state is per instance; 02 T2/08 T2/T3 and source/object lint plus review audit |
| BUS-01 | unclassified | unresolved / flagged | Assumption: explicit map is sufficient skeleton scope, not open-bus fidelity; 04 tests boundaries and limits retain unknowns |
| BUS-02 | empty | resolved / explicit | 05/06 truths: encoded zero is minimum PI/SP transfer, not zero bytes; invalid empty/out-of-range request cannot enqueue |
| BUS-02 | encoding | resolved / explicit | 05/06 truths: independent N-1/len/count/skip/8-byte decode and exact model tick conversion |
| BUS-04 | unclassified | unresolved / flagged | Assumption: raw readable spans are caller precondition, not pointer-validity detection; 02/07 test bounded production path |

The seven flagged assumptions are review inputs, not missing implementation
features or permission checkpoints. They remain unresolved until supported by
explicit verification evidence; this plan does not auto-resolve them.

## Prohibition Recall and No-Silent-Drop Ledger

Three kept domain prohibitions are authored descriptor-less in plan must_haves,
with status unverified and flagged true. No `check_*` descriptor is invented;
downstream review must not silently pass them on planning attestation.

| Raw candidate | Disposition | Reason / authored location |
|---|---|---|
| Core acquires host clocks/files/entropy/threads/global state | KEEP flagged-unverified | 01 must_haves.prohibitions; deterministic architecture value |
| Protected content or incompatible copied source enters tests/core | KEEP flagged-unverified | 02 must_haves.prohibitions; project rights/licensing stop |
| Model/fuzz/digest success becomes hardware/boot/memory-safety claim | KEEP flagged-unverified | 07 must_haves.prohibitions; project evidence value |
| Pointer arithmetic wraps | DROP as routine correctness | Edge tests and code/security review own it |
| DMA source/destination escapes installed RAM | DROP as routine correctness | Complete range/row tests own it |
| Queue is unbounded | DROP as routine correctness | Capacity/budget edge tests own it |
| Rejected operation partially mutates state | DROP as routine correctness | Transaction tests own it |
| Digest hashes padding | DROP as routine correctness | Field audit/inclusion tests own it |
| Generic injection/path traversal | DROP canon referral | Security review owns canonical vulnerability classes; no new domain prohibition |
| Generic authentication/session weakness | DROP canon referral | No authentication/session surface; ASVS applicability recorded in threats |

Equality: 10 raw candidates = 3 kept/authored + 7 explicitly dropped with owner
and reason. Kept prohibition count 3 = 3 flagged descriptor-less items.
No external API, database schema, AI integration or frontend exists in this
phase. The UI keyword match on “Component tests” does not authorize UI work.

## Validation Audit 2026-11-10

| Metric | Count |
|---|---|
| Gaps found | 0 |
| Resolved | 0 |
| Escalated | 0 |
