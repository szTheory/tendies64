#!/usr/bin/env python3
"""Independent decoder for the Tendies64 state container (plan 09-08).

Written from the container layout only, with its own literal offset table and
the standard library alone; it shares no code with the core exporter or with the
C test decoder. It reads a blob once, bounded by the 64 MiB container ceiling,
never indexes past the data, and reports either one accepted JSON line or one
refusal with a kind from a closed list.

The section hashes and the trailer are integrity checks, not authentication: a
blob whose hashes were recomputed after tampering still decodes, and semantic
validation belongs to the importer. This decoder makes no hardware, timing,
cross-build or other-emulator claim, and the layout stays provisional until
Phase 10.

usage: decode_container.py --blob PATH     one JSON line; exit 0 accepted,
                                           1 refused with a kind, 2 usage
       decode_container.py --print-layout  the literal offset table
"""
import hashlib
import json
import struct
import sys

# ---- the layout, all integers big-endian, offsets from the blob start -------
MAGIC = b"T64STATE"
CONTAINER_VERSION = 1
CEILING = 64 * 1024 * 1024
MAX_SECTIONS = 32
DOMAIN = b"t64.state.si.25"
OBSERVER_ID = 16
OBSERVER_BYTES = 40
TRAILER_BYTES = 32
ENTRY_BYTES = 56

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


def decode(data):
    size = len(data)
    if size < HEADER_BYTES:
        raise Refused("total-length", "shorter than the header")
    if data[0:8] != MAGIC:
        raise Refused("magic", "magic bytes differ")
    if u32(data, 0x008) != CONTAINER_VERSION:
        raise Refused("container-version", "container version is not 1")
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
    if canonical_length < len(DOMAIN) or \
            data[canonical_offset:canonical_offset + len(DOMAIN)] != DOMAIN:
        raise Refused("marker", "the domain bytes are wrong")
    expect_marker = canonical_offset + len(DOMAIN)
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
    return {
        "status": "accepted",
        "kind": "ok",
        "total_length": str(total),
        "state_serialization_version": str(u32(data, 0x00C)),
        "section_ids": [str(entry["id"]) for entry in entries],
        "canonical_digest": declared.hex(),
        "guest_tick": str(u64(data, 0x068)),
        "profile_digest": bytes(data[0x020:0x040]).hex(),
        "content_sha256": bytes(data[0x040:0x060]).hex(),
        "content_length": str(u64(data, 0x060)),
        "build_id": build_text,
        "observer": [str(value) for value in observer],
    }


def print_layout():
    for name, offset, width in HEADER:
        print("header.%s 0x%03x %d" % (name, offset, width))
    print("header.bytes 0x%03x" % HEADER_BYTES)
    for name, offset, width in ENTRY:
        print("entry.%s %d %d" % (name, offset, width))
    print("entry.bytes %d" % ENTRY_BYTES)
    print("canonical.domain %s" % DOMAIN.decode("ascii"))
    print("observer.id %d" % OBSERVER_ID)
    print("observer.bytes %d" % OBSERVER_BYTES)
    print("trailer.bytes %d" % TRAILER_BYTES)
    print("max.sections %d" % MAX_SECTIONS)
    print("max.bytes %d" % CEILING)
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
