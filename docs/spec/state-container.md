# State container (export and import)

The state container is a versioned binary blob that carries the canonical state
of one machine of the composite family plus the host observer cursors. This
document specifies the export half (plan 09-08) and the import half (plan
09-09, sections "Import" onward); the boundary matrix, the mutants and the field
audit follow in plan 09-09.1. Container version 1 carries state 25 (the four
si-v1 ids); container version 2 carries state 26 (the four si-v2 ids, plan
10-11) and is specified in the section "Container v2 (state 26)".

## Scope and evidence

- The container is a software-profile artifact. It is a self-consistent copy of
  one build's canonical state: there is no cross-build, cross-version or
  cross-implementation portability statement, and nothing here describes a
  console or any other emulator.
- Stability: state blobs are build-identity-bound and outside the ABI promise: a
  blob is accepted only by the build identity and profile that wrote it, and no
  cross-build or cross-version portability is promised. The container version
  word, the container format constants and the section layout are state-format
  facts of one build family, not public API; they may change with the next
  state serialization version without a compatibility path. Blobs stay private
  0600 artifacts and are never committed.
- The container format strings `t64.state-container.v1` (state 25) and
  `t64.state-container.v2` (state 26) and their pairing with the state versions
  are settled in Phase 10 (plans 10-11 and 10-15), as state-format constants
  outside the ABI promise. Name, layout, flag names and detail names are
  provisional until the Phase 11 freeze candidate (final ABI bind at the end of
  Phase 12). No API or ABI freeze is implied.
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
| 0x008 | 4 | container version: 1 for state 25, 2 for state 26 (the version word is paired with the state word below) |
| 0x00C | 4 | state serialization version, 25 or 26 |
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

Canonical region, at the canonical offset: exactly the digest preimage of the
state (the v25 preimage for state 25, the v26 preimage for state 26), that is
the domain bytes `t64.state.si.25` (state 25) or `t64.state.si.26` (state 26),
15 bytes, and then, for each canonical section, its 32-bit big-endian id followed
by its body. A canonical entry's body
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
56, 43,724, 4,200,260, 336 and 404 for ids 1 to 10, and 40 for id 16 (the id 8 body
ends with the carried RDP tail of 2,097,244 bytes, plan 09-10); the save
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
rule (see the import section), so the container and the digest do not depend on
it.
Observer state gates dispatch, so a default import restores observer
continuity; the observer policy is stated in the import section below.
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
  admitted image is `NOT_READY`; a profile outside the two composite families
  (state serialization version other than 25 and 26) is `INVALID_ARGUMENT`. A section recorder
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
- Measured sizes (bytes), after the carried RDP tail of plan 09-10 (2,097,244
  bytes in section 8; the values before it were 2,097,244 lower): idle stem
  machine 8,448,795; the save ids 8,449,387 (`e4k`), 8,450,923 (`e16k`) and
  8,481,643 (`s32k`); the stem machine with an AI packet being built 8,448,887,
  after its first packet 8,448,875, with a retained video packet 8,448,851 and
  with a pending RDP command 8,448,835. The blob of the authored admit ROM at
  authored completion (guest tick 538,781) is 8,448,755 bytes.
- Arithmetic bounds: strictly above 8,400,908 (RDRAM section 4,194,308, the
  2,097,152-byte triangle coverage array, the 2,097,152-byte memory coverage
  table, the 4,096-byte TMEM and the 8,200-byte SP memory section) and below
  11,534,336 (the bound of state 25, unchanged). The video and audio retained
  packet slots add their packet lengths (at most 307,200 per video packet and
  262,136 per audio packet, two slots each) only while packets are retained.
- State 26 bound (plan 10-11): the stale statement above ("below 11,534,336") does
  not hold for state 26, whose section 8 is 2,097,176 bytes longer. The derived
  bounds are strictly above 10,498,084 (8,400,908 plus 2,097,176) and below
  12,582,912 (12 MiB): the largest idle blob (`s32k`, 10,578,819) plus two video
  packets of 307,200 and two audio packets of 262,136 is 11,717,491, which rounds
  up to 12,582,912. The measured sizes are in the section "Container v2 (state
  26)".

## Container v2 (state 26)

Container version 2 is the one version marker of the state 26 bump (plan 10-11).
It is written by the exporter for the four si-v2 ids (descriptor 13, machine
model 21, state serialization version 26) and differs from container version 1
only where listed here.

- Header: the word at 0x008 is 2 and the word at 0x00C is 26; a state 25 blob
  keeps container version word 1 and stays byte for byte as it was. The pairing is a
  function of the state word: 2 for state 26 and 1 for every other state word.
  The exporter writes it and the importer's header check compares the blob's
  version word with it, before any later header check and before any apply, so
  only the pairs (1, 25) and (2, 26) can pass the header and the registry. Version
  1 with state 26 and version 2 with state 25 are refused with the container
  version reason at offset 0x008. The independent decoder
  `fixtures/state/decode_container_v2.py` (a separate script: the version 1
  decoder is a pinned Phase 9 oracle and is not edited) enforces the same pairing
  with the refusal kind `container-version`.
- Format string (plan 10-15): the public name of container version 2 is
  `t64.state-container.v2` (`T64_STATE_CONTAINER_FORMAT_V2` in
  `core/include/tendies64/t64_machine.h`), beside the version 1 string. It is a
  state-format constant outside the ABI promise (a blob is build-identity-bound, see
  "Stability"), not public API work. The runner pairs the string with the profile:
  v1 with state 25 and v2 with state 26 (the section "Runner surface").
- Canonical region: the domain bytes are `t64.state.si.26`. The section ids,
  their order and the framing are those of state 25: ids 1 to 10, plus 11 for the
  three save ids, then the observer section 16.
- Section 8 (RDP and VI), offsets from the body start: `pending_triangle_word_count`
  n (u32, at most 22) at 196, the n pending words (8 bytes each) from 200, seven
  u32 words after them (primitive color, its valid flag, triangle count, the
  render and shade flags and the triangle coverage width and height, both 1,024),
  then the 2,097,152-byte triangle coverage array. The five depth scalars follow,
  inside the RDP block at the position the v21 depth block had, as five u32 words:
  `depth_image_address`, `depth_image_valid`, `primitive_z`, `primitive_dz`,
  `primitive_depth_valid` (the two valid words are 0 or 1). Their body offset is
  228 + 8n + 2,097,152. They travel as exact words, with no rounding or packing.
- The tail of section 8 ends with the 88-byte combiner block, the 4-byte
  coverage length and the memory coverage table, and then, for state 26 only, the
  4-byte hidden depth length (2,097,152 for 4 MiB of RDRAM) and the hidden depth
  table of that size, byte for byte, ending the body at the section 9 marker.
- Sizes: the state 26 blob of a machine is the state 25 blob of the same builder
  on the matching si-v1 id plus 20 bytes of scalars, 4 bytes of hidden length and
  2,097,152 hidden bytes, 2,097,176 in all, and only section 8 grows. Measured
  idle sizes (bytes, spin loop at guest tick 64): stem 10,545,971, `e4k`
  10,546,563, `e16k` 10,548,099, `s32k` 10,578,819 (each the state 25 idle size of
  the section "Size accounting" plus 2,097,176). A Z triangle in flight (the stem id) is 10,546,059
  and section 8 grows by the pending words alone (8 per word); both video slots and
  both audio slots occupied is 10,546,075 (the `s32k` values are 10,578,907 and
  10,578,923). All are below 12,582,912.
- Import (plan 10-12): the importer reads the layout above only for a machine of
  the state 26 tuple, and the domain text it checks and hashes is the machine's
  own (`t64.state.si.26` for a state 26 machine, `t64.state.si.25` for a state
  25 machine). A blob whose domain was forged to the other text, even fully
  re-signed, is refused at the domain bytes (`SECTION_BOUNDS`, section 0, rule
  `NONE`) and never reaches a reader of the other layout; a state 25 blob on a
  state 26 machine and a state 26 blob on a state 25 machine are refused at the
  registry (`STATE_VERSION` at offset 0x00C) after the version pairing of the
  header check has passed. Within section 8 the reader order is the stream order,
  which puts the five depth scalars before the triangle-render proof (they sit in
  the RDP block, before `work_policy_version`), so the proof of a Z triangle in
  flight is run from the blob values: the same extent function as the acceptance
  (`t64_rdp_triangle_extent`, which takes the depth address, the depth-valid flag
  and the primitive-depth flag from the view), then the Z product bound of the
  acceptance preflight written out over the packet words (the preflight reads the
  live registers, which a validate pass must not write). A reader that took the
  scalars after the proof saw depth-valid false and refused every Z triangle in
  flight; the importer does not.
- Per-member rules of the state 26 members (the audit rows are in
  `docs/spec/state-field-rules.txt`):

  | Member | Rule | Closed range |
  |--------|------|--------------|
  | `depth_image_address` | `ADDR_RANGE` | aligned to 8 bytes and its first halfword inside RDRAM (what SetZImage accepts); for a Z triangle in flight the pixel extent is part of the render proof (`LEN_LE`) |
  | `depth_image_valid`, `primitive_depth_valid` | `BOOL_01` | 0 or 1 |
  | `primitive_z`, `primitive_dz` | `INDEX_LT` | at most 0xFFFF (the 16-bit registers SetPrimDepth keeps) |
  | hidden table length | `PROFILE_DERIVED` | the RDRAM size divided by 2 (2,097,152 for 4 MiB) |
  | each hidden table byte | `ENUM_CLOSED` | 0, or 0x80 to 0x83 (valid bit and the two low delta bits; the writer stores nothing else and every accepted backing write clears the entry) |

  The commit writes the five scalars and the hidden table from the blob for a
  state 26 machine and keeps the Phase 9 behavior for a state 25 machine (both
  stay zero: a peer whose hidden table was dirtied first has every byte zero
  after a state 25 import, case `v25-hidden-zero`). The validation pass runs the
  whole blob before the commit, so a refused state 26 import leaves the machine
  allocation byte for byte and the allocator counters as they were.

Import evidence: `unit.core.state.import-v26-roundtrip` (case `roundtrip`: an
idle state on each of the four ids with an all-zero hidden table; a Z triangle in
flight, imported at its first row and run on equal with the source, with the color
and depth bytes and the hidden table compared; a Z-tested frame inside a scan-out
with a retained video packet and two PCM DMAs in flight on the `s32k` id with an
SRAM write and on the `e4k` id with an EEPROM write, compared field by field for
digests, video and audio packet bytes and save bytes; `v25-hidden-zero`) and
`unit.core.state.import-v26-hostile` (case `hostile`: the rows of
`docs/spec/state-mutants-v26.txt`, the agreement of the importer's Z product proof
with the live acceptance preflight, and the v26 fuzz windows of
`core/tests/fuzz_state_import.c` over the depth scalars, the hidden length, the
hidden entries and the pending Z packet words). The file
`docs/spec/state-mutants-v26.txt` is the state 26 counterpart of the Phase 9
inventory `docs/spec/state-mutants.txt`, which is frozen with its own count sentence
(section "Hostile-input evidence"); it has 16 rows, read by the test through
`T64_STATE_MUTANT_INVENTORY_V26` and compared with the C table name by name, in
order, tuple by tuple. The mutants `v26-import-skip-hidden`,
`v26-import-depth-after-proof`, `v26-import-addr-unchecked` and
`v26-import-z-products-unchecked` of `scripts/p10-mutants.py` are killed by the
roundtrip case (the first two) and the hostile case (the last two). These are
comparisons of the code with itself and with a layout written from this document.

Evidence: `unit.core.state.export-v26` (case `export-v26-idle`: header words,
section ids, the hand-derived idle sizes of the four ids, decoder acceptance, the
parity of the two decoders on a state 25 blob and the version and state pairing
in both the decoder and the importer) and `unit.core.state.export-v26-bounds`
(mid Z-triangle and retained-packet exports, the upper bound, one byte short and
one byte long refused by the decoder, the scalars and the hidden table compared
word for word and byte for byte). The mutants `v26-export-omit-hidden`,
`v26-export-version-word-1` and `v26-reader-pairing-dropped` of
`scripts/p10-mutants.py` are each killed by `export-v26-idle`. These are
comparisons of the code with itself and with a decoder written from the layout;
they make no claim about any other build or implementation.

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
36 characters) and `--state-export-format` (`t64.state-container.v1` or, since plan
10-15, `t64.state-container.v2`), required together. Cross-validation, before the
guest starts, with detail `usage`:

- both flags require the artifact pair (`--artifact-dir`, `--artifact-max-bytes`);
- the format must equal `t64.state-container.v1` or `t64.state-container.v2`
  exactly (`T64_STATE_CONTAINER_FORMAT`, `T64_STATE_CONTAINER_FORMAT_V2`): the
  supervisor checks the closed set from its arguments alone (it has no profile in
  scope) and the runner argument parser checks the same set, so any other string is
  `usage` and the guest is never started;
- refused with `--raw-capture-dir` and in synthetic mode;
- one run has one artifact producer, so refused together with every save flag
  (`--save-import`, `--save-import-size`, `--save-import-sha256`, `--save-kind`,
  `--save-format`, `--save-export-leaf`) and every replay flag
  (`--replay-record`, `--replay-checkpoint-interval`, `--replay-play`,
  `--replay-play-size`, `--replay-play-sha256`, `--replay-report`); the runner
  binary repeats the rule for its forwarded child options;
- allowed with the `--input`, `--input-size` and `--input-sha256` triple, which
  is not an artifact producer.

The string is paired with the profile by the runner at prelaunch (plan 10-15,
`state_request_admitted` in `runner/t64_real.c`, checked after the profile is
resolved and before the guest starts): a state export is admitted for state
serialization version 25 with the v1 string and for state 26 with the v2 string.
Every other pair (state 25 with the v2 string, state 26 with the v1 string, and a
profile of any other state serialization version, such as the state 24 audio
profile, with either string) takes the existing prelaunch refusal (detail
`refusal`, guest tick 0, nothing published; a supervised run reports the
report-error classification the artifact preflight refusal uses). A nonexistent
or unsafe artifact directory is the same existing preflight refusal. The cases
are `runner.state-format-v26` (each wrong pair next to an accepted baseline of the
same setup) and `runner.composite-state-v2` (the positive path: a state 26 export of
the composite with the v2 string, container version word 2, decoded by the
independent version 2 decoder).

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
state blob (8,448,755 bytes for the admit ROM) is larger than the default and
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
| `--state-export-format` string | closed set {`t64.state-container.v1`, `t64.state-container.v2`}, any other string refused (`usage`) by the supervisor and the runner parser |
| format string with the profile | v1 with state 25 and v2 with state 26 only; any other pair refused at the runner prelaunch (`refusal`, guest tick 0) |

## Freeze and status

State content is frozen at F1; the export plan changes no section id, marker,
body byte or digest value, and neither does the import plan. A later change to
the container is a recorded defect fix with regenerated goldens.

## Import

`t64_status t64_machine_state_import(t64_machine *machine, const uint8_t *bytes,
size_t length, uint32_t flags, t64_state_import_report *report_out)`. The name,
the flag, the report record, the statuses (26 to 33), the reason names and the
rule names are provisional until the Phase 11 freeze candidate (final ABI bind at the
end of Phase 12); no API or ABI freeze is implied.

### Scope and contract

- Import is same-build and same-version only: a container is accepted by a machine
  of the same profile (digest), the same admitted content (digest and length) and
  the same build identity, and only for state serialization version 25. Cross-build
  and cross-version import, a diagnostic override of the build binding,
  state-anchored replay, compression and the skipping of unknown sections are not
  implemented; each of those cases is refused with its own status.
- The contract is memory safety plus liveness. A blob that passes validation cannot
  cause an out-of-range access, an unbounded loop or undefined arithmetic: every
  field used as an index, a length, a shift, a loop bound or an array subscript is
  bounded by a named rule before it is used. A state that passes validation but
  is semantically unreachable is accepted and then runs deterministically; no
  statement here or in a test says that an imported state is reachable.
- Hashes (the section hashes, the declared digest and the trailer) are integrity
  checks, not authentication: a blob whose bodies and every hash were recomputed is
  checked by the rules below and nothing else. The evidence is the
  self-consistency of the software model.

### Order of checks

Before the first check below, a non-NULL report whose caller-filled `struct_size`
is under the v1 size (32 bytes) is refused with `STRUCT_TOO_SMALL` (plan 11-13):
nothing is written, the machine is unchanged and the blob is not read. A report
span that wraps or aliases the machine is not read for its size and stays
unusable exactly as in check 1. A NULL report is still accepted and carries no
size. A larger `struct_size` receives the v1 members only, never the
`struct_size` member and never a byte past the v1 size.

1. Arguments and disjointness: a NULL machine answers
   INVALID_ARGUMENT at the entry guard with the report untouched, and only a
   NULL blob answers `ARG_NULL`; length non-zero and at most 64 MiB
   (`ARG_LENGTH`), no flag bit other than `T64_STATE_IMPORT_DROP_OBSERVERS`
   (`ARG_FLAGS`), the blob span and the report span disjoint from the machine
   allocation, the admitted image and both packet storages (`ARG_OVERLAP_LIVE`;
   an unusable report span is never written, and the span is judged before every
   one of these refusals, so even `ARG_NULL`, `ARG_LENGTH` and `ARG_FLAGS` leave
   an aliasing span untouched), then an admitted image (`NOT_ADMITTED`, status
   `NOT_READY`).
2. Header: length at least header plus trailer, magic, container version 1, total
   length equal to the supplied length, section count 1 to 32, header flags zero,
   and the build identity field holding a terminator with only zero bytes after it
   (`HEADER_RESERVED`).
3. Trailer: SHA-256 of every byte before it (`TRAILER_SHA256`, status `STATE_INTEGRITY`).
4. Identity binding, one status per mismatch: the state version is registered for
   the machine (`STATE_VERSION`), the profile digest (`PROFILE_DIGEST`, status
   `STATE_PROFILE`), the content digest and the content length (`CONTENT_DIGEST`,
   `CONTENT_LENGTH`, status `STATE_CONTENT`) and the 128-byte build identity equal
   to the live text with NUL padding (`BUILD_ID`, status `STATE_BUILD`). The importer
   registry has one row, version 25.
5. Section table with checked arithmetic (`state_add_u64`, `state_mul_u64`,
   `state_span_within`): known ids (`SECTION_ID_UNKNOWN`; id 11 only for the three
   save ids), strictly ascending with the observer-continuity id last
   (`SECTION_ORDER`), flags zero for canonical entries and 1 for the observer entry
   (`SECTION_FLAGS`), contiguous bodies behind their four-byte markers
   (`SECTION_OVERLAP` for an earlier offset, `SECTION_BOUNDS` for a gap, an escape
   from the canonical region or a wrong marker), the exact length for each fixed
   section (`SECTION_LENGTH_EXPECTED`), the section SHA-256 (`SECTION_SHA256`) and
   every mandatory section present (`SECTION_MISSING`).
6. Per-field range rules, one reader per section in ascending id order
   (`FIELD_RANGE`, `FIELD_PADDING`, `FIELD_RESERVED`, `FIELD_DERIVED`,
   `OBSERVER_RANGE`; status `STATE_RANGE`), then a section that is not consumed
   exactly is `SECTION_LENGTH_EXPECTED`.
7. Structural and per-device invariants over the scheduler queue and the device
   state (`QUEUE_*` and `DEVICE_*`; status `STATE_INVARIANT`).
8. The canonical digest recomputed from the blob (the domain bytes, then each
   canonical section id and body) against the declared digest (`DECLARED_DIGEST`).
9. One infallible commit.

A refusal reports a closed `(status, reason, section id, rule, position)` record
(the position is a byte position in the blob; it never carries blob bytes), leaves
the machine bit-identical, makes no host call and releases nothing. The check order
of import and export (pointers before admission) differs from the save import and
export of plan 09-05 (admission first) and from `t64_machine_save_info` of plan
09-04 (pointers first, because it must write its output); the three differences are
documented and none is changed here.

### Statuses and reasons

| Status | Reasons |
| --- | --- |
| `INVALID_ARGUMENT` | `ARG_NULL`, `ARG_LENGTH`, `ARG_FLAGS`, `ARG_OVERLAP_LIVE` |
| `NOT_READY` | `NOT_ADMITTED` |
| `STATE_FORMAT` (26) | `MAGIC`, `CONTAINER_VERSION`, `TOTAL_LENGTH`, `SECTION_COUNT`, `HEADER_FLAGS`, `HEADER_RESERVED`, `SECTION_ID_UNKNOWN`, `SECTION_ORDER`, `SECTION_FLAGS`, `SECTION_BOUNDS`, `SECTION_OVERLAP`, `SECTION_LENGTH_EXPECTED`, `SECTION_MISSING` |
| `STATE_INTEGRITY` (27) | `TRAILER_SHA256`, `SECTION_SHA256`, `DECLARED_DIGEST` |
| `STATE_VERSION` (28) | `STATE_VERSION` |
| `STATE_PROFILE` (29) | `PROFILE_DIGEST` |
| `STATE_CONTENT` (30) | `CONTENT_DIGEST`, `CONTENT_LENGTH` |
| `STATE_BUILD` (31) | `BUILD_ID` |
| `STATE_RANGE` (32) | `FIELD_RANGE`, `FIELD_PADDING`, `FIELD_RESERVED`, `FIELD_DERIVED`, `OBSERVER_RANGE` |
| `STATE_INVARIANT` (33) | `QUEUE_COUNT`, `QUEUE_ORDER`, `QUEUE_SEQUENCE`, `QUEUE_TICK`, `QUEUE_KIND_UNKNOWN`, `DEVICE_EVENT_MISSING`, `DEVICE_EVENT_EXTRA`, `DEVICE_TOKEN_MISMATCH`, `DEVICE_STATE_INCONSISTENT` |

### Field rules

Every field-level refusal and the queue reasons record one rule: `COUNT_CAP` (a
count above its capacity), `INDEX_LT` (an index, or a value wider than its member,
at or above its bound), `ENUM_CLOSED` (outside a closed enumeration or a closed
event pair), `BOOL_01` (a boolean word other than 0 or 1), `ALIGN`, `ADDR_RANGE`
(an address span outside installed RDRAM), `TICK_GE` and `TICK_LE` (a tick below or
above its bound), `SEQ_LT` (a sequence at or above `next_sequence`), `SHIFT_LT` (a
shift or mask width at its limit), `LEN_LE` (a length above its maximum),
`PROFILE_DERIVED` (a field that must equal a profile or live value), `DERIVED_EQ`
(a stored derived value different from its recomputation), `PAD_ZERO` (a text
member without a terminator or with bytes after it) and `RESERVED_ZERO` (bits
outside a mask). `MASKED_AT_USE` names a field that is read without a range check
because its consuming site masks it; the sites are listed here and the field-use
audit of plan 09-09.1 maps every one:

| Field | Consuming site |
| --- | --- |
| `sp.mem_address`, `sp.dram_address`, `sp.read_length`, `sp.write_length` | `decode_descriptor` in `core/t64_sp.c` masks every latch when a DMA starts |
| `rsp.pc`, `rsp.gpr`, `rsp.delay_target` | `t64_rsp_issue_event` in `core/t64_rsp.c` masks the program counter and every address to the bank |
| `cpu.gpr`, `cpu.cp0`, the TLB entries | the CPU, memory and MMU units mask or switch over them (`page_size_shift` in `core/t64_mmu.c`) |
| `rdp.render_x_begin`, `rdp.render_x_end`, `rdp.render_y_begin`, `rdp.render_y_end`, `rdp.render_y` | none: a render in progress is bounded by the reader (the last pixel inside RDRAM; for a triangle also the 1,024-row coverage height, the packet and the products, see "Triangle render in progress"); the row handlers do not check the extent again |

Fixed section lengths for this profile: 168, 32, 4 plus the RDRAM size, 8,200, 276,
43,724 and 404 bytes for ids 1, 2, 3, 4, 5, 7 and 10, 20 plus the save size for id
11 and 40 for id 16; ids 6, 8 and 9 are consumed exactly by their readers.

### Per-device rules

The per-device pass reads only the scratch facts and a per-(device, kind) queue
summary (count, first tick, token and value, largest token and how many events
carry it). Each rule comes from the cited source and is accepted for every state
the source permits.

| Device | Rule | Source |
| --- | --- | --- |
| CPU | at most one (CPU, step) event, token and value 0; exactly one unless the CPU is stopped, the machine halted or the attempt outcome is the host error (a refused re-reserve leaves none) | `t64_cpu_step`, `cpu_commit_preflighted_successor` in `core/t64_cpu.c`; the exception re-reserve in `core/t64_exception.c` |
| PI | an active transfer owns exactly one completion event with the active token and the transfer length as its value; an idle PI owns none; the SRAM directions need the SRAM save id and an active transfer | `start_dma`, `t64_pi_complete` in `core/t64_pi.c` |
| SP | no (SP, complete) event on this profile (the functional RSP moves SP DMA through RSP beats); at most one (SP, RSP issue) event whose token is the issue sequence and whose tick is the next issue tick, and none once that tick has passed | `start_rsp_dma` in `core/t64_sp.c`, `t64_rsp_schedule_next` in `core/t64_rsp.c` |
| RDP | at most one work event with token 0, none while the RDP is idle (not busy, rendering, fencing or holding a pending command); one work event while a command, a render, a fence or a texture rectangle is in flight unless a freeze or a flush holds the device | `schedule_next_committed` and `t64_rdp_event` in `core/t64_rdp.c` |
| VI | no event of a future generation; an enabled VI owns exactly one event of the current generation (one that waits for output capacity is accepted: the ring occupancy gates dispatch only); a disabled VI owns none of the current generation; older generations are stale and harmless | `t64_vi_write_register`, `t64_vi_event` in `core/t64_vi.c` |
| AI | an active, unpaused interface owns exactly one sample event with the current token; otherwise none; paused implies active and a positive remaining delay; active implies a queued descriptor | `t64_ai_write_register`, `t64_ai_event` in `core/t64_ai.c` |
| Input | any number, the port field of the value zero | `t64_machine_queue_input` in `core/t64_observe.c` |
| SI | idle: no events, no direction, no pending execute; write: one complete event with the active token at the complete tick, no execute event; read: the same, and while the execute is pending exactly one execute event with the active token at the execute tick with start at or below execute at or below complete | `start_dma`, `t64_si_execute_event`, `t64_si_complete_event` in `core/t64_si.c` |
| Save media | the stored kind equals the profile kind; no events | `core/t64_save.c` |

The uniform queue rules: the count is within the profile queue limit, tuples are
strictly ordered by tick and then sequence, a sequence is below `next_sequence`
(or at it when the exhausted flag is set, which requires `next_sequence` to equal
the sequence limit of the profile), a tick is at or above the machine tick of the
blob (not the live machine tick, because the commit replaces it), and the
(device, kind) pair is one of the ten production pairs of the event ledger; the
synthetic test device is refused.

### Commit

The commit runs after every check has accepted the blob, performs no allocation
and no host call, and cannot fail: every state structure the container describes
starts from zero and receives the blob values through the same reader functions in
apply mode (which evaluate no check), so a member the container does not carry,
and the dirty state of a peer that ran other work, cannot survive. The queue head
is 0 and the handlers are rebuilt from the closed table; the trace ring, the drop
count and the saturation flag are cleared and trace enablement is kept; the RSP
observation record is cleared; the RDRAM, SP memory, coverage tables and retained
packet slots are overwritten (the unused part of the packet storages is zeroed); the
retained video and audio packet strings (profile and build identity, not part of
the v25 state) are regenerated from the live machine for each produced slot.
Profile, content, ROM bytes, the allocator, the storage descriptors and every
pointer are untouched.

### Save media counters and the deadline bound

- `write_generation` (the observer counter of the save device, plan 09-05): one
  successful state import into a profile with a save device adds exactly one,
  saturating at the 64-bit maximum, as the last write of the commit; a refused
  import never touches it; replacement (`t64_machine_admit_rom`) resets it to 0; a
  profile without a save device keeps 0. It never travels in the container, is
  never read from a blob and stays outside the digest, so a peer imported twice from
  one blob holds a different generation than the source with an equal digest
  (case `save-media-roundtrip`).
- `busy_until` (the EEPROM deadline, section 11): an imported deadline is at most
  the blob tick plus the busy window of the profile save kind (375,000 ticks for
  the two EEPROM kinds, 0 for SRAM; rule `TICK_LE`, status `STATE_RANGE`). Only the
  channel-4 EEPROM execute path writes the deadline (`t64_save_channel4_execute` in
  `core/t64_save.c`) and its execute tick is never above the machine tick, so the
  bound holds for every reachable state and refuses a hostile far-future deadline
  that would make every later host save import answer `SAVE_BUSY` for the life of
  the machine. A deadline at or below the blob tick (zero, or the stale deadline of
  a finished window, which every machine that completed an EEPROM write exports) is
  accepted and kept so the digest is reproduced exactly. A different bound,
  requiring the deadline at or above the tick, was considered and rejected: it
  would refuse that stale deadline and fail the round trip itself. The consequence
  is tested: an accepted deadline equal to the blob tick plus the window makes a host
  save import answer `SAVE_BUSY` at the import tick and at the deadline minus one
  and succeed at the deadline tick.

### Observer continuity and the drop flag

By default an import restores the observer section: the output drain cursor and
the video and audio ring head and occupancy of the blob. Their ranges are checked
before use (`OBSERVER_RANGE`: the cursor at most the retained capture length, a head
below the ring capacity, an occupancy at most the ring capacity). Each value is
checked right after its read, in the order cursor (`LEN_LE`), video head
(`INDEX_LT`), video occupancy (`COUNT_CAP`), audio head, audio occupancy, so the
position of a refusal is the byte after the member that failed. The runner tracer
(`test_state_runner --run-mutant observer-range`) edits the drain cursor, the member
of the inventory row `observer-range`, and compares the full tuple (status, reason,
section, rule and position) that the importer of the same core library reports for
the same blob with the row, next to an accepted baseline. The flag
`T64_STATE_IMPORT_DROP_OBSERVERS` is a named policy: both ring occupancies and
heads become 0 and the drain cursor becomes the retained capture length of the
blob, so no pre-import byte is delivered again. The canonical digest is the same
either way (observer state is outside it). Observer state gates dispatch (a full
ring stalls the VI and the AI), so the two modes may differ in their stop points;
results are not claimed equal across submission schedules. Any other flag bit is
refused with `ARG_FLAGS`.

### Same-build ladder

The evidence of the import plan is a ladder of self-consistency checks of the
software model on authored machines of the composite family:

- T1 round trip: an SI read in flight (execute still pending, one CPU step event)
  exported and imported into a fresh peer gives the same canonical digest as the
  source and as the declared digest, the same queue tuples with the ten production
  handlers, queue head 0, an empty trace with its enablement kept and a clear
  scratch (case `roundtrip`).
- T2 export, import, export: byte identical for the idle machine, the SI read state
  and a machine with written SRAM words (case `idempotence`).
- T3 continuation: source and peer advanced from the import tick in one shot and in
  partitions of 1, 7 and 31 ticks for the first 310 ticks and in 4,096 tick steps,
  with the trace on and off, give equal digests at +310, +10,000 and +30,000 and
  equal drained ISViewer bytes; a one-field perturbation of the peer after the
  import is detected at the checkpoint where it is visible (case
  `continuation-partitions`).
- A peer that ran 40,000 further ticks of its own SI transaction, and a source that
  advanced 10,000 ticks and then imports its own earlier blob (a rewind), reproduce
  the digest and the continuation (case `dirty-peer`).

These are comparisons of the code with itself: a state that every schedule gets
wrong in the same way would pass them, and the ladder says nothing about a console.
Fields that the state 25 container does not carry (the RDP depth members and the
hidden depth table, which decision K1 keeps inactive on the state 25 family:
excluded for state 25, carried for state 26; the diagnostic last-coverage members
and the diagnostic observation record) are reset to zero by the commit, so a peer
that used them does not keep them. The state 26 container carries the depth
members (the section "Container v2 (state 26)"), and the importer of a state 26
machine restores them (plan 10-12). The RDP combiner block and the memory coverage
table are carried since plan 09-10 (the tail of section 8) and are therefore part
of the continuation ladder.

## Runner import surface

Flags (real mode, provisional): `--state-import PATH`, `--state-import-size N` and
`--state-import-sha256 HEX`, all three together and never defaulted, the same triple
shape as the schedule input. The supervisor copies the file into an exclusive private
snapshot with its exact size and SHA-256 verified (`t64_verified_input_prepare`,
ceiling 64 MiB), unlinks it and forwards the descriptor to the child as
`--state-import-fd N` through the checked argument helper (the per-option slot sum
counts 2 more slots: 45 of 64 including the terminating NULL, every family counted
as if all co-occurred). The child never opens the caller path. After the ROM is
admitted and before the first advance the child reads the exact size and calls
`t64_machine_state_import` with flags 0; the loop resumes from the guest tick of the
container header, which the importer cross-checked with the machine core section.

Refusals decided in the supervisor before any snapshot is prepared and before any
child starts, with detail `usage` and guest tick 0: an incomplete triple, a size of
zero or above the ceiling, a hash that is not 64 lowercase hex digits, synthetic mode,
raw capture, a custom schedule (`--input-size` and `--input-sha256`; the input path
itself is mandatory in real mode and keeps its default), and every flag of an earlier
artifact family. The child repeats the family rule for its forwarded options (a
directly started child refuses the same way, guest tick 0, before any import call) and
refuses a profile whose state serialization version is not 25 with the existing
prelaunch refusal (detail `refusal`); a state 26 profile is refused too, because the
runner has no state 26 import path (the state 26 import is proven at core level by
`unit.core.state.import-v26-roundtrip`, plan 10-12), and `runner.state-format-v26`
asserts the refusal.

An import the importer refuses, an unreadable snapshot, or a state whose tick is above
the bound ends in the existing prelaunch refusal (`refusal`, `refused`, guest tick 0;
the supervised record is the report-error classification that every prelaunch refusal
has) before the first advance. The closed reason name is written once to the child's
diagnostic stream (`state-import refused: <name>`); the result record keeps its
schema and never carries a reason, a path or a blob byte, and no state byte reaches any
log. `docs/spec/result-protocol.md` and the result and fixture sources are unchanged.

Evidence limit of a resumed run: the runner derives the pass verdict from milestones
and a retirement count it observes through its own trace, which a resumed run does not
have, so a resumed run never reports `pass`. A state exported in process at a tick inside
the long authored spin and imported through the runner resumes to the same completion
tick, the same final state digest and the same log bytes as the uninterrupted run, and
ends in the closed `harness-error` / `host-error` record (case
`import-accepted-continues`). A state published by the runner itself is exported after
the runner drained its whole log (observer cursor at the log length), so its default
import delivers no earlier byte while the machine's attempted total still counts them.
Such a run cannot know its log: it ends in the closed unknown-log form of the result
protocol (both log counts zero, no digest, `harness-error` / `host-error`, the same form a
saturated attempted total uses), which the checker and the supervisor accept, with the
uninterrupted run's guest tick and final state digest (plan 09-10, FX-11), asserted in
the same case and in the tracer baseline row `none`. Before that fix the child wrote a
record its own checker refused and the supervisor ended the run in the closed
`report-error` / `report-invalid` record.

## Option-pair matrix

One run has one artifact producer. Families: SAVE-IN (`--save-import`,
`--save-import-size`, `--save-import-sha256`, `--save-kind`, `--save-format`), SAVE-OUT
(`--save-export-leaf`), REPLAY-REC (`--replay-record`,
`--replay-checkpoint-interval`), REPLAY-PLAY (`--replay-play`, `--replay-play-size`,
`--replay-play-sha256`, `--replay-report`), STATE-OUT (`--state-export`,
`--state-export-format`), STATE-IN (`--state-import`, `--state-import-size`,
`--state-import-sha256`), INPUT (`--input-size`, `--input-sha256`; `--input` is
mandatory and defaulted), RAW (`--raw-capture-dir`) and PAIR (`--artifact-dir`,
`--artifact-max-bytes`).

Every pair of the nine families has a defined result; none is undefined (plan 09-10,
R-03). In the table below `R` is refused (`usage`, guest tick 0, nothing created), `A` is
accepted by the pair rules (the run goes on to its snapshot and child), and a dot is the
family with itself. Each family is complete in itself in every row, and the case
`argv-bound` of `runner/tests/test_phase09_runner.c` runs all 36 pairs, each family
alone as the accepted baseline, and the pairless forms below.

| | SAVE-IN | SAVE-OUT | REPLAY-REC | REPLAY-PLAY | STATE-OUT | STATE-IN | RAW | INPUT | PAIR |
|---|---|---|---|---|---|---|---|---|---|
| SAVE-IN | . | A | R | R | R | R | R | A | A |
| SAVE-OUT | A | . | R | R | R | R | R | A | A |
| REPLAY-REC | R | R | . | R | R | R | R | A | A |
| REPLAY-PLAY | R | R | R | . | R | R | R | A | A |
| STATE-OUT | R | R | R | R | . | R | R | A | A |
| STATE-IN | R | R | R | R | R | . | R | R | A |
| RAW | R | R | R | R | R | R | . | A | R |
| INPUT | A | A | A | A | A | R | A | . | A |
| PAIR | A | A | A | A | A | A | R | A | . |

The rule behind the table, in four statements:

1. The producing families are mutually exclusive: SAVE-IN, SAVE-OUT, REPLAY-REC,
   REPLAY-PLAY, STATE-OUT and STATE-IN refuse each other, with one exception: SAVE-IN
   with SAVE-OUT is the save round trip and is accepted.
2. RAW is refused with every Phase 9 producer and with PAIR (the artifact pair is
   mutually exclusive with raw capture, a Phase 8 contract).
3. SAVE-OUT, REPLAY-REC, REPLAY-PLAY and STATE-OUT need PAIR and are refused (`usage`)
   without it. SAVE-IN and STATE-IN do not need it; PAIR alone is accepted with them and
   no producer uses it.
4. INPUT (the custom schedule options `--input-size` and `--input-sha256`) is not an
   artifact producer. **Six combinations are accepted by design:** INPUT with SAVE-IN,
   SAVE-OUT, REPLAY-REC, REPLAY-PLAY and RAW, and INPUT with PAIR (INPUT with STATE-OUT is
   accepted as well, from plan 09-08). INPUT with STATE-IN is the only refused INPUT
   pair (plan 09-09: no custom schedule together with a state import).
   REPLAY-PLAY with a schedule that has events is accepted by the supervisor and ended
   by the child with a published `input-refused` report (a content rule, case
   `flag-combinations` of `runner.replay-mix`); the options themselves are not refused.

The pairless forms: SAVE-OUT, REPLAY-REC, REPLAY-PLAY and STATE-OUT without PAIR are
refused (`usage`). Synthetic mode refuses every Phase 9 flag, all nineteen of them
(`--artifact-dir`, `--artifact-max-bytes`, the six save flags, the six replay flags and
the five state flags), and the two Phase 8 families the same rule covers, RAW and
INPUT; the case runs each flag alone after a synthetic baseline that is not refused.
The runner repeats the family checks for the options it receives (a directly started
child refuses the same way, guest tick 0), and the direct `--save-import-fd` rows of
`runner.save-roundtrip` pin its descriptor shape.

The child argument array is bounded by one compile-time slot sum, 45 of 64 slots (the
runner path 1, fixture 2, input 2, profile 2, ticks 2, advance step 2, video 2, audio
2, raw capture 4, artifact pair 4, save 8, replay 8, state export 4, state import 2),
asserted in `runner/t64_supervisor.c`. The sum counts every family as if all
co-occurred, so it is an upper bound; `argv-bound` runs 19 of the 45 slots and
`argv-max-save` runs 27, and both end in non-pass records on a family id (the video
observation of an audio fixture), so they prove forwarding and parse, not a passing
maximum run. The state import triple cannot join them (its audio fixture is outside
the composite family, a 09-09 deviation), so the compile-time sum is its bound. The
replay rows state the rule in full: REPLAY-REC and REPLAY-PLAY are refused with RAW,
with every save flag, with each other and with both state families, and need PAIR.

The ordering proof: each one-producer row of the case `import-option-refusals`
passes a nonexistent import path; a supervisor that prepared the snapshot first would
report the snapshot refusal (detail `refusal`) instead of the pair refusal (detail
`usage`). The control row with the same nonexistent path and no conflicting flag
reports the snapshot refusal, which shows the pair refusal precedes the snapshot and
therefore any child start and any guest tick. Rows owned by the save and replay plans
are copied from their committed text; a disagreement between this table and the code
is a finding for the drift sweep, not an edit of an earlier plan.

Child-only names: the supervisor forwards each verified descriptor to the runner
under a child option, `--input-fd`, `--save-import-fd`, `--replay-fd` and
`--state-import-fd`. They are not supervisor options and a caller never passes
them. The runner accepts only an unlinked read-only regular file as a descriptor
(it cannot know where the descriptor came from); the declared size and the SHA-256 are
verified by the supervisor only, so a directly started child checks the shape of the
descriptor and, for a save image, the exact kind size, and skips the rest. Like every
Phase 9 flag name they are provisional until the Phase 11 freeze candidate (final ABI
bind at the end of Phase 12).

## Boundary matrix

Plan 09-09.1 proves the importer on the boundary classes the device models can
reach, not on every machine state. The matrix has seventeen rows, ids `B01` to
`B17` (the plan required sixteen; `B17` was added by the fix cycle of plan 09-10
for the carried RDP members, below); a further row or a substituted construction
source is a recorded deviation, and a row whose boundary cannot be reached is
reported, not removed.
The rows live in one table of `core/tests/test_state_import.c` and a new row is a
table row plus its builder.

Columns of a row: id, name, construction source (the named test builder, confirmed
by the reached predicate), the reached predicate, the one-field perturbation, the
queue events the device owns (the event pair), and the partition policy.

Rules shared by every row:

- **Reached.** The reached predicate is evaluated on the source machine before
  export; a false predicate fails with the row id and the unreached field. A builder
  that does not reach its boundary gives no evidence, so the predicate is part of the
  row, not of a comment.
- **Ladder.** T1: the imported peer digest equals the source digest and the declared
  digest. T2: `export(import(export(M)))` equals `export(M)` byte for byte. T3: the
  source (one-shot) and the peer (the row's partitions) agree on the full advance
  result tuple (status, stop reason, stop tick, machine tick, instruction and frame
  counters) and on the digest at the import tick, plus 310, plus 10,000 and plus
  30,000 ticks. Partitions are one-shot and 7 ticks on every row; 1, 31 and 4,096
  ticks run on the full rows and on every row when `T64_STATE_MATRIX_FULL=1`.
- **Perturbation.** One field of an imported copy is edited; the unperturbed peer
  agrees at the same checkpoints in the same setup and the perturbed peer differs
  at the import checkpoint and at the first continuation checkpoint. A comparison
  that cannot fail is not evidence.
- **Event rules.** For each queue event a device owns, a hash-valid mutant without
  the event is refused with `DEVICE_EVENT_MISSING` and one with its token increased
  by one with `DEVICE_TOKEN_MISMATCH`, after the legitimate exported state of the
  same row is accepted, so a rule that over-refuses a reachable state or admits a
  broken one is caught from both sides.

Limit: every row compares the software model with itself. The evidence is that the
model reproduces its own state across an export and an import, not that the state is
reachable on hardware, authentic or compatible with another emulator. The test
environment scales the matrix (`T64_STATE_MATRIX_FULL`): the gate, hosted and
sanitizer runs are a sample (the 7 tick partition on thirteen of the seventeen rows, every
partition size on the four full rows (B01, B07, B10 and B14), and the instruction-count stop row of the
mid-tick case only in the full matrix), and the full set runs in one recorded native
qualification step (`normal-state-import-full-matrix`), not under the sanitizer.

| Id | Name | Construction source | Reached | Perturbation |
| --- | --- | --- | --- | --- |
| B01 | pi-active | `test_pi_dma.c` busy start on the stem id: a cartridge read of 64 bytes started through the host bus and stopped one tick later | PI active, not canceled, one (PI, COMPLETE) event whose token is the active token and whose tick is above the machine tick | the active transfer DRAM address plus 8 |
| B02 | pi-canceled | `test_pi_dma.c` cancel: the B01 transfer reset through the PI status register | PI active and canceled, one (PI, COMPLETE) event with the active token | the canceled flag cleared |
| B03 | sp-active-pending | `test_sp_dma.c` pending queue case on a quiet machine (CPU spin loop) with distinct source bytes | an active SP descriptor and a valid pending slot | the pending descriptor DRAM source address plus 8 |
| B04 | rsp-mid-program | `test_rsp_control.c` load-store bubble program, looped, stopped at tick 4 | RSP running, memory ready tick and next issue tick above the machine tick, one (SP, RSP_ISSUE) event | the stored register (the data memory word it writes) |
| B05 | rdp-pending-words | `test_rdp_triangles.c` raw words submitted through the DP registers; a command word fetched and waiting for its apply event | `command_pending` with the pending command word, no triangle words, a (RDP, WORK) event and a non-zero triangle coverage word (direct test-only edit) | one bit of the pending command word (the image width) |
| B06 | vi-midfield-retained | `test_video_output.c` advance_one_field (authored 5551 pixels) | VI enabled, at least one retained packet, a current-generation (VI, FIELD) event | the VI phase remainder plus 1 |
| B07 | vi-stalled-event | the same frame advanced until the two-packet queue is full and two further calls return the same resource-limit stop at the same tick | `packet_count` equals the queue capacity, the queue front is the current (VI, FIELD) event, the stalling advance returned the resource-limit stop | `next_boundary_units` plus 1 |
| B08 | ai-two-descriptors | `test_ai.c` prepare_two_descriptor_stream on a quiet machine | two descriptors queued and the sample event present | the second descriptor length minus 8 |
| B09 | ai-paused-partial | `test_ai.c` pause phase resume (17 ticks before the next sample) | paused, a positive remaining delay, a partial packet length above 0, no sample event | the paused remaining ticks plus 1 |
| B10 | ai-output-full-stall | `test_ai.c` output-full same event (two completed packets, a third descriptor) | the queue front sample event waits for output capacity and the stalling advance returned the resource-limit stop | the AI phase remainder plus 1 |

| B11 | cpu-delay-dirty-tlb | `test_cpu_time.c` delay slot (a jump to itself) with a cached store, `test_cpu_path.c` TLB pair (direct test-only edit) | CPU between a jump and its delay slot, a dirty D-cache line, one valid TLB pair, one (CPU, STEP) event | one byte of the dirty line |
| B12 | coincident-events | `test_determinism.c` same-tick order: a PI completion and the CPU step of the same tick (the transfer length is searched over 16 values) | a (PI, COMPLETE) and a (CPU, STEP) event with equal ticks | the PI event token plus 1 |
| B13 | near-exhaustion | `test_scheduler.c` transactional exhaustion: queue room for two events and four sequence numbers, set in memory after create identically on source and peers | room 2, headroom 4, the 64-bit sequence width | `next_sequence` minus 1 |
| B14 | si-mid-transaction | the S1 builder: an RD64B in flight before its execute | active read, execute pending, (SI, EXECUTE) and (SI, COMPLETE) events | `si.execute_tick` plus 1 |
| B15 | eeprom-busy | `test_save.c` tracer on the `-e4k` id: a block write through channel 4 by a WR64B and an RD64B | `busy_until` above the machine tick, SI idle | `busy_until` plus 1 |
| B16 | pending-input | `t64_machine_queue_input` stamped 20,000 ticks ahead on S1 | an (INPUT, SUBMITTED) event queued above the tick | one button bit of the event value |
| B17 | rdp-triangle-render | `test_rdp_triangles.c` edge words after an 8 by 8 fill and the primitive, environment, fog, blend, key and combine registers through the DP registers, stopped after the first row of the triangle | `render_active` with `render_triangle` and a render row above the first row, the combiner block set, 72 memory coverage entries, one (RDP, WORK) event | the fog color (a hashed member that no event overwrites) |

Observer outcomes (rows B07 and B10, D-29). A default import restores the observer
packets, so the occupancy gate that held the event on the source holds it on the
peer: advancing both to a tick beyond the stalled event returns the same
resource-limit stop at the same tick with the same result tuple, and the canonical
digests are equal. An import with the drop-observers flag leaves the canonical
digest equal at once (the packet occupancy is outside it) but the peer has no
retained packet, so the gate is open and the event dispatches: the peer reports a
later stop point (the bound, in the recorded case). After the source drains its
stalled packets and both machines reach the same tick, the digests of the source,
the default peer and the drop peer are equal. The flag changes stop points only;
no equality across submission schedules or host drain schedules is claimed.
Recorded literals (stem id, gate build): row B07 stalls at tick 246 with the event
at 247 and the target 248, the source and the default peer return the resource-limit
stop at tick 246, the drop peer returns the bound at tick 248; row B10 stalls at
tick 7,086, target 7,088, same relation.

### Limits of the matrix

- **Hidden RDP members (closed by plan 09-10).** The RDP combiner block and the
  RDP memory-coverage table were once outside the v25 state: the importer zeroed
  them, a state with a pending triangle or a VI scan-out over coverage marks could
  not continue equal, and the rows B05 to B07 used the nearest builders that avoid
  them. The review of plan 09-10 reproduced the loss (the same triangle drew on the
  source and was refused on the peer) and the family state now carries both in the
  tail of section 8, so the importer restores them and the digest sees them. Row
  B17 holds a triangle render in progress after a fill and the combiner registers;
  the rows B06 and B07 build the VI frame with its coverage marks; the case
  `rdp-state-carry` draws one triangle on the source, on the imported peer and on a
  control peer without the members (the behavior of the old import), and the
  authored fixture `rdp-fill` shows the coverage table carried from real guest
  code.
- **The RSP issue event is optional.** A running RSP without its issue event is a
  legitimate wedged state (core/t64_rsp.c t64_rsp_issue_event leaves it unscheduled
  after an unsupported opcode or an out-of-bank pc), so the drop mutant of the
  (SP, RSP_ISSUE) pair is accepted and the machine returns from a bounded advance.
  The token of a queued issue event is still checked.
- **The RDP continuation event is required** while an operation is in flight and
  the device is not frozen or flushed (found by row B05, rule added to the
  per-device pass: `DEVICE_EVENT_MISSING`).
- Perturbations are one-field edits that persist to the first continuation
  checkpoint; a field that the next event overwrites (the RSP next issue tick, the
  pending SP descriptor length) cannot be a perturbation.

### Required set, mid-tick resume and recorded deviations

The required set is sixteen rows, B01 to B16; none was removed or left unreached,
and plan 09-10 added B17. The D-36 case `mid-tick-resume` is the stop-mode variant of B12 and
is not a seventeenth boundary: B12 keeps its constructed builder and the case adds
states reached by production stops.

`mid-tick-resume` builds the all-devices workload W-ALL of the scheduler matrix
(a local copy of its builder: PI, SP and RSP, RDP, AI, VI and SI work, three inputs,
a horizon of 60,000 ticks) and stops it in the middle of a tick, with the queue front
still at the machine tick and an event of another device queued at that tick:
event budget 1, event budget 2 (the in-memory copy of the profile budget edited
after create), an instruction-count stop (searched over targets 1 to 4,096, run in
the native sanitizer preset) and a budget stop at the VI and AI coincidence tick
1,418. Each state exports and imports into a fresh peer with the default budget
(T1, T2); the source continues under its own budget and the peer in slices to the
horizon and both end at the digest, queue snapshot and result tuple of the
uninterrupted run (T3), also after partitions of the first 310 ticks; a one-field
perturbation of the imported peer changes the digest at the horizon. The 09-06
synthetic-event state (five synthetic instruction events and three synthetic frame
events at one tick) exports with OK and is refused on import with
`QUEUE_KIND_UNKNOWN` (rule `ENUM_CLOSED`, section 6) and a bit-identical peer,
because the closed handler table refuses the synthetic device; it is kept only as
that refused control. The evidence is the self-consistency of the software model,
and nothing is claimed across submission schedules.

Deviations from the plan text, each from the cited device source:

1. B02 is a canceled transfer that is still active with its retained completion
   (the cancel clears the active flag only at the event), not an idle PI with a
   stale event: the importer rule, and the source, allow no queued PI event on an
   idle PI.
2. B03 names the (SP, RSP issue) event (the profile has no SP completion event) and
   perturbs the pending DRAM source address, because the pending length is consumed
   at promotion. B04 perturbs the stored register, because the next issue tick is
   overwritten by the next issue. B12 perturbs the PI token, because both sequence
   numbers are erased at their dispatch.
3. B05 holds a plain command word pending (a pending triangle packet cannot continue
   equal: the combine mode is outside the v25 state).
4. The drop mutants of the (SP, RSP issue) and (INPUT, SUBMITTED) pairs are accepted
   (see "Limits of the matrix"); the token of an input event is never read.
5. B13: the queue capacity and the sequence width are profile members that the blob
   does not carry. A peer with the identical in-memory edit accepts and continues
   with the same result tuple (a resource-limit stop at tick 6 after five CPU
   steps); a peer without the edit also accepts and continues identically because
   the exhaustion is the sequence counter's; a peer whose capacity is below the
   blob queue count is refused at section 6 (`QUEUE_COUNT`, rule `COUNT_CAP`).
6. `mid-tick-resume` row s4 is a budget stop at the coincidence tick: the frame
   event counter is incremented only by the synthetic frame event, so a
   frame-event-count stop cannot happen on production events (the synthetic part
   keeps its frame events).
7. Cost control (D-40): in the gate preset every non-full row compares the peer
   advanced in 7-tick partitions with the one-shot source reference (the one-shot
   peer, 1, 31 and 4,096 run on the full rows and under `T64_STATE_MATRIX_FULL`);
   each distinct queue event pair is checked on its first row (all rows under
   `T64_STATE_MATRIX_FULL`); the digest at the import checkpoint is the T1 case; the
   whole-machine snapshot of the refusal rows compares the allocation, both packet
   storages and the image byte for byte and no longer hashes the state twice.


## Field-use audit

`scripts/check-state-field-use.py` (plan 09-09.1, decision D-45) closes the gap
between the importer's readers and the members that device code uses to index
storage, bound a loop, shift or size a memory function. `--list` is the scan
(plan 09-09): the members of the structures reachable from `struct t64_machine`
that occur in a subscript, a loop comparison, the right operand of a shift or the
length argument of a memory function, directly or through a local that carries them
(93 member names on the gate tree: 72 from the scan of plan 09-09, 20 found by
the nested-access fix of plan 09-10 and `rdp_depth_hidden`, which the state 26
reader of plan 10-12 loops over). `--check` reads the mapping
`docs/spec/state-field-rules.txt`, one or more rows per used member (a name that
several structures share, or that is bounded two ways, has one row per distinct
bound: `render_y` is `ADDR_RANGE` for its RDRAM use and `INDEX_LT` for the coverage
subscript), and fails on an unmapped member, a stale row (a member no longer used),
an identical row twice, an unknown rule, a
placeholder (`?`, `TODO`), a reader that is not a file-scope function of
`core/t64_state.c`, a reader whose body does not mention the member, and a
`MASKED_AT_USE` row without a consuming site that exists and mentions the member.
`--selftest` runs the six cases listed by `--list-cases` on temporary copies of the
tree under `build/`: the unmutated copy passes first, then five seeds (a new indexed
member, a new member reached by a nested access `machine->pi.member`, a removed row,
a misspelled rule, a reader that lacks the member) each fail with their own finding
text. The CTest name is `harness.state-field-use-audit`.

Rules are the `T64_STATE_RULE_*` names without the prefix. `NONE` names an aggregate
member (its scalar members are mapped under their own names) or a constant-size
array used only under `sizeof`; `PROFILE_DERIVED` a member the blob never supplies
(the commit zeroes it or the profile fixes it); `DERIVED_EQ` a member equal to a value
the reader recomputes or fixes; `MASKED_AT_USE` a member whose blob value is masked or
validated where it is consumed, with that consuming site named.

What the audit proves and what it does not. It proves that every member the scan
reports has a named rule in a named reader and that a new used member cannot land
unmapped. It does not prove the rule is correct or complete; that is the work of the
structure-aware mutants, the sweep and the sanitizer-scaled campaign of
`core/tests/test_state_import.c`. The scan matches `->member`, `.member` after any
expression (including a nested access `machine->rsp.dma_beat_cursor` and an indexed
one `cpu->tlb[i].asid`) and a local that carries them. It is lexical: it does not
resolve types, a name that several structures share is mapped for every one of its
bounds but not per structure, and a reader that only mentions the token satisfies a
row, so the audit lists the members and their rules and the sweep, the campaign and
the mutants evidence that the rules are sufficient (the 93 names are the used members
of 472 declared).

Triangle render in progress (plan 09-10 fix cycle, review L2 CR-02, audit T-09-67). The rows
of a triangle run after the command was accepted, from the packet words and the
registers, and index the 1,024-row triangle coverage array and 64-bit shade and
texture products that the rasterizer proved bounded only at acceptance
(`t64_rdp_triangle_extent` in `core/t64_rdp_raster.c`, shared by the acceptance and
the reader). The reader now establishes that proof from the blob values for every
state with `render_active` and `render_triangle`, in this order, each refusal being a
`STATE_RANGE` report in section 8: the rows `render_y` and `render_y_end` below the
coverage height 1,024 (`FIELD_RANGE`, `INDEX_LT`); the opcode of word 0 one the
profile accepts, that is 0x08, 0x0a, 0x0c or 0x0e on the composite family
(`FIELD_RANGE`, `ENUM_CLOSED`); the packet word count equal to the count of the
opcode (`FIELD_DERIVED`, `DERIVED_EQ`); the shade flag equal to the shade form of the
opcode (`DERIVED_EQ`); the extent and the shade and texture products inside the
bounds (`FIELD_RANGE`, `LEN_LE`); `render_y_begin` and `render_y_end` equal to the
first and last row the extent gives (`DERIVED_EQ`); and `render_y` between them
(`INDEX_LT`). A triangle render flag without a render in progress is refused as well
(`DERIVED_EQ`), because a stale flag would run the next fill render through the
triangle row handler. The tile and TMEM state of a texture triangle is not available
to the reader (it is read after the render fields) and is checked at use: the TMEM
accessors validate the tile and every address of each pixel. The mutants
`render-triangle-*` of the inventory carry one accepted baseline and one refusal per
rule.

Carried RDP tail (plan 09-10, state content change). Section 8 ends with the RDP
combiner block and the memory coverage table (see
[machine-contract.md](machine-contract.md)); the reader checks, in order of
appearance: the primitive LOD fraction at most 0xff, the three colors and the combine
mode unconstrained (any 32 or 64-bit register value is what the acceptance stores),
the seven valid flags as booleans (`BOOL_01`), each key center and scale at most 0xff
and each key width at most 0xfff (`INDEX_LT`, the widths of the registers they are
stored from), the table size word equal to the profile derived size
(`PROFILE_DERIVED`) and every table entry either clear or the valid bit with a
coverage of 0 to 8 (`ENUM_CLOSED`). The mutants `rdp-carried-*` of the inventory
carry one refusal per rule.

Reader bounds added by the audit. The nine members the 09-09 hand-off listed as
unbounded were checked against their consuming code. The RDP render rectangle was
the first real hole (the triangle coverage rows and products were the second, found by the 09-10 review and closed by the triangle render rules below): the fill, copy and triangle row handlers index RDRAM from
`color_image_address`, `color_image_width`, `render_x_begin`, `render_x_end`,
`render_y` and `render_y_end` without checking them again, because the check ran when
the command was accepted. The reader now requires, for a render in progress, the
acceptance extents (the last pixel of the last row inside RDRAM, an aligned image,
`render_x_end` inside the image width) with reason `FIELD_RANGE` and rule
`ADDR_RANGE`; the other listed members are masked or validated at use (the VI
geometry, the DP command span, the SP DMA start, the TLB page mask switch, the RSP
register masks) and are mapped `MASKED_AT_USE` with their sites.

## Hostile-input evidence

Plan 09-09.1 completes the importer evidence with structure-aware mutants, a sweep,
a seeded campaign and a liveness list. The contract stays memory safety and
liveness: an accepted state is never claimed reachable, authentic, hardware accurate
or compatible, and no test states that it is.

**Mutant inventory.** `docs/spec/state-mutants.txt` is the independently authored
inventory: one row per mutant with its expected status, reason, section and rule,
grouped under the headings header (9 rows), table (6), fields (31, one per rule id with
a reader use, plus the observer range, the EEPROM deadline beyond the window, the
RDP render extent of a fill render, the eight triangle render rules and the five
rules of the carried RDP tail of plan 09-10),
queue (8), devices (10) and digest (1), 65 in all. The C mutant
table of `core/tests/test_state_import.c` must equal the inventory name by name, in
order, tuple by tuple (case `mutant-inventory`, the file named by
`T64_STATE_MUTANT_INVENTORY`; `--list-mutants` prints the names). Every mutant keeps
magic, section hashes, trailer and, unless its row targets it, the declared digest
valid, is applied next to an accepted unmutated baseline in the same setup, and must
reach exactly its own tuple with the machine allocation, both packet storages and the
image byte identical and the allocator counters unchanged (case `mutant-refusals`).
Five reader-rule removals made out of the tree (a recorded textual edit of a copy of
`core/t64_state.c`, compiled with the gate flags under `-Werror`, linked into a copy
of the test) are each caught by their named row and by no other: the queue count cap
(`queue-count-cap`), the audio reservation slot index (`index-lt-audio-slot`), the
video packet length (`len-le-video-packet`), the queue sequence bound
(`sequence-at-next` and `seq-lt-sequence-above-next`) and the closed (device, kind)
lookup (`unknown-pair` and `synthetic-device`).

**Sweep and campaign.** The sweep flips one bit at sampled header bytes and at body
positions inside named windows of the S1 blob (the rule-bearing fields of section 8:
the RDP scalars and packet words, the tile descriptors and the VI block, and the
carried tail; whole small sections), and, when `T64_STATE_SWEEP_BLOBS` is above 1, of
the boundary blobs B05, B07, B10, B04 and B14, in two integrity modes (every field
stale, every field re-signed). The campaign (`core/tests/fuzz_state_import.c`) is a
fixed seed xorshift run of `T64_STATE_FUZZ_ITERATIONS` iterations over byte flips and
boundary-value field replacement (0, 1, maximum minus one, maximum, 0x7FFFFFFF,
0x80000000, 0xFFFFFFFF, 2^32), seven draws in ten, at positions inside the same
windows three times in four and otherwise in a section other than RDRAM and SP memory,
and over truncation and extension (the header total follows the new length) and swapped
and duplicated section entries, three draws in ten. Six draws in eight re-sign every
integrity field, one keeps the declared digest stale and one leaves all stale; a
second run uses a base blob with a shaded triangle render in progress. Reach is
asserted, not assumed: at least a quarter of the iterations of each run must be
refused by a reader or a later check (reason `FIELD_RANGE` or above) or be accepted,
and the sweep asserts the same for its flips (the first version ended 12 of 12
iterations at the container level, and its `refusals plus acceptances equals
iterations` assertion was true by construction).
Every outcome must be a closed refusal (the status and reason pair agrees with the
reason to status table above) or an acceptance; every refusal leaves the whole
machine allocation byte identical with the allocator counters unchanged and the
scratch clear; every acceptance returns from a bounded advance. The statistics
record (iterations, refusals by reason, acceptances, liveness stops) is printed at
the end. Counts are scaled by the environment: the gate preset samples (6 header and
6 body offsets of the S1 blob, 12 iterations), the sanitizer preset raises them
(12 and 12 offsets over two blobs, 120 iterations), because every input costs several
hashing passes over a 6.3 MB container and sanitizer instrumentation multiplies that
about 22 times. These are samples, not an exhaustive search; the audit and the
rules are the structural defence and the sanitizer runs are the memory-safety
evidence for the inputs tried.

**Liveness.** The case `liveness-bounded-advance` builds semantically unreachable
states by editing a source machine and exporting it (a hash-valid blob by
construction): a VI with a zero vertical period or a zero horizontal sync, an AI
descriptor of length 0 and of the maximum, a queue event at the 64-bit maximum tick
and one below it, an SI table of five entries at their maximum counts, a PI or SP
transfer of length 0, an RDP render range inverted, a CPU program counter at the top of
the address space, an EEPROM deadline at the window and at the 64-bit maximum, and
`next_sequence` at the maximum. An accepted state returns from an advance of 2,000
ticks with a defined status; a refused one is a closed refusal. The recorded
outcomes on the gate build: ten are accepted and return, four are refused (the AI
descriptor of length 0 and the SP transfer of length 0 and the PI transfer of length 0
by a field rule, the EEPROM deadline at the maximum by `TICK_LE`). That a state is
accepted is not a statement that the machine can reach it.
