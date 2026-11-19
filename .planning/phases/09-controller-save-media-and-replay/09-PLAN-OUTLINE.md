# Phase 9 Plan Outline: Controller, Save Media and Replay

Source: the locked eleven-plan sequencing (S-01..S-11) in `09-CONTEXT.md`, with the
"Amendments from planning research" (K1-K16) applied, the per-plan file lists in
`09-RESEARCH.md` and the hidden dependencies N1-N8 in `09-PATTERNS.md`.
Mode: chunked outline-only. No PLAN.md files exist yet. Waves are strictly sequential (one plan
per wave). Every plan leads with a `tracer` task, then expansion tasks.

| Plan ID | Objective | Wave | Depends On | Requirements |
|---------|-----------|------|------------|--------------|
| 09-01 | Tracer: one new-family profile id (`ntsc-cart-4m-openipl3-rdram-b-t1-si-v1`, descriptor 12) created, hashed and digested end-to-end through the explicit P1 membership predicates and the digest sink with a v25 stream that equals the v24 stream once stripped; deliverable is the F0 identity (four ids, `si_policy`/`save_policy` labels, `save_kind`/`peripheral_flags` and quantum validation, v25 walker, E0 ledger and FIFO characterization, corrected `docs/spec/machine-contract.md` digest text). | 1 | none | CORE-02, CORE-04 |
| 09-02 | Tracer: one accepted real-runner operation (a tiny authored admit-and-tick ROM, `fixtures/phase09/admit.S`) on each new id with a per-fixture tick cap and transcript pin, publishing one artifact through the new `runner/t64_artifact_publisher.[ch]` under the exact `--artifact-dir`/`--artifact-max-bytes` FSIZE pair; deliverable is the harness mechanism (K3/K4/K6/K9/K12/K15 seams, rights-scan and upload-allowlist extension, `scripts/gate.sh` private arm, fail-closed `scripts/qualify-phase09.py` stub, `scripts/check-phase09-cases.py`, `docs/spec/phase09-cases.json`, claim skeleton). | 2 | 09-01 | SAVE-01, SAVE-02, SAVE-03, CORE-04, CORE-05 (mechanism only; no behavior claim) |
| 09-03 | Tracer: one scheduled button/axis value read by an authored `pad-poll` ROM through the modeled SI RD64B/WR64B, PIF joybus parser and input latch on port 0, verified through the real runner; deliverable is `core/t64_si.c`, `core/t64_joybus.c`, the K16 capacity pre-check and D-44 store budget (first task), the strictly-after latch, `pad-poll` fixture, the real collector/verifier in `scripts/qualify-phase09.py`, `ci.yml` plus `check-ci-yml.sh` steps, one hosted pair of real artifacts and cross-platform controls built from that data (FH freeze). | 3 | 09-02 | SAVE-01, CORE-02 |
| 09-04 | Tracer: one EEPROM 4 Kbit block write then read through joybus channel 4 on the explicit `-e4k` profile via an authored ROM; deliverable is `core/t64_save.c` (EEPROM 4/16 Kbit with the 375,000-tick busy window, SRAM at `[0x08000000, 0x08008000)` through PI DMA), the `save-media` digest section, `SAVE_*` refusal statuses with defined postconditions, `eeprom-rw` and `sram-rw` fixtures and `docs/spec/save-media.md`. | 4 | 09-03 | SAVE-02, SAVE-04 |
| 09-05 | Tracer: one SRAM image imported through a verified fd, rewritten by the guest and exported through the publisher with the RAW/SWAP16/WORDREV32 transforms, runner side only; deliverable is the save import/export API and `t64_machine_replace` blank-save rules, the D-24 refusal-order matrix with bit-identical-digest assertions, bounded mutation test, `save-roundtrip` fixture, runner import/export flags, upload-allowlist and rights-scan data rows. | 5 | 09-04 | SAVE-03, SAVE-04 |
| 09-06 | Tracer: one coincident-tick pair of device events (VI and AI tie) run whole versus split through the single guest-time scheduler and compared with a pinned order; deliverable is the CORE-02 ordering matrix `core/tests/test_sched_matrix.c` (coincident, partition, interleave, resource-limit resume, MI producers, Count-wrap via direct state), the five non-equivalent K5 mutants as an out-of-tree procedure, the quantum-in-`profile_digest` hash control (K10) and a full gate run (F1 freeze). | 6 | 09-05 | CORE-02 |
| 09-07 | Tracer: one authored `replay-mix` ROM run with an independently authored `t64.input` script, recorded to a `t64.replay` artifact and replayed to reproduce the recorded checkpoint digests; deliverable is `runner/t64_replay.[ch]`, record/play flags and `t64.replay-report`, identity binding with three separate mismatch refusals (profile, build, content), build-independent goldens, `docs/spec/replay.md`. | 7 | 09-06 | CORE-05 |
| 09-08 | Tracer: one authored machine exported to a versioned state container, size query then copy-out, with an independent reference decoder reading the section table; deliverable is `core/t64_state.c`, buffer sink and section markers in `core/t64_digest.c`, the export API, runner export flag with a real-runner state-size control under the raised FSIZE, `docs/spec/state-container.md`. | 8 | 09-07 | CORE-04 |
| 09-09 | Tracer: one exported state re-imported through validate-then-apply and reproducing the canonical digest and continuation digests on the same build; deliverable is the state readers with per-device invariants (D-30), the boundary matrix (D-32), refusals that leave the live machine bit-identical, structure-aware mutants, ASan-only `fuzz_state_import.c`, `scripts/check-state-field-use.py` (D-45), runner import flags. | 9 | 09-08 | CORE-04 |
| 09-10 | Tracer: one real mutated state blob from the 09-09 sweep run through the independent review and security audit and reaching its specific refusal; deliverable is `09-REVIEW.md`, `09-SECURITY.md`, the one budgeted fix cycle (logic and regenerated goldens land here, before F2) and the FR freeze. | 10 | 09-09 | SAVE-01, SAVE-02, SAVE-03, SAVE-04, CORE-02, CORE-04, CORE-05 |
| 09-11 | Tracer: one exact-SHA native run of the full gate through the frozen `scripts/qualify-phase09.py`; deliverable is the F2 identity freeze (literal profile/fixture/input pins, real `t64-claim` blocks one per profile id and fixture set, `docs/evidence/phase09-*.md`), native normal + ASan/UBSan + gcc:13 + both hosted jobs on the exact SHA, with no logic edits and success criteria 1-4 each mapped to a named test row and a claim block. | 11 | 09-10 | SAVE-01, SAVE-02, SAVE-03, SAVE-04, CORE-02, CORE-04, CORE-05 |

Requirement coverage check: SAVE-01 (09-03, 09-10, 09-11), SAVE-02 (09-04), SAVE-03 (09-05),
SAVE-04 (09-04, 09-05), CORE-02 (09-01, 09-03, 09-06), CORE-04 (09-01, 09-08, 09-09),
CORE-05 (09-07). All seven IDs appear at least once.

## Per-plan notes

Threat ID ranges are reserved so IDs stay unique across the phase (continuing T-09-01..). Each
plan must still read the existing 09-*-PLAN.md files for the highest ID in use before
numbering; the ranges below are budgets, not promises. `T-09-SC` is never listed (no plan
installs anything). Severity: high threats are mitigated, never accepted.

**09-01 Core identity and walker** (T-09-01..05, about 5 threats: membership fall-through,
profile field validation, quantum and ratio validation, digest-sink regression, stale spec text)
- Forced first tasks: (1) the golden predicate matrix over every registered profile id, captured
  before any ladder edit (K1; depth stays inactive on descriptor 12, no depth claim, STATE
  follow-up); (2) the script-generated membership-site list including `t64_isviewer_active`
  (K2) and the `t64_has_ai_device` helper; (3) the declaration of both labels (`si_policy`,
  `save_policy`) at their final text so profile hashes are stable (K12 prerequisite).
- Own task, isolated review: refresh of `scripts/check-rsp-no-hle.py` digests for the AI
  functions (seven `t64_has_ai_profile` gates in `core/t64_ai.c`), `core/t64_bus.c ::
  decode_region`, plus the `scripts/tests/test_rsp_no_hle.py` mutation control showing the guard
  still fails on an unauthorized body change (N1). State pin, old and new digest, causing diff,
  control.
- Hidden dependencies: N6 (composite ids are hash-pinned at create, so the new-family tests
  cannot mutate queue capacity through create), N7 (copy the `profile_writer` three-way shape for
  the sink).
- Bound-source files touched: `scripts/check-rsp-no-hle.py` and `scripts/check-test-inventory.sh`
  (data rows and digests only; no logic). `core/` and `runner/` are bound wholesale in the Phase 8
  `SOURCE_PATHS`, so assume the Phase 9 qualifier binds them too.
- F0 freeze after this plan.

**09-02 Runner, supervisor, publisher, harness mechanism** (T-09-06..15, about 10 threats:
publisher substitution race, FSIZE exactness, tick cap, argv overflow, rights denied extension
and magic, upload allowlist, private junit directory, fixture pin churn, claim skeleton wording)
- Forced first tasks, in this order, each its own task: K3 per-fixture tick cap in the
  approved-fixture table (default 600,000 unchanged; a named larger constant such as 4,000,000
  only for fixtures that outlast the busy window; both the `t64_supervisor.c` and `t64_real.c`
  sites; control that a value above the new cap is refused); K4 per-fixture
  `expected_log_bytes`/`expected_log_sha256` pins (0/NULL = legacy hello) in
  `runner/t64_real_fixture.[ch]` and `runner/t64_result_check.c` before any fixture is authored;
  K6 `child_args` enlargement with a checked push helper, `_Static_assert` and an ASan
  maximum-option control; K12 tiny admit ROMs plus the profile-hash print helper.
- Guard-pin refreshes, each in its own task: (a) N2 `scripts/gate.sh` Phase 9 private-directory
  arm for `T64_GATE_JUNIT_OUT` (frozen before FH); (b) N3 rights scanner (`tools/rights/t64_blobscan.c`
  extension row and magic row for the state/replay containers, with 3-letter extensions where
  possible, selftest controls in `scripts/rights-scan.sh`); (c) N1 `scripts/check-rsp-no-hle.py`
  only if a pinned body moves (the publisher is under `runner/`, not `core/`, so likely none);
  (d) N5 exported iteration accessor for `runner.profile-table-id-length` (D-43).
- Other: K9 control through the `t64-runner-test` fault named `harness.fsize-artifact-real-fault`;
  K11 claim skeleton as `t64-claim-example` fences; K15 new modules joined to
  `T64_RUNNER_BUILD_INPUTS` and both runner targets; `scripts/build-boot-fixtures.sh` untouched.
- Bound-source files touched: `scripts/qualify-phase09.py` (new stub),
  `scripts/tests/test_phase09_qualification.py`, `scripts/gate.sh`, `scripts/rights-scan.sh`,
  `tools/rights`, `scripts/check-phase09-cases.py`, `docs/spec/phase09-cases.json`,
  `scripts/check-test-inventory.sh` (the PHASE09 lane and structure; later plans add names only),
  `fixtures/manifest.json`.
- SIZE FLAG: see "Plans flagged as large" below.

**09-03 SI/PIF/controller tracer and first hosted data** (T-09-16..27, about 12 threats: joybus
parser totality and bounds, PIF address/length checks, RD64B/WR64B direction, queue/sequence
exhaustion atomicity, input latch ordering, port-field decode, SI interrupt clear, collector
parsing of hosted artifacts, upload allowlist, CI run-line exactness, platform-invariant compare)
- Forced first task: D-44 profile-gated store budget in `cpu_successor_extra_event_budget`
  (`core/t64_cpu.c`) with the K16 capacity pre-check extracted into `core/t64_scheduler.c`;
  legacy budgets untouched; `unit.core.cpu.si-store-budget`, `unit.core.si.atomic-queue-full`.
  Test workaround per N6 (synthetic queue fill and `next_sequence` near the limit, or direct-state
  `setup_t1_machine` copy), never a mutated descriptor through create.
- Hidden dependencies: K4 pad-poll transcript sizing (N8: three 4,096-byte caps; size polls x
  line length before authoring), K7 (`t64.input` v1 stays port 0; ports 1-3 only in the new core
  API path and the replay schema), K3 (pad-poll fits the 61,000-tick window or uses the per-fixture
  cap).
- Guard-pin refreshes, each in its own task: N1 (add `t64_si.c` and `t64_joybus.c` to
  `CANONICAL_CORE_SOURCES` plus reviewed function inventories and digests for edited pinned
  bodies `event_can_dispatch`, `t64_schedule_cancel`, and any `decode_region` change, with the
  mutation control); N4 (`scripts/check-ci-yml.sh` exact-match changes: `job_runs`, `step_count`,
  `nruns`, run-line ladder, `expected_upload_path()`, selftest mutations, mirrored on the audio
  triple of case run, emit, PII scan) landing once with `.github/workflows/ci.yml`.
- Early verifier-tail replay: the collector/verifier is run against the real retained hosted
  artifacts produced by this plan's own first hosted pair; cross-platform controls are built from
  real two-platform records, not synthetic ones (F-098). Use `scripts/ci-review.sh --append` for
  `docs/ci-reviews/LOG.md`. Full `sh scripts/gate.sh` at the end of this plan.
- Bound-source files touched: `scripts/qualify-phase09.py` (real logic), `.github/workflows/ci.yml`,
  `scripts/check-ci-yml.sh`, `scripts/check-test-inventory.sh` (names), `scripts/check-rsp-no-hle.py`
  (rows). All logic here is the FH freeze; later plans may add data rows only.
- SIZE FLAG: see below. This is the largest plan.

**09-04 Save devices** (T-09-28..35, about 8 threats: save-kind selection by profile only,
EEPROM block mask, busy-window arithmetic, write-while-busy handling, SRAM region carve-out and
alignment, PI read-length start for SRAM, unsupported-device refusal postconditions, trailing
storage sizing)
- Forced first tasks: carve `[0x08000000, 0x08008000)` out of the `T64_DOMAIN2_B_BASE`
  unsupported arm in `core/t64_bus.c` before it is reached (own task, because `decode_region` is
  digest-pinned; N1 refresh with mutation control); `core/t64_pi.c` read-length start for SRAM
  profiles (K1-style regression: non-SRAM profiles still refuse). EEPROM busy deadline computed
  from execute tick E, not the store tick (K5 non-equivalent mutant is authored in 09-06 but the
  deadline rule is decided here). K3 busy-window fixtures use the named larger per-fixture cap; K4
  pins for `eeprom-rw` and `sram-rw` transcripts; K12 one ROM binds exactly one profile id
  (`eeprom-rw` built for `-e4k` and `-e16k` with `--defsym`, `sram-rw` for `-s32k`).
- Guard-pin refreshes in their own task: N1 for `core/t64_save.c` source inventory and the
  `t64_pi_complete` and `decode_region` digests.
- Bound-source files touched: `scripts/check-rsp-no-hle.py` (rows), `scripts/check-test-inventory.sh`
  (names), `fixtures/manifest.json`, `runner/t64_real_fixture.c` rows. No qualifier, ci.yml,
  check-ci-yml.sh or gate.sh edit (frozen at FH). Save policy label and kind numbering freeze after.

**09-05 Save import/export and persistence** (T-09-36..45, about 10 threats: import size set,
check order, no-clobber publication, byte-order transform correctness, format selector,
busy-state import refusal, upload of save bytes, result record leakage, symlink/parent substitution,
replacement restoring blank save)
- Forced first tasks: the D-24 refusal check-order table authored as literals before implementation;
  the hand-authored transform vectors (RAW, SWAP16, WORDREV32; the definitions are assumption A1 in
  09-RESEARCH.md and must be recorded as authored policy, not hardware fact); the K6 argv bound is
  already enlarged in 09-02, so this plan only adds rows through the checked helper.
- Own task: `scripts/rights-scan.sh` upload rules as data rows with controls (bound-source file;
  data rows only; if the scanner needs a logic change that is an FH deviation to resolve in this
  plan, not a silent edit).
- Bound-source files touched: `scripts/rights-scan.sh` (rows), `scripts/check-test-inventory.sh`
  (names), `fixtures/manifest.json`. Result record carries kind, length, format and flags only;
  no save bytes in result or log.

**09-06 CORE-02 ordering matrix** (T-09-46..49, about 4 threats: tie-break regression, resume after
resource stop, mutant-must-compile control, counter overflow on Count wrap)
- Forced first tasks: replace the equivalent mutant with the five K5 non-equivalent mutants and
  confirm each compiles under `-Werror` and reaches its named comparator; K14 Count-wrap via direct
  state (`core/tests/test_cpu_time.c` pattern); K10 explicit quantum hash control (no `t64.result`
  key added). Test-name budget per K13 (at most about 45 new CTest names for the whole phase; this
  plan is mostly case-level inside `test_sched_matrix.c`).
- Bound-source files touched: `scripts/check-test-inventory.sh` (names), `docs/spec/phase09-cases.json`
  (data); no `core/t64_mi.c` or `core/t64_cpu.c` edit unless a per-producer MI tie needs a hook, in
  which case record it and refresh the affected N1 digest in its own task. Full `sh scripts/gate.sh`
  at the end. F1 (state content) freeze after.

**09-07 Replay** (T-09-50..58, about 9 threats: replay JSON tricks (duplicate keys, nesting, huge
numbers), caps before allocation, `at` ordering, identity binding spoofing (stale build/profile/
content), private artifact handling, report record vs `t64.result`, queue-full divergence,
original-run independence, port field restricted to 0)
- Forced first tasks: K7 (`t64.replay` carries `port` restricted to 0; `t64.input` v1 unchanged);
  the independent `t64.input` script and golden checkpoint digests authored before the recorder
  (no recorder-generated expectation); measure the `replay-mix` ROM length first (D-39); K15 add
  `runner/t64_replay.[ch]` to `T64_RUNNER_BUILD_INPUTS` and both runner targets; K6 forwarding rows
  through the checked helper.
- Bound-source files touched: `scripts/rights-scan.sh` (extension and magic rows, if not already
  covered in 09-02), `scripts/check-test-inventory.sh` (names), `fixtures/manifest.json`. No
  qualifier/ci.yml/check-ci-yml.sh/gate.sh edit.

**09-08 State export and container** (T-09-59..65, about 7 threats: container header and section
table bounds, buffer too small convention, export non-interference, size accounting overflow, FSIZE
for a 9-11 MiB blob, private artifact, section hash integrity vs authentication)
- Forced first tasks: K8 frame limit (`-Wframe-larger-than=2048`: bitsets or machine-owned scratch,
  no large stack arrays); N7 size-then-copy-out shape from `t64_profile_serialize`; real-runner
  state-size control under the raised FSIZE (K3/K9 constants); the independent reference decoder
  literal offsets in the test.
- Guard-pin refreshes in their own task: N1 `core/t64_state.c` source inventory and any digest for
  the edited `core/t64_digest.c` functions (the digest is not in the reviewed AI set but check
  with `python3 -B scripts/check-rsp-no-hle.py --build build/gate` after every core edit).
- Bound-source files touched: `scripts/check-rsp-no-hle.py` (rows), `scripts/check-test-inventory.sh`
  (names). State blobs private, never uploaded; rights-scan denied extension verified with a
  control.

**09-09 State import, invariants, boundary matrix** (T-09-66..77, about 12 threats, the densest
set: unchecked index/length/loop bounds, arithmetic overflow, zero padding and reserved fields,
overlap with live storage, identity binding, no-clobber publication, queue invariant summary
storage (K8), liveness on stalled observer stop, closed `(device, kind)` table (D-07), per-device
rules, fuzz memory safety, structure-aware mutants with valid magic and checksums)
- Forced first tasks: the generated field-use audit (`scripts/check-state-field-use.py`) because it
  decides which fields the readers must cover; the D-32 "reached" assertion for each boundary
  builder; mutant-must-compile for every structure-aware mutant so each reaches its own refusal
  (case list equals the JSON inventory).
- Own task: `scripts/check-test-inventory.sh` `ASAN_ONLY` row (data) and the N1 pin refresh for
  any core sources touched.
- Bound-source files touched: `scripts/check-test-inventory.sh` (names, `ASAN_ONLY`), `scripts/check-rsp-no-hle.py`
  (rows). Runner import flags and forwarding rows are ordinary plan content.
- SIZE FLAG: see below.

**09-10 Independent review, security audit and fix loop** (T-09-78..82, about 5 audit-owned
threats; one is a fixed dispositions table, new threats only from audit findings)
- Forced first tasks: freeze-check the bound-source list before any fix (any fix to `scripts/qualify-phase09.py`,
  `.github/workflows/ci.yml`, `scripts/check-ci-yml.sh`, `scripts/gate.sh`, the rights scanner or the inventory
  logic is a declared FH deviation and costs a native plus hosted cycle later); run the review
  against real mutated blobs from the 09-09 sweep; one fix cycle budgeted (D-46); logic fixes land
  here with regenerated goldens, never after F2.
- Bound-source files touched: only through recorded fixes. FR freeze after.

**09-11 Identity freeze and qualification** (T-09-83..86, about 4 threats: stale pin, evidence
privacy, claim wording (the words "accurate", "compatible", "supports" are reserved), hosted
artifact upload scope)
- Forced first tasks: cheap-first (PII and rights scans on the evidence, `sh scripts/check-claims.sh
  --files docs/claims/phase09.md`, `ast.parse`, gcc:13 `-Werror` on every changed unit, inventory
  and claims check), then the F2 literal pins; real `t64-claim` blocks only now (K11);
  `scripts/qualify-phase09.py` data pins only; no logic edits (D-06).
- Freeze-plan check, the files the freeze plan must verify unchanged since FH: `scripts/qualify-phase09.py`
  (logic), `.github/workflows/ci.yml`, `scripts/check-ci-yml.sh`, `scripts/gate.sh`,
  `scripts/rights-scan.sh` and `tools/rights`, `scripts/check-test-inventory.sh` (logic; PHASE09 name
  rows are data), `scripts/check-rsp-no-hle.py` (rows are data). Run the verification as a diff
  of function/structure bodies against the FH tag, not only a hash.
- Plan for at least two qualification cycles and a review-fix loop (effort low-confidence); gate
  nothing on estimates.

## Bound-source map (which plans touch which)

| Bound source file | 09-01 | 09-02 | 09-03 | 09-04 | 09-05 | 09-06 | 09-07 | 09-08 | 09-09 | 09-10 | 09-11 |
|-------------------|-------|-------|-------|-------|-------|-------|-------|-------|-------|-------|-------|
| `scripts/qualify-phase09.py` | | stub | real logic (FH) | | | | | | | fix only | data pins |
| `.github/workflows/ci.yml` | | | steps (FH) | | | | | | | fix only | none |
| `scripts/check-ci-yml.sh` | | | exact-match rules (FH) | | | | | | | fix only | none |
| `scripts/gate.sh` | | private arm (N2) | | | | | | | | fix only | none |
| `scripts/check-test-inventory.sh` | rows | lane structure | rows | rows | rows | rows | rows | rows | rows + `ASAN_ONLY` | fix only | pins |
| `scripts/rights-scan.sh`, `tools/rights` | | logic (N3) | | | rows | | rows | | | fix only | none |
| `scripts/check-rsp-no-hle.py` | digests (N1) | maybe | sources + digests | sources + digests | | | | sources | sources | fix only | none |
| `fixtures/manifest.json` | | rows | rows | rows | rows | | rows | | | | pins |

The FH freeze (end of 09-03) means logic edits to the qualifier, collector/verifier, `ci.yml`,
`check-ci-yml.sh`, scanner and inventory structure must not occur in 09-04..09-11; later plans
add data rows only. If a later plan finds it needs a logic change to a frozen file, it records a
deviation inside that plan with the cost (one native plus hosted cycle) and does not silently
change the file.

## Plans flagged as large (locked list kept; split proposals only if a plan exceeds one executor)

None of these is a split recommendation by itself; the locked list already split earlier
oversized plans. They are flagged because the 2-3 tasks per plan guideline will be strained, and
the orchestrator or checker should expect 4-6 tasks (tracer plus forced first tasks plus guard-pin
refresh tasks) for each. If the plan checker rejects task counts or context estimates, propose
these splits (decimal plan ids keep waves strictly sequential and the S-xx numbering intact):

1. **09-03 (the most at risk).** It carries D-44/K16 core scheduler work, the full SI/PIF/joybus
   device, the latch, the fixture, the real collector/verifier, `ci.yml`/`check-ci-yml.sh` exact-match
   changes, one hosted pair, cross-platform controls and warm/cold timing. If it exceeds about 50
   percent of an executor context, split as 09-03 (core: K16 pre-check, D-44 budget, SI registers,
   joybus parser, latch, unit tests; no CI) and 09-03b (runner `pad-poll`, real collector/verifier,
   `ci.yml`, `check-ci-yml.sh`, hosted pair, cross-platform controls, FH freeze). Keep FH at the end
   of 09-03b.
2. **09-02.** Many seams (K3, K4, K6, K9, K12, K15, N2, N3, N5), but each is small and independent.
   If it exceeds budget, split as 09-02 (publisher, FSIZE pair, K9, K6 argv, rights scan,
   `gate.sh`) and 09-02b (K3 cap, K4 pins, K12 admit ROMs, profile table accessor, qualifier stub,
   cases checker, claim skeleton).
3. **09-04.** EEPROM, SRAM, PI and bus carve-out and three fixture builds. Split only if the
   executor runs short: 09-04 (EEPROM) and 09-04b (SRAM and PI carve-out).
4. **09-09.** Readers, invariants, boundary matrix, fuzz and the field-use audit. Split if needed:
   09-09 (readers, invariants, boundary matrix) and 09-09b (fuzz, field-use audit, runner import
   flags).

Default: keep eleven plans as locked and let each plan author carry the extra tasks.

## OUTLINE COMPLETE
