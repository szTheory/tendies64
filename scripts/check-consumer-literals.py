#!/usr/bin/env python3
"""Literal generator and checker of the installed consumer (Phase 11, plan 11-18).

The installed consumer (tests/installed-consumer) hard-codes every expected value
of the composite fixture in tests/installed-consumer/t64c_composite.h. That header
is generated here from three committed files and from nothing else:

  INDEPENDENT section
      fixtures/composite/composite.expected.json   video, audio, transcript, save, poll
                                                   state (the output of
                                                   fixtures/composite/composite_oracle.py,
                                                   proved by harness.composite-oracle)
      fixtures/composite/composite.input.json      the two input events
  RUNNER-RECORDED PIN section (never called independent)
      fixtures/composite/composite.runner-pins.json  completion tick, stop reason,
                                                   final state digest, video packet
                                                   count, taken from the committed
                                                   Phase 10 record of the supervised
                                                   runs (DR-32)

Phase 12 (plan 12-05) adds a second header, tests/installed-consumer/t64c_title.h, for
the title fixture, generated the same way from
  INDEPENDENT section
      fixtures/title/title.expected.json           frames, audio, transcript and save of
                                                   the short, long, shifted and persist
                                                   scenarios, the blank digests and the
                                                   refused-frame expectations (the output
                                                   of fixtures/title/title_oracle.py,
                                                   proved by harness.title-oracle)
      fixtures/title/title.{short,long,shifted}.input.json
                                                   the event tables (their SHA-256 is
                                                   checked against the expectation file)
  RECORDED PINS section (never called independent)
      fixtures/title/title.runner-pins.json        the runner pins of the short scenario
                                                   (label "runner-recorded pin, not
                                                   independent") and the section consumer
                                                   (long scenario, label "consumer-recorded
                                                   pin, not independent")

Modes:
  --write      regenerate both headers from their committed files
  --check      regenerate in memory and diff with the committed headers; refuse a pins
               file whose labels are not exactly "runner-recorded pin, not independent"
               and "consumer-recorded pin, not independent"; confirm that the cited
               records contain the completion ticks ("599,357" form) and the digest
               literals; confirm the labels are in the headers; consistency of the files
               (recomputed hashes of the blank frame, the blank save, the transcripts,
               the save images and the input scripts)
  --selftest   seeded failures in temporary copies (wrong label, tick and digest not
               in the cited record, a flipped header byte, a wrong schema, a stale
               header, a clean control, and the title seeds); each must be reported
               exactly as seeded; it may be combined with --check
Options: --root DIR (default: the repository of this script), --header FILE (the
composite header), --title-header FILE.

The consumer derives no expected value from the core or the runner at run time;
this script never runs either. Exit 0 pass, 1 findings, 2 usage or missing file.
"""
import argparse
import hashlib
import json
import os
import shutil
import sys
import tempfile

PIN_LABEL = "runner-recorded pin, not independent"
EXPECTED_REL = "fixtures/composite/composite.expected.json"
INPUT_REL = "fixtures/composite/composite.input.json"
PINS_REL = "fixtures/composite/composite.runner-pins.json"
HEADER_REL = "tests/installed-consumer/t64c_composite.h"
RECORD_REL = ".planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md"
GENERATED_BY = "scripts/check-consumer-literals.py --write"

# The title fixture (Phase 12, plan 12-05).
CONSUMER_PIN_LABEL = "consumer-recorded pin, not independent"
CONSUMER_PIN_SOURCE = "plan 12-05 installed.consumer.title"
TITLE_EXPECTED_REL = "fixtures/title/title.expected.json"
TITLE_INPUT_RELS = (
    ("short", "fixtures/title/title.short.input.json"),
    ("short-wrong", "fixtures/title/title.short-wrong.input.json"),
    ("long", "fixtures/title/title.long.input.json"),
    ("shifted", "fixtures/title/title.shifted.input.json"),
)
TITLE_PINS_REL = "fixtures/title/title.runner-pins.json"
TITLE_HEADER_REL = "tests/installed-consumer/t64c_title.h"
TITLE_RECORD_REL = ".planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md"
TITLE_SCENARIOS = ("short", "long", "shifted", "persist")
TITLE_FILES = (TITLE_EXPECTED_REL, TITLE_PINS_REL, TITLE_HEADER_REL, TITLE_RECORD_REL) + tuple(
    rel for _, rel in TITLE_INPUT_RELS)


class Usage(Exception):
    pass


def load_json(root, rel):
    path = os.path.join(root, rel)
    if not os.path.isfile(path):
        raise Usage("missing file: %s" % rel)
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def hex_bytes(text, count, what):
    if not isinstance(text, str) or len(text) != 2 * count:
        raise Usage("%s: expected %d hex bytes" % (what, count))
    try:
        return bytes.fromhex(text)
    except ValueError:
        raise Usage("%s: not hexadecimal" % what)


def c_bytes(data):
    """A brace initializer of unsigned bytes, eight per line."""
    rows = []
    for index in range(0, len(data), 8):
        rows.append(", ".join("0x%02xu" % byte for byte in data[index:index + 8]))
    return "{ \\\n    " + ", \\\n    ".join(rows) + " }"


def c_string(text):
    out = []
    for char in text:
        if char == "\n":
            out.append("\\n")
        elif char == '"':
            out.append('\\"')
        elif char == "\\":
            out.append("\\\\")
        elif 32 <= ord(char) < 127:
            out.append(char)
        else:
            raise Usage("a literal string holds a character outside printable ASCII")
    return '"' + "".join(out) + '"'


def validate(expected, inputs, pins):
    """Structural checks of the three files (raised as Usage)."""
    if expected.get("schema") != "t64.composite-oracle" or expected.get("schema_version") != 1:
        raise Usage("%s: schema is not t64.composite-oracle version 1" % EXPECTED_REL)
    if expected.get("variant") != "accepted":
        raise Usage("%s: variant is not accepted" % EXPECTED_REL)
    if inputs.get("schema") != "t64.input" or inputs.get("schema_version") != 1:
        raise Usage("%s: schema is not t64.input version 1" % INPUT_REL)
    if pins.get("schema") != "t64.composite-runner-pins" or pins.get("schema_version") != 1:
        raise Usage("%s: schema is not t64.composite-runner-pins version 1" % PINS_REL)
    if pins.get("label") != PIN_LABEL:
        raise Usage("%s: label is not exactly %r" % (PINS_REL, PIN_LABEL))
    for key in ("source", "profile_id", "stop_reason", "final_state_digest"):
        if not isinstance(pins.get(key), str) or not pins[key]:
            raise Usage("%s: %s is missing" % (PINS_REL, key))
    for key in ("completion_tick", "video_packet_count"):
        if not isinstance(pins.get(key), int) or isinstance(pins[key], bool) or pins[key] <= 0:
            raise Usage("%s: %s is not a positive integer" % (PINS_REL, key))
    digest = pins["final_state_digest"]
    if len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
        raise Usage("%s: final_state_digest is not 64 lowercase hex digits" % PINS_REL)
    if not pins["stop_reason"].replace("_", "").isalpha() or not pins["stop_reason"].isupper():
        raise Usage("%s: stop_reason is not an upper-case name" % PINS_REL)


def generate(root):
    expected = load_json(root, EXPECTED_REL)
    inputs = load_json(root, INPUT_REL)
    pins = load_json(root, PINS_REL)
    validate(expected, inputs, pins)

    video = expected["video"]
    if video.get("packet_bytes") != 4 * 3 * 4 or video.get("width") != 4 or video.get("height") != 3:
        raise Usage("%s: video is not one 4x3 RGBA8 packet of 48 bytes" % EXPECTED_REL)
    video_bytes = hex_bytes(video["rgba8"], video["packet_bytes"], "video.rgba8")

    audio = expected["audio"]
    if audio.get("packet_count") != 2 or audio.get("packet_bytes") != 16 or len(audio["packets"]) != 2:
        raise Usage("%s: audio is not two packets of 16 bytes" % EXPECTED_REL)
    audio_packets = [hex_bytes(p, 16, "audio.packets") for p in audio["packets"]]

    transcript = expected["transcript"]
    if len(transcript.encode("ascii")) != expected["transcript_bytes"]:
        raise Usage("%s: transcript_bytes differs from the transcript length" % EXPECTED_REL)
    polled = expected["polled"]
    if len(polled) != 3 or any(len(w) != 8 for w in polled):
        raise Usage("%s: polled is not three 8-digit words" % EXPECTED_REL)

    save = expected["save"]
    if save.get("bytes") != 32768:
        raise Usage("%s: save is not 32768 bytes" % EXPECTED_REL)
    ranges = []
    for entry in save["nonzero"]:
        data = bytes.fromhex(entry["hex"])
        if entry["offset"] < 0 or entry["offset"] + len(data) > 32768 or not data:
            raise Usage("%s: a save range is outside the image" % EXPECTED_REL)
        ranges.append((entry["offset"], data))
    if len(ranges) != 1:
        raise Usage("%s: expected exactly one nonzero save range" % EXPECTED_REL)

    events = inputs["events"]
    if len(events) != 2:
        raise Usage("%s: expected exactly two events" % INPUT_REL)

    out = []
    w = out.append
    w("/*\n")
    w(" * GENERATED by %s from\n" % GENERATED_BY)
    w(" *   %s\n *   %s\n *   %s\n" % (EXPECTED_REL, INPUT_REL, PINS_REL))
    w(" * Do not edit by hand: installed.consumer.literals regenerates this file and\n")
    w(" * fails on any difference. Every value is a constant: the consumer derives no\n")
    w(" * expected value from the core or the runner at run time. Macros only, so no\n")
    w(" * translation unit that includes the header can warn about an unused object.\n")
    w(" */\n")
    w("#ifndef T64C_COMPOSITE_H\n#define T64C_COMPOSITE_H\n\n")
    w("/* ======================================================================\n")
    w(" * INDEPENDENT LITERALS: source %s and %s\n" % (EXPECTED_REL, INPUT_REL))
    w(" * (the output of fixtures/composite/composite_oracle.py, proved byte for byte by\n")
    w(" * harness.composite-oracle; the inputs are the committed input script).\n")
    w(" * ====================================================================== */\n\n")
    w("#define T64C_LIT_INPUT_COUNT 2u\n")
    for index, event in enumerate(events):
        w("#define T64C_LIT_INPUT%d_TICK UINT64_C(%d)\n" % (index, int(event["tick"])))
        w("#define T64C_LIT_INPUT%d_PORT %du\n" % (index, event["port"]))
        w("#define T64C_LIT_INPUT%d_BUTTONS %du\n" % (index, event["buttons"]))
        w("#define T64C_LIT_INPUT%d_AXIS_X INT32_C(%d)\n" % (index, event["x"]))
        w("#define T64C_LIT_INPUT%d_AXIS_Y INT32_C(%d)\n" % (index, event["y"]))
    w("\n#define T64C_LIT_VIDEO_WIDTH %du\n" % video["width"])
    w("#define T64C_LIT_VIDEO_HEIGHT %du\n" % video["height"])
    w("#define T64C_LIT_VIDEO_PACKET_BYTES %du\n" % video["packet_bytes"])
    w("/* every video packet of the run equals this one RGBA8 packet */\n")
    w("#define T64C_LIT_VIDEO_PACKET_INIT %s\n" % c_bytes(video_bytes))
    w("\n#define T64C_LIT_AUDIO_PACKET_COUNT %du\n" % audio["packet_count"])
    w("#define T64C_LIT_AUDIO_PACKET_BYTES %du\n" % audio["packet_bytes"])
    for index, packet in enumerate(audio_packets):
        w("#define T64C_LIT_AUDIO_PACKET%d_INIT %s\n" % (index, c_bytes(packet)))
    w("\n#define T64C_LIT_TRANSCRIPT_BYTES %du\n" % expected["transcript_bytes"])
    w("#define T64C_LIT_TRANSCRIPT_TEXT %s\n" % c_string(transcript))
    w("#define T64C_LIT_TRANSCRIPT_SHA256_HEX %s\n" % c_string(expected["transcript_sha256"]))
    for index, word in enumerate(polled):
        w("#define T64C_LIT_POLLED%d_HEX %s\n" % (index, c_string(word)))
    w("\n#define T64C_LIT_SAVE_BYTES %du\n" % save["bytes"])
    w("#define T64C_LIT_SAVE_SHA256_HEX %s\n" % c_string(expected["save_sha256"]))
    w("#define T64C_LIT_SAVE_RANGE0_OFFSET %du\n" % ranges[0][0])
    w("#define T64C_LIT_SAVE_RANGE0_BYTES %du\n" % len(ranges[0][1]))
    w("#define T64C_LIT_SAVE_RANGE0_INIT %s\n" % c_bytes(ranges[0][1]))
    w("\n/* ======================================================================\n")
    w(" * RUNNER-RECORDED PINS (%s): source %s\n" % (PIN_LABEL, PINS_REL))
    w(" * Cited record: %s\n" % pins["source"])
    w(" * These four values are the output of the in-tree runner of Phase 10, recorded\n")
    w(" * and committed. The consumer checks that the installed package agrees with them;\n")
    w(" * they are never an independent expectation and are never called one.\n")
    w(" * ====================================================================== */\n\n")
    w("#define T64C_PIN_LABEL %s\n" % c_string(PIN_LABEL))
    w("#define T64C_PIN_SOURCE %s\n" % c_string(pins["source"]))
    w("#define T64C_PIN_PROFILE_ID %s\n" % c_string(pins["profile_id"]))
    w("#define T64C_PIN_COMPLETION_TICK UINT64_C(%d)\n" % pins["completion_tick"])
    w("#define T64C_PIN_STOP_REASON_NAME %s\n" % c_string(pins["stop_reason"]))
    w("#define T64C_PIN_STOP_REASON T64_STOP_REASON_%s\n" % pins["stop_reason"])
    w("#define T64C_PIN_FINAL_DIGEST_HEX %s\n" % c_string(pins["final_state_digest"]))
    w("#define T64C_PIN_VIDEO_PACKET_COUNT %du\n" % pins["video_packet_count"])
    w("\n#endif /* T64C_COMPOSITE_H */\n")
    return "".join(out), pins


# ---- the title fixture (Phase 12, plan 12-05) ------------------------------------


def sha256_hex(data):
    return hashlib.sha256(data).hexdigest()


def is_hex64(value):
    return isinstance(value, str) and len(value) == 64 and all(c in "0123456789abcdef" for c in value)


def read_bytes(root, rel):
    path = os.path.join(root, rel)
    if not os.path.isfile(path):
        raise Usage("missing file: %s" % rel)
    with open(path, "rb") as handle:
        return handle.read()


def c_str_list(items):
    """A brace initializer of C strings; an empty list gets one placeholder (never read:
    its count macro is 0), because C has no empty initializer."""
    rows = [c_string(item) for item in items] or [c_string("")]
    return "{ \\\n    " + ", \\\n    ".join(rows) + " }"


def c_events(events):
    rows = ["{ UINT64_C(%d), %du, %du, INT32_C(%d), INT32_C(%d) }" % event for event in events]
    return "{ \\\n    " + ", \\\n    ".join(rows) + " }"


def c_pairs(pairs):
    rows = ["{ %du, 0x%02xu }" % pair for pair in pairs]
    return "{ \\\n    " + ", \\\n    ".join(rows) + " }"


def title_event_table(name, rel, script, expected):
    """The (tick, port, buttons, x, y) rows of one committed input script (validated)."""
    if script.get("schema") != "t64.input" or script.get("schema_version") != 1:
        raise Usage("%s: schema is not t64.input version 1" % rel)
    rows = []
    for event in script.get("events", []):
        if set(event) != {"buttons", "port", "tick", "x", "y"}:
            raise Usage("%s: an event does not carry exactly buttons, port, tick, x and y" % rel)
        tick = event["tick"]
        if not isinstance(tick, str) or not tick.isdigit():
            raise Usage("%s: an event tick is not a decimal string" % rel)
        if event["port"] != 0 or not 0 <= event["buttons"] <= 65535 \
                or not -128 <= event["x"] <= 127 or not -128 <= event["y"] <= 127:
            raise Usage("%s: an event is outside port 0, buttons 0..65535 and axes -128..127" % rel)
        rows.append((int(tick), event["port"], event["buttons"], event["x"], event["y"]))
    if [row[0] for row in rows] != sorted(row[0] for row in rows):
        raise Usage("%s: the event ticks are not in order" % rel)
    meta = expected["inputs"][name]
    if meta["events"] != len(rows):
        raise Usage("%s: the event count differs from %s inputs.%s.events" % (rel, TITLE_EXPECTED_REL, name))
    return rows


def title_pins_section(section, label, scenario, rel):
    """Validate one pins record (the top level of the file is the runner record; the
    key consumer is the consumer record) and return (tick, stop_reason, digest, count)."""
    if section.get("label") != label:
        raise Usage("%s: the %s label is not exactly %r" % (rel, scenario, label))
    for key in ("source", "stop_reason", "final_state_digest", "scenario"):
        if not isinstance(section.get(key), str) or not section[key]:
            raise Usage("%s: the %s record has no %s" % (rel, scenario, key))
    if section["scenario"] != scenario:
        raise Usage("%s: the record labeled %r is not the %s scenario" % (rel, label, scenario))
    tick = section.get("completion_tick")
    if isinstance(tick, str) and tick.isdigit():
        tick = int(tick)
    if not isinstance(tick, int) or isinstance(tick, bool) or tick <= 0:
        raise Usage("%s: the %s completion_tick is not a positive decimal" % (rel, scenario))
    count = section.get("video_packet_count")
    if not isinstance(count, int) or isinstance(count, bool) or count <= 0:
        raise Usage("%s: the %s video_packet_count is not a positive integer" % (rel, scenario))
    if not is_hex64(section["final_state_digest"]):
        raise Usage("%s: the %s final_state_digest is not 64 lowercase hex digits" % (rel, scenario))
    stop = section["stop_reason"]
    if not stop.replace("_", "").isalpha() or not stop.isupper():
        raise Usage("%s: the %s stop_reason is not an upper-case name" % (rel, scenario))
    return tick, stop, section["final_state_digest"], count


def title_load(root):
    expected = load_json(root, TITLE_EXPECTED_REL)
    pins = load_json(root, TITLE_PINS_REL)
    scripts = {}
    for name, rel in TITLE_INPUT_RELS:
        raw = read_bytes(root, rel)
        if sha256_hex(raw) != expected["inputs"][name]["sha256"]:
            raise Usage("%s: the SHA-256 of the script differs from %s inputs.%s.sha256"
                        % (rel, TITLE_EXPECTED_REL, name))
        scripts[name] = title_event_table(name, rel, json.loads(raw.decode("utf-8")), expected)
    return expected, scripts, pins


def title_expected_checks(expected):
    """Recompute every hash the expectation file states about itself."""
    if expected.get("schema") != "t64.title-oracle" or expected.get("schema_version") != 1:
        raise Usage("%s: schema is not t64.title-oracle version 1" % TITLE_EXPECTED_REL)
    const = expected["constants"]
    frame_bytes = const["frame_bytes"]
    save_bytes = const["save_bytes"]
    if const["width"] * const["height"] * 4 != frame_bytes or expected["blank_bytes"] != frame_bytes:
        raise Usage("%s: the frame is not width x height x 4 RGBA8 bytes" % TITLE_EXPECTED_REL)
    if sha256_hex(bytes(frame_bytes)) != expected["blank_sha256"]:
        raise Usage("%s: blank_sha256 is not the SHA-256 of %d zero bytes" % (TITLE_EXPECTED_REL, frame_bytes))
    if sha256_hex(bytes(save_bytes)) != expected["blank_save_sha256"]:
        raise Usage("%s: blank_save_sha256 is not the SHA-256 of %d zero bytes" % (TITLE_EXPECTED_REL, save_bytes))
    refused = expected["refused"]
    if refused["frames_each_sha256"] != expected["blank_sha256"]:
        raise Usage("%s: the refused-frame expectation is not the blank digest" % TITLE_EXPECTED_REL)
    if refused["save_sha256"] != expected["blank_save_sha256"]:
        raise Usage("%s: the refused-frame save expectation is not the blank save digest" % TITLE_EXPECTED_REL)
    if refused["audio_packets"] != 0 or refused["completes"] is not False or refused["transcript"] != "":
        raise Usage("%s: the refused section does not expect no audio, no completion and no transcript"
                    % TITLE_EXPECTED_REL)
    images = {}
    for name in TITLE_SCENARIOS:
        scenario = expected["scenarios"][name]
        frames = scenario["frames"]
        if len(frames) != scenario["video_packets"] or not all(is_hex64(f) for f in frames):
            raise Usage("%s: scenario %s frames are not %d hex digests" % (TITLE_EXPECTED_REL, name, scenario["video_packets"]))
        if expected["blank_sha256"] in frames:
            raise Usage("%s: scenario %s has a frame equal to the blank digest" % (TITLE_EXPECTED_REL, name))
        audio = scenario["audio"]
        if len(audio["packet_sha256"]) != audio["packet_count"] or not is_hex64(audio["stream_sha256"]) \
                or not all(is_hex64(p) for p in audio["packet_sha256"]):
            raise Usage("%s: scenario %s audio digests are inconsistent" % (TITLE_EXPECTED_REL, name))
        if audio["packet_count"] and audio["packet_bytes"] != expected["tone"]["descriptor_bytes"]:
            raise Usage("%s: scenario %s audio packet size is not the tone descriptor size" % (TITLE_EXPECTED_REL, name))
        text = scenario["transcript"]
        if not text.isascii() or len(text) != scenario["transcript_bytes"] \
                or sha256_hex(text.encode("ascii")) != scenario["transcript_sha256"]:
            raise Usage("%s: scenario %s transcript, length and SHA-256 disagree" % (TITLE_EXPECTED_REL, name))
        save = scenario["save"]
        if save["bytes"] != save_bytes:
            raise Usage("%s: scenario %s save is not %d bytes" % (TITLE_EXPECTED_REL, name, save_bytes))
        image = bytearray(save_bytes)
        pairs = []
        for entry in save["nonzero"]:
            data = bytes.fromhex(entry["hex"])
            if not data or entry["offset"] < 0 or entry["offset"] + len(data) > save_bytes:
                raise Usage("%s: scenario %s has a save range outside the image" % (TITLE_EXPECTED_REL, name))
            for index, value in enumerate(data):
                if image[entry["offset"] + index] != 0:
                    raise Usage("%s: scenario %s has overlapping save ranges" % (TITLE_EXPECTED_REL, name))
                image[entry["offset"] + index] = value
                pairs.append((entry["offset"] + index, value))
        if sha256_hex(bytes(image)) != save["sha256"]:
            raise Usage("%s: scenario %s save ranges do not hash to save.sha256" % (TITLE_EXPECTED_REL, name))
        images[name] = pairs
    long_frames = expected["scenarios"]["long"]["frames"]
    shifted_frames = expected["scenarios"]["shifted"]["frames"]
    first = next((i for i in range(min(len(long_frames), len(shifted_frames)))
                  if long_frames[i] != shifted_frames[i]), None)
    if first is None or first != expected["distinguishable"]["long_vs_shifted_first_difference"]:
        raise Usage("%s: the long and shifted frame lists do not first differ at the stated checkpoint"
                    % TITLE_EXPECTED_REL)
    if not is_hex64(expected["fresh_boot_frame_sha256"]) \
            or expected["scenarios"]["persist"]["frames"][0] == expected["fresh_boot_frame_sha256"]:
        raise Usage("%s: the persist frame is not distinguishable from a fresh boot" % TITLE_EXPECTED_REL)
    return images, first


def generate_title(root):
    expected, scripts, pins = title_load(root)
    images, first_difference = title_expected_checks(expected)
    if pins.get("schema") != "t64.title-runner-pins" or pins.get("schema_version") != 1:
        raise Usage("%s: schema is not t64.title-runner-pins version 1" % TITLE_PINS_REL)
    runner = title_pins_section(pins, PIN_LABEL, "short", TITLE_PINS_REL)
    if not isinstance(pins.get("profile_id"), str) or not pins["profile_id"]:
        raise Usage("%s: profile_id is missing" % TITLE_PINS_REL)
    if runner[3] != expected["scenarios"]["short"]["video_packets"]:
        raise Usage("%s: the runner packet count is not the short scenario's packet count" % TITLE_PINS_REL)
    if not isinstance(pins.get("consumer"), dict):
        raise Usage("%s: the section consumer is missing" % TITLE_PINS_REL)
    consumer = title_pins_section(pins["consumer"], CONSUMER_PIN_LABEL, "long", TITLE_PINS_REL)
    if pins["consumer"]["source"] != CONSUMER_PIN_SOURCE:
        raise Usage("%s: the consumer record does not cite %r" % (TITLE_PINS_REL, CONSUMER_PIN_SOURCE))
    if consumer[3] != expected["scenarios"]["long"]["video_packets"]:
        raise Usage("%s: the consumer packet count is not the long scenario's packet count" % TITLE_PINS_REL)
    boundaries = expected["boundaries"]
    margin = expected["constants"]["boundary_margin_ticks"]

    out = []
    w = out.append
    w("/*\n")
    w(" * GENERATED by %s from\n" % GENERATED_BY)
    w(" *   %s\n" % TITLE_EXPECTED_REL)
    for name, rel in TITLE_INPUT_RELS[:1] + TITLE_INPUT_RELS[2:]:
        w(" *   %s\n" % rel)
    w(" *   %s\n" % TITLE_PINS_REL)
    w(" * Do not edit by hand: installed.consumer.literals regenerates this file and\n")
    w(" * fails on any difference. Every value is a constant: the consumer derives no\n")
    w(" * expected value from the core or the runner at run time. Macros only, so no\n")
    w(" * translation unit that includes the header can warn about an unused object.\n")
    w(" */\n")
    w("#ifndef T64C_TITLE_H\n#define T64C_TITLE_H\n\n")
    w("/* ======================================================================\n")
    w(" * INDEPENDENT LITERALS: source %s and the input scripts\n" % TITLE_EXPECTED_REL)
    w(" * (the output of fixtures/title/title_oracle.py, proved byte for byte by\n")
    w(" * harness.title-oracle; the scripts are the committed scripts whose SHA-256 the\n")
    w(" * expectation file states). The refused-frame expectations are in this section.\n")
    w(" * ====================================================================== */\n\n")
    const = expected["constants"]
    w("#define T64C_TLIT_WIDTH %du\n" % const["width"])
    w("#define T64C_TLIT_HEIGHT %du\n" % const["height"])
    w("#define T64C_TLIT_FRAME_BYTES %du\n" % const["frame_bytes"])
    w("#define T64C_TLIT_AUDIO_PACKET_BYTES %du\n" % expected["tone"]["descriptor_bytes"])
    w("#define T64C_TLIT_SAVE_BYTES %du\n" % const["save_bytes"])
    w("#define T64C_TLIT_BLANK_SHA256_HEX %s\n" % c_string(expected["blank_sha256"]))
    w("#define T64C_TLIT_BLANK_SAVE_SHA256_HEX %s\n" % c_string(expected["blank_save_sha256"]))
    w("#define T64C_TLIT_FRESH_BOOT_FRAME_SHA256_HEX %s\n" % c_string(expected["fresh_boot_frame_sha256"]))
    w("/* the first checkpoint at which the shifted frames leave the long frames */\n")
    w("#define T64C_TLIT_LONG_SHIFTED_FIRST_DIFFERENCE %du\n" % first_difference)
    for name in TITLE_SCENARIOS:
        scenario = expected["scenarios"][name]
        upper = name.upper()
        packets = scenario["video_packets"]
        if packets >= len(boundaries):
            raise Usage("%s: no field boundary after scenario %s" % (TITLE_EXPECTED_REL, name))
        w("\n/* scenario %s */\n" % name)
        if name in scripts:
            w("#define T64C_TLIT_%s_EVENT_COUNT %du\n" % (upper, len(scripts[name])))
            w("#define T64C_TLIT_%s_EVENTS_INIT %s\n" % (upper, c_events(scripts[name])))
        else:
            w("#define T64C_TLIT_%s_EVENT_COUNT 0u\n" % upper)
        w("/* the run bound: the field boundary after the last frame plus the oracle's margin */\n")
        w("#define T64C_TLIT_%s_BOUND_TICKS UINT64_C(%d)\n" % (upper, boundaries[packets] + margin))
        w("#define T64C_TLIT_%s_COMPLETES %du\n" % (upper, 1 if scenario["completes"] else 0))
        w("#define T64C_TLIT_%s_FRAME_COUNT %du\n" % (upper, packets))
        w("#define T64C_TLIT_%s_FRAMES_INIT %s\n" % (upper, c_str_list(scenario["frames"])))
        audio = scenario["audio"]
        w("#define T64C_TLIT_%s_AUDIO_COUNT %du\n" % (upper, audio["packet_count"]))
        w("#define T64C_TLIT_%s_AUDIO_SHA256_INIT %s\n" % (upper, c_str_list(audio["packet_sha256"])))
        w("#define T64C_TLIT_%s_AUDIO_STREAM_SHA256_HEX %s\n" % (upper, c_string(audio["stream_sha256"])))
        w("#define T64C_TLIT_%s_TRANSCRIPT_BYTES %du\n" % (upper, scenario["transcript_bytes"]))
        w("#define T64C_TLIT_%s_TRANSCRIPT_TEXT %s\n" % (upper, c_string(scenario["transcript"])))
        w("#define T64C_TLIT_%s_TRANSCRIPT_SHA256_HEX %s\n" % (upper, c_string(scenario["transcript_sha256"])))
        w("#define T64C_TLIT_%s_SAVE_SHA256_HEX %s\n" % (upper, c_string(scenario["save"]["sha256"])))
        w("/* every byte of the save image is zero except these (offset, value) pairs */\n")
        w("#define T64C_TLIT_%s_SAVE_NONZERO_COUNT %du\n" % (upper, len(images[name])))
        w("#define T64C_TLIT_%s_SAVE_NONZERO_INIT %s\n" % (upper, c_pairs(images[name])))
    w("\n/* the shifted run is stopped after a prefix of fields: the first moved checkpoint and one more */\n")
    prefix = first_difference + 2
    w("#define T64C_TLIT_SHIFTED_PREFIX_FRAMES %du\n" % prefix)
    w("#define T64C_TLIT_SHIFTED_PREFIX_BOUND_TICKS UINT64_C(%d)\n" % (boundaries[prefix] + margin))
    w("/* the control run of the persistence case stops after one field */\n")
    w("#define T64C_TLIT_PERSIST_CONTROL_BOUND_TICKS UINT64_C(%d)\n" % (boundaries[1] + margin))
    refused = expected["refused"]
    w("\n/* refused-frame ROM (decision DR-34): every packet is blank, nothing else is produced */\n")
    w("#define T64C_TLIT_REFUSED_FRAME_SHA256_HEX %s\n" % c_string(refused["frames_each_sha256"]))
    w("#define T64C_TLIT_REFUSED_AUDIO_COUNT %du\n" % refused["audio_packets"])
    w("#define T64C_TLIT_REFUSED_TRANSCRIPT_BYTES %du\n" % len(refused["transcript"]))
    w("#define T64C_TLIT_REFUSED_COMPLETES 0u\n")
    w("#define T64C_TLIT_REFUSED_SAVE_SHA256_HEX %s\n" % c_string(refused["save_sha256"]))
    w("\n/* ======================================================================\n")
    w(" * RECORDED PINS: source %s\n" % TITLE_PINS_REL)
    w(" * (%s) the four values of the short scenario of the supervised\n" % PIN_LABEL)
    w(" * runner, cited: %s\n" % pins["source"])
    w(" * (%s) the four values of the long scenario of this consumer,\n" % CONSUMER_PIN_LABEL)
    w(" * cited: %s\n" % pins["consumer"]["source"])
    w(" * The consumer checks that the installed package agrees with them; they are never an\n")
    w(" * independent expectation and are never called one.\n")
    w(" * ====================================================================== */\n\n")
    w("#define T64C_TPIN_PROFILE_ID %s\n" % c_string(pins["profile_id"]))
    w("#define T64C_TPIN_RUNNER_LABEL %s\n" % c_string(PIN_LABEL))
    w("#define T64C_TPIN_RUNNER_SOURCE %s\n" % c_string(pins["source"]))
    w("#define T64C_TPIN_RUNNER_COMPLETION_TICK UINT64_C(%d)\n" % runner[0])
    w("#define T64C_TPIN_RUNNER_STOP_REASON_NAME %s\n" % c_string(runner[1]))
    w("#define T64C_TPIN_RUNNER_STOP_REASON T64_STOP_REASON_%s\n" % runner[1])
    w("#define T64C_TPIN_RUNNER_FINAL_DIGEST_HEX %s\n" % c_string(runner[2]))
    w("#define T64C_TPIN_RUNNER_VIDEO_PACKET_COUNT %du\n" % runner[3])
    w("\n#define T64C_TPIN_CONSUMER_LABEL %s\n" % c_string(CONSUMER_PIN_LABEL))
    w("#define T64C_TPIN_CONSUMER_SOURCE %s\n" % c_string(pins["consumer"]["source"]))
    w("#define T64C_TPIN_CONSUMER_COMPLETION_TICK UINT64_C(%d)\n" % consumer[0])
    w("#define T64C_TPIN_CONSUMER_STOP_REASON_NAME %s\n" % c_string(consumer[1]))
    w("#define T64C_TPIN_CONSUMER_STOP_REASON T64_STOP_REASON_%s\n" % consumer[1])
    w("#define T64C_TPIN_CONSUMER_FINAL_DIGEST_HEX %s\n" % c_string(consumer[2]))
    w("#define T64C_TPIN_CONSUMER_VIDEO_PACKET_COUNT %du\n" % consumer[3])
    w("\n#endif /* T64C_TITLE_H */\n")
    return "".join(out), runner, consumer


def check_title(root, header_path):
    """Findings of the title header; Usage for a missing or inconsistent file."""
    findings = []
    text, runner, consumer = generate_title(root)
    if not os.path.isfile(header_path):
        raise Usage("missing file: %s" % header_path)
    with open(header_path, "r", encoding="utf-8") as handle:
        committed = handle.read()
    if committed != text:
        findings.append("%s differs from the regenerated header (run with --write after "
                        "changing a fixture; a hand edit of a literal is a finding)" % TITLE_HEADER_REL)
    for label in (PIN_LABEL, CONSUMER_PIN_LABEL):
        if committed.count(label) < 1:
            findings.append("%s does not carry the label %r" % (TITLE_HEADER_REL, label))
    record_path = os.path.join(root, TITLE_RECORD_REL)
    if not os.path.isfile(record_path):
        raise Usage("missing file: %s (the cited Phase 12 record)" % TITLE_RECORD_REL)
    with open(record_path, "r", encoding="utf-8") as handle:
        record = handle.read()
    for who, pin in (("runner", runner), ("consumer", consumer)):
        if "{:,}".format(pin[0]) not in record:
            findings.append("the cited record does not contain the %s completion tick %s"
                            % (who, "{:,}".format(pin[0])))
        if pin[2] not in record:
            findings.append("the cited record does not contain the %s final state digest" % who)
    return findings


def check(root, header_path):
    """Return the list of findings; Usage for a missing file."""
    findings = []
    text, pins = generate(root)
    if not os.path.isfile(header_path):
        raise Usage("missing file: %s" % header_path)
    with open(header_path, "r", encoding="utf-8") as handle:
        committed = handle.read()
    if committed != text:
        findings.append("%s differs from the regenerated header (run with --write after "
                        "changing a fixture; a hand edit of a literal is a finding)" % HEADER_REL)
    if committed.count(PIN_LABEL) < 1:
        findings.append("%s does not carry the label %r" % (HEADER_REL, PIN_LABEL))
    record_path = os.path.join(root, RECORD_REL)
    if not os.path.isfile(record_path):
        raise Usage("missing file: %s (the cited Phase 10 record)" % RECORD_REL)
    with open(record_path, "r", encoding="utf-8") as handle:
        record = handle.read()
    tick_text = "{:,}".format(pins["completion_tick"])
    if tick_text not in record:
        findings.append("the cited record does not contain the completion tick %s" % tick_text)
    if pins["final_state_digest"] not in record:
        findings.append("the cited record does not contain the final state digest")
    return findings


def run_check(root, header_path, title_header_path):
    total = 0
    for name, function, path in (("composite", check, header_path),
                                 ("title", check_title, title_header_path)):
        try:
            findings = function(root, path)
        except Usage as error:
            print("check-consumer-literals: %s" % error, file=sys.stderr)
            return 2
        for finding in findings:
            print("FINDING: %s" % finding)
        print("check-consumer-literals: %s findings=%d" % (name, len(findings)))
        total += len(findings)
    print("check-consumer-literals: findings=%d" % total)
    return 1 if total else 0


def run_write(root, header_path, title_header_path):
    for generator, path in ((generate, header_path), (generate_title, title_header_path)):
        try:
            text = generator(root)[0]
        except Usage as error:
            print("check-consumer-literals: %s" % error, file=sys.stderr)
            return 2
        with open(path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)
        print("check-consumer-literals: wrote %s" % os.path.relpath(path, root))
    return 0


def selftest(root):
    """Seeded failures in temporary copies of the files each case touches: the composite
    header (seven cases) and the title header (the cases of plan 12-05)."""
    failures = 0
    with tempfile.TemporaryDirectory(prefix="t64-literals-") as scratch:
        def fresh(name, rels=(EXPECTED_REL, INPUT_REL, PINS_REL, HEADER_REL, RECORD_REL)):
            tree = os.path.join(scratch, name)
            for rel in rels:
                target = os.path.join(tree, rel)
                os.makedirs(os.path.dirname(target), exist_ok=True)
                shutil.copyfile(os.path.join(root, rel), target)
            return tree

        def edit(tree, rel, old, new):
            path = os.path.join(tree, rel)
            with open(path, "r", encoding="utf-8") as handle:
                body = handle.read()
            if old not in body:
                raise Usage("selftest seed text not found in %s" % rel)
            with open(path, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(body.replace(old, new, 1))

        def edit_json(tree, rel, change):
            path = os.path.join(tree, rel)
            with open(path, "r", encoding="utf-8") as handle:
                document = json.load(handle)
            change(document)
            with open(path, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(json.dumps(document, indent=2, sort_keys=True) + "\n")

        def expect(name, tree, want_findings, want_usage=False, text=None,
                   function=check, header_rel=HEADER_REL):
            """The seeded case must end as named and, when text is given, the finding or
            usage message must carry that text (the seed is reported with its own text)."""
            nonlocal failures
            try:
                findings = function(tree, os.path.join(tree, header_rel))
                outcome = "findings" if findings else "clean"
                detail = "; ".join(findings)
            except Usage as error:
                outcome = "usage"
                detail = str(error)
            ok = (outcome == "usage") if want_usage else (
                (outcome == "findings") if want_findings else (outcome == "clean"))
            if ok and text is not None and text not in detail:
                ok = False
            print("selftest %-40s %s (%s)" % (name, "ok" if ok else "FAILED", outcome))
            if not ok:
                print("    got: %s" % detail)
                failures += 1

        expect("clean-control", fresh("clean"), False)

        tree = fresh("flipped-header-byte")
        header = open(os.path.join(root, HEADER_REL), encoding="utf-8").read()
        marker = "#define T64C_LIT_VIDEO_PACKET_INIT"
        position = header.index(marker)
        first = header.index("0x00u", position)
        edit(tree, HEADER_REL, header[first:first + 5], "0x01u")
        expect("flipped-header-byte", tree, True)

        tree = fresh("wrong-label")
        edit(tree, PINS_REL, PIN_LABEL, "independent expectation")
        expect("wrong-label", tree, False, want_usage=True)

        tree = fresh("tick-not-in-record")
        edit(tree, PINS_REL, '"completion_tick": 599357', '"completion_tick": 599358')
        expect("tick-not-in-record", tree, True)

        tree = fresh("digest-not-in-record")
        edit(tree, RECORD_REL, "97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c",
             "0" * 64)
        expect("digest-not-in-record", tree, True)

        tree = fresh("wrong-schema")
        edit(tree, EXPECTED_REL, "t64.composite-oracle", "t64.composite-guess")
        expect("wrong-schema", tree, False, want_usage=True)

        tree = fresh("stale-header-after-fixture-edit")
        edit(tree, INPUT_REL, '"x":20', '"x":21')
        expect("stale-header-after-fixture-edit", tree, True)

        # ---- the title header (plan 12-05) ----------------------------------------
        def title_expect(name, tree, want_findings, want_usage=False, text=None):
            expect("title-" + name, tree, want_findings, want_usage, text,
                   function=check_title, header_rel=TITLE_HEADER_REL)

        def title_fresh(name):
            return fresh("title-" + name, TITLE_FILES)

        title_expect("clean-control", title_fresh("clean"), False)

        tree = title_fresh("wrong-runner-label")
        edit(tree, TITLE_PINS_REL, PIN_LABEL, "independent expectation")
        title_expect("wrong-runner-label", tree, False, want_usage=True,
                     text="the short label is not exactly")

        tree = title_fresh("consumer-label-independent")
        edit(tree, TITLE_PINS_REL, CONSUMER_PIN_LABEL, "independent")
        title_expect("consumer-label-independent", tree, False, want_usage=True,
                     text="the long label is not exactly")

        tree = title_fresh("consumer-section-missing")
        edit_json(tree, TITLE_PINS_REL, lambda document: document.pop("consumer"))
        title_expect("consumer-section-missing", tree, False, want_usage=True,
                     text="the section consumer is missing")

        tree = title_fresh("consumer-wrong-source")
        edit(tree, TITLE_PINS_REL, CONSUMER_PIN_SOURCE, "plan 12-05 the runner")
        title_expect("consumer-wrong-source", tree, False, want_usage=True,
                     text="the consumer record does not cite")

        tree = title_fresh("flipped-long-checkpoint-digest")
        header = open(os.path.join(root, TITLE_HEADER_REL), encoding="utf-8").read()
        position = header.index("#define T64C_TLIT_LONG_FRAMES_INIT")
        quote = header.index('"', position) + 1
        digit = header[quote]
        edit(tree, TITLE_HEADER_REL, header[quote - 1:quote + 4],
             '"' + ("0" if digit != "0" else "1") + header[quote + 1:quote + 4])
        title_expect("flipped-long-checkpoint-digest", tree, True,
                     text="differs from the regenerated header")

        tree = title_fresh("flipped-long-checkpoint-oracle")
        edit_json(tree, TITLE_EXPECTED_REL,
                  lambda document: document["scenarios"]["long"]["frames"].__setitem__(
                      1, "f" * 64))
        title_expect("flipped-long-checkpoint-oracle", tree, True,
                     text="differs from the regenerated header")

        tree = title_fresh("changed-event-tick-header")
        header = open(os.path.join(root, TITLE_HEADER_REL), encoding="utf-8").read()
        position = header.index("#define T64C_TLIT_LONG_EVENTS_INIT")
        anchor = header.index("UINT64_C(", position) + len("UINT64_C(")
        edit(tree, TITLE_HEADER_REL, header[anchor:anchor + 3],
             "%d" % (int(header[anchor:anchor + 3]) + 1))
        title_expect("changed-event-tick-header", tree, True,
                     text="differs from the regenerated header")

        tree = title_fresh("changed-event-tick-script")
        edit(tree, "fixtures/title/title.long.input.json", '"tick":"1071354"', '"tick":"1071355"')
        title_expect("changed-event-tick-script", tree, False, want_usage=True,
                     text="the SHA-256 of the script differs")

        tree = title_fresh("blank-digest-not-zero-bytes")
        edit(tree, TITLE_EXPECTED_REL, '"blank_sha256": "2aae7dc8', '"blank_sha256": "2aae7dc9')
        title_expect("blank-digest-not-zero-bytes", tree, False, want_usage=True,
                     text="blank_sha256 is not the SHA-256 of 49152 zero bytes")

        tree = title_fresh("blank-save-digest-not-zero-bytes")
        edit(tree, TITLE_EXPECTED_REL, '"blank_save_sha256": "c3502047', '"blank_save_sha256": "c3502048')
        title_expect("blank-save-digest-not-zero-bytes", tree, False, want_usage=True,
                     text="blank_save_sha256 is not the SHA-256 of 32768 zero bytes")

        tree = title_fresh("refused-expectation-not-blank")
        edit(tree, TITLE_EXPECTED_REL, '"frames_each_sha256": "2aae7dc8', '"frames_each_sha256": "2aae7dc9')
        title_expect("refused-expectation-not-blank", tree, False, want_usage=True,
                     text="the refused-frame expectation is not the blank digest")

        tree = title_fresh("refused-save-not-blank")
        edit(tree, TITLE_EXPECTED_REL, '"save_sha256": "c3502047', '"save_sha256": "c3502048')
        title_expect("refused-save-not-blank", tree, False, want_usage=True,
                     text="the refused-frame save expectation is not the blank save digest")

        tree = title_fresh("refused-expects-completion")
        edit(tree, TITLE_EXPECTED_REL, '"completes": false,\n    "frames_each_sha256"',
             '"completes": true,\n    "frames_each_sha256"')
        title_expect("refused-expects-completion", tree, False, want_usage=True,
                     text="does not expect no audio, no completion and no transcript")

        tree = title_fresh("blank-frame-in-a-scenario")
        edit_json(tree, TITLE_EXPECTED_REL,
                  lambda document: document["scenarios"]["short"]["frames"].__setitem__(
                      1, document["blank_sha256"]))
        title_expect("blank-frame-in-a-scenario", tree, False, want_usage=True,
                     text="has a frame equal to the blank digest")

        tree = title_fresh("runner-tick-not-in-record")
        edit(tree, TITLE_PINS_REL, '"completion_tick": "3693938"', '"completion_tick": "3693939"')
        title_expect("runner-tick-not-in-record", tree, True,
                     text="the cited record does not contain the runner completion tick")

        tree = title_fresh("consumer-digest-not-in-record")
        edit(tree, TITLE_RECORD_REL,
             "151bf84028782ead7a1265bf9d9d77917fdadfcd94bf0ee6dcb38314baad6b34", "0" * 64)
        title_expect("consumer-digest-not-in-record", tree, True,
                     text="the cited record does not contain the consumer final state digest")

        tree = title_fresh("wrong-schema")
        edit(tree, TITLE_EXPECTED_REL, "t64.title-oracle", "t64.title-guess")
        title_expect("wrong-schema", tree, False, want_usage=True,
                     text="schema is not t64.title-oracle version 1")
    print("check-consumer-literals: selftest failures=%d" % failures)
    return 1 if failures else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--root", default=None)
    parser.add_argument("--header", default=None)
    parser.add_argument("--title-header", default=None)
    args = parser.parse_args()
    root = args.root or os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    header_path = args.header or os.path.join(root, HEADER_REL)
    title_header_path = args.title_header or os.path.join(root, TITLE_HEADER_REL)
    if args.write and (args.check or args.selftest):
        parser.error("--write cannot be combined with --check or --selftest")
    if not (args.write or args.check or args.selftest):
        parser.print_usage(sys.stderr)
        return 2
    status = 0
    if args.write:
        status = run_write(root, header_path, title_header_path)
    if args.check:
        status = max(status, run_check(root, header_path, title_header_path))
    if args.selftest:
        try:
            status = max(status, selftest(root))
        except Usage as error:
            print("check-consumer-literals: %s" % error, file=sys.stderr)
            status = max(status, 2)
    return status


if __name__ == "__main__":
    sys.exit(main())
