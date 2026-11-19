# Phase 9 fan-out, cluster B: save media (EEPROM 4/16 Kbit, SRAM)

Status: research recommendation for adversarial check and synthesis. Not an owner decision.
Requirements: SAVE-02, SAVE-03, SAVE-04 (roadmap success criterion 2). FlashRAM (EXP-04),
Controller Pak/Rumble/Transfer Pak (EXP-05), RTC and 64DD are out of scope.

Evidence labels used below: **[HW-DOC]** hardware/SDK document fact, **[COMMUNITY]**
community hardware documentation (single-source or unstable), **[CODE]** observation of this
repository, **[REF]** another emulator or library read as behavior reference only (nothing copied;
licenses noted), **[INFER]** inference, **[POLICY]** named software-profile choice, **[UNKNOWN]**.

## Sources consulted

- Nintendo SDK manual, EEPROM section (archived mirror):
  https://ultra64.ca/files/documentation/online-manuals/man-v5-2/allman52/pro-man/pro26/26-06.htm
  (4k = 512 B = 64 blocks x 8 B; 16k = 2048 B = 256 blocks; "about 15 ms" for a write to reach the
  cell; consecutive writes need 15 ms or `osEepromWrite` polls; `osEepromProbe` 0/1/2; -1 for
  out-of-range block is a library return value.)
- n64brew Joybus Protocol: https://n64brew.dev/wiki/Joybus_Protocol (retrieved 2026-11-19; page
  stamped last edited 5 Oct 2026). Info 0x00 -> 3 bytes (2-byte id + 1 status byte); id 0x0080 =
  4 Kbit, 0x00C0 = 16 Kbit; status bit 0x80 = write in progress; read 0x04 = tx 2 / rx 8; write
  0x05 = tx 10 / rx 1, reply 0x80 if already busy else 0x00; "top two bits of block number are
  ignored" on the 512-byte part so blocks 64..255 repeat 0..63. **The write-time sentence is
  unstable across revisions**: this retrieval says "up to 6 milliseconds"; a search snippet of an
  older revision says "up to 30 milliseconds"; the SDK says ~15 ms. See D-B5.
- n64brew PIF-NUS: https://n64brew.dev/wiki/PIF-NUS . Joybus channel 4 is the cartridge bus (EEPROM);
  RX byte flags: 0x80 no device, 0x40 timeout (also for an unrecognized command or asking for more
  bytes than the device returns); TX/RX length bytes 0x01..0x3F / 0x00..0x3F.
- n64brew Peripheral Interface: https://n64brew.dev/wiki/Peripheral_Interface . 0x08000000-0x0FFFFFFF
  is PI domain 2 (SRAM); domain-2 timing registers apply; "direct I/O accesses can only be done as
  32-bit words". Page does not specify SRAM size, mirroring, or per-device timing values.
- n64brew ROM Header: https://n64brew.dev/wiki/ROM_Header . The official header has no save field
  (byte 0x3F is the ROM version). Only a homebrew "Advanced Homebrew ROM Header" ("ED" at 0x3C,
  save type in the high nibble of 0x3F) carries one. Emulators otherwise use checksum databases.
- Reference only: ares (ISC) `ares/n64/cartridge/joybus.cpp`, `cartridge.hpp`; mupen64plus-core
  (GPL-2.0-or-later) `src/device/cart/eeprom.c`; libdragon (Unlicense) `src/joybus/eeprom.c`;
  cen64 `si/controller.c` seen only as a search snippet (not read directly, license not checked).
  Behaviors learned are listed per decision; no code or text was copied.
- Repository: `AGENTS.md`, `.planning/AUTONOMY.md` section 4, `.planning/REQUIREMENTS.md`,
  `.planning/research/{ARCHITECTURE,FEATURES,PITFALLS,SUMMARY}.md`, `docs/spec/machine-contract.md`,
  `docs/spec/cpu-compatibility.md`, `docs/spec/cpu-profile.md`, `core/t64_bus.c`, `core/t64_pi.c`,
  `core/t64_rom.c`, `core/t64_digest.c`, `core/include/tendies64/t64_machine.h`,
  `core/t64_machine.c` (profile table), and the Phase 8 proposal.
- No ROM, save, PIF/IPL or firmware file was downloaded or used.

## Repository facts that shape the decisions [CODE]

1. Profile identity is a registered immutable `t64_profile_descriptor` with
   `descriptor_version / machine_model_version / state_serialization_version` and per-feature policy
   text fields (`ai_policy` is "Descriptor v11 field", `core/include/tendies64/t64_machine.h:195`,
   `core/t64_machine.c:1031-1034`, serialization gate at `core/t64_machine.c:1741`). The newest
   profile is `ntsc-cart-4m-openipl3-rdram-b-t1-rdp-vi-filter-v9-ai-v1` (descriptor 11, model 19, state 24).
   Older profiles keep their hashes and behavior.
2. The allocator callbacks run only at create/admit/replace/destroy (`docs/spec/machine-contract.md`
   "Host allocator contract"). Save storage therefore has to be sized at creation, which a
   profile-selected device allows and a runtime selection would not.
3. Domain 2 (`0x05000000-0x06000000`, `0x08000000-0x10000000`) is `T64_BUS_REGION_UNSUPPORTED`
   (`core/t64_bus.c:104-108`). Cartridge ROM is `[0x10000000, 0x1fc00000)`, read-only.
4. PI DMA: only write-length (cart -> RDRAM) works and only from the admitted ROM; read-length
   (RDRAM -> cart) is latched and refused (`core/t64_pi.c:209-210`; contract "Only PI write-length
   starts work"). Duration is the profile equation `1 + N/2` ticks, domain latches do not affect it,
   and data is copied atomically at the completion event. Guest tick = 62,500,000 per second
   (`docs/spec/audio-interface.md:55`).
5. ROM admission pattern to mirror for saves (`docs/spec/machine-contract.md`, `core/t64_rom.c`):
   validate length and checked address interval before reading input, normalize endianness into a
   separate copy, commit once, refusals leave digest/state/trace unchanged. Header bytes "never
   choose ... save media" (contract text). `0x3F` is parsed as the ROM version.
6. Replacement reset audit table (`docs/spec/machine-contract.md` "Replacement reset snapshot") must
   gain a row for every new future-affecting state; the digest is a field-by-field SHA-256
   (`core/t64_digest.c`) and observer-only state (trace, drain cursor) is excluded.
7. `t64_machine_queue_input` exists and is explicitly not consumed by SI/PIF yet (contract "timed input").
8. Runner is fd/verified-input based with a staged no-clobber capture module
   (`runner/t64_raw_capture.c`, `runner/t64_verified_input.c`); the core never touches files.

---

## Decision points

### D-B1. How a profile selects a save device

- **Options.** (a) New descriptor field `save_policy` and one registered profile id per device;
  (b) one profile plus a runtime `t64_machine_select_save_device()` call after create; (c) extra
  `t64_machine_create` argument; (d) derive from ROM header/title/database.
- **RECOMMENDED: (a).** Descriptor v12 adds a text field `save_policy` (values
  `none-v1`, `eeprom-4kbit-v1`, `eeprom-16kbit-v1`, `sram-32kbyte-v1`) plus numeric
  `eeprom_write_window_ticks` (see D-B5). Register four profiles sharing one new base
  (the same descriptor/model/state increment that cluster A's SI/PIF work needs; B must not claim its
  own increment): `...-si-v1` (policy `none-v1`, the base), and `...-si-v1-eeprom4k`,
  `...-si-v1-eeprom16k`, `...-si-v1-sram32k` (naming is illustrative; planner picks final ids). All
  variants share model and state numbers; the device kind is encoded in the state/digest, so the
  profile hash differs per variant. Pre-existing profiles gain no save device and keep identity,
  digest and refusals byte-for-byte (`PITFALLS.md` pitfall 4: "an unknown save device becomes
  accepted by default" is the detection signal).
  Header, title, game code, checksum and file name are never inputs. Selection and header cannot
  "disagree" because the header is not consulted; an authored ROM carrying an ED-style save nibble in
  `0x3F` is a required negative control (device stays as the profile selects, header version
  observation is unchanged).
- **Evidence.** [CODE] items 1, 2, 5 above; [COMMUNITY] ROM Header page (no official save field,
  databases used elsewhere); `AGENTS.md` ("Do not infer from a title string"), `EMULATOR-DNA.md` line 31;
  `docs/spec/cpu-compatibility.md` (profile = separately versioned identity; old profiles retained).
  [REF] ares loads the save from a "pak" manifest keyed by an external database (a guess-from-content
  design we deliberately avoid).
- **Failure modes.** Profile proliferation (4 ids; each new device or size multiplies it). Runner and
  replay must carry the profile id (D replay binds it already). A guest probing a profile without the
  device must see "no device" (D-B7), not a crash.
- **Authored evidence.** (1) For each profile, lookup + hash distinct and `save_policy` serialized;
  independent reference encoder for the descriptor v12 bytes (project pattern). (2) Existing profiles'
  hashes and golden digests unchanged. (3) ED-header negative control above. (4) A request for device X
  on profile Y (D-B9) is refused. A mutation that ignores `save_policy` or reads header 0x3F must fail
  (3) and (4).
- **Would change it.** Owner wanting runtime-selectable media for a player UI: then (b) with the
  selection becoming digest-bound mutable state and storage allocated at admit; that is a later
  facade concern (Phase 10 public contract), not Phase 9.
- **Reversibility.** High: profile ids are additive. Merging variants later requires only a new profile.

### D-B2. EEPROM identity, command set and transaction shape (channel 4)

- **Options.** (a) Exact-length joybus commands info/read/write; (b) lenient lengths like ares
  (`send >= 2`, partial writes of `send-2` bytes); (c) also accept 0xFF "reset/identify".
- **RECOMMENDED: (a) exact lengths, and 0x00 only for Phase 9 (0xFF accepted only if cluster A's
  controller channels define 0xFF identically and then with identical reply; otherwise no reply).**
  | Command | tx | rx | Reply | State effect |
  | --- | --- | --- | --- | --- |
  | 0x00 info | 1 | 3 | `00 80`+status (4k) or `00 C0`+status (16k); status `0x80` if busy else `0x00` | none |
  | 0x04 read | 2 (cmd, block) | 8 | 8 bytes of the block, only when idle | none |
  | 0x05 write | 10 (cmd, block, 8 data) | 1 | `0x00` accepted, `0x80` already busy | on accept: commit 8 bytes, start busy window |
  Any other command, any other tx/rx on a recognized command, or a read while busy: no device reply,
  PIF RX flag 0x40 (timeout) and zero bytes written to the reply area, device state unchanged.
  With no EEPROM selected (none or SRAM profile): every channel-4 command yields flag 0x80 (no device),
  so a guest's `osEepromProbe`-style info sees "absent".
  Data of an accepted write is committed to the device bytes at acceptance (visible to export
  immediately); only the busy window governs later commands.
- **Evidence.** [COMMUNITY] n64brew Joybus (ids, lengths, status bit, 0x80/0x00 write reply) and
  PIF-NUS (flags, timeout on unrecognized command or over-long rx); [HW-DOC] SDK (8-byte blocks,
  polling before write); [REF] libdragon polls the identify status bit 0x80 before each op and asserts
  write reply 0x00 (it never reads or writes while busy); ares returns the same ids/status, replies
  0x80 to a write while busy and fills 0xFF for a read while busy; mupen64plus has no busy model and
  ignores out-of-range blocks. [POLICY] exact lengths, no-reply on read-while-busy.
- **Failure modes.** A guest that reads during busy sees a timeout instead of data (hardware behavior
  here is [UNKNOWN]; the wiki is silent). ares' 0xFF-fill is the alternative; the recommendation prefers
  a distinguishable failure over silently returning wrong data. A guest sending an rx length different from the
  model's expectation sees no device; the SDK uses exact lengths, so conforming software is unaffected.
- **Authored evidence.** A guest ROM (authored, via cluster A's PIF-RAM path) issues each command with
  literal expected reply bytes in a table independent of the implementation: info on 4k and 16k;
  read of blocks written earlier; write reply 0x00; second write at tick+1 replies 0x80 and leaves
  the first block's bytes unchanged (compared through export); read at tick+1 yields timeout flag and
  an untouched reply buffer (pre-filled sentinel); info while busy returns status 0x80; wrong lengths
  return timeout with unchanged digest. Negative controls: swapping 4k/16k ids, replying 0x80 on
  success, and accepting writes during busy each must fail a named assertion.
- **Would change it.** Hardware measurement of read-during-busy (owner-held hardware stop) or a primary
  Nintendo document; evidence that retail software issues non-exact lengths.
- **Reversibility.** Medium: reply semantics are in the profile policy text (`eeprom-protocol-v1`);
  changes need a new profile.

### D-B3. EEPROM block addressing and range refusal

- **Options.** (a) Refuse a block number outside the device (no reply, no state change); (b) mirror per
  the wiki ("top two bits ignored" on 4k, so block 64..255 -> block & 0x3F); (c) ignore silently like mupen64plus.
- **RECOMMENDED: (a).** 4k: blocks 0..63 valid; 64..255 get no reply (timeout flag), no state change,
  no busy window. 16k: all 256 u8 block numbers valid (nothing to refuse at the joybus level). The SDK's
  "-1 address out of range" is a library-side check and never reaches the wire [HW-DOC], so the core
  must define its own wire behavior. Mirroring is recorded as the documented alternative
  `eeprom-4k-block-mask-v1`.
- **Evidence.** [COMMUNITY] single-source mirror claim (n64brew Joybus). The repo rule
  (`AGENTS.md`, `AUTONOMY.md` evidence) is that dated research must be independently checked before it
  becomes a support claim; refusal is the repository's default for unmodeled edges (unsupported windows).
  Real software cannot observe the difference (the SDK range-checks first).
- **Failure modes.** If the hardware really mirrors and a title relies on it (very unlikely), the
  refusal would break it and surface as a no-reply, not silent corruption.
- **Authored evidence.** 4k: write block 0x41 with a block-unique pattern; assert timeout, assert block
  0x01 and every other byte unchanged and no busy window. 16k: write block 0x41 and 0x01 with distinct
  patterns; assert both exist (kills an accidental 6-bit mask on the 16k device). Block patterns carry the
  block index in each block so mirrored/aliased blocks cannot coincide.
- **Would change it.** An authoritative statement or hardware measurement of the 4k mirror.
- **Reversibility.** High (new profile policy value).

### D-B4. EEPROM busy window, status reporting and what happens during it

- **Options.** (a) Deadline stored in device state, busy derived by comparing guest tick, no scheduler
  event; (b) scheduler event that clears a busy flag (as PI/SP do); (c) no busy model (mupen64plus).
- **RECOMMENDED: (a).** On an accepted write at guest tick T (the tick at which cluster A executes
  the channel-4 handshake), `busy_until = T + eeprom_write_window_ticks` using checked 64-bit
  arithmetic (overflow: refuse the write before any mutation, no reply, no change). The device is busy
  while `tick < busy_until` and idle at exactly `busy_until`. Because nothing is queued, there is no
  queue-exhaustion path, no interrupt, partition and interleave invariance is by construction, and the
  deadline is canonical state. During the window: info reports status 0x80; write replies 0x80 and is
  discarded (window not extended); read gets no reply (D-B2). `t64_machine_save_info` reports busy and
  `busy_until` as host observation.
- **Evidence.** [COMMUNITY] 0x80 status bit and write reply; [REF] libdragon polls that bit, ares models
  a busy flag with a queued clear. [CODE] PI/SP use reserve-then-commit events because they must
  produce completion work (copy, interrupt); EEPROM busy produces none, so an event would add a failure
  path (`T64_STATUS_QUEUE_FULL`) for no observable benefit. CORE-02 ordering is unaffected because the only
  observation is a comparison at command-execution ticks.
- **Failure modes.** CORE-02 reviewers may expect every device in the scheduler; the answer is that the
  deadline compare is order-independent (include it in the equal-tick matrix: command at `busy_until-1`,
  `busy_until`, `busy_until+1`). Cluster E must supply a defined "tick of handshake execution".
- **Authored evidence.** Boundary triple above, in one-shot and 1/7/31-tick partitions and two interleaved
  machines (the established method); a guest poll loop that counts polls until status clears, expected count
  derived independently from the window and the poll loop's measured cycle cost (or terminate on first idle
  and assert the observed tick is within [busy_until, busy_until + one poll]). Digest differs mid-window vs
  after (the deadline is a digest field); mutating the deadline changes the digest (project field-mutation test).
- **Would change it.** A requirement that EEPROM completion raise an interrupt (it does not on hardware
  [INFER: no documented EEPROM interrupt]) or a PIF model that schedules handshakes asynchronously.
- **Reversibility.** High.

### D-B5. Window duration (evidence-backed vs software policy)

- **Options.** 6 ms (current n64brew; ares), 15 ms (SDK software wait), 30 ms (older n64brew revision).
- **RECOMMENDED: 6 ms = 375,000 guest ticks at 62.5 MHz**, held in descriptor field
  `eeprom_write_window_ticks` and labeled **[POLICY]** `eeprom-write-window-6ms-v1`; not a measured value.
  Rationale: the SDK's documented guidance is that writes need ~15 ms and that its own helper waits that
  long, so a guest that honors the SDK wait or polls the status bit passes with any window <= 15 ms; the
  smaller window is the more permissive of the plausible values and disturbs only a guest that neither
  waits nor polls. The 30 ms figure contradicts the SDK helper's 15 ms wait and is treated as unreliable
  until a source is found. The three sources disagree, so the conflict is recorded rather than resolved.
- **Evidence.** SDK page (15 ms); n64brew retrieved 2026-11-19 (6 ms; older revision snippet 30 ms);
  ares uses 6 ms [REF]; libdragon polls, no constant [REF].
- **Failure modes.** A hardware-measured value above 15 ms would make the SDK wait insufficient, which would
  have been a known SDK bug, so that outcome is unlikely [INFER]. Authored tests that assert a precise window
  are self-consistency tests of the policy, not accuracy evidence; the document and results must say so.
- **Authored evidence.** Boundary triple (D-B4) with the literal 375,000. A mutation using 15 ms or 30 ms fails.
- **Would change it.** Hardware measurement (owner stop) or an authoritative document. Any change is a new
  profile id and descriptor value, never an edit of an existing profile.
- **Reversibility.** High.

### D-B6. SRAM (32 KiB) mapping, access, timing, wrap and unmapped size

- **Options.** (a) 32 KiB device at `[0x08000000, 0x08008000)`, other offsets in domain 2 stay unsupported;
  (b) mirror 32 KiB across the whole 256 MiB window (ares masks to the size for devices <= 32 KiB);
  (c) support banked 96 KiB (768 Kbit) and 128 KiB (1 Mbit) variants.
- **RECOMMENDED: (a), with access paths PI DMA in both directions (required) and CPU/host bus
  32-bit aligned word reads and writes (cheap, include), byte/halfword refused as
  `T64_STATUS_BUS_ALIGNMENT`-class device refusal.**
  - Mapping: offsets `0x0000..0x7FFF` valid. Addresses `0x08008000..0x0FFFFFFF` remain
    `T64_BUS_REGION_UNSUPPORTED` as today; other domain-2/1 windows unchanged. No mirroring and no
    banking in Phase 9.
  - PI DMA: extend `core/t64_pi.c` request validation so the cartridge side may be the SRAM window when
    the profile selects SRAM. `PI_WR_LEN` (SRAM -> RDRAM) and `PI_RD_LEN` (RDRAM -> SRAM) both work, request
    must lie wholly inside the 32 KiB, same even-length/alignment rules and same `1 + N/2` duration
    equation (policy `sram-dma-duration-same-as-rom-v1`; domain-2 latches readable but non-functional,
    as for domain 1 today). Source is read and destination written atomically at the completion event,
    matching the ROM policy; the active descriptor identifies direction and target kind so a stale/canceled
    completion skips the copy. ROM profiles and profiles without SRAM keep refusing read-length starts.
  - Initial content (policy): EEPROM all `0xFF` (erased-cell inference, and mupen64plus formats new EEPROM
    to 0xFF [REF]); SRAM all `0x00` (SRAM power-up content is [UNKNOWN]/physically undefined; chosen so
    digests are deterministic). Never auto-loaded from anywhere.
- **Evidence.** [COMMUNITY] n64brew PI page: SRAM in domain 2 at 0x08000000, direct access 32-bit only;
  size and mirroring "not covered"; [CODE] items 3 and 4; [REF] ares maps `0x08000000..0x0FFFFFFF`,
  masks the offset for devices <= 32 KiB (mirrors), and uses a 0x40000-stride bank layout above 32 KiB.
  Mirroring is a plausible consequence of unused address lines [INFER] but unverified, so refusal is preferred.
- **Failure modes.** Titles that touch SRAM above 32 KiB (banked 768 Kbit/1 Mbit games) fail: they are a
  separate later device kind, not a silent partial. Direct CPU byte/halfword SRAM access is refused (the
  existing bus handles refusals; the CPU surfaces the documented unsupported stop). PI-DMA visibility
  differences (hardware reads RDRAM progressively) are a documented simplification.
- **Authored evidence.** Authored ROM: DMA a 512-byte block-unique pattern RDRAM -> SRAM at offsets 0 and
  0x7E00, DMA back to a different RDRAM region, compare; assert the data was not visible at completion-1 and
  is at completion (visibility boundary, project style); DMA spanning offset 0x8000 refused with unchanged
  digest and a latched-raw-register-only effect per the PI refusal rule; address 0x08008000 refused;
  word write/read round trip; byte write refused. Non-SRAM profiles: read-length still refused, SRAM window
  still unsupported. Mutation checks: a mirror implementation must fail the 0x08008000 assertion; ignoring
  direction must fail the RDRAM->SRAM leg.
- **Would change it.** Evidence of hardware mirroring plus a target title needing it; owner scope for banked SRAM.
- **Reversibility.** High (new `save_policy` value for other SRAM sizes).

### D-B7. Channel 4 with no (or non-EEPROM) device

- **RECOMMENDED.** Profiles without EEPROM (including SRAM-only and `none-v1`) answer every channel-4
  handshake with the PIF "no device" flag (0x80) and write nothing into reply bytes; RTC commands
  0x06..0x08 on an EEPROM profile are unsupported (timeout flag, no state change).
- **Evidence.** [COMMUNITY] PIF-NUS flags; [REF] ares returns invalid for all commands without an EEPROM.
- **Failure modes.** Cluster A must route by channel and report these flags. Overlap with A's generic
  "no device" handling: B only declares the policy; A owns the framing.
- **Authored evidence.** Each non-EEPROM profile: info on channel 4 returns the no-device flag; digest unchanged.
- **Reversibility.** High.

### D-B8. Import/export formats and byte order

- **Options.** (a) Only the raw device image in chip byte order; (b) raw plus explicitly named
  transforms (16-bit pair swap, 32-bit word reversal) selected by the host; (c) autodetect; (d) accept
  other emulators' containers (combined RetroArch `.srm`, DexDrive, padded files).
- **RECOMMENDED: (b), no autodetect, no containers.** Core formats:
  `T64_SAVE_FORMAT_RAW` (the byte sequence as joybus blocks / PI bus present it: EEPROM block n at byte
  8n..8n+7, SRAM byte k at address 0x08000000+k; the canonical stored order), `T64_SAVE_FORMAT_SWAP16`
  (each aligned byte pair exchanged), `T64_SAVE_FORMAT_WORDREV32` (each aligned 4-byte word reversed),
  mirroring the ROM order taxonomy already in `core/t64_rom.c`. Saves are headerless, so every byte string is
  valid in every format and the format cannot be inferred; the caller must name it, default RAW. Exact
  lengths only: 512 (4k), 2048 (16k), 32768 (SRAM). No padding, truncation, trailing data, or combined
  files; other sizes (including a 2048-byte file offered to a 4k device, a common 4k-in-16k habit) are refused
  with a size-mismatch status. Export applies the same transform; the core never writes a container.
  Which emulator uses which swap is not asserted by this project: a host that wants another emulator's
  container converts outside the core.
- **Evidence.** [REF/COMMUNITY] ecosystem confusion: converter READMEs conflict on whether N64 SRAM needs a 16-bit
  pair swap for RetroArch/Project64 interchange (ra_mp64_srm_convert releases disagree); this supports
  (1) never autodetecting and (2) not claiming a mapping. [CODE] the ROM normalization pattern, exact
  length checks and "no padding".
- **Failure modes.** A user imports with the wrong format and the guest sees byte-scrambled data without
  an error (unavoidable for headerless data; the runner must require an explicit flag for non-RAW and log the
  chosen format in its metadata). Two transforms increase the test matrix; if the owner prefers minimal
  scope, RAW-only is acceptable for Phase 9 and the transforms can follow, but then SAVE-03's "endianness
  handling" is thinner than the requirement text suggests.
- **Authored evidence.** Hand-computed literal vectors: 8 bytes `01 02 03 04 05 06 07 08` ->
  SWAP16 `02 01 04 03 06 05 08 07`; WORDREV32 `04 03 02 01 08 07 06 05`. Round-trip import-then-export for
  each format and device with a pattern containing no palindromic or all-equal words. Cross-checks: SWAP16
  vs WORDREV32 produce different guest-visible bytes for the same input; export of a guest-written block under
  each format. Mutation: treating SWAP16 as WORDREV32 fails the literal vectors.
- **Would change it.** Owner wanting interchange containers; evidence of a dominant real-world swap convention.
- **Reversibility.** High (formats are enumerated API values; adding one is additive).

### D-B9. Import/export API shape and refusal postconditions (SAVE-03, SAVE-04)

- **RECOMMENDED shape** (provisional C, matching the existing header style):
  ```
  typedef struct { uint32_t struct_size; uint32_t device_kind; uint32_t size_bytes;
                   bool busy; uint64_t busy_until_tick; uint64_t write_generation; } t64_save_info;
  t64_status t64_machine_save_info  (const t64_machine*, t64_save_info *out);
  t64_status t64_machine_save_export(const t64_machine*, uint32_t device_kind, uint32_t format,
                                     uint8_t *out, size_t capacity, size_t *length_out);
  t64_status t64_machine_save_import(t64_machine*, uint32_t device_kind, uint32_t format,
                                     const uint8_t *bytes, size_t length);
  ```
  `device_kind` enumerates `EEPROM_4K=1, EEPROM_16K=2, SRAM_32K=3` as supported, `FLASH=4` and other named
  accessories as known-unsupported, everything else unknown. The caller always names the kind it believes
  it is moving; the core compares it to the profile's device (this makes SAVE-04 testable and prevents a
  silent cross-device import).
  - Ownership: host provides/receives bytes; the core copies into/out of its own profile-sized device
    storage, allocated at create (no allocator call on import/export, preserving the allocator contract);
    the core never opens files or retains the span. Caller spans must be disjoint from machine storage and
    each other (existing overlap checks). Size caps: the device size is the only accepted length, max 32,768.
  - Order of checks (all before any read of `bytes` or any mutation): machine admitted (`NOT_READY`), pointer
    and length range, profile has a device, `device_kind` known, `device_kind` supported by core,
    `device_kind` equals profile device, `format` known, `length` equals device size, no save-affecting
    work in flight (see below). Only then transform-copy and commit. Because every failure occurs before the
    copy and the copy cannot fail, no staging buffer is needed and atomicity is structural (state this in the
    spec; a test hook that injects a failure after validation is not needed).
  - New statuses (additive): `T64_STATUS_SAVE_UNKNOWN_DEVICE`, `T64_STATUS_SAVE_UNSUPPORTED_DEVICE`,
    `T64_STATUS_SAVE_DEVICE_MISMATCH`, `T64_STATUS_SAVE_BAD_FORMAT`, `T64_STATUS_SAVE_SIZE_MISMATCH`,
    `T64_STATUS_SAVE_BUSY` (alternatively reuse `INVALID_ARGUMENT`, `BUS_DEVICE_REFUSED`; distinct codes make tests
    sharper and cost little). Export too-small buffers follow the `rom_header` convention (`BUFFER_TOO_SMALL`
    with required length, output untouched; NULL/zero capacity queries the length).
  - **Refusal postcondition (all refusals, import and export):** canonical digest, device bytes, EEPROM
    busy deadline, PI/SP/scheduler state, tick, content digest, observation counters, trace ring and
    `write_generation` are unchanged; the caller's `out` bytes are unchanged on export refusal; no allocator
    call; no trace record (host-boundary refusals do not emit device trace). Import while the EEPROM is busy
    or an SRAM PI DMA is in flight is refused (`SAVE_BUSY`) so import cannot change an in-flight completion;
    export is allowed at any time and returns committed bytes.
  - Success postconditions: device bytes equal the format-normalized input; busy deadline cleared (an import
    is not a guest write; EEPROM becomes idle); digest changes iff bytes or busy changed; guest time and
    queue unchanged; `write_generation` increments (observer only).
  - `write_generation` is an observer-only monotonic counter (excluded from the digest, like the drain cursor)
    that increments on any guest write that changes device bytes and on successful import. It lets a host
    decide when to flush without polling bytes; persistence itself is entirely the host's (D-B12).
- **Evidence.** [CODE] ROM admission and `rom_header` conventions; `ARCHITECTURE.md` "Save bytes are canonical
  in-memory state; host persistence is explicit typed I/O, never an automatic flush"; `PITFALLS.md` pitfall 6
  (staged import, full digest compare after every refused import).
- **Failure modes.** The "caller names the device" rule is extra API surface; it is justified because a
  byte span alone cannot identify a device (saves are headerless). `SAVE_BUSY` could surprise a host that
  imports mid-run; hosts import right after admission (D-B10).
- **Authored evidence.** Table-driven refusal matrix, one row per check in the stated order, each asserting
  the full postcondition list by comparing digest, a byte readback via export, scheduler queue count, tick,
  and the generation counter before and after: wrong length (+1, -1, 0, 4k image to 16k profile and the
  reverse), unknown kind (0, 99), FlashRAM kind, kind/profile mismatch, bad format, NULL with nonzero length,
  span overlapping machine storage, import during busy, import before admission, import on `none` profile.
  Order tests: a request that fails two checks reports the earlier status (so the order is pinned). Positive
  control: the same matrix with one valid row succeeds so the harness can fail. Randomized/fuzz target for
  length/kind/format combinations under ASan/UBSan (STACK.md already plans libFuzzer for decoders).
- **Would change it.** Cluster C or Phase 10 imposing a single "typed blob" import API; then this is the
  save-media section handler behind it.
- **Reversibility.** Medium: the C surface is explicitly provisional (contract header); statuses are additive.

### D-B10. Reset and content-replacement semantics

- **Options.** (a) Replacement clears save media to its initial blank content and clears busy;
  (b) replacement preserves save bytes (like a console power cycle with the same cartridge).
- **RECOMMENDED: (a).** Successful `t64_machine_admit_rom` replacement restores the device to the
  profile's blank content (EEPROM 0xFF, SRAM 0x00), clears the busy deadline and `write_generation` and
  keeps the device kind (profile). The host imports after admission, exactly as it admits then runs. This
  preserves the invariant already stated by the replacement table: post-replacement state equals a fresh
  machine plus admission, so a replacement digest is history-independent, and a save from game A can never
  leak into game B or into a replay. Refused admission leaves everything (including save bytes) unchanged
  per the existing refusal rule. A new row is added to the reset audit table. Import requires an admitted image
  (consistent with every other observation returning `NOT_READY` beforehand).
  If cluster C/D later adds a same-content reset or state import, hardware reset does not erase nonvolatile
  save media, so that operation (not replacement) preserves bytes [INFER]; busy deadline handling on such a
  reset is [UNKNOWN] and should be specified when it exists.
- **Evidence.** [CODE] reset audit table and "Successful content replacement ... resets guest tick to zero and
  clears RDRAM"; fresh-equivalence is how existing replacement tests are built.
- **Failure modes.** A host that forgets to re-import after replacement silently starts from a blank save;
  the runner's explicit flags (D-B12) and its metadata (save imported yes/no, format, length) make that visible.
- **Authored evidence.** Write guest data, replace with a second authored ROM, assert digest == fresh machine of
  the same profile after the same admission; refused replacement keeps bytes and digest; replacement during
  busy clears the deadline.
- **Would change it.** Owner decision that replacement models "swap the cartridge but keep the pak".
- **Reversibility.** Medium (replacement-table semantics are observable; changing them needs a new profile).

### D-B11. Save bytes in the canonical digest, state export and privacy

- **RECOMMENDED.** Save bytes and the EEPROM busy deadline are canonical state: a new fixed-order digest
  section `save-media` appended in the next state version (the single state increment shared with A, C):
  device kind (u32), size (u32), `busy_until` (u64, 0 when idle) and the full device bytes. `none-v1`
  profiles serialize kind 0 / size 0. The `write_generation` counter, trace and any host observation are
  excluded. The state-export format (cluster C) carries the same section; import of a state validates
  profile identity (including `save_policy`) and the section's kind/size before any commit, using the same
  validator as D-B9, with state import taking precedence over the standalone path.
  Privacy: saves are rights-bearing/untrusted (`AGENTS.md`). The digest is SHA-256 over the encoding and does
  not expose bytes, but a digest of user save data is a fingerprint. Policy: (1) logs, result JSON, trace
  and CI artifacts never contain save bytes; (2) result files report only device kind, length, format and
  imported/exported flags, plus the standard canonical digest; (3) any state or save file produced by the runner
  is local-only unless it was produced from authored patterns; (4) no public CI fixture is a real save; all
  fixtures are generated authored patterns (index-derived), reproducible from the source.
- **Evidence.** [CODE] digest design (`docs/spec/machine-contract.md` table: "Selected EEPROM/SRAM bytes ...
  Yes: future-affecting guest state", `ARCHITECTURE.md` line 69); AGENTS.md rights rule; `AUTONOMY.md` section 4.
- **Failure modes.** State files containing real saves leak rights-bearing data; the runner must label state
  and save outputs "may contain user data" and refuse to write them into any evidence directory. Digest
  growth: the 32 KiB hash cost per digest call is negligible next to 4 MiB RDRAM.
- **Authored evidence.** Field-mutation test: toggling every device byte class (first/last byte, each block of
  EEPROM, busy deadline, device kind) changes the digest; an independent reference encoder reproduces the
  section; the generation counter and trace changes do not. A scan test over runner result output for the
  authored sentinel pattern asserts it never appears in result JSON.
- **Would change it.** Cluster C choosing a separate "private payload" file class.
- **Reversibility.** Medium (state numbering is shared and versioned, not rewritable).

### D-B12. Persistence ownership and the runner boundary

- **RECOMMENDED.** The core holds the canonical in-memory bytes and exposes only the three calls above.
  The runner owns files: save input arrives through the existing verified-input fd path
  (`runner/t64_verified_input.c`), and save output uses a staged, no-clobber, complete-or-nothing write
  analogous to `runner/t64_raw_capture.c` (write to a temporary sibling, fsync, rename; refuse existing
  destination; leave no partial file on failure). Runner options (names illustrative):
  `--save-import-fd`, `--save-format raw|swap16|wordrev32`, `--save-export-path`; no automatic load,
  flush or path derivation from the ROM name. Export on exit is explicit, never implicit.
  No dirty-flush timers or background threads. The runner result records device kind, imported/exported
  lengths, format and a boolean per action.
- **Evidence.** [CODE] `AGENTS.md` (durable storage separate from the core), runner module list; Phase 8
  capture transaction as the template.
- **Failure modes.** A crash between emulation and export loses the save (by design; hosts choose when to flush
  using `write_generation`). Path safety reuses the capture module's rules.
- **Authored evidence.** Runner-level test: import fd -> run authored ROM -> export -> byte compare to literal
  expected; refused import (wrong size) leaves the machine digest unchanged and exits with the defined status;
  export to an existing path is refused with the file untouched; interrupted write leaves no partial file.
- **Would change it.** A future player host wanting automatic persistence (that belongs to the host, not the core).
- **Reversibility.** High.

### D-B13. Explicit exclusions

FlashRAM, banked/larger SRAM (768 Kbit, 1 Mbit), Controller Pak (mempak), Rumble Pak, Transfer Pak, cartridge
RTC (joybus 0x06..0x08), 64DD media, other-emulator save containers, ROM-header or database save
detection, automatic persistence, and any real save as a fixture. All must be unsupported-with-defined
refusal when requested through the API (known-unsupported kinds return `SAVE_UNSUPPORTED_DEVICE`) or the
channel (no reply). `device_kind` values are reserved now so later additions do not renumber.

---

## Interfaces I assume from other clusters

- **A (SI/PIF/joybus):** a joybus parser that, per channel-4 handshake, calls a device hook with
  `(machine, exec_tick, tx_bytes, tx_len, rx_len)` and receives `(status, reply_bytes, reply_len, rx_flags)`.
  `exec_tick` must be a partition-invariant, well-defined guest tick (the tick at which the handshake executes
  on SI DMA read, per PIF-NUS). A must own framing, length-byte semantics (top two bits ignored), flag
  clearing, and the generic "no device" (0x80) / "timeout" (0x40) bits; B supplies only the device decision.
  A non-OK `status` from the hook means nothing was mutated and A refuses the transaction without a partial
  mutation. Channel 4 routing is separate from ports 0..3. A must treat 0xFF on channel 4 consistently with B's D-B2.
  A and B must share one new descriptor/model/state increment (descriptor v12) and A must not remove B's
  `save_policy` field.
- **C (state export):** a `save-media` section in the state file and digest as in D-B11; state import calls the
  D-B9 validator and commits once; state files carrying saves are classified private. C owns the state
  version number; B only requests the section and its order. A same-content reset operation, if C adds one,
  must preserve save bytes.
- **D (replay):** replay binds profile id/hash (which already pins the save device kind) and content hash; it
  must also bind the initial save image identity (SHA-256 of the imported bytes in canonical order, or "blank")
  and format-independent length, because two replays on the same profile and ROM with different initial saves
  diverge. Replay stores only the hash and metadata, never bytes. Save import is a pre-run host action, not a
  timed input event; a mid-run import is not a replayable operation (it is refused while busy and otherwise
  must be treated as out of scope for replay).
- **E (scheduler/CORE-02):** no new scheduler device for the EEPROM (deadline compare). SRAM DMA reuses the PI
  event path (token, descriptor, cancel semantics extended with direction/target). E's equal-tick matrix should
  add PI-SRAM-completion vs channel-4 handshake at the same tick and the EEPROM `busy_until -1/=/+1` triple.

## Proposed requirement-to-slice mapping

| Slice (suggested plan) | Content | Requirements | Depends on |
| --- | --- | --- | --- |
| B1 profile + device state | descriptor v12 `save_policy` + `eeprom_write_window_ticks`, profile registrations, device storage at create, digest section, reset-audit row, `save_info` | SAVE-02 (selection), SAVE-04 (unknown/unsupported kinds) | shared v12 increment with A/C |
| B2 import/export core API | format transforms, check order, statuses, refusal matrix, ASan/UBSan fuzz target | SAVE-03, SAVE-04 | B1 |
| B3 SRAM path | bus word access, PI DMA both directions, refusals, ROM-profile regression | SAVE-02 | B1 (no A dependency) |
| B4 EEPROM device | joybus channel-4 hook, busy deadline, block rules, no-device policy, guest-visible authored ROM | SAVE-02 | B1 and A's channel framing |
| B5 runner persistence | fd import, staged no-clobber export, result fields, privacy scan | SAVE-03 | B2 |
| B6 integration | authored end-to-end (guest writes, export, state round trip with C, replay binding with D, CORE-02 triple with E), spec text `docs/spec/save-media.md` and contract/reset-table updates | criterion 2, feeds criteria 3 and 4 | B3, B4, C, D, E |

B1-B3 and B5 can proceed before A completes; only B4 waits for A.

## Unknowns that need the owner (or hardware)

1. **EEPROM write window and read-during-busy hardware behavior.** Sources disagree (6/15/30 ms); settling it
   needs hardware measurement or a primary document (owner physical-measurement stop). The recommendation
   proceeds with a named software policy and labeled self-consistency evidence only.
2. **EEPROM 4k mirror vs refusal, SRAM mirroring, SRAM power-up contents:** unverified; defaults are the
   conservative refusal/0x00 choices. Hardware evidence would be needed to change them.
3. **Use of real saves or titles for validation:** explicitly stopped (rights); everything here uses authored
   patterns. Confirm that local-only owner-supplied saves remain out of scope for Phase 9.
4. **Scope of D-B8 transforms:** is RAW-only acceptable, or must two named swaps ship? (Routine; the
   recommendation includes them.)
5. **Replacement clears the save (D-B10):** routine, but it is an observable semantic the owner may care about.
6. **Profile count (4 save variants):** routine; flag only if the owner wants fewer public ids.
7. **0xFF on channel 4 and any joybus "reset" command:** needs A's decision for consistency.
8. cen64 behavior was only seen in a search snippet; the license of cen64 and ares code paths was not
   audited beyond ISC/GPL/Unlicense headers noted above, and nothing was copied.
