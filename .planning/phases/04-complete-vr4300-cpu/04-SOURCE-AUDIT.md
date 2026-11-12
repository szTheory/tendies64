# Phase 4 source coverage and executable dependency audit

Status: planning coverage complete; no implementation, copy, installation, build, ROM or behavioral test is represented as having run. The parent owns review/commit and any research metadata correction. This planner only writes the Phase04 plans, validation, source audit and probe ledger.

## Authoritative scope

All four ROADMAP success criteria, all ten requirement IDs and D-01–D-13 are binding. CPU timing T1, pipelines, RSP execution, video, cold boot and publication of the external diagnostic remain outside this phase. A local diagnostic failure may be reported honestly; a missing/reduced/unreached required CPU member cannot complete SC4. Native-only evidence cannot replace SC4.

## Multi-source coverage

| Source | Item | Implementation/test ownership | Status |
|---|---|---|---|
| GOAL | Complete integer/COP1 CPU with precise translation/memory/cache behavior and separate native/private evidence | 01–18 | COVERED |
| GOAL | SC1 integer/control/CP0/TLB/unaligned/LLSC with independent edge/negative expectations | 01–08; owning native tests | COVERED |
| GOAL | SC2 both FP formats/all rounding modes/FCR31/E/no host FP | 11,17,12–13; raw/COP1 vectors and production source/object lint | COVERED |
| GOAL | SC3 written guest-visible cache scope and functional cache tests | 09–10; cpu-cache.md and cache modes | COVERED |
| GOAL | SC4 pinned local build/run, complete exact CPU per-group status, private outputs | 14–16; qualification, exact manifest and all batches | COVERED |
| REQ | CPU-01 | 01.1,03.1–2; integer table | COVERED |
| REQ | CPU-02 | 02.1–2,03.1–2,12.2; delay/likely/exception matrix | COVERED |
| REQ | CPU-03 | 01–02,04–06; masks/time/interrupt/TLB state | COVERED |
| REQ | CPU-04 | 06–07; translated fetch/data and precise faults | COVERED |
| REQ | CPU-05 | 07–08,10; merge/link/cache invalidation | COVERED |
| REQ | CPU-06 | 09–10; functional split caches/scope | COVERED |
| REQ | CPU-07 | 11,17,12–13; arithmetic/COP1 and lint | COVERED |
| REQ | CPU-09 | Every behavioral slice; edge ledger and independent derivations | COVERED |
| REQ | VER-01 | 14–16; actual pinned local CPU overlay build/run | COVERED |
| REQ | VER-03 | 02–16 inventory updates,14 exact runtime manifest,16 complete aggregation | COVERED |
| CONTEXT | D-01 one core/new identity/old profile preserved | 01,02,09–10,15 | COVERED |
| CONTEXT | D-02 executable reviewed seed/all boot metadata | 01.1–2,15.2 actual startup | COVERED |
| CONTEXT | D-03 precise faults/nested EPC+BD/nonretiring progress | 02,03,05–08,12–13 | COVERED |
| CONTEXT | D-04 CP0 Status mask/RP/TLB/field ownership | 04,06 | COVERED |
| CONTEXT | D-05 Count phase/equality/interrupt/write order | 05.1–2 | COVERED |
| CONTEXT | D-06 functional split caches/LLSC/non-snoop | 07–10 | COVERED |
| CONTEXT | D-07 manual conflict/refusals/model uncertainty/v9 | 01–10; model policy claims reviewed | COVERED |
| CONTEXT | D-08 pinned generic3e license/per-file closure | 11.1–2 before copy | COVERED |
| CONTEXT | D-09 explicit FP context/VR owner/identity/lint/vectors | 11,17,12–13 | COVERED |
| CONTEXT | D-10 every CPU registration/value, exact groups/status | 14.2,16.1–3 | COVERED |
| CONTEXT | D-11 pre-use selected Rust/components/test graph/locks/source/assets/IPL3; excluded nust64 | 16.1 qualification before install/build;16.2 composition/startup | COVERED |
| CONTEXT | D-12 separate local same-core adapter/private inputs/results/closed hello | 14–16 | COVERED |
| CONTEXT | D-13 same hosted jobs/limits/immediate tests/exact inventories/review | Every plan verification;18 public native closeout,16 private checks | COVERED |
| RESEARCH | C17/shared integer interpreter; no dependency expansion | 01–13 | COVERED |
| RESEARCH | Three attempt outcomes distinct from host failure and nonretiring handler scheduling | 02,05 | COVERED |
| RESEARCH | Full-width PC, segment/privilege, staged writes/translation attributes | 02,06–08 | COVERED |
| RESEARCH | TLB duplicate-match shutdown, ASID/global/page/Random/Wired | 04,06 | COVERED |
| RESEARCH | Count idle/wrap/partition and exact same-tick order | 05 | COVERED |
| RESEARCH | MMIO transaction width/unaligned merges/RE/watch priority | 07 | COVERED |
| RESEARCH | Reservation physical width, all external writer coverage and policy granule | 08,10 | COVERED |
| RESEARCH | I/D virtual-index/physical-tag data, dirty/writeback/cache ops | 09–10 | COVERED |
| RESEARCH | Complete canonical v9 state and observer exclusion, warm v8 preserved | 01 plus every introducing slice | COVERED |
| RESEARCH | Official exact SoftFloat pin, no target specialization, both format closure | 11 | COVERED |
| RESEARCH | Per-file license/provenance/source build/hash/object lint, not top-level-only | 11.2 | COVERED |
| RESEARCH | Explicit context/no globals/TLS/hostFP; VR layer owns FR/FCR31/E/FS | 11–13 | COVERED |
| RESEARCH | Independently expected bit/rational FP and integer/negative vectors | Every native owner,11,17,12–13 | COVERED |
| RESEARCH | Exact source-name/value denominator manifest; constructor/README counts not denominator | 14.2,16.1 | COVERED |
| RESEARCH | Tool source correspondence/actual packaged lock/checksum/hooks before use | 16.1; corrected Cargo rule below | COVERED |
| RESEARCH | Font/framebuffer removal/composed asset audit and explicit cleared IPL3 qualification | 16.1–2 | COVERED |
| RESEARCH | Startup does not silently add RSP or broad DMA behavior | 15.2,16.1–2; unresolved CPU device dependency blocks affected completion | COVERED |
| RESEARCH | Verified snapshot + separate adapter; public hello collector stays closed | 15 | COVERED |
| RESEARCH | Existing lifetime4096/sticky output overflow cannot carry whole suite | 14,16: compact one-value fresh batches and complete cross-batch manifest | COVERED |
| RESEARCH | Existing1MiB admission/4MiB loader assumptions explicitly qualified | 15.2 selected new-profile-only packing/admission bound | COVERED |
| RESEARCH | No setup-only wave/testing deferred; exact91/93 preserved; full instrumented build | Every plan, validation map below | COVERED |

No source item is omitted or silently reduced. Qualification outcomes are execution prerequisites rather than asserted accomplishments. Deferred source items are excluded for the stated phase boundary, not relabeled implemented.

## Dependency and file ownership

| Plan | Wave | Needs | Creates/extends capability | Checkpoint |
|---|---:|---|---|---|
| 04-01 | 1 | existing Phase03 core | Executable CPU profile and canonical prepared seed | none; concrete pre-use stops retained |
| 04-02 | 2 | 04-01, 04-14 | Precise guest exceptions and full-width control state | none; concrete pre-use stops retained |
| 04-03 | 3 | 04-02 | Complete integer arithmetic and branch semantics | none; concrete pre-use stops retained |
| 04-04 | 4 | 04-03 | Central CP0 field and privilege semantics | none; concrete pre-use stops retained |
| 04-05 | 5 | 04-04 | Guest-time Count, Compare and interrupt boundaries | none; concrete pre-use stops retained |
| 04-06 | 6 | 04-05 | Paired TLB and 32/64-bit virtual memory | none; concrete pre-use stops retained |
| 04-07 | 7 | 04-06 | Architectural load/store, unaligned merges and endian rules | none; concrete pre-use stops retained |
| 04-08 | 8 | 04-07 | LL/SC reservations and external-writer invalidation | none; concrete pre-use stops retained |
| 04-09 | 9 | 04-08 | Functional instruction cache and stale-code visibility | none; concrete pre-use stops retained |
| 04-10 | 10 | 04-09 | Write-back data cache and primary CACHE operations | none; concrete pre-use stops retained |
| 04-11 | 11 | 04-10 | Qualified explicit-context binary32/binary64 arithmetic | none; concrete pre-use stops retained |
| 04-17 | 12 | 04-11 | Complete raw arithmetic edge/category/rounding vectors before consumers | none; concrete pre-use stops retained |
| 04-12 | 13 | 04-17 | COP1 registers, transfers, FR and precise guest traps | none; concrete pre-use stops retained |
| 04-13 | 14 | 04-12 | Complete COP1 arithmetic, rounding and VR4300 exception policy | none; concrete pre-use stops retained |
| 04-14 | 1 | existing Phase03 core | Closed local CPU diagnostic protocol and complete batch inventory | none; concrete pre-use stops retained |
| 04-15 | 15 | 04-13, 04-14 | Separate headless CPU adapter and qualified larger flat admission | none; concrete pre-use stops retained |
| 04-18 | 16 | 04-15 | Public native gate/instrumented/review/existing hosted evidence | none; concrete publication stops retained |
| 04-16 | 17 | 04-18 | Pre-use tool/asset qualification and complete private CPU suite status | none; concrete pre-use stops retained |

Wave1 consists only of01 and14: their files, tests, CMake ownership and inventory ownership are disjoint. Plan01 extends already registered native tests and never changes core/CMakeLists.txt or scripts/check-test-inventory.sh;14 owns its new runner registrations and the first PHASE04_TESTS entry. Plan02 depends on both and preserves14's additions. Shared interpreter/header/digest/CMake/inventory ownership serializes subsequent waves in the main checkout. No worktree isolation or concurrent mutation is assumed. Plan15 requires complete CPU plus protocol; Plan17 completes broad raw edge vectors before Plan12 consumers. Plan18 closes public native evidence after Plan15, before Plan16 private qualifications. Only01/14 share a wave; all shared writers serialize.

All37 tasks have immediate automated/fails_when checks. The two Plan18 closeout tasks are verification-only over already-tested behavior and may start green; no manufactured RED or inapplicable TDD marker is required. No setup-only Wave0 or deferred umbrella feature-test task exists. Plan11.2 is a single atomic qualified source-closure integration: its many mechanical copied paths are intentionally individually listed and provenance-checked rather than hidden behind an amalgamation. The accepted atomic-copy file-count exception is intact. Plan11 is re-sliced from76k into59k for its production tracer, full per-file audit/context/build/lint closure and immediate per-operation positive/negative/context smoke tests, plus Plan17 at25k for the non-atomic broad edge/category/rounding expansion. The combined84k includes an8k separate-executor/context/verification overhead rather than hiding work or lowering calibration; both plans remain under70k. Plan12 waits for17. No hardware scope or immediate feature evidence is omitted. Other tasks touch at most five exact paths. Plan16 now has10 paths: four implementation/input paths (script, authored overlay directives, adapter tests and production adapter), plus six ignored generated private outputs (qualification, required inventory, qualified build, batches, value status and group status). Its three sequential pre-use/build/run tasks form one bounded private qualification unit at57k; removing public native closeout to Plan18 at19k keeps native evidence independent. No private output is counted as a new production subsystem or public commit target.

Calibration query returned factor1, sample_count0, confidence low. Every estimate therefore equals its raw projection and retains low confidence; no measured runtime is invented.

## Concrete seed and policy decisions

Plan01 fixes Config0x7006E463 from manual fixed fields with EC7 selected model value, EP0,BE1,CU0,K0=3; PRId/FCR0=0x00000B00 with revision0 selected/unqualified, Status0x34000000, Random31/Wired0, Count/Compare0/phase0 and no initial equality latch. These are a prepared software seed, not reset-silicon claims. GPR19..23=0,1,1,0,1 and entry0xA4000040 reuse cleared IPL3 entry ABI. The actual first opcode is0x3044D236 (andi a0,v0,0xd236), from the tracked boot contract, not a guessed instruction.

Status defined mask0xFF57FFFF then fixed RP27 zero gives0xF757FFFF; TS preserved, RFU23/19 zero. Nested oldEXL preserves BOTH EPC and BD. The p181 table/prose conflict remains unqualified and selected bootstrap general vector is0xBFC00380. Count ratio, PageMask refusal, one-shot exact physical/width SC, link byte-overlap granule and nonsnooping DMA are explicit profile policies with reversal/version evidence. New profile boot/loader policy and every future-affecting field enter descriptor3/v9; warm descriptor2/state8 and seven fixture identities are preserved.

The exact SoftFloat generic C/header paths are enumerated in11.2. The per-file audit must close the selected portable FAST_INT64 symbol/header dependency set before copy; failure is reported as a concrete qualification contradiction, never a silent addition/license bypass. Each copied destination gets one precise enforced provenance record (checker P-DUP-PATH forbids many source records pointing to one amalgamation).

## Tool documentation correction and finalization prerequisite

The existing TESTS paragraph saying Cargo uses packaged Cargo.lock by default is incorrect. [Official Cargo install documentation](https://doc.rust-lang.org/cargo/commands/cargo-install.html#dealing-with-the-lockfile) says the packaged lock is ignored by default; use explicit --locked and verify the actual packaged lock plus recorded resolution. Plan16.1 includes the corrected requirement. Parent has corrected the research paragraph and the RESEARCH.md Package Legitimacy Audit table records nust64 as an excluded SUS candidate, not a selected installation. Only official package identity facts may be verified; lock/source/asset qualification remains pending. This does not authorize installing an unqualified package.

## Engine contribution contracts and discovery disposition

All planner-targeted inline contributions from the injected plan-pre result were applied:
- API coverage detector ran over the concrete plan scope and returned detected:false with a real nonempty input, not skipped. No external service/API integration exists; no fabricated capability matrix or extra COVERAGE artifact is needed.
- Assumption-delta scan returned detected:true (the phrase another emulator triggered pluralization). The substantive identity generalization is resolved by promote: validated versioned descriptors are primary, shared primitives dispatch policy, old warm is a preserved variant. Every plan records the decision. No owner checkpoint is introduced.
- Schema scan: no Payload/Prisma/Drizzle/Supabase/TypeORM paths, no live database. No schema-push task applies.
- AI: no AI system/model integration. UI: native component wording is the documented false positive; no frontend or visual flow.
- Security: every plan has actual trust boundaries, unique numbered STRIDE rows, severity/disposition and immediate test ownership; ASVS1/high blocks affected unresolved findings. T-04-SC has one identical shared supply-chain meaning.

Discovery is sufficient from the completed research/pattern map and bounded tracked-source/manual/official arithmetic reads. No general research restart, prohibited emulator source, ROM acquisition/build, installation or behavioral test was performed. No project skills were present, and gsd-planner skills-only query emitted no skill.

## Path and verification grounding

All existing analog/source paths in plans were checked against git ls-files. Intentional new production/test/spec/private paths are exact and owned by their tasks. Private generated paths are under already ignored build/private-cpu-diagnostic; they are never git-add targets or public evidence. No command depends on a private absolute checkout path. Installed runtime references in execution_context are workflow inputs only.

The proven commands are reused: cmake --preset gate; cmake --build --preset gate; anchored ctest --preset gate --no-tests=error ... --output-on-failure; sh scripts/gate.sh; sh scripts/check-test-inventory.sh --selftest. New script commands are created before use in15/16 with offline authored selftests. Every automated clause has an immediately paired fails_when. No newly invented source grep/count acceptance gate is used, so no vacuous text-match baseline is claimed.

The inherited sanitizer environment is ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 and UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1, grounded in Phase03 validation and scripts/expect-sanitizer.sh. Full all-target ASan build precedes focused runs; admission fuzz120-second bounds and the intentional-fault control remain unchanged.

## Residual qualification stops

Per-file source/license closure before each copy; actual package/lock/checksum/source/build-hook/component audit before install/build; final composed assets/font absence/IPL3 format before external use; and actual complete startup/member reachability before SC4. Failure stops the affected use and leaves its criterion incomplete. Routine CPU/native work continues where independent. No human validation, hardware conformance, public diagnostic rights or risk acceptance is inferred.


## Parent recovery and package disposition

The server interrupted the return after all16plans and audit/validation files
were saved. Parent reconciled those files without redispatching planning.
Absolute runtime paths were made portable and escaped shell operators restored
to executable text before first commit. The Cargo lock research paragraph is
corrected and the canonical Package Legitimacy Audit table now records actual
evidence. The candidate nust64 installation is not selected: Plan15 already
uses the project packer plus qualified objcopy/linker, so Plan16 uses cargo build
to ELF, never cargo run/the upstream runner. Real dependency audits and any
required package checkpoint remain before use; no approval is inferred.

## Checker revision closure

All four iteration1 issues are addressed in the plan text, pending independent recheck (no implementation evidence claimed). Every one of37 action blocks names a concrete 04-PATTERNS analog/integration contract or 04-RESEARCH new-role guidance. Calibration remains factor1/sample_count0/confidence low; revised projections reflect an actual task split, never invented measured evidence.

Plans14/15 require one runner/CMakeLists.txt shared t64_cpu_diag_protocol target. Production t64-cpu-diagnostic and authored tests link that same parser/encoder/validator/aggregator from runner/t64_cpu_diag_protocol.c/.h. Plan16 seals t64.cpu-inventory v1, stores the production canonical t64.cpu-batch v1 records and routes --verify-complete through that same production aggregate CLI. No script-local schema/status sums or lookalike JSON can establish SC4. Plan15 controls reach the built adapter with malformed/missing-done/wrong-identity/duplicate/omitted authored inputs and a complete positive set; Plan16 extends existing controls through actual --run-all/--verify-complete routing. Controls are executable, not source grep.

The plan set contains18 plans/17 waves/37 tasks. Public native final verification is Plan18 before private Plan16; later source fixes refresh affected native evidence. Requirements, SC1–4, D-01–D-13,18 edge predicates and five unclassified flags remain covered. Existing20 CTest additions and111/113 totals are unchanged. Runtime references stay @$HOME and executable command chains use literal &&. No new package, build, test, CI run or source change was performed during revision.

Read-only revision validation: all18 frontmatter schemas and plan structures validate; every file closes exactly one output/task-list/threat/verification section. All37 automated clauses parse with sh -n and have immediately paired fails_when; all37 actions cite the mapped analog/research contract. Dependency waves are acyclic and shared-file conflict-free. The only >5-path task remains the accepted atomic copy in11. No command was executed beyond syntax and planning validators, and no implementation success is claimed.

## Iteration 2 sanitizer evidence correction

Plan16 follows Plan18 and modifies tracked adapter C/tests. Every Plan16 task now explicitly configures and builds ALL ASan targets after its owning C/CTest changes, then runs exactly runner.cpu-diagnostic, runner.cpu-diagnostic-protocol and runner.cpu-diagnostic-inventory using the inherited sanitizer environment. Each automated command has an immediate fails_when covering build failure, failed instrumented assertions and missing/empty controls. The exact focused command is:

`cmake --preset asan && cmake --build --preset asan && ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset asan --no-tests=error -R '^(runner\.cpu-diagnostic|runner\.cpu-diagnostic-protocol|runner\.cpu-diagnostic-inventory)$' --output-on-failure`

These are public authored controls through the verified production CLI/shared protocol, never private ROM/results. Any changed code/test invalidates that portion of Plan18's earlier evidence; record the new owning run rather than relabeling old evidence. Core or other subsystem fixes additionally require affected subsystem checks/review and the already specified conditional broader regression/public-native evidence refresh. Reuse unaffected broad/hosted evidence and do not repeat checks solely for metadata. Plan18 remains before private qualification; 18 plans,17 waves,37 tasks and111/113 inventories are unchanged. No implementation, build, test, CI or package use was performed during this planning correction.
