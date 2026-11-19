# Phase 9 fan-out E: completing CORE-02 (one guest-time scheduler for every in-scope device)

Status: research recommendation for the Phase 9 discussion synthesis. No source was changed. Roadmap success
criterion 4 and REQUIREMENTS CORE-02 are the target. Labels used throughout: **[HW]** hardware-document fact,
**[CODE]** code observation (file:line), **[POLICY]** explicit software-profile policy, **[INFER]** inference,
**[UNK]** unknown, **[REF]** reference-only lesson from another emulator or design article.

## 0. Headline findings

1. The scheduler orders by `(tick, sequence)` only. There is no device priority [CODE core/t64_scheduler.c:10-13].
   `sequence` is a global insertion counter, so equal-tick order is a function of scheduling *history*, not of the
   devices. The profile string `event_order_policy = "tick-sequence-fifo-v1"` pins exactly this
   [CODE core/t64_machine.c:49 and every later descriptor].
2. The CPU-to-RSP "synchronization quantum" is **identity-only and inert**. `synchronization_quantum` is validated
   `> 0` (core/t64_machine.c:1568), hashed (core/t64_machine.c:1649) and read by tests, but nothing in the core
   consumes it. A machine built from a test profile with quantum 2 runs identically to quantum 1
   [CODE core/tests/test_profile.c:455-470 creates and runs the contrast machine without refusal]. The same holds
   for `cpu_rsp_ratio_*` (validated and hashed only, core/t64_machine.c:1566-1567, 1647-1648). Real behavior is
   "quantum 1 by construction": the CPU and the RSP are separate one-tick event streams (core/t64_cpu.c:2077,
   core/t64_rsp.c:15-38).
3. Equal-tick order between a CPU step and a device event created by the same instruction is accidental. The CPU
   successor is reserved *before* the store's side effects, but when the instruction queued a device event the
   reservation is stale and is re-reserved *after* it [CODE core/t64_cpu.c:1552-1567]. So the successor gets a
   later sequence only when a device event was queued by that instruction.
4. Host-submitted input events (`t64_machine_queue_input`, core/t64_observe.c:115) take their sequence from the
   host call time. Their order against same-tick guest events, and the numeric `next_sequence` that enters the
   digest, therefore depend on *when the host called*. This is a replay hazard (cluster D) and must be closed by
   either a rank for host events, or a canonical submission discipline, or both.
5. Hardware documents are silent on same-tick ordering **[HW]**: the n64brew MI page lists interrupt sources and
   says the mask "signals right away" but does not say level versus edge or how simultaneous events resolve; the
   RSP pages checked state only the 62.5 MHz clock and say nothing on CPU/RSP synchronization (fetched
   2026-11-19: n64brew.dev/wiki/MIPS_Interface, /Reality_Signal_Processor, /Reality_Signal_Processor/CPU_Core).
   The NEC VR4300 manual and the existing docs (docs/spec/cpu-timing.md:95) already record CPU/RSP equal-tick
   ordering as an unproven T2 checklist item. Therefore **every** coincident-event rule is [POLICY]; none may be
   labeled hardware behavior.

## 1. Current-state audit (per device)

Scheduler core: single bounded array queue, capacity min(profile 256, `T64_EVENT_QUEUE_CAPACITY` 256)
[core/t64_internal.h:14, core/t64_scheduler.c:15-20]. Insertion sort from the tail (core/t64_scheduler.c:149-190).
Reserve-then-commit; a reservation records `expected_queue_count/expected_tick/sequence`, and commit refuses
(`INVALID_ARGUMENT`) on any drift (core/t64_scheduler.c:88-147, 149-166). Refusals (queue full, tick reversed,
sequence exhausted, tick overflow) happen before mutation and do not consume a sequence
[docs/spec/machine-contract.md:267-269]. Dispatch: `event_can_dispatch` gates on counter overflow and on VI/AI
output-queue backpressure (core/t64_scheduler.c:329-336), `dispatch_front` synchronizes CP0 time, pops, sets tick,
traces, runs the handler (core/t64_scheduler.c:338-356). Advance loops: time bound (381), counter bound (435); per
call limit `advance_event_budget` 4096 (core/t64_machine.c:43). Cancel is by `(device, kind, token)`, O(n), errors
on an ambiguous match (core/t64_scheduler.c:192-228).

| Device | How it participates today | Where (file:line) | Gap versus "ordered by the same scheduler" |
| --- | --- | --- | --- |
| CPU | One `T64_DEVICE_CPU` step event per issue interval (1 tick base, MDU intervals longer). Successor preflighted before effects, re-reserved if effects queued events. Exceptions reschedule +1. CP0 Count/Compare is *not* an event: derived from elapsed guest time at each dispatch (`t64_cp0_synchronize_guest_time`). | core/t64_cpu.c:1513-1567, 2077-2091, 2155, 2303, 2581; core/t64_exception.c:91-99; core/t64_scheduler.c:338-346 | Order versus same-tick device events is history-dependent (finding 3). Count==Compare interrupt visibility is "at CPU sample time", not an ordered event; document it. |
| RSP | Separate `T64_DEVICE_SP` kind `RSP_ISSUE` (token = issue sequence) at +1 per opportunity; started/woken by SP register writes and SP DMA promotion. Shares device id 4 with SP DMA completion (kind 1). | core/t64_rsp.c:15-52, 1341, 2458; core/t64_sp.c:202, 787 | Order key must be by (device, kind), not device id: two SP kinds have different roles. Quantum field unused. RSP vs CPU same-tick order untested ([HW]-unknown, docs/spec/cpu-timing.md:95). |
| SP DMA | Completion event at a precomputed `end_tick`; active plus one pending descriptor, promotion at completion. | core/t64_sp.c:265-272, 787-803 | Covered by `test_determinism` equal-tick PI-vs-SP check only. |
| PI | Completion event at `delay = base + len/divisor`, token-cancellable, sets MI PI. | core/t64_pi.c:89-97, 280-291 | Ordered by FIFO; tested PI-vs-SP and clear-vs-request (core/tests/test_pi_dma.c:944). |
| RDP | Work event +1 per command/row; FullSync raises MI DP. | core/t64_rdp.c:160-174, 792-801 | Asserted only RSP-before-RDP via sequence compare (core/tests/test_rdp.c:1144). |
| VI | One field/line event, generation-token, delay from phase arithmetic; docs/spec/vi.md:94 says a zero-delay event is allowed when several boundaries are crossed (code clamps some paths to >=1, core/t64_vi.c:798, 1148; other paths unverified); backpressure stall when video queue full. | core/t64_vi.c:802, 867, 1150, 1210, 1023; core/t64_scheduler.c:310-327 | Reconfiguration re-sequences the event. VI-vs-AI tie asserted in both insertion orders (core/tests/test_ai.c:1411, 2464), i.e. FIFO is the *tested contract*. |
| AI | Sample event at checked-ceiling cadence; pause/resume = `schedule_cancel` + fresh reserve (new, later sequence); MI AI at `ai_full` 1->0; backpressure stall. | core/t64_ai.c:102-106, 324-341, 575; core/t64_ai.c:376-390 | Pause/resume changes the sequence number and therefore tie order versus an uninterrupted run. Fine under FIFO (deterministic) but shows sequence is not a stable identity. |
| MI | **Not a scheduled device.** `t64_mi_set_pending` is called synchronously from source handlers (AI 575, PI 291, RDP 799, RSP 2423, SP 800, VI 1023). The CPU samples `mi_irq_line` at the top of each step. Only `T64_DEVICE_MI_TEST` exists as an event id (core/t64_internal.h:18). | core/t64_mi.c:28-60; core/t64_cp0.c:621-640; core/t64_cpu.c:2177 | "MI delivery to the CPU" = relative order of the producing handler and the next CPU step. Needs an explicit statement and a test per producing device. |
| SI / PIF | Bus window is a *warm* (non-device) region; PIF window is unsupported. No events, no ids. | core/t64_bus.c:138-142, 162 | Entire device missing (cluster A). Needs an id, kinds, rank. |
| Host timed input | `T64_TRACE_DEVICE_INPUT`(6) event whose handler is a no-op; no device consumes it yet. Allowed for `tick >= machine->tick`. | core/t64_observe.c:84-122 | Host-time-dependent sequence (finding 4). Consumer is cluster A's SI/joybus latch. |
| Save media | No events. | n/a | Busy windows (EEPROM write) are cluster B and need an id, kind, rank. |

Existing evidence (what already proves ordering): synthetic FIFO/inclusive-bound/halt-precedence/exhaustion
transactionality (core/tests/test_scheduler.c:163, 228, 281, 331, 564); partitioned vs one-shot vs interleaved two
machines for PI/SP (core/tests/test_determinism.c:285-333 and its same-tick trace check at :198); boot partitions
and a two-instance interleave (core/tests/test_boot_determinism.c:425-459); AI interleaved-machine isolation and
AI/VI tie (core/tests/test_ai.c:2375, 2464); RSP/RDP tie (core/tests/test_rdp.c:1144); PI clear tie
(core/tests/test_pi_dma.c:944); CPU Count partition (core/tests/test_cpu_time.c:274-342). **Missing**: a pairwise
tie matrix over all classes, any CPU-vs-device or CPU-vs-RSP same-tick test, counter-stop mid-tick plus host
injection, quantum-binding tests beyond a hash contrast, and everything involving SI/save/input consumers.

## 2. Decision points

### D-E1. Total order key for coincident events

Options:
- **A. Keep `(tick, sequence)` FIFO and only document and test it.** Zero identity change. Order stays
  history-dependent (findings 3 and 4). Refactoring handler internals can change guest-visible order with no test
  failing unless that exact pair is pinned.
- **B. `(tick, class_rank, sequence)`**, where `class_rank` is a fixed, profile-versioned function of
  `(device, kind)`, sequence breaks ties inside a class. Order becomes a specified property independent of
  scheduling history, cancel/reschedule, and (for host events) call timing.
- **C. Minimal: `(tick, host_vs_guest, sequence)`** - only host events get a rank; guest stays FIFO.

**RECOMMENDED: B, applied only through a new policy string** `event_order_policy = "tick-class-sequence-v2"`
carried by the single new Phase 9 composite descriptor/profile (not a profile per device, per
docs/spec/cpu-compatibility.md and STATE.md decision). All existing profiles keep `tick-sequence-fifo-v1` and
their bytes. `scheduler_version`/`event_order_policy` are already hashed for every descriptor version
(core/t64_machine.c:1636-1690 serialization), so the identity change is automatic and no existing hash moves.

Mechanics: rank is *derived* from `(device, kind)` by a pure table function, stored in the in-memory event at
reserve time, and **not** serialized (state export re-derives it exactly as handlers are re-derived). Compare
function: tick, then rank, then sequence. Rank lookups for unknown `(device, kind)` refuse the reservation
(`INVALID_ARGUMENT`) so a new device cannot be scheduled without a rank.

Candidate rank table (ascending runs first; **[POLICY]**, to be frozen by the characterization step in S1 below):

| rank | class | rationale |
| --- | --- | --- |
| 0 | host input (device 6) | "state stamped tick T is visible to everything that happens at T"; removes host-call-timing dependence |
| 1 | SI/PIF completion (new) | external I/O completes before consumers |
| 2 | save-media busy-end (new) | same |
| 3 | PI DMA complete | preserves the asserted PI-before-SP order (test_determinism.c:198) |
| 4 | SP DMA complete (device 4, kind 1) | |
| 5 | AI sample | |
| 6 | VI field/line | fixed AI<VI replaces the currently-asserted both-orders FIFO; this is an intended v2 change, not a regression |
| 7 | RSP issue (device 4, kind 2) | |
| 8 | RDP work | preserves the asserted RSP-before-RDP order (test_rdp.c:1144) |
| 9 | CPU step (device 5) | CPU samples the world last at its tick: MI line and DMA data produced at tick T are visible to a CPU step at T |

Why: every rule is a policy; the criteria were (1) preserve every ordering the repo already asserts, (2) producers
before consumers, (3) host-injected data before everything, (4) CPU last so interrupt/DMA visibility at a tick is
uniform. The CPU-last choice is the one most likely to need revisiting if T2 evidence (docs/spec/cpu-timing.md:95)
ever contradicts it.

Evidence/facts: [HW] none exists for any pair. [CODE] findings 1-4. [REF] ares organizes time as per-component
clocks synchronized by a scheduler with 64-bit relative time counters (archive.ares-emu.net near.sh schedulers
article; reference only), which does not settle same-instant order either: every emulator must choose.

Failure modes / risks:
- Zero-delay events at an already-dispatched rank (docs/spec/vi.md:94 permits a zero-delay VI catch-up event; verify in code before relying on it) run
  after higher-ranked events of the same tick. State the invariant precisely: "within one tick, dispatch is
  non-decreasing in (rank, sequence) among events present at each pop", not a global sort of the tick. Test it.
- Same-tick livelock from zero-delay loops is bounded by `advance_event_budget` (RESOURCE_LIMIT, state retained,
  resumable). Add a stall guard note for hosts: repeated RESOURCE_LIMIT with an unchanged tick is a host-visible
  "no progress" condition.
- Rank table drift: a swapped rank must fail a test (see evidence plan). Table stored beside the policy string and
  documented in docs/spec (new section in machine-contract.md).
- Host input at `tick == machine->tick` after a time-bound stop has *already dispatched* that tick's events would
  run late despite rank 0. Closed by D-E6.

Authored-evidence plan:
1. Characterization on the **unchanged** v1 policy first (S1): a table-driven test that, for each unordered pair
   of classes that can coincide, builds both insertion orders through public paths where possible, and pins the
   FIFO results. Retains the baseline proof that old profiles did not move.
2. v2 pairwise matrix: an independent *literal* expected-order table in the test (not derived from the
   production table). For each pair (A,B) and both insertion orders, assert dispatch order from trace records,
   queue front, and event sequences. Mutation control: a test build hook (or a copy of the production comparison
   under test) with two ranks swapped must make the matrix fail; the swap is exercised in the test itself via a
   second independently-built expected table with one transposition, asserting the production run does **not**
   match it.
3. Cross-class "causal chain" tests: CPU store -> device event with delay equal to the next MDU-interval step
   (PI delay 1+N/2 equals a DDIV interval); RSP COP0 write -> RDP work; AI sample -> MI AI -> CPU interrupt
   sample at the same tick; VI line interrupt at the same tick as an AI boundary and a CPU step.
4. Both old and new profile run the same authored program once; digest differs only by the intended policy;
   old profile digest vector stays pinned.

What would change the recommendation: owner wants zero new identity this phase (fall back to A with the full tie
matrix pinned as FIFO, cost: accidental-order risk stays); T2 hardware-derived evidence for CPU/RSP ordering;
discovery that a guest workload needs AI-before-VI in one program and VI-before-AI in another (rank cannot
express it; would need a profile-selectable table, i.e. another version).

Reversibility: high. Table and policy string live in one new descriptor; reverting means shipping a v3 policy
with a new profile id. Old identities untouched. Cost: re-baselining the new profile's digests only.

### D-E2. Hardware fact versus explicit software policy (documentation rule)

**RECOMMENDED:** a three-column table in docs/spec/machine-contract.md ("Event classes and ties") listing for
each tie pair: rule, label ([POLICY] for all of them today), source or "no primary source found", and the owning
test name. State plainly: no tie result is hardware evidence; the order is a deterministic model choice. Keep the
interrupt-source facts that *are* [HW] (MI sources, mask behavior: n64brew MI page; SDK RCP/SI documents already
cited in .planning/research/ARCHITECTURE.md:131) separate from the ordering rules. Note the n64brew wording for
AI ("set when the AI starts playing a new audio buffer") against the locked Phase 8 `ai_full` 1->0 policy
(STATE.md decisions; core/t64_ai.c:575): flag as an existing unresolved wording difference, not reopened here.
Evidence: authored doc-consistency check that every rank-table entry has a named test and a label. Reversibility:
documentation only.

### D-E3. CPU-to-RSP quantum: representation, bounds, identity, equivalence

Current: `uint32_t synchronization_quantum` [core/include/tendies64/t64_machine.h:151]; unit implied "model tick"
by docs/profile-limits.md:12; value 1 in all 17 descriptors (grep count); validation only `> 0`; hashed; **no behavior**.

Options:
- **A. Make the field honest, keep behavior at 1.** New descriptor version validates `synchronization_quantum == 1`
  (and `cpu_rsp_ratio` 1:1) and refuses creation otherwise with a profile-validation status; older descriptor
  versions keep today's permissive validation so existing test profiles still construct. Units stated:
  "RSP issue opportunities, equal to guest ticks (`model-rsp-cycle`), executed per RSP event; upper bound equals
  1 in this model version".
- **B. Implement Q>1** as an RSP burst (RSP event executes up to Q opportunities, bounded by the next queued event
  tick) and prove Q-sensitivity.
- **C. Leave as is** and only add documentation.

**RECOMMENDED: A**, plus a one-page statement of the equivalence classes (below). Rationale: roadmap criterion 4
requires the quantum to *remain part of profile identity*, which is already true; leaving an accepted-but-inert
nonzero value lets a profile claim a quantum it does not implement, which is an identity lie and the exact
"quantum as a tuning flag" pitfall (.planning/milestones/v1.0-research/ARCHITECTURE.md:435). B is a performance
feature (needed only for JIT/burst RSP in v2) and would add a second observable timing axis in the same phase as
SI/save/replay.

Equivalence statement (to be written into docs/spec): quantum is **guest-visible**. Differing quanta are allowed
to differ in any result that depends on a CPU-RSP race: RSP DMA reading RDRAM written by the CPU, CPU writes to
DMEM/IMEM/SP registers observed by running RSP code, SP_STATUS polling, SP interrupt latency, RSP-started RDP
work. They must be identical for runs that have no CPU-RSP interaction (RSP halted for the whole run; RSP-only
authored programs between a start and a BREAK with no CPU access to SP resources and no DMA). With only Q=1
implemented the matrix has a single column; the statement is the contract that a future Q>1 implementation must
satisfy and the compatibility rule is: any new quantum is a new profile.

Identity recording: already in the canonical descriptor hash (core/t64_machine.c:1649). Also recommend the
state digest domain for the new profile binds it indirectly via the profile hash (already how state vN works) and
that result records (docs/spec/result-protocol.md) carry the quantum value beside profile id and hash. Check the
result record already prints it; if not, add (small).

Evidence plan: (1) existing hash-contrast test kept for old descriptors; (2) new tests: new-descriptor profile
with Q=2 or ratio 2:1 refused with unchanged host allocator counters; Q=1 accepted; profile hash for the v2
profile pinned to a fixed vector computed by an independent reference encoder (the repo already does this,
core/tests/test_boot_entry.c:661); (3) a run-level test that the quantum value appears in the retained
result/identity record. Optional stretch (only if owner wants Q proven non-trivial): a test-only burst harness
demonstrating one CPU-store-to-DMEM race that differs between Q=1 and Q=4 and one RSP-isolated program that does
not.

Risks: refusing Q != 1 for the new descriptor could break a downstream consumer that builds a custom descriptor;
none exist in-repo (runner/t64_skeleton_probe.c:58 uses 1). What would change it: a v2 performance milestone that
needs RSP bursts. Reversibility: high (loosening validation in a later descriptor version).

### D-E4. What partition/interleave checks must assert

**RECOMMENDED:** one shared harness `core/tests/test_scheduler_order.c` (or an extension of test_determinism.c)
running a fixed workload per class pair and for a combined "all devices live" workload, comparing:
1. whole advance vs split at every tick in `{T-1, T, T+1}` around each coincidence tick vs host step sizes
   `{1, 3, 7, 4096, one-shot}` (primes avoid aliasing with 4096 budget): final state digest, trace record
   sequence (device, kind, tick, value), queue contents (tick, sequence, device, kind, token), stop result;
2. instruction-count and frame-count stops landing *mid-tick* (core/t64_scheduler.c:435-492 leaves later same-tick
   events queued), then resume: digest equals the uninterrupted run;
3. two machines of the same profile (and one of a different profile) advanced alternately in uneven slices: each
   equals its solo run (existing pattern test_determinism.c:285);
4. resource-limit stops (budget 4096, backpressure from VI/AI output queues) followed by resume equals uninterrupted;
5. trace-enabled vs trace-disabled equals digest (existing, keep for new devices);
6. exported-then-imported at a mid-tick stop equals uninterrupted (interface with cluster C; this is the
   strongest check that sequences and ranks round-trip).
Quantum-equality assertions: none beyond Q=1 (see D-E3); the harness takes the quantum as a parameter so a future
second value only adds a column.
Not asserted: equality between v1 and v2 policies (they intentionally differ on ties).
Failure modes: tests that read host wall time or thread scheduling (forbidden); comparing digests of runs whose
host events were submitted at different guest points (see D-E6) - a false failure, not an engine bug.
Reversibility: tests only.

### D-E5. Gaps to "every in-scope device ordered by the same scheduler" (concrete work list)

1. Class rank derivation and the v2 compare function: core/t64_scheduler.c:10-13 (compare), :88-147 (reserve
   stores rank, refuses unknown pair), :149-190 (commit uses new compare); event struct in
   core/t64_internal.h:835-844 gains a rank field that is not serialized.
2. Policy gating: `t64_has_*_profile`-style predicate for the v2 policy in core/t64_internal.h (near :927-1094) so
   the compare function selects FIFO or class order per profile; legacy profile path must be bit-identical.
3. CPU: nothing changes in how steps are reserved; the re-reserve quirk (core/t64_cpu.c:1552-1567) becomes
   irrelevant for ordering under v2 but must stay for v1.
4. RSP: both kinds already share device id 4; rank table splits them.
5. MI: document "synchronous in the producing handler; CPU samples at step start (core/t64_cpu.c:2177)" and add
   per-producer same-tick tests (AI, PI, RDP, RSP BREAK, SP, VI, SI). Do **not** make MI an event class; it has no
   delay of its own in the model **[INFER]** and doing so would be a behavior change beyond CORE-02.
6. SI/PIF (A) and save busy-end (B): register new `T64_DEVICE_*` ids (next free: 10, 11; ids 0-9 and
   0xffff000x are taken), kinds, rank slots, handlers, and trace ids; each must use reserve-then-commit with
   preflight of the whole mutation (SI DMA copy completion must be reserved before any PIF RAM write, mirroring
   PI/SP "request is atomic").
7. Host input: consumer in SI latch (A); order rule (rank 0) and submission rule (D-E6).
8. State export (C): serialize queue as ordered `(tick, sequence, device, kind, value, token)` and next sequence;
   import must rebuild handlers and ranks from `(device, kind)` and reject unknown pairs, ticks below machine
   tick, duplicate or >= next_sequence sequences, counts over capacity, out-of-order keys, tokens that do not match
   the device's latched token (VI generation, AI token, PI/SP token). This is the same validation the scheduler
   already assumes (core/t64_scheduler.c:88-147).
9. Documentation: machine-contract.md ordering section (:246-270) and vi.md:94 / audio-interface.md:66 phrasing
   ("existing tick/sequence FIFO") must name the policy version they refer to.

### D-E6. Host-submitted events: canonical submission rule (replay-critical)

Problem (finding 4): sequence numbers and `next_sequence` depend on when the host submits, and `next_sequence`
and queue contents are in the digest (docs/spec/machine-contract.md:279-281). Two replays that submit the same
input script with different lookahead or step sizes can have identical *behavior* but different digests.

Options: (A) leave and forbid digest comparison across host schedules; (B) give host events a separate sequence
namespace; (C) define canonical submission points and refuse ambiguous ones.

**RECOMMENDED: C, together with rank 0 (D-E1).** For the v2 policy: `queue_input` requires `tick > machine->tick`
(strictly future; today `>=` allowed, core/t64_observe.c:115), and the replay driver (cluster D) submits each
input only at a *completed-tick boundary* (after an inclusive time-bound stop at apply_tick-1, which drains all
events at or before that tick including ones scheduled during the drain, core/t64_scheduler.c:381-433). Then
sequence numbering and the pending-queue contents at any digest point are a function of guest time only. The
queue stays shallow (a bulk-queued long script would otherwise fill 256 slots: every pending input costs one).
Option B is rejected: pending input events are in the digest regardless, so a separate counter does not fix
lookahead dependence. Keep v1 behavior unchanged for old profiles.

Evidence: test that submits the same script (a) just-in-time at each boundary with step sizes 1/3/4096 and
(b) via two different canonical lookahead drivers; digests at replay checkpoints equal; and a refusal test for
`tick <= machine->tick` leaves the digest, queue and sequence unchanged. Risk: replay of a recording whose first
input is at tick 0/1 needs a defined first boundary (the machine tick at admission). What would change it: cluster
D chooses a pull-model input timeline instead of queued events, which makes this decision moot.

### D-E7. Failure modes

- **Sequence exhaustion:** 64-bit width, ~1.8e19 events; at the model rate of at most a few events per tick and
  62.5M ticks/s that is on the order of 10^4 years of guest time **[INFER]**, so it is a profile-configurable test
  hazard (`sequence_width_bits`, core/t64_machine.c:42) rather than a real limit. Contract stays
  `reject-before-mutation-v1` (core/t64_scheduler.c:114-120, 149-166). Phase 9 requirement: every new device
  request path (SI start, EEPROM write start, input submit) must be proven atomic on `SEQUENCE_EXHAUSTED` and
  `QUEUE_FULL` with the digest unchanged, using a small-width and small-capacity test profile.
- **Queue-full refusal atomicity:** already proven for PI/SP/RDP/VI/AI/CPU successor; the CPU preflight reserves
  capacity for device events caused by a store (docs/spec/cpu-timing.md:60-63). New requirement: SI DMA start and
  save busy start count in that preflight (a store that starts SI DMA must not half-commit).
- **Capacity budget:** pending max today is roughly CPU 1, RSP 1, SP 2, PI 1, RDP 1, VI 1, AI 1 plus SI 1, save 1,
  and input depth (D-E6 keeps it small): well under 256 **[INFER]**; export/import of a full 256 queue is a test
  case.
- **Starvation/event storms:** no priority starvation across ticks is possible because time only advances by
  dispatch; same-tick storms are bounded by the 4096 budget with resumable RESOURCE_LIMIT. Add a long-run test
  that a VI/AI backpressure stall with a full output queue never skips or reorders the stalled event (existing
  Phase 8 property; extend to v2 ranks).
- **Cancel ambiguity:** `t64_schedule_cancel` fails closed on duplicate `(device, kind, token)`
  (core/t64_scheduler.c:206-208); SI/save events must use unique tokens.

### D-E8. Performance and cost

Rank is a one-byte table lookup at reserve plus one extra integer compare in the tail-insertion sort; negligible
against a per-instruction handler, trace emit and CP0 sync. Per-tick work is dominated by one CPU event per
instruction and one RSP event per opportunity (about 1M+ events per 60 Hz frame at 62.5M ticks/s **[INFER]**);
the 4096 budget means a frame is ~250+ `advance` calls. Phase 9 should not optimize this; if profiling later
shows queue overhead, per-device "next event" slots can replace the array without changing the order key.
Test cost: the matrix is small authored workloads; keep inside the existing ten-minute jobs.

### D-E9. Compatibility policy application

Follow docs/spec/cpu-compatibility.md: one new composite descriptor version and machine/state version for Phase 9
(carrying SI/PIF, save selection, policy v2, quantum validation), appended to the profile table with a new
profile id; no existing descriptor, hash, domain or refusal changes. Pin one old-profile authored success and
refusal control per affected family (PITFALLS: .planning/research/PITFALLS.md:53-57). New-descriptor identity
check from AUTONOMY.md section 2: mutate every policy field (including `event_order_policy`,
`synchronization_quantum`, new SI/save policy strings) through hash, serialization and known-id canonical
construction; test empty/unterminated policy text.

## 3. Interfaces assumed from other clusters

- A (SI/PIF/joybus): allocates SI device id and kinds; supplies a handler for the host-input event (currently a
  no-op, core/t64_observe.c:84-93); registers rank slots through the table function in D-E1; uses
  reserve-then-commit with whole-effect preflight; joybus poll reads the controller latch "as of dispatch".
- B (save busy windows): allocates a device id/kind for busy-end, unique tokens, and attaches it to the CPU
  store preflight; busy-window durations come from the profile (cluster B), not the scheduler.
- C (state export): serializes the ordered queue entries exactly as listed in D-E5 item 8 plus next sequence and
  exhaustion flag; ranks are re-derived, never trusted from the file; imports are all-or-nothing.
- D (replay): submits inputs only at completed-tick boundaries and with `tick > machine tick` (D-E6); binds the
  replay to profile hash, build identity and content hash; digest checkpoints only at recorded boundaries.

## 4. Proposed requirement-to-slice mapping (CORE-02 and criterion 4)

| Slice | Content | Depends on | Proves |
| --- | --- | --- | --- |
| E0 | Characterization: pairwise tie matrix on existing profiles (FIFO pinned), CPU-vs-device and CPU-vs-RSP probes, quantum-inertness evidence recorded as a finding | none (can start now) | old identities unchanged; current ties documented |
| E1 | Policy v2: rank table, compare function, new descriptor/model/state versions, new profile, quantum/ratio validation, pinned independent hash vector | E0 | order key is a specified, versioned property; quantum cannot be silently ignored |
| E2 | Host-event rule: strictly-future input, rank 0, canonical submission tests | E1, interface from D | replay-safe ordering |
| E3 | Register SI/PIF and save busy-end classes; per-device atomic refusal tests at queue-full/sequence-exhausted | A and B device hooks landed | every in-scope device is in the table with an owning test |
| E4 | Partition/interleave/mid-tick-stop/resource-limit/export-import matrix over the all-devices workload | E1-E3, C export | criterion 4 "coincident-event and partition/interleave checks" |
| E5 | Docs and claim: machine-contract ordering section, quantum equivalence statement, claims entry limited to "unit-tested software ordering policy, authored workloads", no hardware claim | E4 | closes CORE-02 honestly |

CORE-02 is complete only when E0-E5 are done; the earliest slices (E0, E1) can run in parallel with A/B/C/D
design because they do not need the new devices.

## 5. Unknowns that need the owner

1. Accept a new event-order policy (v2, class rank) in the Phase 9 composite profile, versus keeping FIFO and only
   pinning it (D-E1)? Recommendation: v2. Either is reversible and non-blocking for other clusters, but A/B/C/D
   should know which before they finalize their device hooks.
2. Accept refusing `synchronization_quantum != 1` and ratio != 1:1 in the new descriptor (D-E3)? Recommendation:
   yes.
3. Accept the "completed-tick boundary, strictly-future" input submission rule as a Phase 9 contract (D-E6),
   coordinated with D's replay format? Recommendation: yes.
4. Final rank order for AI vs VI and CPU-last: no primary source resolves either; the owner may prefer to defer
   freezing until E0's characterization output is reviewed. [UNK, policy]
5. Whether to spend effort on the optional Q>1 proof harness (D-E3 stretch); default: no.
6. Whether Count/Compare interrupt visibility should be documented as a tick-ordered class or remain "CPU
   sample time" (it is not an event today). Default: document as is, no change.

## 6. Summary (for the synthesis)

See the 10-line summary returned with this report.
