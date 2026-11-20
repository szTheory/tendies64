# Phase 9 fan-out C: versioned machine state and input replay (CORE-04, CORE-05)

Status: research recommendation for the Phase 9 discussion synthesis. It is not
a decision record and it writes no source. Everything API/ABI-shaped is
provisional; Phase 10 owns the freeze. Roadmap success criterion 3 is the target:
"Exporting and re-importing machine state reproduces the canonical digest
exactly, and a recorded input replay bound to profile, build and content hash
reproduces the original run's digests."

Evidence labels used below: [DOC] hardware or vendor document fact, [CODE]
observation of this repository at the cited place, [REF] other-emulator design
lesson (reference only; nothing copied), [INFER] inference, [UNK] unknown.

## 0. Code observations that shape every decision

1. [CODE] There is no state serialization today. `t64_machine_digest`
   (`core/t64_digest.c:170`) is a one-way SHA-256 over a hand-ordered field
   stream, gated by about 20 `has_vNN_state` booleans. The active composite
   profile is descriptor 11 / model 19 / state serialization 24, domain string
   `t64.state.audio.ai.24` (`core/t64_digest.c:197`, `docs/spec/audio-interface.md`
   lines 1-9). "Export" therefore means writing a new lossless writer and a new
   strict reader; the digest cannot be reused as a container.
2. [CODE] `docs/spec/machine-contract.md` states the digest is "a versioned
   SHA-256 encoding, not a memory dump" and lists what is excluded (allocator,
   pointers, queue head, padding, trace, output-drain cursor). Old digests are
   preserved by version gating: `docs/spec/cpu-compatibility.md` shows v9, v10,
   v11 coexisting with separate profile hashes and domains, and the strict
   profile keeps its old identity.
3. [CODE] The digest already covers a lot more than the machine-contract table
   says: the full VR4300 state including TLB, I/D cache lines and FPRs
   (`hash_vr4300_cpu_state`, lines 105-168), RSP registers/vector/accumulator/
   ready ticks (lines ~455-490), RDP registers, TMEM, pending command and
   triangle words and the 2-byte-granule coverage arrays (lines ~496-600), VI
   registers, latched `active_registers`, phase counters and retained packets
   (lines ~600-760), AI descriptors, phase remainder, pause remainder and
   packet history (the `has_v24_state` block, lines ~745-819). The machine-contract
   table is stale against v24; the spec rewrite in this phase should fix it.
4. [CODE] Retained output is in the digest, but its observer cursor is not.
   Video and audio packet slot bytes and metadata are hashed for every slot of
   the 2-slot rings (`core/t64_digest.c` comment above the VI loop: "packet_head/
   count are observer state and intentionally stay out of the digest";
   `core/t64_internal.h:777-779, 815-817`). Audio slot is `(sequence-1) % 2`
   (`core/t64_ai.c:116-119`), independent of the drain head. Consequence: an
   exported state must carry already-drained packet slots (up to about 1.1 MiB),
   and the restored observer head/count must be reconstructed consistently.
5. [CODE] The retained packet metadata includes `build_id` (128 bytes) and
   `profile_id`, and both are hashed (`core/t64_ai.c:547`, `core/t64_vi.c:1067,
   1247`, digest loops). So today the canonical digest depends on the build the
   moment any packet exists. `T64_BUILD_ID` is a SHA-256 fingerprint over the
   manifest of compiler id/version, flags, platform and every core source hash
   (`core/CMakeLists.txt:45-66`), so any source change anywhere changes it.
   This is the single most important finding for build binding (D-C5).
6. [CODE] Pending scheduler events are tuples `{tick, sequence, device, kind,
   value, token}` plus a function pointer `handler` (`core/t64_internal.h:839-847`).
   The queue is a bounded sorted array of 256 (`T64_EVENT_QUEUE_CAPACITY`),
   insertion ordered by `(tick, sequence)` (`core/t64_scheduler.c:151-181`). The
   pointer is not serializable: import must rebuild it from `(device, kind)`
   through a closed table. Synthetic-test events have file-private handlers
   (`core/t64_scheduler.c:28-82`).
7. [CODE] Timed input today is a scheduler event (device 6, kind 1) with a
   no-op handler, pre-submitted by the runner at start: the real runner queues
   the whole `t64.input` v1 schedule before the first advance
   (`runner/t64_real.c:1135-1155`), capped at 128 events / 64 KiB
   (`runner/t64_input.h:11-14`). The pending event tuple, and therefore the
   sequence counter, is in the digest (`docs/spec/machine-contract.md`, "Output
   observation and timed input"). So the digest at a checkpoint depends on when
   each input was submitted, not only on its target tick (D-C8).
8. [CODE] Existing patterns to reuse rather than invent: bounded closed JSON
   parser plus canonical bytes plus SHA-256 (`runner/t64_input.c`), decimal-string
   u64s in a JSON manifest (`docs/spec/raw-capture.md`), atomic no-replace
   publication (raw capture), disjointness checks for caller spans
   (`t64_internal_observation_range_is_disjoint`), field-mutation digest tests
   with independent reference encoders, `core/tests/fuzz_rom_admission.c`.
9. [CODE] Core allocator callbacks run only at create/admit/replace/destroy
   (`docs/spec/machine-contract.md`, Host allocator contract). A staged import
   needs memory; see D-C4.
10. [CODE] ROM bytes are not part of the digest; only content size and SHA-256
    are. Admission resets all state (replacement reset table). So the natural
    order is admit, then import; the blob never contains the ROM.
11. [CODE] Approximate canonical size on the RDP/VI profile: RDRAM 4 MiB, RDP
    memory-coverage 2 MiB and depth-hidden 2 MiB (2-byte granule,
    `core/t64_internal.h:123-128`), video ring up to 614,400 bytes, audio ring up
    to 524,272 bytes, TMEM 4 KiB, CPU caches about 24 KiB. About 10 MiB total.

## Decision points

### D-C1. What "canonical machine state" covers (digest-equivalence rule)

Options:
- A. Export a curated list per device, maintained independently of the digest.
- B. Export exactly the digest's field set: invariant "every field hashed is
  serialized; every serialized field is hashed or appears in a written exclusion
  table". Add new fields to both together.
- C. Export a raw struct dump (rejected: layout/padding/endianness/pointers).

RECOMMENDED: B, with the new Phase 9 composite profile's digest defined as
SHA-256 over a domain string plus the framed canonical payload itself (see
D-C3, option O1). The payload is the digest preimage, so drift between digest
and export is impossible by construction; the reader is the inverse of the
writer; export-import-reexport byte equality (D-C6 T2) proves losslessness.

Coverage the canonical payload must have in the new profile (every item must be
reachable from a per-device reset-audit row and a field-mutation test):

| Device / area | Contents (from code today unless marked) |
| --- | --- |
| Identity header | profile digest, state serialization version, content size/SHA-256, parsed ROM header/title/code/version, loaded flag |
| Machine | guest tick, instruction/frame counters, sticky error, halt, scheduler next_sequence and exhausted flag |
| Scheduler | logical queue count and ordered tuples `{tick, sequence, device, kind, value, token}`; head normalized to 0 on import. Includes pending timed-input events |
| CPU VR4300 | GPR/HI/LO, PC/last/delay/origin, CP0 (incl. Count phase, Compare deadline, equality latch generation), pending exception, TLB (32), I-cache and D-cache lines with tags/valid/dirty/data, FPR/FCR0/FCR31, LL link state, COP2 latch, stop reason/detail; cache policy/last-operation fields |
| RSP | GPR, vector regs, accumulators, VCO/VCC/VCE, PC, delay/branch-bubble state, divider, scalar/vector/accumulator ready ticks, memory-access ready tick, issue sequence, SP status/semaphore, DMA cursors and tokens; SP DMEM/IMEM (8 KiB total); SP DMA raw latches and active/pending descriptors, next token, exhaustion |
| RDRAM | RDRAM bytes (profile length), RDRAM protocol/guest-device state (MI version, RI select, known flags) |
| RDP | register file, tiles, TMEM (4 KiB), pending command word and pending triangle words, color/texture/depth/other-modes state, counters, fence generation, triangle coverage, memory coverage and depth-hidden arrays |
| VI | raw and latched (`active_registers`) registers, phase remainder/cycles/units, boundary units, field and latch state, event generation, sequence exhaustion, retained video packet ring (metadata and bytes) |
| AI | staged address, control, DACRATE/BITRATE (raw and latched), phase remainder, sample index, descriptor queue (count, ordered entries), active/paused/pause remainder, partial packet (length, start tick, first sample, reserved slot), event token/generation, packet history count and rolling SHA-256, retained audio packet ring |
| PI / MI | PI raw latches, error, active/canceled flag, immutable descriptor, tokens, exhaustion; MI pending, mask, derived IRQ line |
| ISViewer / output | register buffer, retained 4 KiB capture, loss flag, attempted-byte total and saturation |
| SI / PIF / controller | [A] SI registers, DMA descriptor/busy/ERROR, PIF RAM image, joybus transaction/phase and per-port latched controller state, pending input timeline |
| Save devices | [B] selected device kind and size, EEPROM/SRAM bytes, EEPROM busy-window deadline/state and pending write; unselected devices contribute no bytes |
| Quantum | [E] CPU-to-RSP quantum is profile identity (hashed through the profile digest), not a field of state |

Derived values (MI IRQ line, PI busy, SP busy/full, AI busy/full) are
serialized only where the existing digest hashes them, and the importer must
recompute and compare them rather than trust them (D-C4 invariant checks).

Evidence: `core/t64_digest.c` (whole file), `docs/spec/machine-contract.md`
"Deterministic scheduler and state digest", `.planning/research/FEATURES.md` row
CORE-04 ("serialize every future-affecting machine field, including scheduler/
device queues; exclude host file handles and policies"), `.planning/research/
ARCHITECTURE.md` ownership table.

Failure modes / risks:
- A new device (A's SI/PIF, B's saves) lands state in the digest but not in the
  walker: export silently loses it. Mitigation: option O1 makes this impossible
  for the new profile, plus a struct-size tripwire (below).
- Padding or uninitialised struct bytes leaking into the stream: writer is
  field-by-field, big-endian, like `hash_u32/hash_u64`.
- The packet history in the canonical state is redundant for audio (a rolling
  history hash already exists) but needed for exact digest equality, because
  `packet_head/count` are excluded and so drained slots must remain hashed
  (code observation 4). Recorded as a wart, not redesigned (see D-C2).

Authored-evidence plan: (1) per-field mutation matrix for every serialized
field: toggle in a source machine, export, import into a clean peer, digest must
equal the source and differ from the unmutated baseline (the existing project
pattern for PI/SP/warm fields); (2) a tripwire test pinning `sizeof` and
`offsetof` of each state struct (`t64_pi_state`, `t64_sp_state`, `t64_rsp_state`,
`t64_cpu_state`, `t64_rdp_state`, `t64_vi_state`, `t64_ai_state`, and the new
SI/PIF/save structs) so adding a field breaks the build/test until the walker
and reset-audit table are updated; (3) independent reference encoder in the
test, built from the spec table, producing a fixed vector for a cleared
authored fixture state (pinned digest), separate from production code; (4) a
deliberately omitted field control: a test-only build flag that skips one field
in the writer must make T1/T2 fail.

What would change the recommendation: if the walker refactor of the digest is
judged too risky for legacy versions, keep `t64_digest.c` untouched for state
v5-v24 and use the walker only for the new profile (already the recommendation).
Reversibility: high; the container and walker are new files; no legacy digest
changes.

### D-C2. What is excluded, and why

Excluded from the canonical payload and the digest (consistent with current
code):
- Host observation: trace ring/drop count/enablement, ISViewer drain cursor,
  video/audio ring head and count (the observer cursor), `rsp_observed_*`
  diagnostics. [CODE `core/t64_internal.h:894-912`, `docs/spec/machine-contract.md`
  exclusion table.]
- Caller buffers, allocator callbacks/context, storage descriptors, pointers,
  queue head, unused slots, padding, build identity (as machine behavior).
- Durable host files: a save file on disk is host-owned (AGENTS.md deterministic
  core separate from host files/storage).

Justification: excluded state cannot change any guest-visible or future canonical
result; including it would make the digest depend on host drain cadence (the
Phase 8 tests already compare different drain timings).

Open sub-question: do undrained observer packets survive a restore? Options:
(a) refuse export unless observers are drained (clean but hostile to hosts);
(b) export a separate observer-continuity section: video/audio ring head and
count, ISViewer drain cursor; (c) silently drop (breaks "continue equals
uninterrupted run" for drained output bytes).

RECOMMENDED: (b). The section is in the container, covered by the integrity
trailer, excluded from the canonical digest, restored by default, and a test
proves the digest is identical whether the section is present, tampered with
(within validity) or absent. On import, the head is validated to be consistent
with the next audio/video descriptor sequence (slot is `(sequence-1) mod 2`
for audio; video is driven by producer sequence) and count at most 2. Trace
records are never exported and the live trace enablement is preserved by import
(host configuration). If the host passes "drop observers", head/count become the
consistent empty cursor.

Evidence: observation 4; `docs/spec/audio-interface.md` lines 100-107 ("observer
destination addresses and queue-drain position are host state, not canonical
guest state. Producer state and packet contents are included"); `docs/spec/
raw-capture.md` (host-side capture is separate; packet sequencing).

Failure modes: restoring head/count inconsistently with producer slots would
make the next produced packet overwrite an undrained slot or the consumer pop
an empty slot: the importer must validate rather than trust (D-C4).

Authored-evidence plan: T3 (continue equivalence) must compare drained packet
bytes, not only digests; a control that restores with a deliberately wrong head
must be refused.

What would change it: if Phase 10's output-ring contract changes (output rings
with backpressure are in the Phase 10 freeze list), revisit; a cleaner future
design removes retained-history bytes from the canonical digest and relies on
the cumulative history hash, but that changes video (no history hash exists) and
is not recommended now. Reversibility: high (section-level).

### D-C3. Binary container format

Options:
- O1. Container with framed sections where canonical digest = SHA-256(domain ||
  identity header || framed canonical sections); the stored canonical sections
  are exactly the preimage.
- O2. Container wraps a separately specified payload; digest stays a separate
  algorithm over machine fields (as today).
- O3. Text or JSON state (rejected: 10 MiB of mostly binary, parse cost, ambiguity).
- O4. Third-party serializer (rejected: `.planning/research/STACK.md:24-25`, no
  new dependency; explicit fixed-endian bytes).

RECOMMENDED: O1, big-endian, fixed-width integers (matches `hash_u32/hash_u64`,
the bus and the existing docs), no compression (the host may compress; core
does not).

Proposed layout (provisional; names illustrative):

```text
header (fixed 192 bytes, all big-endian)
  magic            8   "T64STATE" (not "t64.state.": that is the digest domain)
  container_ver    u32 1   (framing only; independent of state serialization ver)
  state_ser_ver    u32     (equals profile.state_serialization_version)
  total_length     u64     (must equal the supplied span length)
  section_count    u32     (<= 32)
  flags            u32     (must be 0 in v1; nonzero = refuse)
  profile_digest   32      (t64_profile_hash of the exporting profile)
  content_sha256   32
  content_length   u64
  build_id         128     (NUL padded, same text as packet metadata)
  guest_tick       u64     (informational duplicate; checked against payload)
  canonical_digest 32      (declared; recomputed on import)
section table: section_count x { id u32, flags u32 (0), offset u64, length u64,
                                  sha256 32 }
sections: canonical (ids ascending), then observer-continuity (excluded from digest)
trailer: SHA-256 of every preceding byte
```

Section ids are one per device group (identity, scheduler, cpu, cache/cp0 etc,
rsp+sp, rdram, rdp, vi, ai, pi/mi, isviewer, si/pif [A], save [B]) so a
divergence can be localised to a device. Per-section SHA-256 in the table is for
localisation and corruption detection, not security (an attacker can recompute;
see D-C4 on why structural validation, not hashes, is the safety boundary).
The canonical digest is `SHA-256("t64.state.<profile-family>.<N>" ||
len-prefixed identity fields || for each canonical section: id, length, bytes)`;
the exact domain string follows the existing `t64.state.*` naming but gets a new
number so no legacy digest is touched.

Caps and framing rules: total length must be at most the profile-derived
maximum (computed from profile: RDRAM bytes, coverage arrays, ring sizes,
fixed sections) plus small slack, and the importer rejects any length mismatch
before reading section bodies; hard ceiling 64 MiB for any profile; section
count at most 32; section lengths must equal the profile-derived expected size
for fixed-size sections and be bounded for variable ones; no trailing bytes; no
overlapping or out-of-order sections; unknown section id, unknown flag bit or
nonzero reserved field refuses (no "skip unknown" in v1, because every
canonical byte must be understood to claim an exact digest); all counts are
bounded by the compile-time capacities (queue 256, descriptors 2, TLB 32, cache
512 lines).

Compatibility and forward-refusal: a blob is importable only into a machine
whose profile digest equals the blob's and whose
`state_serialization_version` equals the blob's (the profile digest already
binds it; the field exists for a precise error). Unknown `container_ver`,
newer state versions or older ones with no importer refuse with distinct
statuses. No in-place migration: a field addition is a new profile and state
version (as `docs/spec/cpu-compatibility.md` does for v10/v11); older state
versions remain digest-only identities with their pinned vectors; the exporter
supports only profiles that declare exportable state (the Phase 9 composite
profile; descriptor 11 may be added only if cheap because its field set is a
strict subset).

Evidence: `docs/spec/cpu-compatibility.md` (separate profile identity and domain
per state version; v9 bytes unchanged); `docs/spec/machine-contract.md` ("fixed-
width big-endian fields"); [REF] libretro requires a nonzero-size state that
"cannot ever increase over time" for rewind, and says nothing about version
portability (https://docs.libretro.com/development/cores/developing-cores/):
lesson, a size/shape contract has to be stated explicitly, here via a
profile-derived expected length. [REF] Mupen64Plus savestate history: the
original format was unversioned and later got a magic and version after a TLB
save/restore bug; converting up makes older builds unable to load it
(Mupen64Plus `savestate_convert` notes via https://cocalc.com/github/alexbevi/
BizHawk/blob/master/libmupen64plus/mupen64plus-core/tools/savestate_convert.txt):
lesson, put magic and version in from day one and keep migration out of v1.
[REF] RetroArch's Mupen64Plus-Next uses its own state format incompatible with
standalone (same search): lesson, do not promise cross-emulator state.
[UNK] I could not retrieve ares's serializer source (404 on guessed paths), so
no ares claim is made.

Failure modes: a length or count read before validation causing an oversize
allocation or overread; version confusion between container framing and state
schema (hence two numbers); declared digest trusted without recomputation.

Authored-evidence plan: header field-by-field mutation matrix (each byte
position class: magic, versions, length +-1, section count 0/33/huge, offset
overlap, length overflow, flags, reserved, digest, trailer) all refused with
status X and unchanged live digest; independent reference decoder in the test
(not sharing production code) that parses an export and asserts literal
expected values for a hand-built tiny state; fixed container vector for an
authored fixture state with the build id injected by the test (see D-C5).

What would change it: a need for streaming/partial restore or compressed
rewind buffers would motivate TLV skippable sections and a size hint API;
Phase 10's ABI shape for the export call. Reversibility: container version
bump; the digest domain number would also bump.

### D-C4. Import validation order and atomic refusal

Options for staging: (A) allocate a staging image through the host allocator
at import, validate and fully populate it, then commit by an infallible
field-wise copy; (B) no allocation: a complete read-only validation pass over
the blob, then an infallible apply; (C) a second persistent shadow machine
allocated at create (doubles resident memory permanently).

RECOMMENDED: A, as a deliberate, documented extension of the allocator
contract (precedent: ROM admission builds a separate candidate and installs it
as one commit, `docs/spec/machine-contract.md` "Raw ROM admission"). B is the
fallback if the owner refuses new callback sites, but B only gets safety if the
validation pass is a faithful dry run of apply, which duplicates parsing logic.
C is rejected on memory.

Validation order (cheap and non-mutating first; each step refuses atomically):
1. Argument and span checks: non-null, length at most the hard ceiling,
   address-range representable, span disjoint from machine storage, image,
   RDRAM and the allocator descriptor (reuse the admission range helpers).
   Machine must be admitted and not poisoned; unready returns NOT_READY.
2. Fixed header read (192 bytes): magic, container version, flags, reserved,
   `total_length == span length <= profile-derived maximum`.
3. Whole-blob SHA-256 trailer (streaming, no allocation).
4. Identity binding: profile digest equals live; state serialization version
   equals live profile's; content length and SHA-256 equal the live admitted
   content; build id policy (D-C5). Each mismatch has its own status.
5. Section table: count, ordering, non-overlap, lengths equal the
   profile-derived expectations, per-section SHA-256.
6. Only now allocate the staging image (size is profile-derived and trusted,
   not read from the blob, so a malicious blob cannot cause a large allocation;
   allocation failure refuses with ALLOCATION_FAILED, live machine untouched,
   the exact returned descriptor released once under the same disjointness
   rules as admission).
7. Parse every section into staging with per-field range checks.
8. Structural invariants on the staged image (the real safety boundary, because
   a hash does not make a state reachable):
   - counts and heads within capacity (queue at most 256, descriptors at most 2,
     ring count at most 2, TLB/cache indices);
   - scheduler tuples strictly ordered by `(tick, sequence)`, every
     `sequence < next_sequence` unless exhausted, every `tick >= guest tick`,
     and each `(device, kind)` present in the closed production handler table
     (synthetic-test device refused in production builds; test builds may allow
     through a test hook);
   - reservation invariants: every busy device has exactly its reserved events
     with matching tokens/generations (PI active implies one PI completion
     event with the active token; SP active/pending descriptors imply their
     completion events; AI active and not paused implies exactly one sample event
     with matching token and generation; VI enabled implies its field event;
     SI busy [A] and EEPROM busy window [B] likewise);
   - derived fields recomputed and compared (MI IRQ line, PI/SP/AI busy/full);
   - enums/booleans in range, ring heads consistent with producer sequence,
     retained packet lengths at most the packet maximum.
9. Recompute the canonical digest from the staged image and require equality
   with the declared digest and with SHA-256 of the canonical section bytes (this
   makes every import also a mini round-trip proof).
10. Single commit point: infallible field-wise copy of staging into the live
    machine (including RDRAM and rings), handler pointers rebuilt from the table,
    queue head reset to 0, trace ring cleared and enablement preserved, observer
    section applied or reset to the consistent empty cursor, then release staging.
    Nothing after the commit can fail.

Postcondition for every refusal at steps 1-9: the live machine is bit-identical
for every observation (canonical digest, trace records and drop count, output
and observer cursors, all allocation ownership counters, outstanding host
callbacks count), matching the existing refusal assertions in
`docs/spec/machine-contract.md` ("Replacement reset snapshot" last paragraph).
Import must not call `release` on a descriptor it cannot prove disjoint.

Evidence: `docs/spec/machine-contract.md` admission and allocator sections;
`.planning/research/PITFALLS.md:71-79` (pitfall 6: parse into temporary state,
validate version, structure, endianness and profile before one commit point,
compare the full digest before and after every refused import).

Failure modes: a crafted but hash-valid blob breaking an invariant the machine
assumes (for example a completion event with no descriptor leads to a dispatch
defect); partial commit if apply can fail (hence infallible copy); stale
handler pointers; staging overlapping live storage if the host allocator lies
(same limit as admission: the core can check the descriptor, not the object).
[UNK] whether every in-core consumer defends against the invariant violations
listed in step 8 at runtime; the plan should audit this per device and prefer
rejecting at import.

Authored-evidence plan: refusal matrix described in D-C3 plus per-invariant
negative controls (hand-built blobs violating each invariant with a valid
trailer and valid per-section hashes) asserting refusal and unchanged digest;
an allocation-failure control (allocator that fails on the staging request) and
an allocator that returns overlapping storage; a libFuzzer target
`fuzz_state_import` following `core/tests/fuzz_rom_admission.c`, seeded from
in-process authored exports, run under ASan/UBSan, plus a deterministic
mutation sweep in CTest (flip one bit in every 4 KiB stride and every header
byte) so the check runs without the fuzzer toolchain; a control that a mutation
which recomputes only the trailer is still refused by invariant/digest checks.

What would change it: owner objection to new allocator call sites (use B);
profile growth making 2x memory unacceptable. Reversibility: medium (touches
allocator contract text; Phase 10 would freeze it).

### D-C5. Binding to profile, build and admitted-content digest

Facts: profile and content are hard identities; build is awkward (observation 5):
`build_id` changes with any core source, compiler or flag change, and today it
leaks into the canonical digest through retained packet metadata.

Options for state import:
1. Exact binding on profile digest, state version, content digest and build id;
   mismatch refuses.
2. Hard binding on profile/state/content; build id recorded and mismatch refuses
   by default but a caller opt-in flag allows import with an explicit
   "continuation under a different build, no exactness claim" label.
3. Ignore build.

Options for the digest: keep `build_id` hashed in retained packet metadata
(status quo) or exclude it from the new profile's digest.

RECOMMENDED: Option 1 for v1 (opt-in 2 deferred and listed as an owner
question), and for the new Phase 9 profile's digest, exclude `build_id` (and the
redundant `profile_id` text) from the hashed packet metadata while still storing
them in the exported ring metadata and in the container header. Rationale: the
build is already bound in the container and in the replay header; hashing it into
state makes every checkpoint digest build-dependent, so a replay run diagnostically
on another build can never separate "behavior changed" from "string changed",
and golden state vectors cannot be pinned in tests without injecting the build id.
Because this is a new profile/state version, no legacy digest changes. The
profile hash already covers the quantum [E] and save selection [B].

Mismatch behavior: refusal before any state read beyond the header and
trailer; statuses distinguish PROFILE, CONTENT, BUILD, STATE_VERSION; live
machine unchanged. A state can therefore not be restored onto a different build
in v1. Cross-build restore is useful to developers (load yesterday's state) but
the correctness claim ("restore-then-continue equals uninterrupted run") only
holds when the same code continues; so it is a labeled diagnostic mode, not
the default.

Evidence: `core/CMakeLists.txt:45-66`; digest packet-info loops; Phase 8 spec
(`docs/spec/audio-interface.md`) showing `build_id` in packet metadata;
`.planning/research/PITFALLS.md:71-79`. [REF] Mupen64 `.m64` stores ROM CRC and
country, plugin names, and names a start type of snapshot/power-on/EEPROM
(https://tasvideos.org/EmulatorResources/Mupen/M64); the page notes plugin names
are recorded but not enforced, a cautionary example of identity without
enforcement. [REF] BK2 stores a hash that may be CRC32, MD5 or SHA1 with no
algorithm prefix (https://tasvideos.org/BizHawk/BK2Format): lesson, name the
algorithm (here, SHA-256 only, lowercase hex).

Failure modes: digest still build-dependent if any other build-derived text is
hashed (audit `t64_build_identity` uses; only packet info found); an owner
decision to keep build in the digest would force goldens to inject it.

Authored-evidence plan: control that mutates the header build id (trailer fixed
up) and expects the BUILD refusal; test that exports from two machines created
with different injected build strings (test-only hook) produce identical
canonical digests for the new profile but differing container headers; reference
encoder pins the packet-metadata field list so re-adding build_id fails the
vector.

What would change it: owner wants cross-build developer restore in v1 (then
add the opt-in flag and label) or wants build kept in digest.
Reversibility: high for import policy, medium for the digest field list (state
version bump).

### D-C6. Restore-then-digest exactness and "restore then continue equals uninterrupted run"

Claim to prove, in increasing strength:
- T1 round trip: `digest(import(export(M))) == digest(M)` on a peer machine
  (fresh, same profile, same admitted content).
- T2 canonical idempotence: `export(import(export(M)))` is byte-identical to
  `export(M)` (proves the reader is the writer's inverse, field by field,
  including derived fields).
- T3 continuation: run M to boundary X; export; continue M to Y recording the
  final digest, drained video/audio packet bytes, ISViewer bytes and every
  intermediate checkpoint; fresh peer imports and continues to Y, using several
  advance partitions (one shot, 1/7/31 ticks as the Phase 3/8 tests do); all
  digests and output bytes equal. Also import onto a machine that has run
  something else (restore overwrites everything) and onto the source machine
  after advancing (rewind).
- T4 non-interference: `export` takes `const t64_machine *` and does not change
  digest, trace, cursors or counters (compare before and after).

Boundary matrix for T1-T3 (nontrivial, each built from authored fixtures, not
protected content): PI DMA active and canceled; SP DMA active with a pending
second descriptor; RSP mid-program with nonzero ready ticks; RDP with pending
partial command words and pending triangle words and nonzero coverage; VI
mid-field with latch pending and a retained undrained packet; AI with two queued
descriptors, a partial packet, paused with remaining ticks, nonzero phase
remainder, and an AI event blocked on output-full; coincident same-tick events
of different devices (both orders); CPU in a delay slot with a dirty D-cache
line and nonempty TLB; scheduler near exhaustion limits; SI/PIF mid-transaction
[A]; EEPROM within its busy window and a pending write [B]; pending timed
input events not yet dispatched. A sweep exports at every K-tick stop across one
authored fixture run (K chosen so the sweep stays within the existing ten-minute
CI limit; the first stage runs on a small number of hand-chosen boundaries per
AUTONOMY section 2 cheap-gate rule).

Because export only occurs between `advance` calls, boundaries are tick bounds,
event-count stops and resource-limit stops; resource-limit yields (4096-event
budget) are a named boundary class. [INFER] "Mid-instruction" is not a boundary
the public API can reach, so there is nothing to serialize there; verify with
the CPU owner that no instruction-internal state escapes `advance`.

Honest scope: the evidence proves self-consistency and determinism of the
software model for authored fixtures and the current build/host, not hardware
accuracy (AGENTS.md evidence rules).

Evidence: `docs/spec/machine-contract.md` ("one-shot and 1/7/31-tick partitions,
trace enabled and disabled, different output-drain timing, two interleaved
machines"); `docs/spec/audio-interface.md` "Determinism checks"; FEATURES CORE-04
("at a nontrivial boundary (including an in-flight/queued device operation)").

Failure modes: state that is "future affecting" but forgotten from the walker
shows only in T3, not T1 (hence both); output-observer mismatch hides in digest
equality (hence compare bytes); import on a dirty machine leaving residual state
(hence the dirty-peer control).

Authored-evidence plan: each boundary needs a failure control: a fixture-side
mutation hook that perturbs exactly one field (for example the AI phase
remainder or an RDP pending triangle word) in the imported copy only, which T3
must detect as divergence at the expected checkpoint. Also the tripwire and
omitted-field controls from D-C1.

What would change it: if the CPU/RSP interleave quantum [E] introduces hidden
intra-quantum state; add it to the walker and the boundary matrix.
Reversibility: n/a (test design).

### D-C7. Privacy and rights of state blobs

State contains RDRAM, RSP memories, TMEM, coverage arrays, retained video/audio
frames and, per [B], save media bytes. When the guest is protected content these
are derived from rights-bearing material (a PI DMA copies cartridge bytes into
RDRAM; saves are called rights-bearing in AGENTS.md). A SHA-256 proves identity,
not rights.

RECOMMENDED policy:
- State blobs are private by default: created mode 0600 by the runner through the
  same no-replace, no-symlink staged publication used by raw capture; never
  written next to committed files by default; never uploaded by CI; hosted CI
  never receives a state from a user.
- Public evidence uses only states generated in-process from authored fixtures
  (`fixtures/manifest.json` entries with cleared rights), and the committed
  evidence is the projection (digests, sizes, field-mutation results), not the
  blob. If a blob is ever committed it needs an explicit manifest entry naming
  the authored fixture, and the tree scanner (the existing rights/PII gates)
  gets a rule that refuses `*.t64state`/`*.t64replay` by extension and by magic
  unless allowlisted.
- The blob contains no ROM bytes, no path, no host name, no wall-clock time and no
  user name (build id and profile text are fixed vocabularies); header fields must
  be listed in a manifest the PII scan can check.
- Save bytes in a state are a second copy of the save; importing a state does not
  write the host save file (host policy, runner-owned).
- Replay artifacts contain content hash, profile/build id, input events and
  digests but no memory; they are not rights-bearing in themselves, but a replay
  bound to a protected ROM discloses which title was played; default private
  locally, and publishing the hash of a protected ROM is an owner question.

Evidence: `AGENTS.md` (rights-bearing inputs; "a hash proves identity, not
rights"); `docs/spec/raw-capture.md` first paragraph ("may contain rights-bearing
content; do not commit or upload it"); `.planning/AUTONOMY.md` rights stops.

Failure modes: a developer commits a state/replay from a protected title; CI
artifact upload includes `build/` states. Mitigation: scanner rule plus no state
files under any uploaded artifact path (CI configuration audit item).

Authored-evidence plan: scanner negative control (a planted `.t64state` with
valid magic is refused, an allowlisted authored one passes); runner test that
the staged state is mode 0600, no-replace, and symlink parent refused; a grep
test that the header contains none of the forbidden vocabulary.

What would change it: owner clearing a public authored-state corpus; a release
channel policy (D6). Reversibility: high.

### D-C8. Replay artifact

What is recorded (operation log, not a frame-indexed input stream): the exact
sequence of host calls that influence the machine, each with the guest tick `at`
when it was made:
- `input` ops: `at` (guest tick when submitted), `event_tick` (target tick),
  and the logical input payload from cluster A's schema (today: port 0, buttons,
  axes; Phase 9: whatever A defines for the standard controller and controller
  present/absent state);
- `checkpoint` ops: `at`, expected canonical state digest (hex);
- one terminal `end` op: `at`, expected final digest, and a stop kind
  (guest tick bound or guest completion).

Why an operation log with submission ticks: the digest includes pending input
events and the scheduler sequence counter (observation 7), so a live recording
(inputs submitted just in time) and a scripted schedule (everything queued at tick
0, as `runner/t64_real.c` does) produce different checkpoint digests for the
same logical input. The artifact must therefore say when each input was
submitted. Replay semantics: for each op in array order, advance to `at`
(inclusive tick stop, as `docs/spec/machine-contract.md` defines), then perform
the op; `at` must be nondecreasing; ops with equal `at` run in array order; a
checkpoint at tick T is taken after the advance and before any later op at T.
The core's partition invariance means the runner's advance step size is not
part of the artifact (verified by replaying with different steps).

Start condition: `boot` is either `{"kind":"cold-admit"}` (create machine with the
recorded profile, admit the recorded content, no state) or `{"kind":"state",
"state_digest":..., "container_sha256":...}` naming an external state blob the
host must supply; the blob is never embedded in the replay (keeps replays
non-rights-bearing and small). [REF] m64's "start from snapshot" depends on a
sibling `.st` file and breaks when it is missing or mismatched
(https://tasvideos.org/EmulatorResources/Mupen/M64); BK2 embeds an optional
CoreState (https://tasvideos.org/BizHawk/BK2Format) and treats the file's
presence as authoritative: lessons are to bind the anchor by hash and to refuse
on mismatch rather than guess.

Encoding: canonical JSON, schema `t64.replay`, `schema_version` 1, following the
repo conventions: closed key set, bounded parser, canonical re-serialization,
decimal-string u64 ticks, lowercase 64-hex SHA-256, no floats, UTF-8 ASCII
subset, duplicate keys refused, replay identity = SHA-256 of the canonical
bytes (reuse the canonicalization approach of `runner/t64_input.c`). Header:

```text
schema, schema_version,
profile_id, profile_digest, descriptor/model/state_serialization versions,
build_id (binding), source_revision (informational, not binding),
content_sha256, content_size,
boot, input_schema ("t64.input" version of the payload), ops[], 
op_count, end_digest
```

Caps (provisional): document at most 4 MiB, ops at most 65,536 of which
checkpoints at most 4,096, ticks at most u64 and `at` nondecreasing, tick
difference arithmetic checked. The current t64.input cap (128 events/64 KiB) is
a startup-schedule cap and too small for a recorded session; ops submitted just
in time also avoid the 256-slot scheduler queue limit that pre-queuing a long
script would hit. Text over binary because the artifact is the authored-evidence
and review surface (BK2's plain-text input log is the positive example
[REF]); m64 is a 4-byte-per-sample binary stream whose sample alignment depends
on the game's controller poll order and which has reserved header values with
side effects (same TASVideos page), both pitfalls this design avoids by logging
ticks and refusing reserved/unknown fields. If long live sessions make text too
large, schema version 2 can add a binary op section; not needed in Phase 9.

Binding and mismatch: before any guest advance the replayer verifies profile
digest, state version, build id and admitted content digest equal the live
machine's (and, for a state anchor, the supplied blob's container hash and state
digest). Any mismatch refuses with a named field and no guest execution
(`replay-identity-mismatch:<field>`); there is no "partial replay".
`source_revision` is recorded for humans only. Dropping the build binding for a
diagnostic comparison is not offered in v1 (see D-C5 digest recommendation which
would make it meaningful later).

Failure modes / divergence reporting: a runtime result of
`match | diverged | identity-mismatch | refused | truncated`; on `diverged`
report the first differing checkpoint index, its `at` tick, expected and actual
digests, the live tick reached, and, in a diagnostic mode, which exported
sections (by per-section digest from the container machinery) differ, using an
export of the live machine at that point (the same section map as D-C3).
Truncated/oversized/trailing/unknown-field/duplicate-key/non-monotone `at`/
over-cap artifacts refuse before execution (parse completely, then validate,
then run). `end` must be present or the artifact is `truncated`. Also refuse an
op whose `event_tick < at` (the core would refuse it) at parse time. The
divergence report is a separate closed record (`t64.replay-report` v1) rather
than extending `t64.result` v2, because the result protocol is a closed schema
(`docs/spec/result-protocol.md`, R-SCHEMA/R-VERSION); the result checker will
need to know the new record before CI can validate it.

Replay vs recording API shape at the runner boundary: the core gains only the
state export/import calls (D-C3/D-C4); it knows nothing about replays, files,
clocks or JSON, preserving AGENTS.md separation. A recorder is a thin wrapper
in the runner around the existing `t64_machine_queue_input`, `advance` and
`digest` calls that logs `(at, payload)` at call time from `t64_machine`'s tick
(no host clock ever enters the artifact). The replayer is a driver over the
same public calls. Both live beside `runner/t64_input.c` as a host-side module
(parser/canonicalizer/driver separable from file I/O so Phase 10 can re-home the
driver behind the frozen ABI). `--record-replay <new-path>` and
`--replay <path>` are runner options using the same descriptor-pinned input
copy (`t64_verified_input_prepare`) and no-replace output publication as
raw capture. Recording happens only for a runner session whose inputs are all
runner-submitted (physical input mapping is a frontend concern per
ARCHITECTURE.md).

Evidence: observations 7 and 8; `.planning/research/FEATURES.md` CORE-05 ("same
named checkpoints/final digest... not physical controller mapping or a player-
facing movie editor"); PITFALLS 6 ("compare per-checkpoint digests, not just final
pixels"; "replay passes after any bound identity field is altered" as detection).

Authored-evidence plan: (1) record a scripted authored-ROM session (controller
reads at known guest times, A's tracer) and replay it: all checkpoints and the
final digest match; (2) mutation controls, each must be refused or diverge at
the named checkpoint: alter one byte of profile digest, build id, content hash,
one input tick or payload, one checkpoint digest, delete the `end` op,
reorder ops, append trailing data, duplicate a key, exceed each cap, change
`submit` ordering relative to a coincident event; (3) differing advance steps and
interleaved machines give identical checkpoints; (4) "just-in-time vs up-front"
control proving the artifact's `at` field matters: a replay with `at` rewritten to
0 must diverge when a same-tick coincident event exists; (5) state-anchored
replay: anchor mismatch refused; (6) fuzz the parser (extend the closed JSON
parser's existing tests).

What would change it: if cluster A makes controller state a separate input
timeline not queued in the scheduler, `at` may lose meaning for ordering but
stays useful as the call tick; a requirement for human editing at frame
granularity would motivate a frame-indexed convenience layer that compiles to
the op log. Reversibility: medium (schema_version 2 is additive).

### D-C9. Scheduler pending events and sequence counters in state

Decision: serialize the logical queue (count, ordered tuples
`{tick, sequence, device, kind, value, token}`) and `next_sequence` plus
`sequence_exhausted`; omit `handler`, head and unused slots; rebuild
`handler` from a closed `(device, kind)` table at import. The table is the
single authority for "which pending events are legal"; the importer refuses any
tuple outside it. Reserved-but-uncommitted `t64_event_reservation` objects are
transient inside a call and cannot exist across calls (verify; [UNK] audit that no
subsystem keeps a reservation across `advance`).

Interface needed from cluster E: a stable list of devices/kinds and their
reservation invariants (what must be queued when a device is busy), and any new
event kinds (SI, EEPROM busy, quantum markers) registered in the same table.
Evidence: observation 6; PI/SP specs ("every accepted request has one reserved
queue slot before busy becomes visible"). Failure modes and tests: covered in
D-C4/D-C6 (reservation invariants; coincident-event boundary). Reversibility high.

### D-C10. Versioning policy and ownership of identity

- Container version (framing) is separate from state serialization version
  (schema, owned by the profile). Phase 9 should take one new composite
  profile/state version for the phase's device set, not one per device plan:
  intermediate plans append to the working profile. [INFER; coordinate with A,
  B, E so the new number is allocated once, provisionally 25.]
- A device plan's definition of done: reset-audit row, walker fields, invariant
  validator entry, field-mutation test, refusal test, event-table entry. This
  operationalises the machine-contract rule "new future-affecting state must be
  added to the reset, digest and refusal tests when introduced".
- Old state-vN digests are untouched and their independent vectors remain pinned
  (`docs/spec/cpu-compatibility.md` pattern); the importer is registered per
  state version, so unsupported versions return a distinct "known but not
  importable" versus "unknown" status.
- Provisional list for Phase 10: the export/import call signatures, the
  staging-allocation contract, status codes, container constants, replay file
  schema, and the divergence record are all provisional and may change before
  the API freeze; only the digest schema of a shipped profile is immutable.

## Interfaces I assume from other clusters

- A (SI/PIF/controller): a fixed list of serializable fields for SI, PIF RAM,
  joybus transaction state and per-port controller latch; whether controller
  input remains a scheduler event (`device 6, kind 1`) or becomes a separate
  timeline; the logical input payload schema (buttons, axes, connected, pak);
  how timed input is defined (guest tick) and refused when late. I assume a
  `t64_machine_queue_input`-like call remains the only way input enters the
  machine and input events remain in the canonical scheduler state until consumed.
- B (save devices): selected device kind/size is in the profile hash; the
  media bytes and EEPROM busy deadline/pending-write state are canonical machine
  state; SAVE-03's byte import/export is a separate checked-span operation with
  its own refusal postcondition and does not use the state container; media bytes
  appear in the state container only as part of device state.
- E (scheduler): a published `(device, kind)` table with reservation invariants,
  the CPU-to-RSP quantum in profile identity (so it is bound by the profile
  digest, not a state field), and any state the quantum adds (for example a
  mid-quantum cursor, which would be a new field in the walker).
- Phase 7/8 (already landed): AI and VI are in the digest as described above.

## Proposed requirement-to-slice mapping

| Slice | Content | Requirements | Needs |
| --- | --- | --- | --- |
| C0 | State walker and digest preimage for the new profile with independent reference encoder, struct-size tripwires, reset-audit and exclusion tables rewritten in `docs/spec/machine-state.md` (also correct the stale machine-contract digest table), `build_id` decision | CORE-04 | none; should land before A/B/E state fields so they add to one walker |
| C1 | Container writer (`t64_machine_state_export`), independent reference decoder in tests, T2 idempotence, T4 non-interference | CORE-04 | C0 |
| C2 | Staged import: validation order, invariant validators, handler table, atomic commit, refusal matrix, allocation controls, fuzz target, mutation sweep | CORE-04 (SAVE-03 pattern shared, not shared code) | C1; E's event table |
| C3 | Replay schema and parser/canonicalizer, op-log driver, identity binding, recorder, divergence report, result-checker record | CORE-05 | A's input schema; can start with port-0 observation events |
| C4 | Boundary matrix round-trip and continuation proofs with all Phase 9 devices; replay of an authored controller session; replay mutation controls | CORE-04, CORE-05; success criterion 3 | C2, C3, A and B state landed |
| C5 | Runner CLI (`--state-out/--state-in/--record-replay/--replay`), private-file publication, scanner/rights rule, CI audit that no state/replay is uploaded, spec docs, evidence, closeout | CORE-04, CORE-05 | C4 |

CORE-02 completion (criterion 4) belongs to E; C depends on E only for the event
table and for including the quantum in the profile hash.

## Unknowns that need the owner (or a targeted check)

1. State import across builds: refuse in v1 (recommended) or add an opt-in
   labeled diagnostic mode now. Also confirm excluding `build_id` from the new
   profile's digest.
2. Extending the allocator contract to allow staging allocation during import
   (recommended) versus a no-allocation two-pass design.
3. Whether the hash of a protected ROM may appear in a committed replay or
   evidence record (hash proves identity, not rights). Default: only authored
   fixtures in public evidence.
4. Whether one text replay format is acceptable for live-play sessions in Phase 11,
   or a binary op section should be designed now. Caps proposed are provisional.
5. [UNK, code audit needed] whether every consumer of scheduler/device state
   defends against the cross-structure invariants listed in D-C4 step 8; and
   whether any subsystem holds a scheduler reservation across `advance`.
6. [UNK] exact `sizeof(t64_machine)` and whether staging a value-only image
   (rather than the whole struct with pointers) is cheap; this affects only the
   implementation shape of D-C4.
7. [UNK] ares serializer behavior and Project64/Mupen64Plus savestate headers were
   not retrieved; no claim from them is relied on.

## Sources consulted (reference only; no code or text copied)

- https://tasvideos.org/EmulatorResources/Mupen/M64 (m64 header, start type,
  ROM CRC/country, sample alignment caveats)
- https://tasvideos.org/BizHawk/BK2Format (text input log, sync-settings blob,
  optional CoreState anchor, hash-algorithm ambiguity)
- https://docs.libretro.com/development/cores/developing-cores/ (serialize size
  must not grow, no stated cross-version portability)
- Mupen64Plus savestate version history via the `savestate_convert` notes
  (https://cocalc.com/github/alexbevi/BizHawk/blob/master/libmupen64plus/
  mupen64plus-core/tools/savestate_convert.txt)
