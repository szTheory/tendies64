# Phase 9 discussion: adversarial review of 09-DISCUSSION-PROPOSAL.md

Reviewer scope: attack the proposal before planning. Code claims were re-read in
the tree (file:line below). External facts were re-fetched from n64brew and the
libdragon reference. No ROM, save, firmware, oracle or hardware was used. Nothing
else was edited.

**VERDICT: PROCEED WITH CHANGES.** No blocker. The core decisions (FIFO order kept,
strictly-after input latch, deadline-compare EEPROM busy, profile-selected media,
quantum pinned to 1) survive attack. Six HIGH findings concern plan structure,
evidence that cannot fail, and under-costed import/CPU work. They change the plan
breakdown, not the architecture.

---

## Findings

### HIGH

**H1. Plan 09-01 is two or three plans, and its biggest item is mis-described.**
Claim: Plan 01 lands identity family, sink walker, quantum, E0 ledger, artifact-dir
and FSIZE, table-driven supervisor forwarding, fixture/profile table generalization,
harness stub, claim skeleton and scanner rules (Proposal section 4).
Why it fails:
- `--artifact-dir` is called "an exact analog" that reuses `runner/t64_raw_capture.c`
  "rules rather than a second publisher" (Conflict 6). That file is 1,352 lines and
  purpose-built for video/audio packets plus a manifest (`write_packet_file` `:558`,
  `append_manifest` `:690`, `validate_stream_files` `:917`, `valid_video_info`
  `:785`). It is not a generic blob publisher. Either it is generalized (editing the
  one file where Phase 8's critical CR-01 publication-source race lived,
  `08-REVIEW.md`, `runner/t64_raw_capture.c:1231-1249`) or a second publisher is
  written. Both are real security-sensitive work with no line in the cost estimate.
- "Table-driven supervisor forwarding" is designed in Plan 01 for runner flags that
  do not exist until Plans 04, 06 and 07. The supervisor's option handling is
  hand-rolled (`runner/t64_supervisor.c:474-513`, child argv build `:1496-1527`).
  A table designed before its consumers will be wrong and will be edited anyway.
- The version-membership sweep alone is large: 207 model/state comparisons and 39
  descriptor comparisons across `core/` and `runner/` (grep), plus the cumulative
  predicate chain in `core/t64_internal.h:927-1100`.
Concrete failure: Plan 01 overruns, "Plan 02 may absorb Plan 01 residue" (Proposal
section 7), and the harness-freeze guarantee (R3) is lost before it starts.
Change: split into 09-01 (identity family, membership sweep, sink + v25 walker, Q==1,
E0 ledger; core only) and 09-02 (supervisor/runner/publisher mechanism, fixture and
profile table, scanners, harness stub). Do not pre-build forwarding for flags that
have no consumer; instead add the forwarding row in the plan that adds the flag and
declare that edit a normal part of that plan, qualified once at the end. Cost the
publisher explicitly, include the CR-01 substitution control against any new leaf
names, and re-run the Phase 8 raw-capture controls. Plan count becomes nine.

**H2. The independent review and security audit sit after the F2 freeze.**
Claim: F2 freezes literal pins at the start of Plan 08, Plan 08 does "review,
security audit, write literal pins once, ... no logic edits" (Proposal sections 2 and 4).
Why it fails: Phase 8's independent review found a critical race and a partial-
evidence gap after the code was "done" (`08-REVIEW.md` CR-01, WR-01), and native
qualification passed on its sixth attempt (`08-05-SUMMARY.md`). Phase 9 adds three
new untrusted-input parsers (state, replay, save), which is a larger review surface.
Findings after F2 are by definition logic edits, which the proposal itself treats as
a declared qualification reset (R3).
Concrete failure: the audit finds a state-import invariant hole; fixing it changes
section contents or validation, so F1/F2 and every replay golden are reopened inside
the plan that was supposed to be mechanical.
Change: add 09-08 "review and security audit, fix loop" ahead of F2 (end of Plan 07,
or a dedicated plan), with F2 and qualification as 09-09. Budget one fix cycle.
Require the audit to run against real mutated blobs from the Plan 07 sweep, not
only the code.

**H3. The v25 walker has no oracle for what it omits; the proposal's fallback is
aimed at the wrong risk.**
Claim: legacy pins prove byte-identity after the sink refactor; fallback O2 is a
separate exporter (Conflict 3, D-5).
What I verified: the sink seam itself is cheap and low risk. `core/t64_digest.c`
(819 lines) is one function; every write funnels through three helpers
(`hash_u32` `:8`, `hash_u64` `:18`, `hash_event` `:29`) and about 63
`t64_sha256_update` sites. SHA-256 updates are concatenation-invariant, so routing
them through a sink that either hashes or appends is byte-identical by construction.
The real risk is different: the *v25* walker is a new, section-ordered rewrite. The
legacy pins only exercise the legacy branch, so a field dropped or reordered in the
v25 port is invisible to every legacy pin and to T1/T2 (round trip through the same
omission passes). Only T3 against a boundary that exercises that field would catch
it. The proposal's `unit.core.digest.v25-reference` encodes descriptor and framing
only.
Change: (a) keep the sink design, retire O2 as the stated risk response; (b) add a
Plan 01 differential: the v25 stream with markers, new sections (SI, save),
`build_id` and `profile_id` stripped must equal the v24 stream for the same machine,
for each of the existing authored machines (idle, AI mid-DMA, VI frame, RDP state).
That is an independent oracle for the port and is cheap; (c) keep the
`sizeof/offsetof` tripwire as a second net; (d) record in D-5 that v25 digests are
no longer comparable to v24, which is intended.

**H4. State import: "no allocation, infallible commit" is achievable, but the
invariants are specified as reachability, which is not checkable and not needed.**
Claim: validate then apply with a bounded invariant summary; "every busy ... state
has exactly its reserved events with matching tokens" (D-30).
Attack results:
- Scheduler invariants are checkable in bounded memory: the queue is at most 256
  entries (`core/t64_internal.h:14`), the section table gives random access, and per
  (device, kind, token) counters fit on the stack. This part holds.
- Per-device "busy implies exact events" is not a uniform rule. CPU has one STEP
  event unless stopped, with a re-reserve path (`core/t64_cpu.c:1552-1567`); an
  exception path schedules another (`core/t64_exception.c:91`); PI has a canceled
  state with no event; VI/AI events may be queued but undispatchable
  (`core/t64_scheduler.c` `vi_event_waits_for_output_capacity`,
  `t64_ai_event_waits_for_output_capacity`). Each rule must be derived per device
  state machine; a wrong rule either refuses a legitimate export (caught by T1) or
  admits garbage (caught by nothing).
- A SHA-256 trailer and "declared digest" are integrity checks, not authentication.
  Anyone can build a blob whose hashes are all valid. The only defensible import
  contract is: a blob that passes validation cannot cause memory unsafety, an
  unbounded loop or arithmetic UB; semantically unreachable states are accepted and
  then deterministic. CPU icache/dcache/TLB contents are arbitrary-by-design and
  cannot be validated for reachability.
- Some observer state gates dispatch: `packet_count` (declared "not part of guest
  identity", `core/t64_internal.h` vi/ai state) decides whether a VI/AI event may
  dispatch. After import with "drop observers" the source and the restored machine
  can stop at different points of the same advance (resource-limit stop), while
  digests are equal.
Concrete failure: a crafted blob carries a TLB/RSP/RDP index or length field that
the importer range-checks nowhere because the per-field audit missed one of roughly
500 fields; the host reads out of bounds on the first advance.
Change: (a) restate D-30's contract as memory-safety plus liveness, explicitly
disclaiming reachability; (b) Plan 07 deliverable: a generated field-use table (every
field used as an index, length, shift, loop bound or array subscript in `core/`
mapped to its import range rule), checked by a script; (c) T3 includes the stalled
VI/AI event boundary and asserts the stop point after "drop observers" versus
continuity-restored import; (d) in tests only, recompute the digest after apply and
compare to the declared digest (not a rollback mechanism; the commit stays
infallible because no allocation or host call is involved: confirmed, core lint
forbids allocation `tools/lint/rules.txt` L1-NAME-ALLOC and mutable statics
L1-STATIC-MUTABLE, and `grep` found no mutable file-scope state in `core/`).

**H5. Plan 09-07's boundary matrix has no fixtures behind it.**
Claim: T3 boundary matrix of about 14 boundaries (PI active and canceled, SP active
plus pending, RSP mid-program, RDP pending triangle words, VI mid-field with
retained packet, AI paused partial packet and output-full stall, coincident events,
CPU delay slot with dirty cache/TLB, near-exhaustion, SI mid-transaction, EEPROM
busy, pending input) (D-32).
Why it fails: D-39 authors five ROMs (`pad-poll`, `eeprom-rw` x2, `sram-rw`,
`save-roundtrip`, `replay-mix`); none reaches RSP mid-program, RDP triangle words,
AI pause, or near-exhaustion. Those states are reachable only through existing
core test builders or new ones. Plan 07 already holds the exporter, container,
importer, invariants, fuzz, mutation sweep, T1-T4, runner flags and the FSIZE
real-runner control: too big for one executor and one review.
Change: name the construction source of each boundary now (existing `core/tests`
builders versus authored ROM), cap the required set, and split Plan 07 into export
plus container, then import plus invariants plus matrix.

**H6. The CPU store preflight question is not unknown; the answer shows a required
change to a shared path.**
Claim: [UNK] whether the CPU preflight counts SI's two events (Conflict 7 item 8).
Traced: `cpu_successor_extra_event_budget` returns 0 for any store whose width is
not 4, and exactly 1 for a 32-bit store to a non-RDRAM address
(`core/t64_cpu.c:1486-1503`); the successor preflight then requires
`needed_events` free slots and sequence headroom (`:1513-1550`). An SI start that
reserves execute and complete events before busy is visible needs 2, and an 8-byte
`sd` to an SI register needs a rule (refuse or count).
Concrete failure: with the queue at capacity minus one, an SI start reserves one
event, the second reserve fails after the CPU has committed the store; atomicity
(D-9) breaks, or the guest sees a half-started DMA.
Change: Plan 02's first task is the profile-gated budget change (legacy behavior
unchanged, pinned by the old-profile controls), with a small-capacity,
small-sequence-width test at `QUEUE_FULL` and `SEQUENCE_EXHAUSTED` and a width-8
store control. Alternative to cost: reserve only the execute event at start and
reserve the completion inside the execute handler, with a defined overflow outcome.

### MEDIUM

**M1. EEPROM facts: the documented behavior is replaced by an undocumented one, and
the label says "exact".**
Re-fetched: n64brew Joybus Protocol states the 512-byte part ignores the top two
block bits (blocks 64-255 repeat 0-63) and that a write "may take up to 6
milliseconds"; the libdragon reference says busy "for up to 6 milliseconds". I could
not find any 15 ms or 30 ms figure in the libdragon reference or the n64brew page;
treat those as unconfirmed.
Problems: D-19 chooses refusal for blocks 64-255, which has no source, over the one
documented behavior (mirror). D-20's label `eeprom-exact-6ms-375000t-v1` reads as a
hardware measurement; the sources give a *ceiling*. (Tick unit is 62.5 MHz,
`core/t64_vi.c:23`, so 6 ms = 375,000 ticks is arithmetically right.)
Change: default to the documented mirror (`block & 0x3F`), keep block-index test
patterns, record refusal as the alternative; rename the label to say ceiling or
policy (for example `eeprom-busy-ceiling-6ms-v1`), and delete "6 ms is the
permissive value" (a fixed-length busy at the maximum is not obviously permissive
to guests that do not poll). Classification stays "no owner stop"; the "later
hardware measurement" path is correctly an AUTONOMY section 4 stop (physical
measurement) and must stay described that way.

**M2. `save_kind` and `peripheral_flags` are not validated anywhere today.**
`core/t64_machine.c:1640-1641` only hashes them; no validator reads them (grep).
`T64_SAVE_NONE` is the only constant (`core/include/tendies64/t64_machine.h:95`).
A legacy-version descriptor with `save_kind = 3` is accepted and silently has no
device, which is the opposite of "explicit selection, unknown requests refused"
(SAVE-02, SAVE-04, criterion 2). D-23 covers the runtime API, not descriptor
creation.
Change: Plan 01 adds create-time refusal for unknown kinds, for non-zero kind or
flags on descriptor versions before 12, and for kind/flag combinations the version
does not define, each with an unchanged-allocator-counter control and an old-profile
pin showing legacy behavior is otherwise unchanged.

**M3. Replay and state artifacts can never be pinned goldens, and the CORE-05 tier
may compare code with itself.**
`build_id` hashes compiler id/version/flags, build type and the SHA of every core
input (`core/CMakeLists.txt:44-66`). Normal, ASan/UBSan and gcc:13 builds therefore
differ, and every core edit changes it. A recorded replay or state blob bound to
build id cannot be committed as a golden, so "regression-pinned: an earlier run of
this project" (section 8) has no stable artifact.
Also: if the recorder logs the driver's intent and the original run is driven by the
same op list, an omission in the recorder is replayed faithfully and nothing fails.
Change: goldens are build-independent (ops plus checkpoint digests as literals; the
header is composed at test time); ASan/gcc13 tests record fresh in-process; the
recorder wraps the actual accepted `t64_machine_queue_input` calls and the original
run is driven from an independently authored `t64.input` script; each of profile,
build and content mismatches has its own refusal control reaching the comparator;
`unit.replay.at-matters` mutates one `at` and must diverge.

**M4. CORE-02 completion: redefinition is legitimate, but the evidence is weaker than
the claim.**
Redefinition check: ROADMAP criterion 4 and `.planning/research/FEATURES.md:29`
require one guest-time total order, equal-tick order, split/whole advances,
interleaved instances and the quantum in profile identity. Nothing requires a class
rank or Q > 1. FIFO plus Q == 1 satisfies the text. Legitimate.
Weaknesses: (a) E0 with FIFO pinned characterizes existing behavior (regression pins);
the whole-versus-split matrix compares code with itself under different schedules,
so a consistently wrong tie passes. State this in the claim. (b) Required mutants:
handler reads `machine->tick` instead of `event->tick`; reservation sequence taken
at commit time; SI completion derived from per-advance budget; input promotion
before/after sample reversed; each must fail a named test. (c) D-4 asserts "results
may differ only for CPU-RSP races" with no test; delete it or label [UNK]. (d)
Refusing Q != 1 on descriptor 12 means "quantum sensitivity of the hash" can only be
shown at the hash function level (existing contrast at `core/tests/test_profile.c:455`
uses Q=2 on an older version); add that control explicitly. (e) MI is not an event
(`core/t64_cpu.c` samples at step start); criterion 4 lists MI, so the claim text
must say MI is ordered through its producers' events.

**M5. Hosted time budget uses the warm-cache figure.**
Warm macOS is about 275 s (`docs/ci-reviews/LOG.md` F-100), but the cold-cache run was
461 s and the Phase 8 binding run 507 s (`08-05-SUMMARY.md`). The cache is keyed by a
file the proposal promises not to touch, but caches expire and runner images change.
+40 s on 507 s leaves under 60 s of 600 s.
Change: set the budget against the cold figure; measure both in the Plan 02 hosted
pair; put state-hash-heavy T3/mutation loops native-only from the start.

**M6. "Harness frozen after Plan 01" contradicts Plan 02.**
Plan 01 ships a fail-closed stub, yet Plan 02 owns `harness.phase09-collect-real`
(collector and verifier built "against real data", the right Phase 8 lesson, F-098).
Then `scripts/qualify-phase09.py`, `ci.yml` and the scanner change in two plans.
Change: Plan 01/02 stub only; the real collector/verifier plus the `ci.yml` steps land
once, in the plan that first has real hosted data; freeze after that; list it in the
plan's frozen-after line.

**M7. SI behavior choices that contradict the one source or each other.**
n64brew PIF-NUS and Serial Interface (fetched): execution happens on the SI read DMA
(matches D-9/D-10, confirmed); bits 0x04/0x08 of the command byte are valid
run-mode flags (TX-to-RX delay about 520 us versus about 7 us); "reads issued during
a write wait until the write finishes"; DMA_ERROR is set on overlapping requests and
is cleared only by power reset. The proposal (a) refuses a write DMA with any command
bit other than 0x01 (D-10) while D-11 says malformed frames are "never host
refusals"; (b) sets sticky DMA_ERROR on a read started during a write, where the
source says it waits. A guest that writes then immediately reads is a normal pattern.
Change: pick one rule and label it: either model the read waiting (one extra state)
or keep refusal and state it as a named divergence; make D-10 consistent with D-11
(unknown bits are guest-visible, not host refusals, or document why not).

**M8. Fixture time evidence is derived from the model it tests.**
D-16 says the guest cannot know ticks, so expected times come from literals plus the
trace of the RD64B write. Those literals are the policy constants W, X and H, so the
"expected guest time" is self-consistency of policy, and wording must say so. The
guest can read CP0 Count; having the ROM print Count per poll gives an in-guest
time witness that does not depend on the harness trace (note the model's
Count = tick / 2 mapping, `core/t64_cp0.c:588`, and do not present Count as hardware
time).

**M9. Profile id budget is already spent.**
Current id is 55 of 63 characters (`runner/t64_real.c:40-41`, pinned again at `:488`
and `:681`). Four ids each carrying `si`, a version and a media suffix do not fit;
B's variants are acknowledged not to fit but no scheme is given.
Change: Plan 01 chooses the id scheme now (for example a shortened family stem plus
`-sv1-e4k`/`-e16k`/`-s32k`), and updates the runner's allowed-profile table
(`runner/t64_real_fixture.h`) with a test that every registered id is at most 63
characters.

### LOW

**L1. FSIZE raise.** Confirmed correct scope: the limit is set once for the whole
real-mode child (`runner/t64_supervisor.c:1534-1536`), so it also loosens the
report and capture files and stderr from about 68 KiB to the raised bound. Report
reading stays bounded (`:1608-1627`, `read_report` size-capped), so validation is not
weakened, but the kill-at-68 KiB backstop for a runaway runner becomes a kill at the
raised bound. Set the ceiling to `artifact_max_bytes + slack` exactly (the proposal's
flat 32 MiB is generous against a 10 MiB estimate that is [INF]), add controls that an
oversize report/capture is still refused with the option present, and note the
changed backstop in `docs/spec/raw-capture.md`.

**L2. Machine-contract digest text is stale** (`docs/spec/machine-contract.md:270-300`
describes v5/v8 as active). The proposal already schedules the fix in Plan 05; move
the stale-table correction to Plan 01 so the new domain string and section framing are
documented where the existing text lives.

**L3. Scanner rules already exist for part of this.** `scripts/rights-scan.sh:567-597`
already denies `.sav` and near-miss extensions with controls; extend that table for
state/replay extensions and magic rather than inventing a parallel scanner. Register
every new test in `scripts/check-test-inventory.sh` (baseline lists are hand-kept) and
every new fixture in `fixtures/manifest.json` with a build-only identity; the Phase 8
audio entry (`fixtures/manifest.json:658-710`) shows the required fields including the
IPL3 composition note.

**L4. Provenance of [COMM] facts.** Record source URL and fetch date per fact in the
spec; n64brew text may be licensed CC BY-SA, so state facts in authored words and do
not paste tables or vectors verbatim.

**L5. Effort.** 32-51 active hours is plausible for wall-clock but undercounts cycles.
Phase 8 (one device plus capture, five plans) took about 26 hours of commits from its
first to its closeout commit and six native qualification attempts. Phase 9 is
roughly three times the surface (SI/PIF, two save classes, three serialization
parsers, walker refactor). Plan for a review/fix loop and at least two qualification
cycles; do not gate anything on the estimate.

**L6. Replay mechanics.** The existing runner pre-queues up to 128 events at tick 0
(`runner/t64_input.h:11-12`, `runner/t64_real.c:1011-1155`) against a 256-entry queue;
a recorder of a scripted run must log `at = 0` for all of them. Define the driver's
outcome when a queued input hits `QUEUE_FULL` (divergence report, not silent).

---

## Requirement and success-criterion coverage

| Item | Plans/tests in proposal | Can a test fail it? | Gap |
| --- | --- | --- | --- |
| SAVE-01 / criterion 1 | 02: `runner.pad-poll`, `controls.real.pad-wrong-value`, latch tests | Yes: input shifted across E, wrong byte, wrong port-1 flag | Expected times are policy-derived (M8); fixture needs its own Count witness |
| SAVE-02 / criterion 2 (EEPROM, SRAM) | 03: selection, eeprom, busy-triple, sram, refusals | Yes, if descriptor-level refusal is added | M2 (silent unknown kind), M1 (mirror vs refusal) |
| SAVE-03 / criterion 2 (round trip, refused import digest unchanged) | 04 | Yes: literal 8-byte transform vectors, refusal matrix | None beyond privacy scan |
| SAVE-04 | 03, 04 | Yes | M2 |
| CORE-02 / criterion 4 | 01 ledger, 02 SI share, 05 matrix | Partly: ties are pinned regression; partition tests compare code with itself | M4: required mutants, MI wording, D-4 untested claim |
| CORE-04 / criterion 3 (state) | 01 walker, 07 | T3 can fail it; T1/T2 cannot detect omissions | H3, H4, H5 |
| CORE-05 / criterion 3 (replay) | 06 | Only with an independent original run and per-binding controls | M3 |
| Criterion 4 "quantum in identity" | D-4, D-37 | Hash-level only after Q != 1 is refused | M4(d) |

## Determinism trace (scenarios actually walked)

Latch: input X at tick 10 then Y at tick 10 (same tick, later call) gives
`next = {Y, 11}`; a sample at E = 10 sees the old value, E = 11 sees Y, an input at
tick 11 promotes Y first then stores Z with `visible_from = 12`. The result does not
depend on dispatch order at equal tick, and later calls win at equal stamped tick.
Scheduler: all queue operations are `(tick, sequence)` (`core/t64_scheduler.c:10-13`),
sequence is taken at reservation (`:120`), commit rechecks it (`:152-163`).
Two submission schedules of the same logical input (pre-queued at tick 0 versus
queued at a boundary) produce the same behavior but different `next_sequence` and
pending tuples, so different digests; the proposal states this and the `at` log
handles it. EEPROM `busy_until`: start and check are both guest events, so
partition-invariant; no cross-structure invariant exists. Count/Compare
synchronization (`core/t64_cp0.c:558-620`) is piecewise lossless for whole versus
split spans; I did not test a span longer than one Count wrap, which the E4 matrix
should include.

## Attacks that failed (decisions confirmed)

- FIFO retention (D-2) and quantum pinned to 1 (D-4) satisfy the roadmap wording.
- Strictly-after latch plus `at` discipline closes the host-timing hazard; no
  scheduler change is needed. Rejecting E's `tick > machine->tick` core refusal is
  correct (legacy API and runner path keep working).
- EEPROM busy as a deadline compare with no event is partition-invariant and keeps
  import free of an EEPROM invariant.
- Sink seam byte-identity for legacy digests is cheap (H3); the legacy pins are a
  sufficient guard for the seam.
- No allocation at import is consistent with the allocator contract and core lint;
  the commit can be infallible.
- CPU digest coverage: the CPU struct's fields (TLB, caches, Count latches, pending
  exception, delay-slot state) are all present in `core/t64_digest.c`.
- Rights and authority: no ROM, save, firmware, PIF image, hardware measurement,
  oracle or protected-content hash is required; the "no owner stop" classification is
  honest (`.planning/AUTONOMY.md` section 4). The authored ROMs rely on the same
  separately rights-cleared IPL3 composition as the audio fixture.
- Facts re-verified (n64brew Joybus Protocol, PIF-NUS, Serial Interface; libdragon
  `joybus.h`): SI base `0x04800000`, DRAM_ADDR `+0x00`, RD64B `+0x04`, WR64B `+0x10`,
  STATUS `+0x18`, any STATUS write acknowledges the interrupt, DMA_ERROR sticky;
  PIF RAM `0x1FC007C0`, 64 bytes, command byte at offset 63, bit `0x01` parse,
  execution on the read DMA; escapes `0x00`, `0xFD`, `0xFE`, `0xFF`; TX flags `0x80`
  skip / `0x40` reset, RX flags `0x80` no device / `0x40` timeout; five channels;
  controller identify `05 00` plus status; EEPROM ids `0x0080`/`0x00C0`
  (n64brew and libdragon agree), commands `0x04` (tx 2, rx 8) and `0x05` (tx 10, rx 1),
  busy `0x80`, up to 6 ms; bit times 4 us, console stop 3 us, controller stop 4 us.
  Labels: confirmed (two sources): EEPROM ids, busy bit, 6 ms ceiling. Confirmed
  (single source, n64brew): PIF layout, parse/execute split, escapes, flags, SI
  registers. Single-source and consistent with common layouts but not re-verifiable
  from text here: 16 button bit positions (n64brew uses an image). Not verified:
  SRAM at `0x08000000`/32 KiB, `0xFF`/`0x00` blank contents, the 7 us gap versus the
  520 us flag-dependent delay (M7), the 15 ms and 30 ms EEPROM figures. Contradicted:
  nothing outright; the refusal choices in M1 and M7 diverge from the single source.

## Questions that genuinely need the owner

None block planning. By the AUTONOMY section 4 standard, every item above is a
reversible engineering default. One optional point to record rather than ask: the
Phase 9 claims stay `unit-tested`/`regression-pinned` with all timing constants
labeled policy; a later hardware measurement of EEPROM or SI timing would be an
owner stop at that time (physical console measurement), not now.
