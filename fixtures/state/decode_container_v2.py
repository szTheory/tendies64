#!/usr/bin/env python3
"""Independent decoder for the Tendies64 state container, versions 1 and 2.

Version 1 is the state 25 container of plan 09-08 and version 2 is the state 26
container of plan 10-11. This file is a separate script on purpose: the version 1
decoder fixtures/state/decode_container.py is a closed Phase 9 oracle whose
SHA-256 is pinned (the F2 oracle pins of harness.phase09-qualification and the
state-decoder-sha256 revision of Phase 9 claim block 8), so it stays byte for byte
as it was and this decoder carries its code paths forward without editing it.
For a version 1 blob the accepted line and every refusal are exactly those of the
version 1 decoder (a parity test compares the two outputs byte for byte); the
version 2 additions are only new keys of the accepted line and the pairing rule.

Written from the container layout only, with its own literal offsets and the
standard library alone; it shares no code with the core exporter or with the C
test decoders. It reads a blob once, bounded by the 64 MiB container ceiling,
never indexes past the data, and reports either one accepted JSON line or one
refusal with a kind from a closed list.

Version pairing: the container version word at 0x008 is 2 exactly when the state
serialization version word at 0x00C is 26, and 1 for every other state word.
A blob with word 2 and another state word, or with word 1 and state 26, is
refused with the kind container-version. The domain bytes at the canonical
offset are t64.state.si.25 for version 1 and t64.state.si.26 for version 2.

Version 2 section 8 (RDP and VI), offsets from the body start, all big-endian:
  196          u32 pending_triangle_word_count n (at most 22)
  200          n x u64 pending triangle words
  200 + 8n     seven u32: primitive_color, primitive_color_valid,
               triangle_count, render_triangle, shade_triangle,
               coverage width (1024), coverage height (1024)
  228 + 8n     2,097,152 bytes of triangle coverage (1024 x 1024 x u16)
  S = 228 + 8n + 2,097,152
  S            five u32 depth scalars: depth_image_address,
               depth_image_valid, primitive_z, primitive_dz,
               primitive_depth_valid (the two valid words are 0 or 1)
and at the end of the body, in this order: u32 coverage length and the memory
coverage table (RDRAM bytes / 2), then u32 hidden length and the hidden depth
table of the same size, ending the body. The decoder reports the scalars, the
hidden table length and offset and the SHA-256 of the hidden table.

The section hashes and the trailer are integrity checks, not authentication: a
blob whose hashes were recomputed after tampering still decodes, and semantic
validation belongs to the importer. This decoder makes no hardware, timing,
cross-build or other-emulator claim. State blobs are build-identity-bound and
outside the ABI promise.

usage: decode_container_v2.py --blob PATH     one JSON line; exit 0 accepted,
                                              1 refused with a kind, 2 usage
       decode_container_v2.py --print-layout  the literal offset table
"""
import hashlib
import json
import struct
import sys

# ---- the layout, all integers big-endian, offsets from the blob start -------
MAGIC = b"T64STATE"
CEILING = 64 * 1024 * 1024
MAX_SECTIONS = 32
DOMAIN_V1 = b"t64.state.si.25"
DOMAIN_V2 = b"t64.state.si.26"
STATE_V2 = 26
OBSERVER_ID = 16
OBSERVER_BYTES = 40
TRAILER_BYTES = 32
ENTRY_BYTES = 56
RDRAM_ID = 3
RDP_VI_ID = 8

# (name, offset, width) of the 0x120-byte header
HEADER = (
    ("magic", 0x000, 8),
    ("container_version", 0x008, 4),
    ("state_serialization_version", 0x00C, 4),
    ("total_length", 0x010, 8),
    ("section_count", 0x018, 4),
    ("flags", 0x01C, 4),
    ("profile_digest", 0x020, 32),
    ("content_sha256", 0x040, 32),
    ("content_length", 0x060, 8),
    ("guest_tick", 0x068, 8),
    ("declared_canonical_digest", 0x070, 32),
    ("canonical_offset", 0x090, 8),
    ("canonical_length", 0x098, 8),
    ("build_id", 0x0A0, 128),
)
HEADER_BYTES = 0x120
# (name, offset, width) of one 56-byte section table entry, the table at 0x120
ENTRY = (
    ("id", 0, 4),
    ("flags", 4, 4),
    ("body_offset", 8, 8),
    ("body_length", 16, 8),
    ("body_sha256", 24, 32),
)
# Section 8 of a version 2 blob (offsets from the body start)
S8_TRIANGLE_WORD_COUNT = 196
S8_TRIANGLE_WORDS = 200
S8_TRIANGLE_MAX_WORDS = 22
S8_TRIANGLE_TAIL_WORDS = 7
S8_COVERAGE_DIMENSION = 1024
S8_TRIANGLE_COVERAGE_BYTES = 1024 * 1024 * 2
S8_DEPTH_SCALARS = (
    "depth_image_address", "depth_image_valid", "primitive_z",
    "primitive_dz", "primitive_depth_valid",
)

KINDS = (
    "magic", "container-version", "total-length", "section-count", "flags",
    "table-order", "section-range", "marker", "section-hash",
    "canonical-digest", "observer", "trailer",
)


class Refused(Exception):
    def __init__(self, kind, detail):
        super().__init__(detail)
        assert kind in KINDS
        self.kind = kind
        self.detail = detail


def u32(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


def u64(data, offset):
    return struct.unpack_from(">Q", data, offset)[0]


def sha(data, offset, length):
    return hashlib.sha256(memoryview(data)[offset:offset + length]).digest()


def expected_container_version(state_word):
    return 2 if state_word == STATE_V2 else 1


def decode_depth(data, entries):
    """The version 2 additions: the depth scalars and the hidden table."""
    by_id = {entry["id"]: entry for entry in entries}
    if RDP_VI_ID not in by_id or RDRAM_ID not in by_id:
        raise Refused("section-range", "section 3 or 8 is missing")
    rdram = by_id[RDRAM_ID]
    body = by_id[RDP_VI_ID]
    start = body["offset"]
    length = body["length"]
    if rdram["length"] < 4 or u32(data, rdram["offset"]) != rdram["length"] - 4:
        raise Refused("section-range", "the RDRAM length word is wrong")
    table_bytes = (rdram["length"] - 4) // 2
    if length < S8_TRIANGLE_WORD_COUNT + 4:
        raise Refused("section-range", "section 8 is too short")
    count = u32(data, start + S8_TRIANGLE_WORD_COUNT)
    if count > S8_TRIANGLE_MAX_WORDS:
        raise Refused("section-range", "pending triangle word count is too large")
    tail = S8_TRIANGLE_WORDS + 8 * count
    scalars = tail + 4 * S8_TRIANGLE_TAIL_WORDS + S8_TRIANGLE_COVERAGE_BYTES
    hidden_begin = length - table_bytes
    coverage_begin = hidden_begin - 4 - table_bytes
    if scalars + 20 > coverage_begin - 4 - 88 or coverage_begin < 4 + 88:
        raise Refused("section-range", "section 8 is too short for the depth block")
    if u32(data, start + tail + 20) != S8_COVERAGE_DIMENSION or \
            u32(data, start + tail + 24) != S8_COVERAGE_DIMENSION:
        raise Refused("section-range", "triangle coverage dimensions differ")
    if u32(data, start + coverage_begin - 4) != table_bytes or \
            u32(data, start + hidden_begin - 4) != table_bytes:
        raise Refused("section-range", "a coverage or hidden length word differs")
    values = [u32(data, start + scalars + 4 * k) for k in range(5)]
    if values[1] > 1 or values[4] > 1:
        raise Refused("section-range", "a depth valid word is not 0 or 1")
    record = {
        "depth_scalars_offset": str(start + scalars),
        "hidden_table_length": str(table_bytes),
        "hidden_table_offset": str(start + hidden_begin),
        "hidden_table_sha256": sha(data, start + hidden_begin, table_bytes).hex(),
    }
    for name, value in zip(S8_DEPTH_SCALARS, values):
        record[name] = str(value)
    return record


def decode(data):
    size = len(data)
    if size < HEADER_BYTES:
        raise Refused("total-length", "shorter than the header")
    if data[0:8] != MAGIC:
        raise Refused("magic", "magic bytes differ")
    version = u32(data, 0x008)
    state_word = u32(data, 0x00C)
    if version != expected_container_version(state_word):
        # The message of the version 1 decoder is kept for every state word it
        # accepted with version 1.
        raise Refused("container-version",
                      "container version is not 1" if state_word != STATE_V2
                      else "container version is not 2 for state 26")
    domain = DOMAIN_V2 if version == 2 else DOMAIN_V1
    total = u64(data, 0x010)
    if total != size:
        raise Refused("total-length", "declared total differs from the size")
    count = u32(data, 0x018)
    if count < 2 or count > MAX_SECTIONS or \
            HEADER_BYTES + ENTRY_BYTES * count > size:
        raise Refused("section-count", "section count out of range")
    if u32(data, 0x01C) != 0:
        raise Refused("flags", "header flags are not zero")
    canonical_offset = u64(data, 0x090)
    canonical_length = u64(data, 0x098)
    entries = []
    for index in range(count):
        base = HEADER_BYTES + ENTRY_BYTES * index
        entries.append({
            "id": u32(data, base + 0),
            "flags": u32(data, base + 4),
            "offset": u64(data, base + 8),
            "length": u64(data, base + 16),
            "hash": bytes(data[base + 24:base + 56]),
        })
    for index in range(1, count):
        if entries[index]["id"] <= entries[index - 1]["id"]:
            raise Refused("table-order", "ids are not strictly ascending")
    table_end = HEADER_BYTES + ENTRY_BYTES * count
    if canonical_offset != table_end or \
            canonical_length > size - canonical_offset or \
            canonical_offset + canonical_length + OBSERVER_BYTES + \
            TRAILER_BYTES != size:
        raise Refused("section-range", "canonical region does not fit")
    canonical_end = canonical_offset + canonical_length
    last = entries[-1]
    if last["id"] != OBSERVER_ID or last["flags"] != 1 or \
            last["offset"] != canonical_end or last["length"] != OBSERVER_BYTES:
        raise Refused("observer", "the last entry is not the observer section")
    for entry in entries[:-1]:
        if entry["flags"] != 0:
            raise Refused("flags", "a canonical entry carries flags")
    if canonical_length < len(domain) or \
            data[canonical_offset:canonical_offset + len(domain)] != domain:
        raise Refused("marker", "the domain bytes are wrong")
    expect_marker = canonical_offset + len(domain)
    for entry in entries[:-1]:
        if entry["offset"] < canonical_offset + 4 or \
                entry["offset"] > canonical_end:
            raise Refused("section-range", "body offset outside the region")
        if u32(data, entry["offset"] - 4) != entry["id"]:
            raise Refused("marker", "the id before a body differs")
        if entry["offset"] != expect_marker + 4 or \
                entry["length"] > canonical_end - entry["offset"]:
            raise Refused("section-range", "bodies are not contiguous")
        expect_marker = entry["offset"] + entry["length"]
    if expect_marker != canonical_end:
        raise Refused("section-range", "bodies do not end the region")
    for entry in entries:
        if sha(data, entry["offset"], entry["length"]) != entry["hash"]:
            raise Refused("section-hash", "a body hash differs")
    declared = bytes(data[0x070:0x090])
    if sha(data, canonical_offset, canonical_length) != declared:
        raise Refused("canonical-digest", "declared digest differs")
    observer = [u64(data, canonical_end + 8 * k) for k in range(5)]
    if sha(data, 0, size - TRAILER_BYTES) != bytes(data[size - TRAILER_BYTES:]):
        raise Refused("trailer", "the trailer hash differs")
    build = bytes(data[0x0A0:0x120])
    build_text = build.split(b"\0", 1)[0].decode("ascii", "replace")
    record = {
        "status": "accepted",
        "kind": "ok",
        "total_length": str(total),
        "state_serialization_version": str(state_word),
        "section_ids": [str(entry["id"]) for entry in entries],
        "canonical_digest": declared.hex(),
        "guest_tick": str(u64(data, 0x068)),
        "profile_digest": bytes(data[0x020:0x040]).hex(),
        "content_sha256": bytes(data[0x040:0x060]).hex(),
        "content_length": str(u64(data, 0x060)),
        "build_id": build_text,
        "observer": [str(value) for value in observer],
    }
    if version == 2:
        record["container_version"] = "2"
        record.update(decode_depth(data, entries))
    return record


def print_layout():
    for name, offset, width in HEADER:
        print("header.%s 0x%03x %d" % (name, offset, width))
    print("header.bytes 0x%03x" % HEADER_BYTES)
    for name, offset, width in ENTRY:
        print("entry.%s %d %d" % (name, offset, width))
    print("entry.bytes %d" % ENTRY_BYTES)
    print("canonical.domain.v1 %s" % DOMAIN_V1.decode("ascii"))
    print("canonical.domain.v2 %s" % DOMAIN_V2.decode("ascii"))
    print("observer.id %d" % OBSERVER_ID)
    print("observer.bytes %d" % OBSERVER_BYTES)
    print("trailer.bytes %d" % TRAILER_BYTES)
    print("max.sections %d" % MAX_SECTIONS)
    print("max.bytes %d" % CEILING)
    print("section8.triangle_word_count %d" % S8_TRIANGLE_WORD_COUNT)
    print("section8.triangle_words %d" % S8_TRIANGLE_WORDS)
    print("section8.triangle_coverage_bytes %d" % S8_TRIANGLE_COVERAGE_BYTES)
    return 0


def main(argv):
    if len(argv) == 2 and argv[1] == "--print-layout":
        return print_layout()
    if len(argv) != 3 or argv[1] != "--blob":
        sys.stderr.write(__doc__)
        return 2
    try:
        with open(argv[2], "rb") as handle:
            data = handle.read(CEILING + 1)
    except OSError:
        sys.stderr.write("cannot read the blob\n")
        return 2
    if len(data) > CEILING:
        print(json.dumps({"status": "refused", "kind": "total-length",
                          "detail": "larger than the 64 MiB ceiling"},
                         separators=(",", ":"), sort_keys=True))
        return 1
    try:
        record = decode(data)
    except Refused as refusal:
        print(json.dumps({"status": "refused", "kind": refusal.kind,
                          "detail": refusal.detail},
                         separators=(",", ":"), sort_keys=True))
        return 1
    print(json.dumps(record, separators=(",", ":"), sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
