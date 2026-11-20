# Phase 9 discussion proposal: Controller, Save Media and Replay

Synthesis of the five cluster recommendations (A SI/PIF/controller, B save media,
C state and replay, E scheduler, F QA and slicing) into one internally
consistent proposal. **Revised after the independent adversarial review**
(`09-DISCUSSION-ADVERSARIAL.md`, verdict PROCEED WITH CHANGES); every finding's
disposition is recorded in section 10, and the body below already reflects the
accepted changes (where the body and an earlier cluster file disagree, this
document wins). It is a recommendation, not an owner decision and not a plan. Requirements SAVE-01..04, CORE-02, CORE-04 and
CORE-05 are unchanged. Phase 7 / VID-04 / Plan 13 stay stopped; nothing here
grants oracle, rights, physical, publication or release authority.

Evidence labels: **[FACT]** hardware-document or primary-source fact;
**[COMM]** community hardware documentation (single source, medium confidence);
**[CODE]** code observation (file:line, verified by the synthesizer unless
stated); **[INF]** inference; **[POL]** named deterministic software-profile
policy (a model choice, not a hardware claim); **[UNK]** unknown. Other
emulators were reference only; nothing is copied. No ROM, save, PIF/IPL or
firmware is used or needed; every fixture is authored.

## 1. Verification log (code claims re-checked directly)

| Cluster claim | Result | Evidence |
| --- | --- | --- |
| Scheduler order key is `(tick, sequence)` only; policy string `tick-sequence-fifo-v1` | Verified | `core/t64_scheduler.c:10-13` (`event_precedes`); string in every descriptor, e.g. `core/t64_machine.c:49,1007`; `event_order_policy` is written into the profile hash for all descriptor versions, unconditionally (`core/t64_machine.c:1669`), so a new string changes only the new profile's hash |
| Sequence is assigned at reservation time | Verified | `core/t64_scheduler.c:120` (`event.sequence = machine->next_sequence`); reserve refuses past ticks, queue full, exhaustion before mutation (`:100-118`) |
| Digest state version and `has_vNN` gating | Verified, with a cost note | `domain_v24 = "t64.state.audio.ai.24"` `core/t64_digest.c:197`; `has_v24_state = t64_has_ai_profile(machine)` `:231`; that predicate is an exact tuple match (descriptor 11, model 19, state 24) `core/t64_internal.h:927-931`. Any new tuple falls through every `t64_has_*` chain (16 predicates) and every model/state ladder in `core/t64_machine.c` (e.g. `:2216-2300`, `is_ai_profile` `:1189`) unless added explicitly. This is the AUTONOMY "keep version membership explicit" hazard |
| Digest hashes pending events and `next_sequence` | Verified | `core/t64_digest.c:348-356`, `hash_event` `:29-36` (tick, sequence, device, kind, value, token) |
| `build_id`/`profile_id` leak into the digest via packet metadata | Verified (C is right) | hashed in the video packet loop `core/t64_digest.c:725-728` and audio loop `:811-814`; text is stamped at `core/t64_ai.c:546-547`, `core/t64_vi.c:1067,1247`. Nuance: the AI rolling history hash (`core/t64_ai.c:150-172`) does not include them |
| `synchronization_quantum` is identity-only and inert | Verified | declared `core/include/tendies64/t64_machine.h:151`; validated `> 0` only `core/t64_machine.c:1568`; hashed `:1649`; value 1 in all 17 descriptors and in `runner/t64_skeleton_probe.c:58`; no behavioral consumer in `core/` or `runner/` (grep) |
| RLIMIT_FSIZE about 68 KiB | Verified | `runner/t64_supervisor.c:1534-1536`: `T64_REAL_RESULT_MAX_BYTES` (4096, `runner/t64_real_result.h:9`) + `FSIZE_SLACK` (64 KiB, `runner/t64_supervisor.c:44`) = 69,632 bytes, set in the forked real-mode child before exec. A second, different limit (`log_limit + slack`) applies to generic commands at `:1880-1882` and is untouched. Known limit documented in `docs/spec/raw-capture.md` |
| `t64_machine_queue_input` and its sequence | Verified | `core/t64_observe.c:96-122`: port must be 0, axes in range, machine ready; `value = buttons | x<<16 | y<<24`; token 0; handler is a no-op (`:84-93`); accepts `tick == machine->tick`; sequence comes from `machine->next_sequence` at call time, i.e. host call timing |
| PI DMA model that SI would mirror | Verified | duration `pi_duration_base_ticks + length/pi_duration_bytes_divisor` (1 + N/2, `core/t64_pi.c:78-87`, values `core/t64_machine.c:1002-1003`); reserve-then-commit `:88-100`; busy second start sets sticky `error`, retains the active request, returns `BUS_DEVICE_REFUSED` `:56-63`; invalid span refuses without setting error `:65-73`; token exhaustion refuses `:74-77`; only write-length (cart to RDRAM) works, read-length is latched and refused (`core/t64_pi.c:204-209`) |
| Allocator limits allocation to create time | **Imprecise** | the contract says callbacks run only during creation, admission, replacement and destruction (`docs/spec/machine-contract.md`, "Host allocator contract"); create already allocates profile-dependent storage (`core/t64_machine.c:2278-2345`, video and audio rings keyed to `is_ai_profile`). So device storage sized by profile at create is an established pattern, but an allocation during state import would be a new callback site |
| Next free device id is 10 | Verified in part | ids in use: PI 3, SP 4, CPU 5, input trace 6, RDP 7, VI 8, AI 9 (`core/t64_internal.h:18-21,85-87`, `core/include/tendies64/t64_machine.h:22-29`); 0xffff000x are test ids |

### Corrections to cluster claims

1. **B missed existing descriptor fields.** `t64_profile_descriptor` already has
   `peripheral_flags` and `save_kind` (u32), serialized into the profile hash for
   every descriptor version (`core/t64_machine.c:1640-1641`); `T64_SAVE_NONE = 0`
   exists (`core/include/tendies64/t64_machine.h:95`); all 17 descriptors set both
   to 0. B's new `save_policy` text plus numeric window field duplicates the
   selector. Corrected below (D-1).
2. **A is wrong that the runner has no timed-input option.** The real runner
   already parses a closed `t64.input` v1 schedule (`runner/t64_input.h`: 128
   events, 65,536 bytes) and pre-queues every event before the first advance
   (`runner/t64_real.c:1011-1155`). C and F are right. What is missing is a
   consumer (SI), the port field, and recording.
3. **The runner and supervisor are pinned to approved identities.** Fixtures are
   admitted from a closed table of id, SHA-256, profile id and profile hash
   (`runner/t64_real_fixture.h`, `t64_real_fixture_profile_allowed`);
   `runner/t64_real.c` branches on `T64_REAL_AUDIO_PROFILE_ID` and on model
   numbers 11..14 (`:485-507,679-764`); the supervisor carries a fixed
   `REAL_INPUT_SIZE 54u` (`runner/t64_supervisor.c:45,584-589`). A new profile
   family, new fixtures and larger schedules therefore need real-runner and
   supervisor edits that no cluster budgeted explicitly. These belong to 09-02
   (mechanism; adversarial H1 split it out of the core plan), and each later plan
   adds its own flag-forwarding row. Exact supervisor behavior at
   `:584-589` was not fully read; 09-02 must read it first.
4. **Profile id length.** `T64_PROFILE_TEXT_CAPACITY` is 64 including the
   terminator (`core/include/tendies64/t64_machine.h:15`), so ids and every policy
   label are at most 63 characters; the current id is 55. B's "illustrative"
   variant names (`...-ai-v1-si-v1-eeprom16k`) do not fit. Policy labels must
   also fit 63 characters.
5. **F's "supervisor is a bound-source file".** Verified only that
   `runner/t64_supervisor.c` is in the Phase 8 GCC13 required-unit set
   (`scripts/qualify-audio.py:996-1000`). Whether it is also in the bound source
   manifest was not verified; treat it as bound (conservative).
6. **E says the CPU re-reserve quirk is real.** Verified at
   `core/t64_cpu.c:1552-1567` (successor re-reserved after effects when the
   reservation drifted). It is deterministic given guest history, so it is not a
   partition hazard; only host-submitted events are (see conflict 1).
7. **External fact re-checked.** n64brew Joybus Protocol was re-fetched by the
   synthesizer: current revision says "It may take up to 6 milliseconds for the
   data to be written to EEPROM", says the 512-byte part ignores the top two block
   bits, uses status `0x80` for write in progress and replies `0x80` to a write
   while busy. The SDK 15 ms and the older 30 ms figure were not re-fetched here
   (carried from B). The remaining A and B protocol facts stay [COMM] single-source.

## 2. Reconciliation of the known conflicts

### Conflict 1. Input timing semantics (A vs E vs C)

**Facts.** Input is a scheduler event whose sequence number is the host call
time's `next_sequence`; the pending tuple and `next_sequence` are in the digest
[CODE]. Guest-created events are partition-invariant (their reservation order is
a function of guest history); only host-submitted events are not.

**Resolution: three layers, no scheduler change.**

1. **Core behavior rule (A's strictly-after latch, adopted).** Input is level
   state per port. A sample at tick E observes the last input whose stamped tick
   is strictly less than E; at equal stamped tick the later host call wins
   (call order is the only order the host can express). Implemented with
   per-port `current` and `next{value, visible_from}`; the input handler promotes
   `next` if `visible_from <= event.tick`, then stores the new value with
   `visible_from = tick + 1`; the SI execute event promotes `next` if
   `visible_from <= E` and then samples `current`. The result does not depend on
   the dispatch order at `E == T`, hence not on the host's call timing or the
   advance partition. [POL]
2. **Core API unchanged (E's `tick > machine->tick` refusal rejected).** Under
   the latch, a submission at `tick == machine->tick` is accepted and is simply
   invisible to samples at that tick, whatever remains queued at that tick. The
   existing refusals (past tick, absent port, axis range, not ready, halted, queue
   full, sequence exhausted) stay, so old tests and the existing runner schedule
   path keep working. The port field is packed into event-value bits 32-33 under
   the new profile only; ports 1-3 are refused by the profile's connected mask.
3. **Replay discipline (C's `at`, adopted; E's boundary rule becomes a driver
   rule, not a core rule).** The replay is an operation log recording the guest
   tick `at` of every host call, because the digest depends on the submission
   schedule. Runner-produced recordings and replays submit input only at
   **completed-tick boundaries**: the driver first issues an inclusive tick-bound
   advance to the intended `at` (which drains every event at or before that tick,
   `core/t64_scheduler.c` bound loop), then queues, then continues. A recorder
   that finds the machine at a mid-tick stop (resource-limit or counter stop)
   performs that boundary advance before logging. Then the pending queue and
   `next_sequence` at every checkpoint are a function of the recorded ops and
   guest time, independent of advance step size. Equal digests across two
   *different* submission schedules of the same logical input are explicitly not
   claimed.

**Why this and not E's v2.** E's rank 0 plus strictly-future refusal closes the
same hazard but only by introducing a new ordering policy that also reorders
every other device pair (see D-2), and by narrowing a public API. The latch
closes the behavioral hazard with a few state fields; the `at` log closes the
digest hazard.

**Reversal evidence.** If the E0 characterization or the all-devices matrix
finds any coincident pair whose order differs between partitions for guest-only
reasons, or if a real guest needs "input at T visible at T", adopt E's rank 0
under a new policy version (a later profile, not an edit).
**Reversibility:** high (profile-scoped, state fields additive).

### Conflict 2. Identity and versioning (A, B, C, E vs F)

**Resolution: one composite profile *family*, one version triple, grown while
unfrozen.** F is right that Phase 9 takes exactly one descriptor/model/state
bump. B is right that SAVE-02 needs the profile to select the device. The two
combine as a family: one triple, one set of policy strings, four registered
profile ids that differ only in `save_kind`.

Contents of the single new profile (all in plan 01 as declarations, grown later):

| Item | Value | Notes |
| --- | --- | --- |
| Version triple | descriptor 12, model 20, state 25 | next numbers after 11/19/24 [CODE]; domain string new, e.g. `t64.state.si.save.25` (final text chosen in 09-01; same `t64.state.<family>.<N>` convention) |
| Registered ids | scheme fixed now (adversarial M9): stem `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` (38 characters, base, `save_kind` none) with suffixes `-e4k`, `-e16k`, `-s32k` (longest 43 characters) | every registered id is at most 63 characters, enforced by a test over the profile table and the runner's allowed-profile table; the base id is also the home of controller-only and SAVE-04 "no device" tests. The stem deliberately drops the `rdp-vi-filter-v9-ai-v1` lineage text (55 characters already, `runner/t64_real.c:40-41`); the lineage is carried by the descriptor/model/state triple and the spec, not the id |
| Device selector | existing `save_kind` u32 (new constants 1 = EEPROM 4 Kbit, 2 = EEPROM 16 Kbit, 3 = SRAM 32 KiB; 0 = none) | no new layout; hashed already. **Validated (adversarial M2):** descriptor 12 refuses any `save_kind` outside 0..3 and any `peripheral_flags` bit other than bit 0 in `profile_fields_are_valid`; `t64_machine_create` additionally refuses a non-zero `save_kind` or `peripheral_flags` on descriptor versions below 12 (they cannot be honored, so silently ignoring them would contradict explicit selection). Hashing of legacy descriptors is unchanged: the serialization vector in `core/tests/test_profile.c:152-153` (`save_kind = 7`) is hashed only, never created |
| Controller presence | existing `peripheral_flags`, bit 0 = standard controller on port 0 | ports 1-3 not connected |
| New text field `si_policy` | label naming the SI/PIF/joybus HLE model and its timing constants | descriptor-12 field, same mechanism as `ai_policy` at descriptor 11 |
| New text field `save_policy` | label naming EEPROM protocol, block rule, write window, initial contents, import formats | window constant lives in code behind the label (existing `ai_policy` practice) |
| `event_order_policy` | unchanged `tick-sequence-fifo-v1` | see D-2 |
| `synchronization_quantum` / ratio | validated exactly 1 and 1:1 for descriptor 12 only | older versions keep permissive validation |
| Digest | `has_v25_state`; canonical-payload preimage (conflict 3); `build_id` and `profile_id` excluded from hashed packet metadata | legacy digests untouched |

**Growth without breaking old identities.** (a) Old profiles keep their literal
pinned hashes, digests and refusals from 09-01 onward (an "old pins" control
set exactly like `unit.core.ai.profile-pins`: SI window still the warm stub,
PIF window unsupported, input handler observer-only, port refusal unchanged,
PI read-length still refused, domain 2 still unsupported). (b) Every
`descriptor_version == 11u` branch in the validator and writer, every `t64_has_*`
predicate, every create-time storage ladder and the runner's observation
checks gain version 12 explicitly. (c) The AUTONOMY new-descriptor check applies:
mutate every policy field through hash, serialization and known-id construction,
test empty and unterminated policy text, pin retained older bytes.
(d) While unfrozen, tests reference the family through a symbolic accessor, never
a literal new hash.

**What is frozen when (restated after adversarial M6 and H2).**

| Freeze | When | What is frozen | What may still change |
| --- | --- | --- | --- |
| F0 | end of 09-01 | version-membership pattern, section id assignment and framing, id scheme, `save_kind`/`peripheral_flags` validation | section contents (devices append), policy labels |
| FH (harness logic) | end of 09-03, the first plan with real hosted data | qualifier, collector/verifier, `ci.yml`, policy-scanner and test-inventory logic | data rows only (case JSON, inventory list, manifest rows). Supervisor option-forwarding rows and runner flags are normal content of the plan that adds each flag and are qualified once at the end; they are not "harness logic" |
| F1 (state content) | end of 09-06 | the contents of every canonical section | a defect fix, recorded as such, with replay and state goldens regenerated (they are test-time generated, see D-34) |
| FR (review) | end of 09-10 | all review- and audit-driven logic changes | nothing beyond documentation |
| F2 (identity pins) | start of 09-11 | literal profile hashes, claim blocks, evidence and qualifier pins are written once | nothing; any later change is a declared qualification reset (a new native plus hosted cycle, about 15 minutes per F-096/F-098) or a new version |

Until F2 the profile is unpublished and unqualified, and tests reach it through a
symbolic accessor.

**Reversal evidence.** The owner prefers runtime-selectable media (then a Phase
10 facade concern with digest-bound mutable selection), or a descriptor-field
count is judged too costly (fall back to label-only identity in `si_policy`).
**Reversibility:** medium until F2, then new-version cost.

### Conflict 3. Ordering of work and the canonical-payload writer (F vs C)

**Resolution: land a frozen walker skeleton in 09-01; devices append sections.**

- 09-01 introduces a small state *sink* seam in `core/t64_digest.c` (hash sink
  for the digest, buffer sink for export; section begin marker for v25). Legacy
  profiles use the hash sink with the existing field order and are proven
  byte-identical by the existing pinned vectors. The v25 branch emits
  `domain || (section id u32 || section bytes)...` in fixed ascending id order;
  section bytes are self-delimiting (fixed-width big-endian fields, counts
  bounded by compile-time capacities), so lengths are not hashed. Section
  lengths and per-section SHA-256 exist only in the export container's table,
  derived from the markers.
- Section ids are assigned per device group in 09-01 (identity, scheduler, CPU,
  RSP/SP, RDRAM, RDP, VI, AI, PI/MI, ISViewer/output, SI/PIF, save). 09-01
  populates every existing device (ported once, mechanically); SI/PIF (09-03),
  save (09-04) and any later field are appended inside their own section. No
  earlier plan needs rework: an unfrozen profile's digest may change when a
  section grows, and tests use symbolic accessors.
- Export (09-08) is then the buffer-sink run of the same walker plus a header,
  section table and trailer; the declared digest is the SHA-256 of the same
  bytes. Import (09-09) is a hand-written inverse reader, which is the only
  genuinely new per-field code in the state plans. Writer/reader drift is closed by T2
  idempotence, per-field mutation and a `sizeof`/`offsetof` tripwire for each
  state struct.
- Wave order stays F's in shape: devices, ordering matrix, replay, state export, state import, review, qualification.
  Replay before state is kept: a power-on replay needs only the digest and the
  `at` log; its checkpoint digests then become an independent oracle for
  state continuation.

**Where the risk actually is (adversarial H3, re-verified).** The sink seam
itself is low risk: `core/t64_digest.c` funnels writes through `hash_u32` (`:8`),
`hash_u64` (`:18`), `hash_event` (`:29`) and 49 direct `t64_sha256_update` call
lines, and SHA-256 updates are concatenation-invariant, so routing them through
a sink that hashes or appends is byte-identical by construction; the legacy pins
are a sufficient guard for the seam. The danger is the **v25 port**: it is a new
section-ordered walker, and a field dropped or reordered there is invisible to
every legacy pin and to T1/T2 (a round trip through the same omission passes).
Therefore 09-01 adds an independent oracle: a **strip-and-compare differential**.
For each existing authored machine (idle, AI mid-DMA, VI frame, RDP state, at
least), the v25 payload stream with section markers, the new SI and save
sections, and identity-tagged spans (profile hash, `build_id`, `profile_id`)
removed must equal the v24 stream of the same machine state, byte for byte.
The sink exposes tag begin/end hints in a test-only recording mode so the strip is
exact. The `sizeof`/`offsetof` tripwire stays as a second net, and the v25 digest
is documented as intentionally not comparable to v24.

**No equal fallback.** C's option O2 (separate exporter beside an untouched
legacy function) is retired as a stated risk response: it moves, rather than
removes, the drift risk. If the legacy pins cannot be held after the mechanical
seam change, that is a reopened design question to be raised in 09-01, not a
pre-approved alternative. **Reversibility:** high (new files and one mechanical
refactor guarded by pins and the differential).

### Conflict 4. EEPROM busy window (B vs C vs E)

**Resolution: B's deadline compare, no scheduler event; C's invariant and E's
rank are adjusted to match.**

- State is `busy_until` (u64; 0 or any value). A write accepted at the handshake
  execute tick `E` (the SI read-DMA execute event, a guest tick, partition
  invariant) sets `busy_until = E + window` with checked arithmetic. Busy is
  `tick < busy_until`; a command at exactly `busy_until` sees idle. No event
  exists, so there is no queue-full path, no interrupt, and no rank.
- C's import invariant "every busy device has its reserved event" applies to PI,
  SP, AI, VI, SI and RDP, **not** to EEPROM. EEPROM has no cross-structure
  invariant: any `busy_until` is legal; a value at or below the machine tick means
  idle. A stale deadline is kept as-is (so the digest records history; this is
  over-specific but deterministic and exact on round trip). Standalone save import
  sets `busy_until = 0` and is refused while busy (D-24).
- State export carries: device kind (redundant with the profile hash, checked),
  size, `busy_until`, and the full device bytes. `write_generation` and host
  observation are excluded.
- E's rank slot for save devices disappears (no event, and D-2 keeps FIFO). E's
  equal-tick tests still apply: command at `busy_until - 1`, `busy_until`,
  `busy_until + 1` in whole and split advances and interleaved machines, plus PI
  SRAM completion against a channel-4 handshake at the same tick.

**Reversal evidence.** A requirement that EEPROM completion raise an interrupt or
be observable without a command, or an asynchronous PIF model. **Reversibility:**
high.

### Conflict 5. Unverified timing constants

**Resolution: every constant is a labeled [POL]; none is an owner stop.**

| Constant | Source quality | Label and location | Upgrade evidence |
| --- | --- | --- | --- |
| Joybus wire times (4 us per bit, 3 us console stop, 4 us controller stop, about 62.5 us reply bound, about 7 us TX-to-RX gap), channel formula `2000n + 188 + 438 + 2000m + 250` ticks | [COMM] single source, medium confidence; formula is [POL] derived from it | `si_policy` label, e.g. `si-hle-joybus-wire-v1` (the constants are fixed in code and documented in the spec) | measurement or a second primary source |
| SI write-DMA ticks `W = 2500`, read tail `X = 4096` | no evidence | same label; named constants, "placeholder policy" | measurement |
| Single sample point `E` for all channels | [POL] simplification | same label | per-channel sampling = new version |
| EEPROM busy duration: the sourced **ceiling** of up to 6 ms (375,000 ticks at 62.5 MHz, `core/t64_vi.c:23`) used as a fixed deterministic busy duration | n64brew states "up to 6 milliseconds" (re-fetched by the synthesizer and, independently, by the reviewer) and libdragon's reference agrees on "up to 6 milliseconds" (reviewer); an SDK figure of about 15 ms (carried from B, not re-fetched) and an older 30 ms revision (search snippet, unconfirmed) are not corroborated | `save_policy` label that says ceiling, e.g. `eeprom-busy-ceiling-6ms-v1` (adversarial M1: never "exact"); constant in code. The text says the sources give an upper bound; the model's fixed 6 ms is a policy choice, not a measurement, and the earlier "6 ms is the permissive value" argument is withdrawn | a primary document or measurement |
| EEPROM read-while-busy returns the timeout flag | [UNK] (no source) | `save_policy` | measurement |
| 4 Kbit EEPROM blocks 64..255 | n64brew (single source, re-fetched): "the top two bits of block number are ignored: blocks 64-255 are repeats of the first 64"; no source supports refusal | default is the documented mirror (`block & 0x3F`) under the `save_policy` label; refusal is recorded as the alternative (adversarial M1). Single-source [COMM], not independently verified | a second source or measurement |
| Initial contents (EEPROM 0xFF, SRAM 0x00) | [UNK] physically undefined | `save_policy` | none needed; named policy |

Each constant's changing is a new label and a new profile hash, never an edit of
a qualified profile. Tests that assert a precise value are self-consistency tests
of the policy and must be described that way. **A genuine owner stop exists only
if someone chooses to pursue hardware evidence**: physical console measurement is
an AUTONOMY section 4 stop, but Phase 9 needs none. Using the sourced 6 ms ceiling
as the fixed busy duration is a routine reversible default; no claim is made that
a ceiling-length busy window is gentle or permissive for guests that do not poll.

### Conflict 6. RLIMIT_FSIZE versus MiB-scale exports

**Facts.** The cap is 69,632 bytes per written file for the real-mode runner
child [CODE]. Save export is at most 32,768 bytes (fits). A state export is about
10 MiB (C's estimate, [INF]: RDRAM 4 MiB, RDP coverage and depth-hidden 2 MiB
each, rings about 1.1 MiB, remainder small). A replay document is small in
practice but capped at 4 MiB. Larger raw capture is the same existing limit
(STATE follow-up); this phase does not need it. The limit is set once for the
whole real-mode child (`runner/t64_supervisor.c:1534-1536`), so any raise also
loosens the report, capture and stderr files of that child (adversarial L1).

**Resolution: narrow, option-driven, owned by plan 09-02 (not 09-01).**

- 09-02 adds one supervisor option pair, `--artifact-dir` and
  `--artifact-max-bytes`, modeled on the raw-capture pair. When and only when the
  pair is present, the real-mode child's `RLIMIT_FSIZE` is set to
  `artifact_max_bytes + slack` **exactly** (not a flat ceiling); the accepted
  `--artifact-max-bytes` range is bounded by a constant of 32 MiB (the Phase 9
  profile's state fits; the container-level ceiling of 64 MiB is not reachable
  through the runner). The default stays 69,632. The result report remains bounded
  by its own validation (`runner/t64_supervisor.c:1615,1663`), so report
  validation is not weakened, but the kill-at-68 KiB backstop for a runaway runner
  becomes a kill at the raised bound whenever the option is present. 09-02 adds
  controls that an oversize report and capture are still refused with the option
  present and records the changed backstop in `docs/spec/raw-capture.md`.
- **A second, small publisher is written; `runner/t64_raw_capture.c` is not
  generalized (adversarial H1, overruling F's "do not write a second publisher").**
  Verified: that file is 1,352 lines and packet/manifest-specific
  (`write_packet_file` `:558`, `append_manifest` `:690`, `valid_video_info` `:785`,
  `validate_stream_files` `:917`) and is the locus of the Phase 8 critical
  publication-source race (`08-REVIEW.md` CR-01). A new module in `runner/` copies
  the Phase 8 transaction pattern (parent-fd walk, 0700 staging, 0600 files,
  `RENAME_EXCL`/`RENAME_NOREPLACE`, identity-checked cleanup, unsupported
  platforms fail closed) with a provenance note, and carries its own substitution
  and no-clobber controls, including the CR-01 style source-substitution control
  against the new leaf names. The Phase 8 raw-capture controls stay in the gate
  unchanged because that file is untouched.
- **No supervisor forwarding is pre-built for flags that do not exist yet.** The
  supervisor's option parsing is an `opts[]` table, but each flag has bespoke
  `seen` handling and the child argv is assembled by hand
  (`runner/t64_supervisor.c:470-513,1496-1527`), so a forwarding scheme designed
  before its consumers would be wrong and edited anyway. The plan that adds a
  runner flag (09-05 save, 09-07 replay, 09-08/09-09 state) adds its supervisor
  forwarding row as ordinary plan content; those edits are qualified once, at the
  end, and are not a qualification-reset because they land before F2.
- Cheap control in 09-02 (no state exporter exists yet): the existing synthetic
  flood child (`runner/t64_synthetic.c`) proves (a) without the option a 69,633-byte
  write still dies with the SIGXFSZ backstop, (b) with the option a write up to the
  declared maximum succeeds and one byte beyond dies. A real-runner control writing
  a state-size file is owned by 09-08 (the first real consumer).

**Reversal evidence.** If a generalized publisher proves cheaper than the second
module (for example the CR-01 review finds the copied rules diverge), refactor
the shared primitive in its own reviewed slice; if the owner prefers a smaller
blast radius, route large exports through an inherited fd the supervisor owns.
**Reversibility:** high.

### Conflict 7 / Additional conflicts

1. **Event ordering policy (E vs the rest, and vs evidence transfer).** E wants a
   new `tick-class-sequence-v2` class-rank order. Resolved as D-2: keep FIFO.
   Reasons: the only history-dependent hazard (host call time) is closed by
   conflict 1 without ranks; ranks would reorder already qualified Phase 7/8
   device interplay in the new profile (AI/VI tie is asserted in both insertion
   orders today, `core/tests/test_ai.c`; CPU-last has no evidence), so earlier
   evidence would not transfer; every rank is [POL] with no primary source; and
   the roadmap wording is satisfied by "one scheduler plus coincident and
   partition checks". Reversal: see D-2.
2. **Import staging allocation (C vs the allocator contract).** C recommends a
   host-allocated staging image at import, extending the contract. Resolved as
   D-30: a single section reader with a validate mode and an apply mode, a bounded
   invariant summary, no allocation; the declared digest is checked by hashing the
   blob's own canonical bytes (possible because payload = preimage).
3. **Replay header lacks the initial save identity (B vs C).** B requires replay
   to bind the initial save image identity; C's header omits it. Resolved: add
   `save_init` (`blank` or SHA-256 plus length of the canonical-order bytes) to the
   replay header; save import is a pre-run host action; mid-run import is not a
   replayable operation. The profile hash already binds the device kind.
4. **"Controller present/absent" as an input op (C) vs profile-derived mask (A).**
   Rejected as an op: connection is profile identity (`peripheral_flags`).
5. **Runner flag naming (F `--save-device`, `--state-export-at`; B
   `--save-import-fd`; C `--state-out/--state-in/--replay`).** The device is chosen
   by naming the registered profile id; any `--save-device` convenience maps to a
   registered id and is refused if it conflicts with the fixture's profile. State
   and replay flags are provisional, validated before guest start, and refuse every
   combination not explicitly defined. Names are a planner choice; none is an ABI
   promise (Phase 10).
6. **B's refusal statuses vs "defined postconditions" in C.** Compatible: adopt B's
   distinct `SAVE_*` statuses and C's refusal postcondition list (digest, trace,
   cursors, counters, allocation ownership unchanged); both apply to save import and
   state import.
7. **Plan count/placement of the harness freeze (F).** F places inventory, qualifier
   and CI changes in Plans 1-2 but the supervisor, fixture table and input-size
   constants also change for the new profile and fixtures (correction 3). Resolved
   after adversarial H1/M6: 09-01 is core-only; 09-02 carries the runner/supervisor/
   publisher/fixture-table mechanism and a fail-closed harness stub; the real
   collector/verifier and `ci.yml` steps land once in 09-03, the first plan with
   real hosted data; later plans add data rows and their own flag-forwarding rows.
8. **SI event capacity in the CPU store preflight (A, E).** Now traced (adversarial
   H6, re-verified): `cpu_successor_extra_event_budget` returns 0 for any store whose
   width is not 4 and exactly 1 for a 32-bit store to a non-RDRAM address
   (`core/t64_cpu.c:1483-1511`), and the successor preflight requires
   `1 + extra` free slots and sequence headroom (`:1513-1550`). An SI read-DMA start
   reserves two events (execute and complete) before busy is visible, so it needs an
   extra budget of 2; an 8-byte `sd` to an SI register needs a defined outcome.
   Resolved as D-44: the first task of 09-03 is a profile-gated budget change that
   leaves every legacy profile's budget untouched (pinned by the old-profile
   controls), with `QUEUE_FULL` and `SEQUENCE_EXHAUSTED` controls at a small queue
   capacity and sequence width and a width-8 store control. The alternative
   (reserve only the execute event at start and the completion inside the execute
   handler) is rejected as the default because a handler-time reservation failure
   has no refusal path after the guest store has committed.
9. **Replay platform binding.** Replay and state bind the exact build, and build ids
   differ per platform, so a macOS recording does not replay on Linux. Cross-platform
   evidence compares checkpoint digests of independent runs, which are
   platform-invariant for the new profile because `build_id` is excluded from its
   digest (D-5). This is a consequence, not a defect.
10. **EEPROM `0xFF` on channel 4 and the controller `0xFF` reset (A vs B).** Resolved:
    channel 4 accepts only `0x00`, `0x04`, `0x05` with exact lengths; `0xFF` on
    channel 4 yields the no-reply timeout flag; `0xFF` is a controller-channel
    reset-plus-info command only.

## 3. Decisions (D-1..D-46), grouped by requirement

Reversibility: H high (profile-scoped or doc-level), M medium, L low. "Reversal"
names the evidence that would change the default.

### Cross-cutting identity, ordering and state (CORE-02, CORE-04, CORE-05)

- **D-1 Profile family.** Descriptor 12, model 20, state 25; four ids sharing it
  (scheme in D-43); selector `save_kind` and `peripheral_flags` bit 0, both now
  validated (descriptor 12 range checks; create-time refusal of non-zero values on
  older descriptor versions); new `si_policy` and `save_policy` labels; ids and
  labels at most 63 characters. Label: [POL] over [CODE]. Reversal: owner wants
  runtime-selectable media. Reversibility: M until F2.
- **D-2 Scheduler order stays `tick-sequence-fifo-v1`.** No class rank, no new
  policy string. SI joins the scheduler as a new device id (10) with two kinds
  (execute, complete); EEPROM has no event. Label: [POL] (no hardware evidence for
  any tie). Reversal: E0 or the matrix finds a guest-history-independent order
  defect, a real guest needs a specific tie, or the owner prefers a specified rank
  table (then a later profile with E's table). H.
- **D-3 Host input.** Strictly-after latch plus `at` replay discipline (conflict 1).
  [POL]. Reversal: see conflict 1. H/M.
- **D-4 Quantum.** Descriptor 12 refuses `synchronization_quantum != 1` and
  ratio != 1:1; unit written as "RSP issue opportunities equal to guest ticks";
  the documented statement is only that any other quantum is a new profile.
  The earlier sentence that results "may differ only for CPU-RSP races" is
  **deleted**: no test exists for it, so it is [UNK] and is not written into the
  spec (adversarial M4). Q > 1 is not implemented. Quantum sensitivity of the
  identity can be shown only at the hash-function level once Q != 1 is refused;
  the existing contrast at `core/tests/test_profile.c:450-470` (quantum 2 on an
  older descriptor) stays and a new explicit control pins that the hash changes
  when only the quantum changes. [CODE] inert today + [POL]. Reversal: v2
  performance milestone. H.
- **D-5 Canonical payload as digest preimage (new profile only).** Sink seam and
  section framing in 09-01; `build_id` and `profile_id` excluded from hashed
  packet metadata (still stored in ring metadata and the export header); importer
  re-derives those two text fields from the live machine, valid because binding is
  exact. 09-01 adds the strip-and-compare differential of conflict 3 as the
  independent oracle for the v25 port; the v25 digest is documented as not
  comparable to v24. [CODE]+[POL]. Reversal: the legacy pins cannot be held after
  the seam change (a reopened design question in 09-01, no pre-approved fallback),
  or the owner wants build in the digest. M.
- **D-6 Freezes F0, FH, F1, FR, F2** as tabulated in conflict 2. [POL]. H.
- **D-7 Closed `(device, kind)` handler table** introduced as the 09-01 ledger
  (E0), consumed by the state-import plan (09-09); unknown pairs are refused at
  import. [CODE]. H.

### SAVE-01: SI, PIF and the standard controller

- **D-8 HLE PIF behind SI, no PIF ROM.** 64-byte model-owned PIF RAM reachable only
  through SI DMA; CPU access to `0x1FC00000..0x1FC007FF` stays unsupported;
  WR4B/RD4B refused; PIF boot info and CIC challenge are not modeled. [POL]
  (PIF ROM is rights-bearing: AGENTS.md). Reversal: a later rights-cleared guest
  needs direct access or the challenge. H.
- **D-9 SI registers and DMA.** Registers `+0x00` DRAM address (24 bits, last
  value readable), `+0x04` RD64B (PIF to RDRAM, write-only), `+0x10` WR64B (RDRAM
  to PIF), `+0x18` STATUS; PIF address must equal `0x1FC007C0`; RDRAM span 8-byte
  aligned and in range with checked arithmetic; one DMA at a time (a second start
  sets sticky DMA_ERROR and is refused, PI precedent); reserve-then-commit; all
  events reserved before busy is visible; completion sets MI SI pending
  regardless of mask; any STATUS write clears it; IO_BUSY, DMA_STATE and PCH_STATE
  read 0. A misaligned DRAM address is refused **and sets sticky DMA_ERROR**
  (n64brew, re-fetched: DMA_ERROR is "set when overlapping DMA requests occur, or
  when writing to a misaligned address" and clears only by power reset; the
  alignment mask itself is [UNK]); an out-of-range span is refused without setting
  the error ([POL]). A read DMA started while a write DMA is active is treated as
  an overlapping request (refused, sticky error, PI precedent). The source is
  silent on whether a DMA read waits for a DMA write: the sentence that "reads
  while a write is in progress are correctly delayed" refers to direct reads of the
  PIF-mapped window, not DMA (adversarial M7 overstated it). This is a named
  [POL] divergence; a guest that polls busy before the next DMA, as SDK-style code
  does, never overlaps. Write DMA reads RDRAM and replaces PIF RAM at the start
  tick (a named difference from PI's completion-time read, with an in-flight
  mutation witness); read DMA copies atomically at completion. Register layout
  [FACT/COMM]; the rest [POL]; alignment mask and post-DMA address are [UNK].
  Reversal: a second source for alignment, overlap or status effects. H.
- **D-10 PIF command byte (offset 63).** Only bit `0x01` (parse) is honored and is
  cleared after parsing; bit 0x01 clear with no other bits replaces RAM without
  re-parse (the read DMA re-executes the last parsed table). Any other bit is a
  request for an **unmodeled PIF control function** and refuses the write-DMA start
  with unchanged state. n64brew (re-fetched) lists run-mode bit 0x02 as
  challenge/response and the "joybus flag bits" 0x04 and 0x08 as changing the
  TX-to-RX delay (about 7 us versus about 520 us), while the same page also calls
  them "bits 0x02 and 0x03": the page is internally inconsistent, so which bits
  select which delay is [UNK] and is not modeled. The refusal is a named [POL]
  divergence from the PIF, which would honor the timing bits; it is consistent with
  D-11 because command-byte bits are control functions, not frame bytes (see D-11).
  [COMM] single source for the bit list; refusal is [POL]. Reversal: a guest needs
  another bit, or a second source resolves the timing-bit layout. H.
- **D-11 Joybus parser.** Total over all 512-bit inputs, bounded, deterministic;
  escapes `0x00`, `0xFD`, `0xFE`, `0xFF`; TX/RX counts in the low six bits, flags
  in the top two; handshake must fit offsets 0..62; five channels; malformed
  frames are guest-visible (DMA completes), never host refusals; fuzz target. The
  rule is "unmodeled function = refusal (D-10); malformed or unsupported frame
  *data* = guest-visible outcome", stated once in the spec so the two cannot be
  read as contradicting. [COMM] + [POL] for edges. Reversal: second source
  contradicting escapes. H.
- **D-12 Channels and presence.** Ports 0-3 are channels 0-3 and channel 4 is the
  cartridge; only port 0 has a controller; others and channel 4 without EEPROM
  return RX flag `0x80` with no data. Schema carries ports 0-3. [COMM]/[POL].
  Reversal: owner wants multiplayer (additive mask). H.
- **D-13 Controller responses.** `0x00` and `0xFF`: `05 00 02`; `0x01`: four bytes
  (A B Z Start Up Down Left Right, then RST 0 L R C-up C-down C-left C-right,
  signed X, signed Y); pak read/write and anything else: timeout flag `0x40`, no
  data, no fabricated pak or CRC. Host `buttons` bits `0x00C0` are masked, not
  refused (preserves the existing API); initial latch is neutral. Button mask
  hex values are [INF] from the libdragon field order, so 16 single-button literal
  vectors are pinned and a second primary-ish source is sought before any
  wording beyond "the authored mapping". Reversal: a cross-check disagrees. M.
- **D-14 Timing.** Policy `joybus-wire-time` per conflict 5; execute tick
  `E = start + H`, completion `C = E + X`; all controllers sampled at `E`. Tests
  compute `E` and `C` from literals independent of production helpers. [POL]. H.
- **D-15 Input schema.** Absolute guest ticks (u64), level semantics, later row
  wins at the same `(tick, port)`; row `{tick, port, buttons u16, x i8, y i8}`;
  frame- or VI-relative authoring is host-side only. The existing `t64.input` v1
  path stays; the new profile accepts port field 0 only in practice but the
  schema/replay carry ports 0-3. [POL]. M (API-visible before the Phase 10 freeze).
- **D-16 Authored diagnostic `pad-poll`.** Fetched MIPS code (cleared authored,
  same build and manifest gate as `fixtures/audio/pcm.S`) builds literal joybus
  blocks, polls via SI DMA and prints one ASCII line per poll through the ISViewer
  window. Expected lines come from the host harness: the harness reads the tick of
  the RD64B register-write trace record and computes `E_n` from the policy
  literals; inputs are placed at `E-1`, `E`, `E+1`. **That derivation is
  self-consistency of the policy constants, and the wording must say so (adversarial
  M8).** To add an in-guest time witness that does not depend on the harness trace,
  the ROM also reads CP0 Count before each poll and prints it on the poll line
  (the model advances Count once per two guest ticks, `core/t64_cp0.c:580-590`,
  which is a model mapping, not hardware time). The harness checks that the printed
  Count spacing is consistent with the scheduled inputs and the instruction timing
  to within the two-tick Count resolution; exact-tick assertions stay with the
  trace-derived `E_n`. Controls (each reaches its comparator): input tick shifted
  across `E`, one wrong byte, wrong port-1 flag, all-neutral baseline.
  [authored; no hardware evidence]. H.

### SAVE-02 and SAVE-04: explicit save devices and refusal

- **D-17 Selection is the profile.** Header, title, game code, checksum, file
  name are never inputs; an authored ROM carrying a save-type nibble in header byte
  `0x3F` is a negative control. [CODE]+[POL]; SAVE-02 text. Reversal: Phase 10 facade
  runtime selection. M.
- **D-18 EEPROM commands (channel 4, exact lengths).** `0x00` info (tx 1, rx 3):
  `00 80` or `00 C0` plus status (`0x80` busy); `0x04` read (tx 2, rx 8) only when
  idle; `0x05` write (tx 10, rx 1): reply `0x00` accepted or `0x80` already busy,
  data committed at acceptance, window not extended while busy. Any other command or
  length, or a read while busy: timeout flag `0x40`, nothing written, state
  unchanged. [COMM] ids/lengths/status; exact lengths and no-reply on busy read
  are [POL]; hardware busy-read behavior [UNK]. H/M.
- **D-19 Block rule (changed by adversarial M1).** 4 Kbit: the **documented
  mirror** is the default, `block & 0x3F`, so blocks 64..255 address blocks
  0..63 (n64brew, single source, re-fetched by the synthesizer and the reviewer; not
  independently verified). Refusal of blocks 64..255 (timeout, no state change, no
  busy window) has no source and is recorded only as the alternative
  `eeprom-4k-block-refuse`. 16 Kbit: all 256 blocks. Test patterns carry the block
  index so aliasing and a wrong mask on either part are visible (write block 0x41 on
  the 4 Kbit part and read block 0x01; on the 16 Kbit part blocks 0x41 and 0x01 hold
  distinct data). Real software range-checks first (SDK), so the difference is not
  observable to conforming guests. [COMM] single source; the policy label names the
  mirror. Reversal: a second source or measurement contradicting the mirror. H.
- **D-20 Busy window.** Conflict 4 and 5: `busy_until` compare against the sourced
  6 ms ceiling used as a fixed 375,000-tick duration; label
  `eeprom-busy-ceiling-6ms-v1` (never "exact"). [POL] over a sourced upper bound.
  H.
- **D-21 SRAM.** 32 KiB at `[0x08000000, 0x08008000)`; the rest of domain 2 stays
  unsupported (no mirroring, no banking); PI DMA in both directions with the same
  `1 + N/2` duration equation, source/destination effect atomic at the completion
  event, direction and target kind in the active descriptor; CPU/host 32-bit aligned
  word access, narrower refused; profiles without SRAM keep refusing read-length
  starts. Initial contents: EEPROM `0xFF`, SRAM `0x00`, never auto-loaded.
  Labels: SRAM base address and 32-bit-only direct access are [COMM] from B's
  n64brew PI-page citation and were **not** re-verified by the synthesizer or the
  reviewer; the 32 KiB size, blank contents and non-mirroring are [UNK] and are
  named [POL] choices. H.
- **D-22 Channel 4 without EEPROM.** Every handshake returns `0x80`, writes
  nothing; RTC commands `0x06..0x08` are unsupported (timeout flag). [COMM]. H.
- **D-23 Unknown, unsupported and deferred requests.** `device_kind` values
  reserved now: 1/2/3 supported, FlashRAM and Pak kinds known-unsupported
  (`SAVE_UNSUPPORTED_DEVICE`), everything else unknown (`SAVE_UNKNOWN_DEVICE`);
  kind-versus-profile mismatch is `SAVE_DEVICE_MISMATCH`. A guest-side request on
  the wire follows D-18/D-22 (flags, no state change). Tests assert the specific
  status and an unchanged digest, never a silent success such as reading `0xFF`.
  [POL]. H.

### SAVE-03: import and export

- **D-24 API and formats.** `t64_machine_save_info/export/import` provisional C
  surface; formats `RAW` (canonical joybus-block / bus byte order), `SWAP16`,
  `WORDREV32`; the caller always names kind and format; no autodetect, no
  containers; exact lengths 512 / 2048 / 32,768 only. Check order (all before any
  read of caller bytes or mutation): admitted, pointer/length, profile has a device,
  kind known, kind supported, kind equals profile, format known, length equals size,
  no save-affecting work in flight (import refused while EEPROM busy or an SRAM PI
  DMA is active; export allowed any time). Refusal postconditions: digest, device
  bytes, busy deadline, scheduler, tick, counters, trace, `write_generation`
  unchanged; export refusal leaves caller bytes unchanged; too-small export reports
  required length. Success: bytes equal the normalized input, busy cleared, queue
  and time unchanged, `write_generation` increments (observer only, excluded from
  digest). Literal 8-byte vectors pin the transforms; the project asserts no
  mapping to any other emulator's convention. [CODE] ROM-admission pattern + [POL].
  Reversal: owner wants interchange containers, or RAW-only scope. H/M.
- **D-25 Replacement semantics.** Successful ROM replacement restores blank save
  content and clears the deadline (post-replacement equals fresh machine plus
  admission); refused admission leaves bytes unchanged; the host imports after
  admission. A future same-content reset must preserve bytes. [CODE]+[INF]. M.
- **D-26 Persistence is host-owned.** The core never opens files; the runner
  imports through the verified-input fd path and exports via the staged no-clobber
  publisher; no automatic load, flush, timers or path derivation; result JSON and
  logs report only kind, length, format and flags, never save bytes. [CODE]. H.
- **D-27 Save in digest/state.** A `save-media` section holds kind, size,
  `busy_until` and the full bytes; unselected devices contribute nothing. [CODE]. M.

### CORE-02: one scheduler for every in-scope device

- **D-36 Deliverables.** (E0) pairwise tie characterization on existing profiles
  with FIFO pinned (both insertion orders, independent literal expectations, a
  transposed-table mutation control), plus CPU-vs-device and CPU-vs-RSP probes;
  (E3) SI added with refusal atomicity at `QUEUE_FULL` and `SEQUENCE_EXHAUSTED`
  using a small-width, small-capacity profile; (E4) one shared matrix: whole
  advance versus splits at `{T-1, T, T+1}` and step sizes `{1, 3, 7, 4096,
  one-shot}`, mid-tick counter stops and resource-limit stops then resume, two
  and three interleaved machines (one of a different profile), trace on/off,
  and, after the state plans, export/import at a mid-tick stop. The matrix also
  includes a span longer than one Count wrap (adversarial determinism note). MI
  stays "synchronous in the producing handler, sampled by the CPU at step start"
  (the step checks `t64_cp0_interrupt_pending` first, `core/t64_cpu.c:2175-2180`)
  with a per-producer same-tick test; MI is not made an event, so **criterion 4's
  "MI is ordered by the scheduler" is stated as "ordered through its producers'
  events"** in the spec and the claim. Every tie is documented as [POL] with owning
  test name; no tie result is hardware evidence. Count/Compare visibility is
  documented as CPU sample time.
  **Required mutants (adversarial M4), each of which must fail a named test:** a
  handler that reads `machine->tick` instead of `event->tick`; a reservation whose
  sequence is taken at commit time instead of reserve time; an SI completion
  derived from the per-advance event budget; the input-promotion/sample order
  reversed. **Evidence limit stated in the claim:** E0 pins existing behavior
  (regression), and the whole-versus-split matrix compares the code with itself
  under different schedules, so a consistently wrong tie would pass; the claim
  says "ordered and partition-invariant under the authored cases", not
  "correct". [CODE]+[POL]. H.
- **D-37 Quantum evidence.** Refusal of `Q != 1` on descriptor 12 with unchanged
  allocator counters, accepted `Q = 1`, pinned independent reference hash, the
  explicit hash-sensitivity control of D-4, quantum present in result identity
  records (check, add if missing). [CODE]. H.

### CORE-04: machine state export and import

- **D-28 Container (provisional).** Binary, big-endian: magic `T64STATE`, container
  version, state serialization version, total length, section count (at most 32),
  flags (must be 0), profile digest, content SHA-256 and length, build id,
  guest tick (cross-checked), declared canonical digest, section table (id, flags,
  offset, length, SHA-256), canonical sections, an observer-continuity section
  excluded from the digest, SHA-256 trailer. No compression, no unknown-section
  skipping in v1, hard length ceiling 64 MiB and a profile-derived exact length.
  [CODE]+[POL]. H (provisional until Phase 10).
- **D-29 Exclusions and observers.** Excluded: trace, drain cursors, ring head/count,
  `rsp_observed_*` diagnostics, caller buffers, pointers, allocator, queue head,
  padding, build identity as machine behavior. Retained packet slots (about 1.1 MiB)
  are carried, because the digest hashes drained slots; the observer-continuity
  section restores head/count validated against producer sequence (count at most 2);
  a "drop observers" flag restores the consistent empty cursor. **Observer state
  gates dispatch (adversarial H4, re-verified):** `packet_count` is excluded from the
  digest yet decides whether a stalled VI or AI event may dispatch
  (`core/t64_scheduler.c:310-336`, `core/t64_ai.c:384,416,487`), so a machine
  restored with "drop observers" can stop at a different point of the same advance
  than the source while the digests are equal. Default import therefore restores
  observer continuity; "drop observers" is documented as changing stop points (not
  guest-visible results) and T3 asserts both. T3 compares drained packet bytes, not
  only digests. [CODE]. M.
- **D-30 Import: validate then apply, no allocation; contract is memory safety plus
  liveness (restated by adversarial H4).** **Contract:** a blob that passes
  validation cannot cause memory unsafety, an unbounded loop or arithmetic
  undefined behavior in any later call; semantically unreachable states are
  accepted and then run deterministically. Reachability is explicitly **not**
  promised: hashes and the declared digest are integrity checks, not
  authentication, and CPU cache, TLB and similar contents are arbitrary by design.
  One reader per section with a validate mode and an apply mode over the same code;
  a bounded invariant summary on the stack (the queue is at most 256 entries,
  `core/t64_internal.h:14`); order: argument and disjointness, header, whole-blob
  SHA-256 trailer, identity binding (each mismatch its own status), section table,
  per-field range checks, structural checks, recompute the digest by hashing the
  blob's canonical bytes against the declared digest, then one infallible commit
  (handlers rebuilt from the closed table, queue head 0, trace cleared with
  enablement preserved). Structural checks that are uniform: tuples strictly
  ordered, sequence below `next_sequence`, tick at or above machine tick, `(device,
  kind)` in the table, derived lines recomputed, ranges and enums valid. **Per-device
  "busy implies exact events" is not uniform and is derived device by device**
  (CPU has one step event unless stopped plus the exception re-reserve path,
  `core/t64_cpu.c:1552-1567`, `core/t64_exception.c:91`; PI has a canceled state with
  no event; VI and AI events may be queued but undispatchable); a wrong rule either
  refuses a legitimate export (caught by T1) or admits garbage (caught by nothing),
  so each rule is written from the device's state machine and tested both ways.
  EEPROM excluded (conflict 4). **Generated field-use audit (deliverable of 09-09):**
  a script lists every state field used in `core/` as an index, length, shift, loop
  bound or array subscript and maps it to its import range rule; the check fails on
  an unmapped field (roughly 500 fields, so manual review is not enough). In tests
  only, the digest is recomputed after apply and compared with the declared digest
  (a check, not a rollback: the commit stays infallible because it performs no
  allocation or host call, and core lint forbids allocation names and mutable
  file-scope statics, `tools/lint/rules.txt` L1-NAME-ALLOC and L1-STATIC-MUTABLE).
  Every refusal leaves the live machine bit-identical and never calls `release` on
  an unprovable descriptor. [CODE]+[INF]. Reversal: the invariant summary cannot
  cover a needed cross-check (then a staged allocation as an explicit contract
  extension, a Phase 10 matter, not an owner stop). M.
- **D-31 Binding.** Exact profile digest, state version, content digest and build id;
  cross-build import is refused in v1 (a labeled diagnostic mode is deferred).
  Distinct statuses PROFILE, CONTENT, BUILD, STATE_VERSION. Importer registered per
  state version; "known but not importable" differs from "unknown". [POL]. H.
- **D-32 Proof ladder.** T1 round trip, T2 export-import-export byte identity, T3
  continuation (one-shot and 1/7/31-tick partitions, onto a dirty peer and onto the
  source after advancing), T4 non-interference of export. **The boundary matrix
  names its construction source (adversarial H5).** The five Phase 9 ROMs reach only
  the new-device states; the rest come from existing in-process builders in
  `core/tests/`, named per boundary by the plan's pattern mapper and each guarded by
  a "boundary reached" assertion on the state itself before export (builder names
  below are candidate sources from the file inventory, not verified to reach the
  state):

  | Boundary | Source |
  | --- | --- |
  | SI mid-transaction, EEPROM inside its busy window, pending input | authored ROMs (`pad-poll`, `eeprom-rw`, `replay-mix`) through the real runner or an in-process machine |
  | AI two descriptors, paused partial packet, output-full stall | `fixtures/audio` plus `core/tests/test_ai.c` builders |
  | PI active and canceled; SP active plus pending | `core/tests/test_pi_dma.c`, `core/tests/test_sp_dma.c` |
  | RSP mid-program | `core/tests/test_rsp_control.c` and sibling RSP tests, `fixtures/rsp` |
  | RDP pending command and triangle words | `core/tests/test_rdp.c`, `core/tests/test_rdp_triangles.c`, `fixtures/rdp` |
  | VI mid-field, retained undrained packet, stalled VI event | `core/tests/test_vi.c`, `core/tests/test_video_output.c` |
  | CPU delay slot, dirty D-cache line, non-empty TLB | `core/tests/test_cpu_cache.c`, `core/tests/test_cpu_mmu.c`, `core/tests/test_cpu_path.c` |
  | coincident same-tick events (both orders) | `core/tests/test_determinism.c`, `core/tests/test_scheduler.c` |
  | near-exhaustion (queue, sequence) | small-capacity, small-width test profile as in `core/tests/test_scheduler.c` |

  The **required set is capped** at these rows; any additional boundary is optional.
  T3 includes the stalled VI and AI event boundary (D-29). Each boundary has a
  one-field perturbation control that T3 must detect. Mid-instruction is not
  reachable through the public API [INF, verify with CPU owner]. The evidence is
  self-consistency of the model, not hardware. M.
- **D-33 State privacy.** Blobs are private (0600, no-replace, no symlink parents),
  never uploaded or committed; public evidence is a bounded projection of
  authored-fixture digests and sizes; scanner rules refuse `*.t64state`,
  `*.t64replay` and save patterns by extension and magic; no host paths, times or
  user names in headers. [CODE]. H.

### CORE-05: input replay

- **D-34 Replay artifact.** Canonical JSON `t64.replay` v1 (closed keys, decimal
  string u64, lowercase SHA-256, no floats, duplicate keys refused, bounded parser,
  identity = SHA-256 of canonical bytes). Header: profile id/digest, descriptor/
  model/state versions, build id (binding), source revision (informational), content
  SHA-256 and size, `save_init`, input schema version. Ops: `input{at, event_tick,
  port, buttons, x, y}`, `checkpoint{at, digest}`, one terminal `end{at, digest,
  stop kind}`; `at` nondecreasing, ties in array order, checkpoint after the advance
  and before later ops at that tick; `event_tick >= at`. **Goldens are
  build-independent (adversarial M3, re-verified):** `build_id` hashes compiler id,
  version, flags, build type and the SHA-256 of every core input
  (`core/CMakeLists.txt:44-66`), so normal, ASan/UBSan and gcc:13 builds differ and
  every core edit changes it; committed goldens are therefore ops plus checkpoint
  digests as literals with the header composed at test time, and sanitizer and gcc:13
  tests record fresh in-process. The recorder wraps the actual accepted
  `t64_machine_queue_input` calls, and the *original* run is driven from an
  independently authored `t64.input` script, not from the recorder's own op list,
  so a recorder omission cannot be replayed faithfully into a pass. A scripted run
  that pre-queues up to 128 events at tick 0 (`runner/t64_input.h:11-12`,
  `runner/t64_real.c:1011-1155`) is recorded with `at = 0` for all of them; if the
  driver's submission hits `QUEUE_FULL` the outcome is a divergence/refusal report,
  never a silent skip (adversarial L6). Each of profile, build and content mismatch
  has its own refusal control that reaches the comparator, and a test that mutates a
  single `at` must diverge. Boot is `cold-admit` only
  in v1 (state-anchored replay deferred; the key is reserved). Caps: 4 MiB, 65,536
  ops, 4,096 checkpoints. Identity mismatch is refused before any guest execution
  with a named field. Divergence is reported in a separate closed
  `t64.replay-report` record (first differing checkpoint, ticks, digests), not by
  extending `t64.result` v2. The core knows nothing of replays; the recorder and
  driver are runner-side, re-homable behind the Phase 10 ABI. [CODE]+[POL]. M.
- **D-35 Runner surface (provisional).** Input schedule keeps its existing path;
  new flags for save import/export, state export/import and replay record/play
  validate before guest start, use the verified-input snapshot for inputs and the
  staged no-clobber publisher for outputs, and refuse undefined combinations.
  Documented as provisional with a version string in each format. H.

### QA, security, CI and claims

- **D-38 Harness mechanism lands in 09-02 (core work stays in 09-01).** Artifact-dir
  and FSIZE option plus the second narrow publisher (conflict 6), runner fixture and
  profile tables and profile-observation branches generalized for model 20 and the
  new ids, inventory lane, `scripts/qualify-phase09.py` fail-closed stub, upload
  allowlist rule refusing state/save/replay paths, extension of the existing
  `scripts/rights-scan.sh` denied-extension table (it already denies `.sav` and
  near-miss forms with controls, `scripts/rights-scan.sh:567-597`) with state/replay
  extensions and magic rather than a parallel scanner, test registration in
  `scripts/check-test-inventory.sh` (baseline lists are hand-kept), fixture rows in
  `fixtures/manifest.json` with build-only identity and the IPL3-composition note as
  in the audio entry (`fixtures/manifest.json:658-710`), claim skeleton. The real
  collector/verifier and `ci.yml` steps land once, in 09-03, against the first real
  hosted data; the harness-logic freeze FH follows. `scripts/build-boot-fixtures.sh`
  is not touched (it is the Binutils cache key, F-100); new ROMs get their own build
  script. [F] F-096..F-100. H.
- **D-39 Fixtures.** Five authored ROMs from three assembler sources, each with a
  manifest row, build-only identity and rights gate before guest use, independent
  literals, and a same-setup wrong-value control: `pad-poll` (09-03), `eeprom-rw`
  (4k and 16k builds of one source) and `sram-rw` (09-04), `save-roundtrip`
  (09-05), `replay-mix` (09-07; length measured first). H.
- **D-40 Cheap-first order inside every plan.** Fixture identity/rights, schema/
  inventory/claims/CI scanners, `ast.parse` for Python, gcc:13 `-Werror` on changed C
  units, one accepted operation through the real runner with cleared input,
  focused CTest via the existing preset (four jobs), ASan selection, old-profile
  regressions, then the full gate only at the end of 09-03 (first hosted pair),
  09-06 and 09-11. Retain LastTest before the next CTest; unique JUnit paths.
  **Hosted budget is set against the cold-cache figure (adversarial M5, re-verified):**
  warm macOS was about 275-279 s (`docs/ci-reviews/LOG.md` F-100, run rows at lines
  116-117) but the cold run was 464 s (gate-macos 461 s) and the Phase 8 binding run
  507 s (`08-05-SUMMARY.md:65`) against a 600 s limit, so always-on hosted additions
  are capped at about +25 s macOS and +25 s Linux measured against cold, both figures
  are measured in the 09-02/09-03 hosted pair, and state-hash-heavy T3 and mutation
  loops are native-only from the start. Fuzz corpora, ASan, gcc:13 and the replay
  soak stay native; mutation loops over a state blob sample header and device
  regions, not every byte of 4 MiB. No new CI job; take F-097 and the F-100
  candidates only before F2. H.
- **D-41 Importer security rules (state, save, replay, input).** Parse then validate
  then commit; caps before allocation; checked arithmetic; closed size sets; no
  overlap with live storage; identity binding; no-clobber publication; zero padding
  and zero reserved fields; structure-aware mutation tests whose mutants keep magic
  and checksums valid and each reach their specific refusal; an independent review
  and security audit (09-10) run against the real mutated blobs of the 09-09 sweep
  and complete, with its fix loop, **before** the identity freeze F2. [F] F-D5. H.
- **D-42 Claims.** One `docs/claims/phase09.md` with the tiers in section 8; skeleton
  checked in 09-02, counts filled in 09-11. [COMM] facts carry source URL and fetch
  date in the spec and are restated in authored words, not pasted (adversarial L4;
  n64brew text may be licensed CC BY-SA). H.
- **D-43 Profile id scheme (adversarial M9).** Stem
  `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1`, suffixes `-e4k`, `-e16k`, `-s32k`; a
  test asserts every registered id is at most 63 characters, and the runner's
  allowed-profile table (`runner/t64_real_fixture.h`) and the observation checks that
  branch on `T64_REAL_AUDIO_PROFILE_ID` (`runner/t64_real.c:40-41,488,681`) are
  updated in 09-02. [CODE]. H.
- **D-44 Profile-gated CPU store preflight (adversarial H6).** First task of 09-03:
  for the new profile family only, the extra-event budget of a 32-bit store to the SI
  window is 2 for RD64B and 1 for WR64B (legacy budgets untouched and pinned by the
  old-profile controls); 8-byte stores to the SI window have a defined outcome and a
  control; atomic refusal at `QUEUE_FULL` and `SEQUENCE_EXHAUSTED` at a small queue
  capacity and sequence width. [CODE]. Reversal: a cheaper atomic design (for example
  a single event) is found. M.
- **D-45 Generated field-use audit (adversarial H4).** See D-30; a 09-09
  deliverable checked by a script in the gate's native lane. [INF]. H.
- **D-46 Independent review before freeze (adversarial H2).** 09-10 precedes F2;
  review findings become fix tasks inside 09-10 (or gap plans), never logic edits
  after F2; one fix cycle is budgeted. [F] Phase 8 `08-REVIEW.md` CR-01/WR-01. H.

## 4. Plan breakdown and waves

Strictly sequential (shared inventory, case JSON, CMake and runner registrations
collide in parallel, per Phase 8). **Eleven plans in eleven waves** after the
adversarial review: the first plan is split in two (H1), the state plan is split in
two (H5), and an independent review and fix loop precedes the identity freeze (H2).
More plans are acceptable: each is one verified slice, and the coordination cost is
lower than the overrun and reopen cost the review found. Summaries and state are
written by the orchestrator.

Effort is low-confidence [INF] (adversarial L5). Phase 8 (one device plus capture,
five plans) took about 25 hours of commits from first to closeout and six native
qualification attempts; Phase 9 is roughly three times the surface (SI/PIF, two save
classes, three serialization parsers, a walker refactor). Plan for a review/fix loop
and at least two qualification cycles; nothing is gated on any estimate.

| Plan | Wave | Requirements | Scope |
| --- | --- | --- | --- |
| 09-01 | 1 | CORE-02 (ledger, quantum), CORE-04 (walker skeleton) | Core identity family, membership sweep, sink and v25 walker with the differential; core only |
| 09-02 | 2 | harness for all | Runner, supervisor, publisher, fixture/profile table, scanners, fail-closed harness stub |
| 09-03 | 3 | SAVE-01, CORE-02 (SI share) | CPU preflight change, SI/PIF/controller tracer, first hosted pair, real collector/verifier (FH) |
| 09-04 | 4 | SAVE-02, SAVE-04 | Explicit EEPROM and SRAM devices and refusals |
| 09-05 | 5 | SAVE-03, SAVE-04 | Save import/export and runner persistence |
| 09-06 | 6 | CORE-02 | Cross-device ordering matrix; state-content freeze F1 |
| 09-07 | 7 | CORE-05 | Replay record/play |
| 09-08 | 8 | CORE-04 | State export and container |
| 09-09 | 9 | CORE-04 | State import, invariants, field-use audit, boundary matrix |
| 09-10 | 10 | all | Independent review, security audit and fix loop; FR |
| 09-11 | 11 | all | F2 pins, claims, evidence, qualification |

**09-01 Core identity and walker (core only).**
Scope: descriptor 12/model 20/state 25 family and four ids by the D-43 scheme, with
a symbolic accessor; **the membership sweep** (generate the exact list by script
first: this synthesizer's grep counted 105 model/state and 76 descriptor-version
comparisons in `core/` and `runner/` with a narrow pattern, the reviewer counted
207 and 39 with another, large either way; cover the validator, writer, the
cumulative `t64_has_*` chain at `core/t64_internal.h:927-1100`, create-time storage
ladders and `is_ai_profile`); `save_kind`/`peripheral_flags` validation (D-1);
quantum and ratio validation (D-4); sink seam, framed v25 walker with all existing
devices ported once and section ids assigned, `build_id`/`profile_id` exclusion;
**the strip-and-compare differential (D-5)**; E0 ledger (closed `(device, kind)`
table, pairwise FIFO characterization on existing profiles); correction of the stale
digest table in `docs/spec/machine-contract.md:270-300` and documentation of the new
domain string and framing there (adversarial L2, moved up from the ordering plan).
Requirements: CORE-02 (ledger, quantum), CORE-04 (walker).
Owning tests: `unit.core.profile.v12-identity`, `unit.core.profile.old-pins`,
`unit.core.profile.save-kind-validation`, `unit.core.profile.id-length`,
`unit.core.digest.sink-legacy-identity`, `unit.core.digest.v25-reference`,
`unit.core.digest.v25-strip-differential`, `unit.core.sched.ledger`,
`unit.core.sched.characterize-fifo`, `unit.core.profile.quantum-identity`.
Authored fixtures: none new (the existing audio fixture is only used in-process).
RED before GREEN: each new test must fail first for its intended behavioral reason;
the differential must fail against a deliberately dropped-field mutant of the v25
port (mutant-must-compile).
Cheap-first: old-pin vectors first (they prove the seam), then the independent
reference encoder for descriptor and framing, then the differential, then the sweep.
Frozen after (F0): version-membership pattern, section ids and framing, id scheme.

**09-02 Runner, supervisor, publisher and harness mechanism.**
Scope: `--artifact-dir`/`--artifact-max-bytes` supervisor pair with the exact FSIZE
rule (conflict 6, adversarial L1); the second narrow publisher module with its own
no-clobber and CR-01-style substitution controls; runner fixture and profile tables,
profile-observation branches and any fixed input-size handling
(`runner/t64_supervisor.c:45,584-589`, read first) generalized for the new ids;
inventory lane and `scripts/qualify-phase09.py` fail-closed stub (zero cases fails);
upload allowlist rule and `rights-scan.sh` extension for state/replay/save
extensions and magic (with mutation controls); fixture manifest rows scaffold; claim
skeleton; one real-runner admission and a single canonical tick on each new id with
a cleared authored input (AUTONOMY first-actual-consumer rule).
Requirements: harness for all.
Owning tests: `harness.fsize-artifact-synthetic`, `harness.fsize-report-capture-still-bounded`,
`runner.artifact-publisher`, `controls.artifact-substitution`, `harness.phase09-stub-fails-closed`,
`harness.rights-scan-state-replay`, `harness.upload-allowlist-artifacts`,
`runner.profile-table-id-length`.
Cheap-first: the synthetic flood control, then the publisher happy path, then the
scanner controls.
Frozen after: publisher module, supervisor artifact pair.

**09-03 SI/PIF, the standard controller, and the first hosted data.**
Scope: **first task: the profile-gated CPU store preflight change (D-44)** with its
small-capacity controls; SI registers, DMA, events, status, MI SI; PIF RAM; joybus
parser; controller info/reset/state/refusals; port-0 latch with the strictly-after
rule; input port field under v12; SI/PIF section and reset-audit rows; `pad-poll`
(with the CP0 Count witness, D-16); real-runner scripted poll; push and one hosted
pair; **the real collector/verifier and `ci.yml` steps land here, once, against the
first real hosted artifacts, and cross-platform comparison controls are built from
that real two-platform data**; measure warm and cold macOS time.
Requirements: SAVE-01, SI share of CORE-02.
Owning tests: `unit.core.cpu.si-store-budget`, `unit.core.si.registers`,
`unit.core.si.dma-timing`, `unit.core.si.status`, `unit.core.si.pif-command`,
`unit.core.joybus.parser`, `unit.core.joybus.fuzz-bounded`,
`unit.core.input.latch-partition`, `unit.core.si.refusals`,
`unit.core.si.atomic-queue-full`, `runner.pad-poll`, `controls.real.pad-wrong-value`,
`harness.phase09-collect-real`.
Authored fixtures: `pad-poll` (state, info, absent port, pak read, skip,
command-bit refusal, two-read re-execution) plus its wrong-value variant.
Cheap-first: one scripted poll through the actual runner returning the expected line
before any suite; literal frame and mask vectors before decoder breadth; the hosted
pair as soon as one case passes natively.
Frozen after (FH): qualifier, collector/verifier, `ci.yml`, scanner and inventory
logic; SI and joybus policy label and constants; input schema; controller mask layout.

**09-04 Save devices.**
Scope: `save_kind` selection, create-time device storage, EEPROM channel 4 (D-18 to
D-20, mirror default), SRAM bus and PI DMA both directions, channel-4 no-device
policy, unknown/unsupported/deferred refusals, `save_info`, save section and
reset-audit row.
Requirements: SAVE-02, SAVE-04 (device-side).
Owning tests: `unit.core.save.selection`, `unit.core.save.eeprom`,
`unit.core.save.eeprom-mirror-and-16k`, `unit.core.save.eeprom-busy-triple`,
`unit.core.save.sram`, `unit.core.save.refusals`, `runner.eeprom-rw`,
`runner.sram-rw`, `controls.real.save-wrong-byte`.
Authored fixtures: `eeprom-rw` (4 Kbit and 16 Kbit builds), `sram-rw`.
Cheap-first: info on each of the four profiles through the runner, then blocks.
Frozen after: save policy label and constants, device kind numbering.

**09-05 Save import/export and persistence.**
Scope: core save API, formats, check order, refusal matrix, bounded mutation test;
runner fd import and staged export through the 09-02 publisher, forwarding rows for
the new flags (ordinary plan content), result fields, privacy scan.
Requirements: SAVE-03, SAVE-04 (API-side).
Owning tests: `unit.core.save.import-export`, `unit.core.save.refusal-matrix`,
`unit.save.mutation`, `runner.save-roundtrip`, `controls.real.save-import-refused`,
`harness.upload-allowlist-save`.
Authored fixtures: `save-roundtrip`; literal transform vectors.
Cheap-first: a single valid import then export byte-compare before the matrix.
Frozen after: save API shape (provisional) and statuses.

**09-06 CORE-02 ordering matrix and state-content freeze.**
Scope: E3/E4 deliverables (D-36) including the required mutants and the Count-wrap
span, MI per-producer ties, quantum statement and spec text, device-spec updates,
the reset/digest/serialize audit table complete.
Requirements: CORE-02.
Owning tests: `unit.core.sched.cross-device`, `unit.core.sched.partition`,
`unit.core.sched.interleave`, `unit.core.sched.resource-limit-resume`,
`unit.core.sched.mi-producers`, `unit.core.sched.required-mutants`, doc-consistency
check (every tie has a label and a test).
Cheap-first: one pair across both insertion orders, then the matrix, then the
all-devices workload. Full gate at the end.
Frozen after (F1): the contents of every canonical section.

**09-07 Replay.**
Scope: `t64.replay` schema, parser and canonicalizer, op-log driver with
completed-tick boundary submission, recorder wrapping the accepted calls, identity
binding with separate refusals for profile, build and content, divergence report,
result-checker record, runner flags with their forwarding rows, mutation and fuzz
tests; build-independent goldens.
Requirements: CORE-05.
Owning tests: `unit.replay.format`, `unit.replay.mutation`, `unit.replay.partition`,
`unit.replay.at-matters`, `unit.replay.independent-original-run`,
`unit.replay.queue-full-divergence`, `runner.replay-mix`,
`controls.real.replay-mismatch-profile`, `controls.real.replay-mismatch-build`,
`controls.real.replay-mismatch-content`.
Authored fixtures: `replay-mix` (controller poll, EEPROM write, AI DMA; length
measured first).
Cheap-first: record then replay one short session before mutations.
Frozen after: replay schema v1 (provisional).

**09-08 State export and container.**
Scope: buffer-sink export from the same walker, container, observer-continuity
section, size-query then copy-out, T2 and T4, the runner export flags with their
forwarding rows, the real-runner state-size control under the raised FSIZE, scanner
rules for state files.
Requirements: CORE-04 (export).
Owning tests: `unit.core.state.export-layout`, `unit.core.state.idempotence-shape`,
`unit.core.state.non-interference`, `runner.state-export`,
`controls.real.state-export-fsize`.
Cheap-first: export one authored machine at one boundary and decode it with an
independent reference decoder before anything else.
Frozen after: container v1 (provisional).

**09-09 State import, invariants and the boundary matrix.**
Scope: validate/apply reader per section, per-device structural rules, **generated
field-use audit** (D-45), refusal matrix, fuzz target plus deterministic mutation
sweep (sampled offsets in the gate), T1/T3 with the named construction sources
(D-32), the stalled VI/AI event boundary, test-only digest recompute after apply,
runner import flags.
Requirements: CORE-04 (import).
Owning tests: `unit.core.state.roundtrip`, `unit.core.state.idempotence`,
`unit.core.state.continuation`, `unit.core.state.boundary-matrix`,
`unit.core.state.stalled-observer-stop`, `unit.core.state.refusal`,
`unit.core.state.mutation`, `harness.state-field-use-audit`,
`controls.real.state-import-refused`.
Cheap-first: round trip at one boundary, then the field-use audit script, then the
matrix, then the sweep.
Frozen after: importer rules (still reviewable in 09-10).

**09-10 Independent review, security audit and fix loop (FR).**
Scope: independent code review and a security audit of all three parsers and the
runner paths, **run against real mutated blobs from the 09-09 sweep as well as the
code**; one budgeted fix cycle; any resulting section or validation change
regenerates goldens. Findings are fixed here (or in a gap plan), never after F2.
Requirements: all.
Owning tests: the review's own named regression tests plus the full existing
inventory in the native lane.
Frozen after (FR): all logic.

**09-11 Identity freeze and qualification (F2).**
Scope: write literal pins once, claims, evidence document, one cohesive
qualification (native normal, ASan/UBSan, gcc:13, both hosted jobs on the exact
SHA); no logic edits. Success criteria 1-4 each map to a named test row and a claim
block.
Owning tests: `harness.phase09-qualification` and the full existing inventory.
Frozen after: everything; phase closeout.

## 5. Owner-level versus routine decisions

**Owner-level items: none are required to proceed.** Under AUTONOMY section 4 no
rights-bearing asset, physical hardware, PII or credential, destructive history
rewrite, paid service, oracle/VID-04/Plan 13 authority, D6 channel, D7 name, D2,
D4 or D5 stop is triggered, because every fixture, save, state and replay is
authored and local. Nothing here changes any of those stops.

Routine reversible defaults recorded with rationale (do not ask): every decision
D-1..D-46, including the new profile family and four ids, FIFO retention,
latch semantics, all timing constants, the EEPROM 6 ms ceiling, the 4 Kbit mirror, SRAM size and
initial contents, save formats, replacement-clears-save, import validation without
staging allocation, exact-build binding with cross-build import refused,
replay `cold-admit` only, text replay format, the artifact-dir FSIZE mechanism and
its ceiling, the second narrow publisher, the plan split to eleven plans, the docs-only
CI lane (stays declined, F-099), and the five fixtures. The adversarial review found
no genuine owner stop either (`09-DISCUSSION-ADVERSARIAL.md`, last section), and
re-verification here agrees.

Conditions that would become real stops (not triggered; do not pre-ask):
using any commercial ROM, real save, PIF/IPL or firmware to validate behavior;
any physical console measurement to settle a constant; committing or publishing a
state, save or replay derived from protected content, or the hash of a protected
ROM; any oracle execution. Hardware evidence is an optional later upgrade path for
the timing constants, not a Phase 9 requirement.

## 6. Non-goals and deferred items

- FlashRAM (EXP-04), Controller Pak, Rumble Pak and Transfer Pak (EXP-05), RTC
  (EXP-06), 64DD (EXP-07): refused with defined postconditions only; no stubs that
  appear to work; device kinds reserved.
- Banked or larger SRAM (768 Kbit, 1 Mbit), SRAM address mirroring, save
  containers from other emulators, header or database save detection, automatic
  persistence, any real save as a fixture.
- Ports 1-3 hardware behavior (absent only; schema ready), hot-plug, stick
  shaping, RST/recentering, per-channel sequential sampling, direct CPU PIF RAM
  access, WR4B/RD4B, CIC challenge, PIF ROM execution, mouse, keyboard, VRU, link
  cables, physical controller mapping (Phase 11).
- Class-rank scheduler policy and Q > 1 RSP bursts (v2 performance milestone).
- Cross-build or cross-version state import, state-anchored replay, binary replay
  schema v2, compression, rewind and save-state UI (PLAY-01).
- Larger raw capture and the Phase 8 capture-limit follow-up beyond what the
  artifact-dir mechanism incidentally provides.
- API/ABI freeze, installed consumer and the C surface shapes (Phase 10). Every
  Phase 9 call signature, status, container and flag is provisional.
- Hardware accuracy, oracle or differential claims, VID-04, release (D6), naming
  (D7).

## 7. Risk register (top 8)

| # | Risk | Likelihood / impact | Mitigation | Early-warning signal |
| --- | --- | --- | --- | --- |
| R1 | Export or walker omits a future-affecting field; round trip passes but continuation diverges | M / H | one walker is both digest and export; tripwires; per-field mutation; T3 against replay digests; reset-audit row required per device | digest equal but a later digest differs; a struct field with no audit row |
| R2 | The v25 port drops or reorders a field (invisible to legacy pins and to T1/T2), or version-membership drift leaves a branch falling through at 12 | M / H | old-pin vectors first in 09-01; the strip-and-compare differential against the v24 stream; script-generated membership list; `sizeof/offsetof` tripwire; no equal fallback (a failure reopens the design) | any legacy pin, refusal control or differential mismatch; a `t64_has_*` branch missing 12 |
| R3 | Bound-source churn: qualifier, collector, `ci.yml`, scanner or supervisor logic edits after the native pass (about 15 minutes per cycle) | H / M | 09-02 owns the mechanism, 09-03 lands the real collector once against real hosted data, then FH; later flag-forwarding rows land before F2 and are qualified once; review fixes happen in 09-10 before F2 | a diff touching `scripts/qualify-*.py`, `ci.yml`, `check-*.sh` or `runner/t64_supervisor.c` logic after 09-03 other than a forwarding row, or any logic change after F2 |
| R4 | Joybus, EEPROM and SI facts rest on single community sources and invented constants; wording drifts toward hardware claims | H / M | every constant labeled [POL] in a policy label; unit-tested tier; a second-source check for button masks; no reserved words | a spec sentence with no label; a claim using reserved words |
| R5 | A crafted blob passes hashes but carries an unchecked index, length or loop bound (memory safety), or the review finds a hole after the freeze | M / H | contract is memory safety plus liveness; generated field-use audit; per-device rules derived from state machines; mutants that keep checksums valid; independent audit in 09-10 on real mutated blobs before F2 | an unmapped field in the audit; all mutants failing at the magic check; a refusal without a digest-unchanged assertion |
| R6 | Hosted time grows past the ten-minute limit: the cold-cache macOS run was 461-507 s, so warm figures mislead | M / M | budget against the cold figure (about +25 s macOS); measure warm and cold in the 09-03 hosted pair; native-only fuzz, ASan, soak and state-hash loops; sampled mutation offsets | macOS job over about 540 s cold or 400 s warm on any push |
| R7 | FSIZE raise loosens the child's backstop more than intended, or kills real-runner exports | M / M | option-driven, exact `artifact_max_bytes + slack`, report and capture still bounded; synthetic control in 09-02, real control in 09-08; backstop change documented | SIGXFSZ in the first export; limit raised when the option is absent; oversize report accepted |
| R8 | Rights or privacy leak: a save, state or replay tracked or uploaded; workstation paths in evidence | L / H (stop condition) | private by default; explicit upload lists; scanner rules with mutation controls; PII scan before evidence commits; repo-relative paths | scanner finding; a `.sav`, state or replay path in an upload list |

Secondary: the CPU store preflight change on a shared path (now a profile-gated first
task of 09-03, D-44); input-latch partition behavior
(`unit.core.input.latch-partition`); the stalled-observer stop point after import
(D-29); plan-count coordination cost across eleven plans.

## 8. Claim tiers and wording boundaries

Tiers come from `docs/claims/TEMPLATE.md`. The words "accurate", "compatible" and
"supports" are reserved for hardware-derived claims with a scope and must not
appear in any Phase 9 claim block or in a prose line starting with the project
name. No `hardware-derived` or `emulator-differential` claim is made. One file,
`docs/claims/phase09.md`, drafted as a skeleton in 09-02 with counts filled in
09-11 (`sh scripts/check-claims.sh --files docs/claims/phase09.md` is the
seconds-long local check; flag use unconfirmed).

| Requirement | Tier | Oracle kind | Wording boundary (what may be said) | Not claimed (must be listed) |
| --- | --- | --- | --- | --- |
| SAVE-01 | `unit-tested` (the harness, not the guest, holds the expected values, so not `self-checking-fixture`) | authored literal contract | An authored ROM reads the scheduled button and axis values for the standard controller on port 0 through the modeled SI/PIF joybus at the scheduled guest ticks of the model's named timing policy, under the named profile and build; the in-guest Count readings are a model-time witness, not hardware time | other ports, other peripherals, physical controllers, joybus timing against hardware, rumble, Controller Pak, commercial software |
| SAVE-02, SAVE-04 | `unit-tested`; `self-checking-fixture` only if the guest itself emits a pass/fail verdict through the verdict grammar | authored literal contract | Explicitly selected EEPROM 4 and 16 Kbit and SRAM follow the authored block, busy-window and refusal contracts; unselected, unknown and deferred device requests are refused with the stated postconditions | FlashRAM, cartridge save-chip timing, title-based save detection, battery or wear behavior, the 6 ms ceiling or the 4 Kbit mirror as measured hardware facts |
| SAVE-03 | `unit-tested` | authored literal contract | Exported save bytes round-trip with the named byte-order transforms; a refused import leaves the canonical digest unchanged | third-party save formats or containers, interchange with other emulators |
| CORE-02 | `unit-tested` | authored coincident/partition matrix | Device events are ordered by one guest-time scheduler under the authored coincident, partition and interleave cases, pinned as regression behavior and compared across schedules (a consistently wrong tie would pass); MI is ordered through its producers' events; the CPU-to-RSP quantum is part of profile identity and fixed at 1 | hardware event ordering, cycle accuracy, RSP/RDP timing against hardware; any tie result as hardware evidence; any statement about quantum values other than 1 |
| CORE-04 | `unit-tested` | authored state round-trip contract | Exported state re-imports and reproduces the canonical digest and continuation digests for the authored machines on the same build | cross-build or cross-version import, state from other emulators, hardware behavior |
| CORE-05 | `regression-pinned` for recorded-run replays, plus `unit-tested` for format and refusals | an earlier run of this project | A recorded input replay bound to profile, build and content hash reproduces the recorded checkpoint digests; the original run was driven from an independently authored input script, and committed goldens are build-independent literals recomposed at test time | portability across builds, platforms or profiles; frame-exact claims on hardware; equality of digests across different submission schedules |

Each block's `not_claimed` also names other builds, profiles and fixture sets,
behavior the fixtures do not exercise, and commercial software. Hosted Linux and
macOS evidence compares platform-invariant guest outputs and digests only; their
build ids differ by design. Pass counts are never copied from a plan.

## 9. Unknowns and least-settled points

1. **The v25 port (D-5).** The seam is cheap; the new walker is where an omission
   hides. The differential is the oracle; there is no equal fallback.
2. **Import without staging allocation and the per-device rules (D-30).** The
   memory-safety-plus-liveness contract is achievable, but the per-device structural
   rules and the field-use audit are unproven until written; the escalation path is a
   documented allocator-contract extension, not an owner stop.
3. **Joybus frame and EEPROM edge behavior and SI timing constants.** Single community
   sources (one of them internally inconsistent on the command-byte flag bits) and
   unmeasured constants; SRAM base address and size are not independently verified.
4. Smaller: whether `runner/t64_supervisor.c:584-589` input-size handling needs a
   structural change; exact state size (about 10 MiB is an estimate) and hosted
   hashing cost; whether the result record already prints the quantum; whether the
   physical hex values of the button masks match a second source; the exact
   builder names for the state boundary matrix.


## 10. Adversarial dispositions

Source: `.planning/phases/09-controller-save-media-and-replay/09-DISCUSSION-ADVERSARIAL.md`
(verdict PROCEED WITH CHANGES, no blocker). Each finding's code or source claim was
re-checked by the synthesizer before disposition. "Re-verified" below means checked
directly in the tree or re-fetched, not copied from the review. Dispositions: accept,
accept-modified, reject. No finding was rejected outright; four are accept-modified
with a stated difference from the review. No finding or re-check produced a genuine
owner-level stop (AUTONOMY section 4).

| ID | Disposition | Re-verification and reason | Where applied |
| --- | --- | --- | --- |
| H1 | **accept-modified** | Re-verified: `runner/t64_raw_capture.c` is 1,352 lines and packet/manifest-specific (`write_packet_file` `:558`, `append_manifest` `:690`, `valid_video_info` `:785`, `validate_stream_files` `:917`); supervisor option parsing is an `opts[]` table with bespoke per-flag handling and hand-built child argv (`runner/t64_supervisor.c:470-513,1496-1527`); the membership sweep is large (this grep: 105 model/state and 76 descriptor-version comparisons with a narrow pattern; the review's 207 and 39 use another pattern). Accepted: split into 09-01 (core) and 09-02 (runner/supervisor/publisher), no pre-built forwarding, publisher costed with CR-01 controls. Modified: the review left open "generalize or write a second publisher"; this proposal chooses a **second small module** and leaves `t64_raw_capture.c` untouched, overruling F's "no second publisher" (generalizing would touch the CR-01 locus and force re-qualifying Phase 8 controls) | sections 2 (conflict 6, item 7), 4, D-38 |
| H2 | **accept** | Re-verified in the repository record (`08-REVIEW.md` CR-01/WR-01 and `08-05-SUMMARY.md` attempt history are cited by the review; the structure argument is logical: findings after F2 are logic edits). Review and audit plan 09-10 added before F2; F2 moves into 09-11; audit runs on real mutated blobs from the 09-09 sweep; one fix cycle budgeted | sections 2 (freeze table), 4, D-41, D-46 |
| H3 | **accept** | Re-verified: `hash_u32` `:8`, `hash_u64` `:18`, `hash_event` `:29`, 49 direct `t64_sha256_update` lines in `core/t64_digest.c` (the review says about 63; the exact number does not matter); concatenation invariance makes the seam byte-identical by construction; the v25 port is the unguarded part. Strip-and-compare differential added to 09-01; O2 retired as a stated fallback (failure reopens design); tripwire kept; v25 not comparable to v24 documented | conflict 3, D-5, 09-01 |
| H4 | **accept** | Re-verified: queue capacity 256 (`core/t64_internal.h:14`); CPU exception path reserves another step (`core/t64_exception.c:91`) and re-reserve path (`core/t64_cpu.c:1552-1567`); PI canceled state; `packet_count` gates VI/AI dispatch (`core/t64_scheduler.c:310-336`, `core/t64_ai.c:384,416,487`) while excluded from the digest; lint rules `L1-NAME-ALLOC` and `L1-STATIC-MUTABLE` exist (`tools/lint/rules.txt:16,19`). Contract restated as memory safety plus liveness; generated field-use audit added; stalled VI/AI boundary and drop-observers stop-point test added; test-only digest recompute after apply (not a rollback). The review's "mutable file-scope state: none found" was not independently greped by the synthesizer beyond the lint rule | D-29, D-30, D-45, 09-09 |
| H5 | **accept** | Re-verified: the five authored ROMs (D-39) reach only SI/save/AI/PI states; RSP mid-program, RDP triangle words, VI stalls and CPU cache/TLB states come from in-process builders in `core/tests/` (file inventory checked; whether each builder reaches the exact state is **not** verified and each boundary carries a "reached" assertion). Construction source named per boundary, required set capped, plan split into 09-08 (export, container) and 09-09 (import, invariants, matrix) | D-32, section 4 |
| H6 | **accept** | Re-verified: `cpu_successor_extra_event_budget` returns 0 unless the store width is 4 and 1 for a 32-bit store to a non-RDRAM address (`core/t64_cpu.c:1483-1511`); `cpu_preflight_successor` needs `1 + extra` slots and sequence headroom (`:1513-1550`). An SI read-DMA start reserves two events. Profile-gated budget change is the first task of 09-03 with small-capacity and width-8 controls; the single-event alternative is rejected as default because a handler-time reservation failure has no refusal path after the store commits | conflict 7 item 8, D-44, 09-03 |
| M1 | **accept** | Re-verified by re-fetching n64brew Joybus Protocol: "up to 6 milliseconds", top two block bits ignored on the 512-byte part ("blocks 64-255 are repeats of the first 64"), busy `0x80`. 6 ms = 375,000 ticks at 62,500,000 ticks/s (`core/t64_vi.c:23`). Default flipped to the documented mirror; label renamed to a ceiling (`eeprom-busy-ceiling-6ms-v1`); the "permissive" argument withdrawn; the 15 ms (carried from B) and 30 ms figures treated as uncorroborated; hardware measurement stays an AUTONOMY section 4 stop described as such | conflict 5, D-19, D-20 |
| M2 | **accept-modified** | Re-verified: `save_kind`/`peripheral_flags` are only hashed (`core/t64_machine.c:1640-1641`), no validator reads them; `T64_SAVE_NONE` is the only constant. Modified: the review proposed refusing at descriptor creation on legacy versions; legacy **hashing** must stay unchanged because `core/tests/test_profile.c:152-153` pins a serialization vector with `save_kind = 7` (hash only, not created). So: descriptor 12 range-validates in `profile_fields_are_valid`; `t64_machine_create` refuses non-zero values on versions below 12; a grep found no other test or code creating a machine with non-zero values (`core/tests/test_boot_entry.c:651-652,1297-1298` only read or hash them). Allocator-counter and old-pin controls required | conflict 2 table, D-1, 09-01 |
| M3 | **accept** | Re-verified: `build_id` hashes compiler id/version/flags, build type and the SHA-256 of every core input (`core/CMakeLists.txt:44-66`). Build-independent literal goldens, fresh in-process recording for sanitizer/gcc:13, recorder wrapping the accepted calls, original run from an independently authored `t64.input` script, separate profile/build/content mismatch controls, `at`-mutation test | D-34, 09-07, section 8 |
| M4 | **accept** | Re-verified: FIFO plus Q = 1 satisfies the roadmap wording (no class rank required); the quantum contrast test uses Q = 2 on an older descriptor (`core/tests/test_profile.c:450-470`); MI is sampled at CPU step start (`core/t64_cpu.c:2175-2180`). Required mutants named, evidence limits stated in the claim, the untested "differ only for CPU-RSP races" sentence deleted (labeled [UNK]), explicit hash-sensitivity control added, MI worded as "ordered through its producers' events" | D-4, D-36, D-37, section 8 |
| M5 | **accept** | Re-verified: cold-cache run 464 s with gate-macos 461 s (`docs/ci-reviews/LOG.md:116`), warm 279 s (`:117`), Phase 8 binding macOS run 507 s (`08-05-SUMMARY.md:65`), limit 600 s. Budget set against cold (about +25 s macOS), both measured in the 09-03 hosted pair, state-hash-heavy loops native-only | D-40, R6 |
| M6 | **accept** | The contradiction was real (my Plan 01 froze harness logic while Plan 02 built the real collector). Stub-only in 09-02; real collector/verifier and `ci.yml` steps land once in 09-03 against the first real hosted data; the freeze table now states what is frozen when | conflict 2 freeze table, D-38, 09-02, 09-03 |
| M7 | **accept-modified** | Re-fetched n64brew PIF-NUS and Serial Interface. Confirmed: execution happens on SI DMA reads; run-mode 0x04/0x08 "joybus flag bits" change the TX-to-RX delay (about 7 us versus about 520 us), though the page also calls them "bits 0x02 and 0x03" (internally inconsistent); DMA_ERROR sticky until power reset. **Differs from the review:** the page's "reads while a write is in progress are correctly delayed" refers to direct reads of the PIF-mapped window, not DMA, so the source is silent about DMA-read-during-DMA-write; and DMA_ERROR is also documented as set "when writing to a misaligned address". Result: D-10 keeps refusal of unmodeled command-byte bits but as a named policy divergence made consistent with D-11 (unmodeled function versus malformed data); misaligned DRAM address now sets DMA_ERROR (documented); overlapping DMA stays refused with sticky error as a labeled policy | D-9, D-10, D-11 |
| M8 | **accept** | Re-verified: Count advances once per two guest ticks (`core/t64_cp0.c:580-590`). `pad-poll` prints CP0 Count per poll as an in-guest model-time witness; the exact-tick expectation is stated as policy self-consistency; Count is not presented as hardware time | D-16, section 8 |
| M9 | **accept** | Re-verified: current id is 55 of 63 characters, pinned at `runner/t64_real.c:40-41,488,681`. Scheme defined now: stem `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` (38) with suffixes `-e4k`, `-e16k`, `-s32k` (longest 43); id-length test over the profile and runner tables | D-43, conflict 2 table |
| L1 | **accept** | Re-verified: the limit is set once for the whole child (`runner/t64_supervisor.c:1534-1536`). Exact `artifact_max_bytes + slack`, controls that oversize report/capture stay refused, backstop change recorded in `docs/spec/raw-capture.md` | conflict 6 |
| L2 | **accept** | Re-verified: `docs/spec/machine-contract.md:270-300` still describes v5/v8 digests. Stale-table fix moved from the ordering plan to 09-01 | 09-01 |
| L3 | **accept** | Re-verified: `scripts/rights-scan.sh:567-597` already has denied-extension controls for `.sav` and near-misses; `fixtures/manifest.json:658-710` shows the required manifest fields including the IPL3-composition note. Extend the existing table, register tests in `scripts/check-test-inventory.sh`, add manifest rows with build-only identity | D-38 |
| L4 | **accept** | Source URL and fetch date per [COMM] fact, restated in authored words | D-42 |
| L5 | **accept** | Phase 8 ran about 25 hours first-to-last commit (`08-05-SUMMARY.md`: 2026-11-18 09:47 to 2026-11-19 10:23) with six native attempts; the earlier 32-51 hour figure is withdrawn as an under-count. Review/fix loop and at least two qualification cycles planned; nothing gated on estimates | section 4 |
| L6 | **accept** | Re-verified: the runner pre-queues up to 128 events at tick 0 (`runner/t64_input.h:11-12`, `runner/t64_real.c:1011-1155`) against a 256-entry queue. Recorder logs `at = 0` for them; a driver `QUEUE_FULL` is a divergence/refusal report, never silent | D-34 |

Also accepted from the review's other sections: the determinism trace result (the
Count-wrap span is added to the matrix, D-36); the coverage table gaps (each maps to
a row above); "Attacks that failed" are recorded as confirmations of D-2, D-3, D-4,
conflict 4 and the no-allocation import. The review's fact-label list is adopted:
two-source confirmed (EEPROM ids, busy bit, 6 ms ceiling), single-source confirmed
(PIF layout, escapes, flags, SI registers), not verified (SRAM base and 32 KiB,
blank contents, the 7 us versus 520 us delay layout, the 15 ms and 30 ms figures,
button bit positions).

**Owner-level result.** None. The review and the re-verification agree that every
change is a reversible engineering default under the autonomy policy.
