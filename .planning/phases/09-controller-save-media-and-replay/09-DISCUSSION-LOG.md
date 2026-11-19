# Phase 9: Controller, Save Media and Replay - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-11-19
**Phase:** 09-controller-save-media-and-replay
**Areas discussed:** Input timing semantics; Profile identity and versioning; Canonical-payload walker and work order; EEPROM busy window; Unverified timing constants; Supervisor file-size limit; Scheduler ordering policy; State import staging; Replay artifact; EEPROM block rule; Plan structure and review placement

**Basis and honesty note.** This discussion ran through the decision-focused fan-out
required by `.planning/AUTONOMY.md` section 1: five independent cluster researchers, one
synthesizer, and one independent adversarial reviewer. No question was put to the owner and
no owner approval of any individual decision is recorded or claimed. The basis for proceeding
is the autonomy policy for routine reversible decisions plus the owner's instruction "follow
ur recs" to proceed with planning Phase 9. "Selected" below means the synthesized default
carried into `09-CONTEXT.md`. No ROM, save, firmware, oracle or hardware measurement was used.

---

## The fan-out: clusters and what each recommended

| Cluster | Scope | Main recommendation | Outcome after synthesis and review |
|---------|-------|---------------------|------------------------------------|
| A (`09-FANOUT-A-si-pif-controller.md`) | SAVE-01: SI, PIF, joybus, controller | HLE PIF behind SI (no PIF ROM); SI registers and DMA mirroring the PI model; joybus parser; port-0 controller; strictly-after input latch; software-policy timing; `controller-poll` fixture | Adopted, with corrections: misaligned address sets DMA_ERROR, command-byte refusal made consistent with the "malformed data is guest-visible" rule, in-guest Count witness added; claim that the runner has no input path was wrong |
| B (`09-FANOUT-B-save-media.md`) | SAVE-02/03/04: EEPROM, SRAM, import/export | Profile-selected devices; exact-length EEPROM commands; `busy_until` deadline with no scheduler event; 32 KiB SRAM via PI DMA both ways; explicit RAW/SWAP16/WORDREV32 formats; replacement clears saves | Adopted, with corrections: reuse the existing `save_kind`/`peripheral_flags` instead of new fields and validate them; 4 Kbit mirror instead of refusal; busy label says ceiling not exact |
| C (`09-FANOUT-C-state-replay.md`) | CORE-04/05: state and replay | Canonical payload as digest preimage; binary container; staged import with an allocator extension; exact build binding; `build_id` out of the new digest; replay as an operation log with submission tick `at` | Adopted, except import is validate-then-apply with no allocation, and the invariant contract is memory safety plus liveness; replay header gains `save_init`; goldens must be build-independent |
| E (`09-FANOUT-E-scheduler.md`) | CORE-02: scheduler ordering and quantum | New `tick-class-sequence-v2` class-rank policy; quantum pinned to 1 for the new descriptor; strictly-future host input at tick boundaries | Quantum pin adopted; class-rank policy and the core API change rejected (FIFO kept); boundary-submission discipline moved into the replay driver |
| F (`09-FANOUT-F-qa-slicing.md`) | QA, security, DevOps, plan slicing | One composite profile for the phase; eight sequential plans; harness frozen early; five authored ROMs; threat-model checklist; claim tiers; risk register | Adopted in shape; plan count grew to eleven; "harness frozen after Plan 1" corrected; "no second publisher" overruled |
| Synthesis (`09-DISCUSSION-PROPOSAL.md`) | Reconciliation | One recommendation with verification log, seven conflict resolutions, decisions, plans, risks, claim tiers | Revised after the adversarial review (section 10 of the proposal) |
| Adversarial (`09-DISCUSSION-ADVERSARIAL.md`) | Independent attack | Verdict PROCEED WITH CHANGES; findings H1-H6, M1-M9, L1-L6 | All accepted or accepted-modified (none rejected outright); no genuine owner-level stop |

Each synthesizer claim that a decision depended on was re-checked directly in the tree
before reliance; the proposal's verification log records which cluster claims were verified,
imprecise or wrong (for example: allocation callbacks also run at admission and replacement;
`save_kind` already exists; the runner already queues a `t64.input` schedule; the runner and
supervisor are pinned to approved fixture and profile identities).

---

## Input timing semantics

| Option | Description | Selected |
|--------|-------------|----------|
| A: strictly-after latch | Per-port `current`/`next` with `visible_from = stamp + 1`; no scheduler change | ✓ |
| E: rank 0 plus strictly-future submission | Host input dispatches first at its tick; core refuses `tick <= machine tick`; submit only at completed-tick boundaries | |
| C: replay-only fix | Leave core as is; record the submission tick `at` so replay is exact | ✓ (combined with A) |

**Choice recorded:** the synthesized default combines A (behavior) with C's `at` log (digest),
and turns E's boundary-submission idea into a recorder/driver rule only.
**Notes:** the only history-dependent hazard is the host call time that sets the sequence number
of a host-submitted event; the latch removes the behavioral dependence without a scheduler
change, and the `at` log makes the pending-queue digest exact. Rejecting E's core refusal keeps
the legacy API and runner path working. Reversal evidence: a partition-variant coincident pair
or a guest needing "visible at the stamped tick".

---

## Profile identity and versioning

| Option | Description | Selected |
|--------|-------------|----------|
| One composite family | One descriptor/model/state triple (12/20/25); four ids differing only in `save_kind` | ✓ |
| Version per plan or per cluster | Separate bumps for SI, save, state, scheduler | |
| Reuse the Phase 8 profile with optional devices | No new identity | |

**Choice recorded:** one family; selector is the existing `save_kind`, now validated; new `si_policy`
and `save_policy` labels; id scheme fixed under the 63-character limit; identity frozen at the start
of the final plan after review.
**Notes:** the compatibility policy forbids a version per fix; SAVE-02 requires the profile to
select devices. The review added field validation and the id scheme.

---

## Canonical-payload walker and work order

| Option | Description | Selected |
|--------|-------------|----------|
| Frozen walker skeleton in the first plan | Sink seam, framed v25 sections, differential against the v24 stream; devices append sections | ✓ |
| Separate exporter beside an untouched digest | Keep the digest as is and maintain a parallel writer | |
| Build the walker last with state export | Earlier plans add digest fields only | |

**Choice recorded:** skeleton in 09-01; replay before state export; state split into export and
import plans. The separate-exporter option is retired as a fallback.
**Notes:** the review showed the seam is byte-identical by construction and the risk is the new
v25 port, which legacy pins cannot see; the strip-and-compare differential is the oracle.

---

## EEPROM busy window

| Option | Description | Selected |
|--------|-------------|----------|
| Stored deadline, compared at command time | No scheduler event; partition-invariant; no import invariant for EEPROM | ✓ |
| Scheduler event clears a busy flag | Every busy device has a reserved event; needs a rank and a queue slot | |
| No busy model | Writes always succeed | |

**Choice recorded:** deadline compare. The import invariant "busy implies reserved event" applies
to the other devices, not EEPROM; state carries `busy_until`.

---

## Unverified timing constants

| Option | Description | Selected |
|--------|-------------|----------|
| Named software-profile policy in the profile labels | Constants fixed in code behind labels; tests described as self-consistency | ✓ |
| Block on hardware measurement | Owner physical measurement before implementing | |
| Treat one source as authoritative | Pick 6, 15 or 30 ms as fact | |

**Choice recorded:** labeled policy; EEPROM busy is the sourced "up to 6 ms" ceiling used as a
fixed 375,000-tick duration (label says ceiling); SI constants are placeholders. No owner stop:
Phase 9 needs no hardware claim, and measurement would be an AUTONOMY section 4 stop only if
someone chose to pursue it.

---

## Supervisor file-size limit (RLIMIT_FSIZE about 68 KiB)

| Option | Description | Selected |
|--------|-------------|----------|
| Option-driven raise, exact bound, second small publisher | `--artifact-dir`/`--artifact-max-bytes`; limit = requested bytes plus slack only when present | ✓ |
| Generalize the raw-capture module | One publisher for all outputs | |
| Raise the limit globally | Larger default for the real runner | |

**Choice recorded:** option-driven raise owned by the runner/supervisor plan; a second narrow
publisher copies the Phase 8 transaction pattern because the raw-capture file is packet-specific
and was the locus of the critical Phase 8 publication race.

---

## Scheduler ordering policy

| Option | Description | Selected |
|--------|-------------|----------|
| Keep `tick-sequence-fifo-v1` | Characterize and pin ties; add required mutants | ✓ |
| `tick-class-sequence-v2` class rank | Fixed rank table by device and kind; CPU last | |
| Rank only for host events | Minimal rank for input | |

**Choice recorded:** FIFO retained. The rank table would reorder qualified Phase 7/8 interplay in
the new profile with no hardware evidence for any tie, and the host-timing hazard is closed
without it. Reversal evidence: a partition-variant pair or a guest-required tie.

---

## State import staging

| Option | Description | Selected |
|--------|-------------|----------|
| Validate then apply, no allocation | Single reader with two modes; memory-safety-plus-liveness contract; generated field-use audit | ✓ |
| Staged image through the host allocator | Extends the allocator contract to import | |
| Persistent shadow machine | Doubles resident memory | |

**Choice recorded:** no allocation; the allocator contract (callbacks only at create, admit,
replace, destroy) is unchanged. Reachability is explicitly not promised.

---

## Replay artifact

| Option | Description | Selected |
|--------|-------------|----------|
| Operation log with submission tick `at`, canonical JSON | Binds profile, build, content and initial save identity; cold-admit only | ✓ |
| Frame-indexed input stream | Binary, per-frame samples | |
| Embed a state snapshot | State-anchored replay | |

**Choice recorded:** operation log; build-independent committed goldens; original run driven from
an independently authored script; state-anchored replay deferred.

---

## EEPROM block rule (4 Kbit blocks 64-255)

| Option | Description | Selected |
|--------|-------------|----------|
| Documented mirror `block & 0x3F` | Single-source community documentation, re-fetched | ✓ |
| Refuse out-of-range blocks | Cluster B's choice; no source | |

**Choice recorded:** documented mirror as the default, labeled single-source and unverified; refusal
kept as the recorded alternative.

---

## Plan structure and review placement

| Option | Description | Selected |
|--------|-------------|----------|
| Eleven plans; review and fix loop before the identity freeze | Core and runner mechanism split; state split into export and import | ✓ |
| Eight plans (cluster F) | Review and audit inside the qualification plan | |
| Nine plans | Split only the first plan | |

**Choice recorded:** eleven strictly sequential plans (09-01 core identity and walker; 09-02 runner,
supervisor, publisher, harness; 09-03 SI/controller tracer and first hosted data; 09-04 save devices;
09-05 save import/export; 09-06 ordering matrix; 09-07 replay; 09-08 state export; 09-09 state
import; 09-10 review, audit and fix loop; 09-11 freeze and qualification).

---

## Adversarial review and dispositions

The reviewer returned PROCEED WITH CHANGES with no blocker. The synthesizer re-verified each finding's
code or source claim before disposing of it (details in section 10 of `09-DISCUSSION-PROPOSAL.md`).

| Finding | Subject | Disposition |
|---------|---------|-------------|
| H1 | First plan is too large; publisher and forwarding mis-described | accept-modified (split; second small publisher chosen; no pre-built forwarding) |
| H2 | Review and audit after the freeze | accept (09-10 before F2) |
| H3 | v25 walker has no oracle for omissions; wrong fallback | accept (strip-and-compare differential; fallback retired) |
| H4 | Import invariants specified as reachability | accept (memory safety plus liveness; field-use audit; stalled-event test) |
| H5 | Boundary matrix without fixtures | accept (named construction sources; capped set; plan split) |
| H6 | CPU store preflight undercounts SI events | accept (profile-gated first task of 09-03) |
| M1 | EEPROM mirror and "exact" label | accept (mirror default; ceiling label) |
| M2 | `save_kind`/`peripheral_flags` unvalidated | accept-modified (validate on descriptor 12 and refuse non-zero at create for older versions; legacy hashing unchanged) |
| M3 | Build-dependent goldens; recorder circularity | accept |
| M4 | CORE-02 evidence weaker than claimed | accept (required mutants; limits stated; untested sentence deleted) |
| M5 | Hosted budget used the warm figure | accept (cold-cache budget) |
| M6 | Harness freeze contradiction | accept (collector lands once in 09-03; freeze table) |
| M7 | SI choices versus the source | accept-modified (the source is silent on DMA-versus-DMA waiting; misaligned address documented as setting DMA_ERROR; refusal kept as named policy) |
| M8 | Fixture time evidence policy-derived | accept (CP0 Count witness) |
| M9 | Profile id budget | accept (scheme fixed) |
| L1-L6 | FSIZE scope, stale digest text, scanners, provenance, effort, replay mechanics | accept |

**Owner-level result:** none. The review and the synthesizer's re-verification found no rights,
physical, credential, paid, destructive, oracle, release-channel, naming or D2/D4/D5 stop.

---

## Claude's Discretion

Internal representations, final label and domain strings within the 63-character limit, section ids,
test names, container field order, state-boundary builder choice (with a reached assertion), provisional
flag names and per-plan task breakdown.

## Deferred Ideas

Class-rank scheduler order and Q > 1 bursts; FlashRAM, Pak accessories, RTC, 64DD; ports 1-3 and PIF
challenge; cross-build state import, state-anchored replay, binary replay schema, rewind; optional hardware
measurement of EEPROM/SI timing (an owner stop when pursued); larger raw capture; API/ABI freeze and
installed consumer (Phase 10); host player and playable homebrew (Phase 11); a docs-only CI lane.
