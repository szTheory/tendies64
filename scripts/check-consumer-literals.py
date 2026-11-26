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

Modes:
  --write      regenerate the header from the three files
  --check      regenerate in memory and diff with the committed header; refuse a pins
               file whose label is not exactly "runner-recorded pin, not independent";
               confirm that the cited Phase 10 record contains the completion tick
               ("599,357" form) and the digest literal; confirm the label is in the
               header; consistency of the three files
  --selftest   seeded failures in temporary copies (wrong label, tick and digest not
               in the cited record, a flipped header byte, a wrong schema, a stale
               header, a clean control); each must be reported exactly as seeded;
               it may be combined with --check
Options: --root DIR (default: the repository of this script), --header FILE.

The consumer derives no expected value from the core or the runner at run time;
this script never runs either. Exit 0 pass, 1 findings, 2 usage or missing file.
"""
import argparse
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


def run_check(root, header_path):
    try:
        findings = check(root, header_path)
    except Usage as error:
        print("check-consumer-literals: %s" % error, file=sys.stderr)
        return 2
    for finding in findings:
        print("FINDING: %s" % finding)
    print("check-consumer-literals: findings=%d" % len(findings))
    return 1 if findings else 0


def run_write(root, header_path):
    try:
        text, _ = generate(root)
    except Usage as error:
        print("check-consumer-literals: %s" % error, file=sys.stderr)
        return 2
    with open(header_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
    print("check-consumer-literals: wrote %s" % os.path.relpath(header_path, root))
    return 0


def selftest(root):
    """Seeded failures in a temporary copy of the five files each case touches."""
    failures = 0
    with tempfile.TemporaryDirectory(prefix="t64-literals-") as scratch:
        def fresh(name):
            tree = os.path.join(scratch, name)
            for rel in (EXPECTED_REL, INPUT_REL, PINS_REL, HEADER_REL, RECORD_REL):
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

        def expect(name, tree, want_findings, want_usage=False):
            nonlocal failures
            try:
                findings = check(tree, os.path.join(tree, HEADER_REL))
                outcome = "findings" if findings else "clean"
                detail = "; ".join(findings)
            except Usage as error:
                outcome = "usage"
                detail = str(error)
            ok = (outcome == "usage") if want_usage else (
                (outcome == "findings") if want_findings else (outcome == "clean"))
            print("selftest %-28s %s (%s)" % (name, "ok" if ok else "FAILED", outcome))
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
    print("check-consumer-literals: selftest failures=%d" % failures)
    return 1 if failures else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--root", default=None)
    parser.add_argument("--header", default=None)
    args = parser.parse_args()
    root = args.root or os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    header_path = args.header or os.path.join(root, HEADER_REL)
    if args.write and (args.check or args.selftest):
        parser.error("--write cannot be combined with --check or --selftest")
    if not (args.write or args.check or args.selftest):
        parser.print_usage(sys.stderr)
        return 2
    status = 0
    if args.write:
        status = run_write(root, header_path)
    if args.check:
        status = max(status, run_check(root, header_path))
    if args.selftest:
        try:
            status = max(status, selftest(root))
        except Usage as error:
            print("check-consumer-literals: %s" % error, file=sys.stderr)
            status = max(status, 2)
    return status


if __name__ == "__main__":
    sys.exit(main())
