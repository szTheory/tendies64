# Phase 6 CPU timing decision: Count, Random, and T1 costs

**Recommendation:** Treat CPU-08 as a bounded but real model change. Count's
guest-time accumulator and Random's retirement step already exist, but the CPU
still schedules every next attempt at one fixed guest tick. A qualification
or documentation pass alone would leave the accepted T1 per-class cost table
unimplemented. Add a small, versioned T1 model on a new profile, preserve all
current profile identities, and keep pipeline/hazard accuracy in T2.

## Options

| Option | Pros | Cons | Complexity | Recommendation |
|--------|------|------|------------|----------------|
| Keep the fixed one-tick CPU step and qualify its current Count/Random behavior | Reuses the independent Count clock and retirement-only Random step; no profile or scheduler behavior changes | One fixed interval is not ADR-0003 T1's per-instruction-class cost table; does not distinguish ordinary instructions from documented multicycle stalls | Documentation and native qualification only -- Risk: falsely labeling T0 behavior as T1 | Rec only if CPU-08 is explicitly re-scoped to T0 |
| Add a compact, versioned instruction-class cost table (T1) | Fulfills ADR-0003 and CPU-08; reuses Count phase, Compare interrupt, and Random retirement logic; primary manual supports a simple issue interval plus explicit multiply/divide stall classes | Cost table is a software timing model; it cannot represent pipeline overlap, operand hazards, cache misses, variable bus latency, or every exception-stage interaction | CPU scheduling and exception rescheduling, profile/digest identity, timing tests and docs -- Risk: event-order changes and mistaken hardware-accuracy claims | Rec if CPU-08 remains instruction-granular and the first table stays fixed-cost and bounded |
| Implement a cycle-accurate pipeline/interlock model (T2) | Can represent documented pipeline overlap, interlocks, and stage-ordered exceptions | Much larger state/scheduler change; requires stage-level CPU/RSP/device arbitration and evidence beyond a static class table | CPU, scheduler, cache/bus, digest, and device ordering -- Risk: broad semantic drift and unbounded scope | Rec only if later evidence shows T1 cannot represent a required behavior; keep deferred for this phase |

**Rationale:** The current CPU already has the right foundational split: Count
is synchronized from guest time, while Random advances on CPU retirement.
However, a fixed one-tick next-step schedule is not a cost model by instruction
class, so it cannot meet the accepted T1 contract by passing the existing
Count/Random checks. A separate profile can add the missing schedule semantics
without changing the warm-v1 retirement Count behavior or the frozen strict,
compatibility, and RSP profile identities. Describe its result as a selected
instruction-granular software model, not cycle accuracy or silicon
qualification.

## Current implementation and seam

- `core/t64_cp0.c:558-618` advances Count from elapsed guest ticks with a
  serialized half-clock phase, wrap handling, and Compare equality latch. The
  implementation already covers idle jumps and arbitrary advance partitions;
  Count does not depend on CPU retirement.
- `core/t64_cpu.c:1960-1974` schedules every next CPU attempt one tick later.
  `core/t64_exception.c:76-100` independently schedules the next handler
  attempt one tick after an exception. These are the missing instruction-cost
  semantics.
- `core/t64_cpu.c:2028-2052` synchronizes Count before interrupt sampling and
  instruction work. At `core/t64_cpu.c:2398-2418`, a prepared CP0 effect is
  committed, retirement counters increment, Random steps, and the next CPU
  event is scheduled. Thus TLBWR currently reads its selection before that
  instruction's Random step.
- `core/t64_cp0.c:678-680,720-753` resets Random to 31 on Wired writes and
  steps Random within the selected Wired..31 range. `core/tests/test_cpu_cp0.c`
  separately checks Wired reset and direct retirement steps; it does not pin
  their combined guest-instruction boundary ordering.
- `core/t64_scheduler.c:282-297,325-370` synchronizes time before every
  event, then dispatches stable `(tick, sequence)` FIFO order under the
  existing event budget. Variable CPU intervals will let already-scheduled
  RSP/device work run between CPU attempts; it must remain deterministic.
- Existing focused owners are `core/tests/test_cpu_time.c` (Count phase,
  Compare, idle partition, interrupt ordering, Random retirement) and
  `core/tests/test_cpu_cp0.c` (Wired/Random/TLBWR behavior). `test_cpu_time.c`
  currently selects the strict model only and directly tests the retirement
  helper, so T1 needs profile-specific integration assertions rather than
  relying on those checks alone.
- The strict VR4300 descriptor currently labels its timing tier
  `instruction-step-model-policy` (`core/t64_machine.c:158-205`). The active
  profile set includes warm-v1, strict CPU, compatibility v1/v2, and the
  functional RSP profile. Their model/state version pairs are exact profile
  identities, not ordered feature levels. CPU timing capability must be
  selected by an explicit profile predicate and identity check, never by
  `model_version >= N` or `state_version >= N`.

## T1 model contract to plan

1. Keep the current time unit (`model-rsp-cycle`) and the selected CPU:RSP
   `1:1` model ratio. They are software profile fields, not measured console
   clocks. Interpret one ordinary instruction issue interval as one guest
   tick. With Count's existing half-clock phase, two such ticks advance Count
   once; Random advances exactly once for a retired instruction regardless of
   that instruction's cost.
2. Use the NEC manual's smooth-flow statement as the baseline rationale: the
   five-stage pipeline can execute five instructions simultaneously, and a
   smoothly flowing pipeline completes one instruction per PCycle after fill.
   This supports a one-slot T1 issue interval, not a five-cycle serialized
   instruction latency. Keep the manual's branch delay slot as its own
   instruction; do not add a second branch penalty on top of the real delay
   slot.
3. Add a small fixed override for documented multiply/divide pipeline stalls.
   NEC §3.2.2 Table 3-12 (printed p.76) labels its values as cycles stalled:
   MULT/MULTU 5, DIV/DIVU 37, DMULT/DMULTU 8, DDIV/DDIVU 69 PCycles. The direct
   T1 reading is a base issue interval plus the named stall interval. Record
   this conversion as model policy, since a serialized instruction table does
   not reproduce the manual's overlapped pipeline. Keep ordinary load/store,
   CP0, COP1, and other instructions at the base interval unless this phase's
   evidence supplies a narrowly justified additional class. The separate
   cache/RDRAM timing decision owns cache-hit, write, miss, and dynamic device
   costs; expose one deterministic seam for those costs and compose it with
   CPU issue intervals instead of copying those rules into opcode
   classification here.
4. Define non-retiring boundaries explicitly: a sampled interrupt or
   instruction/fetch guest exception consumes the selected base boundary
   interval, changes no Random or retirement counter, and synchronizes Count
   through elapsed guest time. This preserves the current model's precise
   non-retirement rule; the manual does not state a fault-versus-Random
   micro-order. A model-unsupported/refused/host-error result
   must keep its existing no-retirement and no-Random effects. Count remains
   driven by elapsed ticks, including idle and intervening device events;
   it is not advanced once per attempt. Do not weaken stop limits, event
   budgets, result denominators, or refusal postconditions.
5. Preserve commit order for TLBWR: capture the pre-retirement Random entry,
   commit the TLB write, then step Random once for the retired instruction.
   The manual says a Wired write resets Random to 31 and Random decrements as
   instructions execute, but does not pin their same-instruction
   micro-order. The current guest path commits Wired then performs the normal
   retirement step, so the next visible Random value can be 30 (or remain 31
   when Wired is 31). Keep this as an explicit software policy unless a
   cited expectation requires a new profile policy; add a guest-level
   MTC0-Wired/Random/TLBWR sequence to pin the selected order.
6. Use a new profile ID for T1, retaining all current profile digests and
   behavior. The existing `timing_tier` field is serialized into the profile
   hash, so it can carry a value such as `t1-instruction-cost-v1` without
   changing the descriptor schema. Add an explicit canonical profile/capability
   predicate; don't infer T1 from model/state numbers. Bind future cost-table
   edits to a new profile/tier version. The current digest's state-v12 domain
   is specifically RSP-functional, so the new profile also needs its own
   accepted digest identity/domain rather than being smuggled through the RSP
   profile predicate. No new serialized CPU timing state is required if the
   pending CPU event tick and profile hash fully capture future timing.
7. Keep T2 deferred. Promotion requires a versioned target profile plus
   primary-document or independently hardware-derived evidence that identifies
   pipeline issue/retirement, cache/bus stalls, CP0 hazards, exception-stage
   priority, and CPU/RSP/device interleave for the named hardware revision.
   A passing software suite or agreement between emulators is not that
   evidence. Any new physical measurement remains subject to the owner's
   existing hardware stop.

## First discriminating probe and acceptance focus

Use a small project-authored instruction sequence with no external ROM or
protected asset. Compare a baseline pair of NOPs with a sequence containing
MULT and DIV. Assert relative CPU event ticks against the selected cost table,
retirement count, Count/half-phase (including a Compare equality at the
intervening tick), and exactly one Random step per retired instruction. At
phase zero, two NOP intervals end at `Count + 1`; a MULT interval of one issue
plus five stall ticks ends at `Count + 3`, with one Random step for the MULT.
These expectations derive from the cited half-PClock and stall counts and are
model outputs, not measured console results. Add a guest exception case to
prove no retirement/Random step while Count follows elapsed time, plus an
equal-tick RSP/device event case proving the existing FIFO order and
Count-before-handler boundary. Run the same sequence as one advance and
partitioned advances, then interleave two instances and compare canonical
digests. Keep event/resource caps and the existing refusal matrix unchanged.

These assertions establish the chosen deterministic T1 software contract and
that the suite's cited expectation is represented. They do not establish
hardware cycle accuracy. Full-base suite membership and the RDRAM/MI/cold-boot
qualification are owned by their separate Phase 6 decisions; this decision
only specifies how their CPU timing observations meet the shared scheduler.

## Primary references and evidence boundary

The canonical source is NEC, [*VR4300/VR4305/VR4310 64-Bit Microprocessor
User's Manual*](https://hack64.net/docs/VR43XX.pdf), U10504EJ7V0UMJ1, 7th
edition, August 2000, SHA-256
`b7c1900f1707a4b238ebd82af1669e9628e4df4875ceabfa012a9d95b4716b75`, as
indexed in [Phase 3 Primary Evidence Index](../03-first-headless-rom-through-open-ipl3/03-PRIMARY-EVIDENCE-INDEX.md).
The bytes remain outside the repository.

| Manual evidence | Expectation it supports | Limit |
|---|---|---|
| §4.1, pp.89–90; §4.6.4, p.109 | Five pipeline stages; at least five PCycles for a single instruction's end-to-end path; one completed instruction per PCycle in smooth flow; multicycle instructions can stall the pipeline | T1's single issue interval is a serialized model of throughput, not full pipeline latency/overlap |
| §3.2.2, Table 3-12, p.76 | Fixed integer MDU stall classes: 5/5, 37/37, 8/8, and 69/69 PCycles for MULT/MULTU, DIV/DIVU, DMULT/DMULTU, and DDIV/DDIVU | A tabled stall count does not specify every inter-instruction hazard or the best conversion into the emulator's chosen guest tick |
| §3.2.3, Table 3-13, p.77 | Branch and jump each have one architectural delay-slot instruction | The interpreter already executes the delay slot as a guest instruction; this is not an extra cost to add twice |
| §5.4.2, pp.146–147; §5.4.4, p.150 | Random decrements as instructions execute within Wired..31; writing Wired resets Random to 31; TLBWR uses Random's TLB index | Manual does not specify a software-visible same-instruction Wired-reset/decrement order |
| §6.3.3–6.3.4, pp.164–165 | Count free-runs at half PClock independently of instruction execution; Count/Compare equality sets IP7; Compare write clears timer pending | Profile's 1:1 CPU:RSP guest tick and half-tick mapping are selected software values, not measured NUS-CPU-03 clocks |
| §11.2, pp.274–275; §11.3, pp.277–282 | Cache read/write and refill behavior can add timing beyond a fixed instruction-class cost | RDRAM, cache miss, and bus timing require separate evidence and are not claimed by this CPU08 model |

The T1 model should cite these manual sections and its own explicit mapping
rules in the versioned CPU timing spec. Keep hardware-document facts,
model-policy choices, tests, and actual suite outcomes labeled separately.

## Reversal evidence

### Synthesis constraints from the approved compatibility policy

The strict-family Random/Wired discussion above is not a replacement for the
already approved compat-v2 policy. New cold profiles derive from that selected
compatibility family: preserve its six-bit Wired/Random behavior, including
the Wired high-bit modulo64 case, as documented in `docs/spec/cpu-cp0.md`.
Keep retirement-only stepping and pre-step TLBWR selection, and test both the
ordinary range and the approved compatibility case through actual instructions.
Do not silently select the strict family's range merely because its test was
the original timing owner.

The Phase 6 initial table remains static and bounded. Dynamic cache/bus timing
is not an additional Phase 6 workstream; retain the existing functional cache
and memory policies. Any later additional cost class needs concrete evidence
and its own versioned policy. The primary table's stall-to-guest-tick mapping
must be checked against the actual cited page before implementation; the
chosen issue-plus-stall conversion remains explicit model policy.

Revisit this recommendation if a rights-cleared authored expectation cannot be
represented by the fixed per-class table, or if primary/manual or
independently hardware-derived evidence shows the relevant result depends on
overlap, interlocks, or stage ordering that T1 deliberately omits. In the
first case, refine the table only with a named new profile version; in the
second, promote the scope to T2 with its own evidence and identity. Do not
revise the historical behavior or digest of warm-v1, strict CPU, compatibility,
or functional RSP profiles.
