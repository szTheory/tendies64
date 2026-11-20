# State container (export)

The state container is a versioned binary blob that carries the canonical state
of one machine of the composite family plus the host observer cursors. This
document specifies the export half (plan 09-08); the import half and the
boundary matrix are appended by plan 09-09.

## Scope and evidence

- The container is a software-profile artifact. It is a self-consistent copy of
  one build's canonical state: there is no cross-build, cross-version or
  cross-implementation portability statement, and nothing here describes a
  console or any other emulator.
- Name, layout, format string, flag names and detail names are provisional until
  Phase 10. No API or ABI freeze is implied.
- The evidence is the independent decoders: the literal-offset decoder in
  `core/tests/test_state_export.c` (cases `export-layout`, `decoder-controls`,
  `states-and-ids`, `observer-continuity`, `idempotence-shape`,
  `non-interference`, `refusals` and `size-arithmetic`), the stdlib decoder
  `fixtures/state/decode_container.py` and the real supervised runner cases in
  `runner/tests/test_state_runner.c`. The state content itself is the F1
  content recorded in [machine-contract.md](machine-contract.md); the section
  hashes there are regression pins taken from the walker.

## Container layout

All integers are big-endian. Offsets are from the start of the blob. The total
length is `0x120 + 56 x section count + canonical length + 40 + 32`.

Header, 0x120 bytes:

| Offset | Width | Field |
| --- | --- | --- |
| 0x000 | 8 | magic, the bytes `T64STATE` |
| 0x008 | 4 | container version, 1 |
| 0x00C | 4 | state serialization version, 25 |
| 0x010 | 8 | total length of the blob |
| 0x018 | 4 | section count (table entries, the observer entry included) |
| 0x01C | 4 | flags, zero |
| 0x020 | 32 | profile digest (`t64_profile_hash` of the machine profile) |
| 0x040 | 32 | content SHA-256 of the admitted image |
| 0x060 | 8 | content length |
| 0x068 | 8 | guest tick of the machine |
| 0x070 | 32 | declared canonical digest, equal to `t64_machine_digest` |
| 0x090 | 8 | canonical offset, `0x120 + 56 x section count` |
| 0x098 | 8 | canonical length |
| 0x0A0 | 128 | build identity, NUL padded |

Section table at 0x120, one 56-byte entry per section: id u32 at +0, flags u32
at +4, body offset u64 at +8, body length u64 at +16, body SHA-256 at +24 (32
bytes). Entries are in strictly ascending id order. Canonical entries carry
flags 0. The last entry is the observer-continuity section: id 16, flags 1
(outside the digest), body offset at the end of the canonical region and body
length 40.

Canonical region, at the canonical offset: exactly the v25 digest preimage, that
is the domain bytes `t64.state.si.25` (15 bytes) and then, for each canonical
section, its 32-bit big-endian id followed by its body. A canonical entry's body
offset points at the body, so the id sits at the offset minus 4, the first id
follows the domain bytes, and each next id starts where the previous body ends.

Observer body, 40 bytes, directly after the canonical region: five u64 values:
the output drain cursor, the video packet head, the video packet count, the
audio packet head and the audio packet count.

Trailer, 32 bytes at the end: SHA-256 of every byte before it.

The decoder constants and `python3 fixtures/state/decode_container.py
--print-layout` list the same offsets; the offsets above were compared with
that output field by field.

## Section ids

Ids 1 to 11 are the canonical sections described in
[machine-contract.md](machine-contract.md): 1 identity, 2 machine core, 3 RDRAM,
4 SP memory, 5 interrupt, PI and SP, 6 scheduler, 7 warm devices, 8 RDP and VI,
9 audio, 10 SI, PIF and the input latch, 11 save media. Id 16 is the observer
continuity section and is never emitted by the digest walker.

Section 11 exists for the three save ids only (a profile with no save device has
no such section). The id lists written by the export are therefore:

| Profile ids | Section ids in table order |
| --- | --- |
| the stem id `ntsc-cart-4m-openipl3-rdram-b-t1-si-v1` | 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16 |
| the three save ids (suffixes `-e4k`, `-e16k`, `-s32k`) | 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 16 |

Body lengths of the idle stem machine (bytes): 168, 32, 4,194,308, 8,200, 276,
56, 43,724, 2,103,016, 336 and 404 for ids 1 to 10, and 40 for id 16; the save
ids add 532, 2,068 or 32,788 bytes for id 11.

## Hash semantics

- The declared canonical digest equals `t64_machine_digest` and equals the
  SHA-256 of the canonical region. No section body, marker, section id or digest
  value changes in this plan: state content is frozen at F1.
- Section hashes and the trailer are integrity checks, not authentication. A
  blob whose bodies, section hashes, declared digest and trailer were all
  recomputed after tampering still decodes (the case `decoder-controls` records
  this limit by name), and its declared digest then differs from the digest of
  any machine that produced it. Semantic validation belongs to the importer.

## Exclusions and observers

Not part of the canonical region: trace state and drain history, the output
drain cursor, ring heads and counts, the `rsp_observed_*` diagnostics, caller
buffers, pointers, allocator state, the queue head, padding and the build
identity as behavior. The retained packet slots are carried because drained
slots are hashed.

The observer section holds exactly the five values above and nothing else.
`write_generation` (a host counter of successful save imports) never travels in
the container and no export byte reads it; the importer maintains it by its own
rule in the import plan, so the container and the digest do not depend on it.
Observer state gates dispatch, so a default import restores observer
continuity; the statement about dropping observers is owned by the import plan.
Observer progress changes only the observer entry hash, the observer body and
the trailer (case `observer-continuity`); trace enablement and queued trace
records never reach the blob.

## Export API

`t64_status t64_machine_state_export(const t64_machine *machine, uint8_t
*bytes_out, size_t capacity, size_t *length_out)`, with the format string
`T64_STATE_CONTAINER_FORMAT` (`t64.state-container.v1`).

- Size query: a NULL buffer, zero capacity or a capacity below the required
  length returns `BUFFER_TOO_SMALL`, sets `*length_out` to the required length
  and writes no byte. An exact or larger buffer receives the container; bytes
  past the length are not touched.
- Check order: a NULL machine or length pointer, a length span inside
  machine-owned storage, and (when a buffer is given) an invalid span or one
  overlapping machine storage or the length value return `INVALID_ARGUMENT`; no
  admitted image is `NOT_READY`; a profile outside the composite family (state
  serialization version other than 25) is `INVALID_ARGUMENT`. A section recorder
  overflow, a build identity of 128 bytes or longer, or a container above the
  size ceiling is `RESOURCE_EXHAUSTED`. An internal inconsistency found while
  writing (unreachable for a const machine) clears the first total bytes of the
  buffer and returns `BAD_STORAGE`, so a defect never publishes a partial
  container.
- Every refusal leaves the caller bytes, the length value and the machine
  unchanged. The machine is const: export calls nothing that mutates it, does
  not allocate and does not call the host (case `non-interference`: whole
  structure, digest, observer cursors, trace and allocator counters are
  identical before and after, after a failed export too, and a machine that
  exports between advances reaches the same digests as one that never exports).
- Implementation shape: a planning pass over the one canonical walker into a
  counting sink (the section recorder yields the canonical length and each
  marker offset), then an emit pass that streams the same bytes into the caller
  buffer while hashing them; the header, table, observer body and trailer are
  written through bounds-checked helpers.

## Size accounting

- Totals use checked 64-bit arithmetic (`t64_state_container_length`): a section
  count of zero or above 32, any wrap and a total above 64 MiB (67,108,864) are
  refused; a total of exactly 64 MiB is allowed. Case `size-arithmetic` covers
  the edge values and a compile-time worst case.
- The artifact maximum of the runner (at most 32 MiB) is a separate, stricter
  bound than the container ceiling.
- Measured sizes (bytes): idle stem machine 6,351,551; the save ids 6,352,143
  (`e4k`), 6,353,679 (`e16k`) and 6,384,399 (`s32k`); the stem machine with an
  AI packet being built 6,351,643, after its first packet 6,351,631, with a
  retained video packet 6,351,607 and with a pending RDP command 6,351,591. The
  blob of the authored admit ROM at authored completion (guest tick 538,781) is
  6,351,511 bytes.
- Arithmetic bounds: strictly above 6,303,756 (RDRAM section 4,194,308, one
  2,097,152-byte RDP coverage array, the 4,096-byte TMEM and the 8,200-byte SP
  memory section) and below 11,534,336. The video and audio retained packet
  slots add their packet lengths (at most 307,200 per video packet and 262,136
  per audio packet, two slots each) only while packets are retained.

## Privacy and publication

State blobs hold guest memory and device state and are private. The runner
writes them 0600, no-replace, through the artifact publisher into an owner-only
directory; they are never uploaded or tracked. The `sta` extension and the
`T64STATE` magic have rights-scanner rows (plan 09-02) that are unchanged. No
host path, time or user name appears in any header byte: the build identity is
a short constant with no path character and the rest of the header holds
digests, lengths and the guest tick (case `export-accepted` checks the
identity string and the independent decoder reads the same bytes).

## Runner surface

Flags (real mode, provisional): `--state-export LEAF` (a `.sta` leaf of at most
36 characters) and `--state-export-format t64.state-container.v1`, required
together. Cross-validation, before the guest starts, with detail `usage`:

- both flags require the artifact pair (`--artifact-dir`, `--artifact-max-bytes`);
- the format must equal `t64.state-container.v1` exactly;
- refused with `--raw-capture-dir` and in synthetic mode;
- one run has one artifact producer, so refused together with every save flag
  (`--save-import`, `--save-import-size`, `--save-import-sha256`, `--save-kind`,
  `--save-format`, `--save-export-leaf`) and every replay flag
  (`--replay-record`, `--replay-checkpoint-interval`, `--replay-play`,
  `--replay-play-size`, `--replay-play-sha256`, `--replay-report`); the runner
  binary repeats the rule for its forwarded child options;
- allowed with the `--input`, `--input-size` and `--input-sha256` triple, which
  is not an artifact producer.

A profile whose state serialization version is not 25 takes the existing
prelaunch refusal (detail `refusal`, guest tick 0; a supervised run reports the
report-error classification the artifact preflight refusal uses). A nonexistent
or unsafe artifact directory is the same existing preflight refusal.

Detail names (harness-error with a host-error stop only; the closed list is in
[result-protocol.md](result-protocol.md)): `state-export-open` (the leaf is not a
`.sta` file or the publisher refuses it, for example an occupied target; before
the guest runs, guest tick 0), `state-export-incomplete-terminal` (the guest did
not reach authored completion), `state-export-core` (the size query or the export
failed), `state-export-cap` (the required length is above the artifact maximum;
nothing is allocated or published), `state-export-alloc` (the exact allocation
failed) and `state-export-publish` (the publisher write or finalize failed). The
guest record is kept and only the outcome, the stop kind and the detail are
replaced; no `t64.result` key is added and no state byte reaches the record, the
log or standard output. The longest name is 32 characters, so the result detail
field is 48 bytes.

File-size backstop: the supervisor sets the child file-size limit to the artifact
maximum plus 65,536 bytes only when the pair is present (the exact rule is in
[raw-capture.md](raw-capture.md)); the default limit stays 69,632 bytes. The
state blob (6,351,511 bytes for the admit ROM) is larger than the default and
fits the raised limit: with the maximum equal to the size the file is published,
one byte below the run ends with `state-export-cap`, and the test twin's exact
write fault at that size is not signalled while one byte more is
(`runner.state-export`: `export-accepted`, `export-size-cap`,
`export-fsize-at-state-size`, `export-refusals`, `export-nonterminal`,
`export-decode`, `checker-vocabulary`).

Option-pair matrix recorded for the later drift sweep:

| Option | With `--state-export` |
| --- | --- |
| artifact pair | required |
| `--raw-capture-dir` | refused (`usage`) |
| save flags (six) | refused (`usage`) |
| replay flags (six) | refused (`usage`) |
| `--input` triple | allowed |
| synthetic mode | refused (`usage`) |

## Freeze and status

State content is frozen at F1; this plan changes no section id, marker, body byte
or digest value. A later change to the container is a recorded defect fix with
regenerated goldens. The import notes are appended by plan 09-09.
