#!/usr/bin/env python3
"""Comparator of the host pump runs of the title (Phase 12, plan 12-08, API-05).

Reads the JSON lines that `t64h_tests --case title-two-speeds` prints (one line per script
and per fake-clock speed, schema t64.host-pump-run) and judges them against two things that
do not come from the pump or from any run of it:

  * fixtures/title/title.expected.json, the oracle's expectation (frames in order, audio
    packets and stream, the exported save image, the transcript), for the scenario of each
    script; and
  * fixtures/title/title.runner-pins.json, the final digest, the completion tick, the stop
    reason and the video packet count that a headless run recorded: the section `runner`
    (the top level of the file) for the short script and the section `consumer` for the
    long one. Both are recorded pins and not independent values (their labels say so); the
    oracle file is the independent one.

Per script it requires at least two distinct speeds, every value that depends on the run
only (not on the pace of the clock) byte-identical across the speeds, and each run equal to
the oracle and to the pins. Every mismatch names its key and its speed. The `info` object of
a line (step, advance and stall counters) is information only: it is printed, never
compared, and no time figure exists anywhere in the lines.

Overrides, for the seeded failure of a plan record: the environment variables
T64_TITLE_EXPECTED and T64_TITLE_PINS name scratch copies of the two files.

  check-host-title.py --runs FILE      judge the lines of FILE (- is standard input)
  check-host-title.py --selftest       seeded failures of the comparator itself

Standard library only; run with `python3 -I -B`. Exit codes: 0 equal, 1 a mismatch, 2 usage
or an unreadable input. No absolute path is printed.
"""

import copy
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIXTURES = os.path.join(ROOT, "fixtures", "title")
EXPECTED_DEFAULT = os.path.join(FIXTURES, "title.expected.json")
PINS_DEFAULT = os.path.join(FIXTURES, "title.runner-pins.json")

# script file name -> (scenario of title.expected.json, section of the pins file)
SCRIPTS = {
    "title.short.input.json": ("short", "runner"),
    "title.long.input.json": ("long", "consumer"),
}

SCHEMA = "t64.host-pump-run"

# The keys of a line that must agree across speeds (everything but the speed and the
# information counters).
RUN_KEYS = (
    "completed",
    "stop_reason",
    "completion_tick",
    "final_digest",
    "video_packets",
    "video",
    "audio_packets",
    "audio",
    "audio_stream",
    "transcript_bytes",
    "transcript",
    "save_bytes",
    "save",
)

HEX64 = re.compile(r"^[0-9a-f]{64}$")


class Refused(Exception):
    pass


def load_json(path):
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return json.load(handle)
    except (OSError, ValueError) as error:
        raise Refused("cannot read %s: %s" % (os.path.basename(path), error.__class__.__name__))


def parse_lines(text):
    """The run lines of the text: the JSON objects of schema t64.host-pump-run."""
    runs = []
    for number, line in enumerate(text.splitlines(), start=1):
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            record = json.loads(line)
        except ValueError:
            raise Refused("line %d is not JSON" % number)
        if not isinstance(record, dict) or record.get("schema") != SCHEMA:
            continue
        for key in RUN_KEYS + ("script", "speed_hz"):
            if key not in record:
                raise Refused("line %d lacks the key %s" % (number, key))
        runs.append(record)
    return runs


def expected_values(expected, scenario):
    """The oracle's values for one scenario, as the keys of a run line."""
    section = expected["scenarios"][scenario]
    return {
        "completed": True,
        "video_packets": section["video_packets"],
        "video": list(section["frames"]),
        "audio_packets": section["audio"]["packet_count"],
        "audio": list(section["audio"]["packet_sha256"]),
        "audio_stream": section["audio"]["stream_sha256"],
        "transcript_bytes": section["transcript_bytes"],
        "transcript": section["transcript_sha256"],
        "save_bytes": section["save"]["bytes"],
        "save": section["save"]["sha256"],
    }


def pin_values(pins, section):
    """The recorded pins of one section, as the keys of a run line, with the pin name."""
    block = pins if section == "runner" else pins[section]
    return {
        "completion_tick": (int(block["completion_tick"]), "completion_tick"),
        "stop_reason": (block["stop_reason"], "stop_reason"),
        "final_digest": (block["final_state_digest"], "final_state_digest"),
        "video_packets": (block["video_packet_count"], "video_packet_count"),
    }


def judge(runs, expected, pins):
    """Return (lines, problems): the report lines and the mismatches."""
    lines = []
    problems = []
    by_script = {}
    for run in runs:
        by_script.setdefault(run["script"], []).append(run)
    if not by_script:
        return lines, ["no run line of schema %s" % SCHEMA]
    for script, group in by_script.items():
        if script not in SCRIPTS:
            problems.append("script %s: unknown (expected one of %s)" % (script, ", ".join(sorted(SCRIPTS))))
            continue
        scenario, section = SCRIPTS[script]
        speeds = sorted({run["speed_hz"] for run in group})
        if len(group) != len(speeds):
            problems.append("script %s: a speed appears twice" % script)
        if len(speeds) < 2:
            problems.append("script %s: needs at least two distinct speeds, found %s" % (script, speeds))
        want = expected_values(expected, scenario)
        pinned = pin_values(pins, section)
        first = group[0]
        for run in group:
            speed = run["speed_hz"]
            for key in RUN_KEYS:
                if run[key] != first[key]:
                    problems.append(
                        "script %s: key %s differs between %d Hz and %d Hz" % (script, key, first["speed_hz"], speed)
                    )
            for key, value in want.items():
                if run[key] != value:
                    problems.append(
                        "script %s, %d Hz: key %s differs from the oracle (scenario %s): got %s, want %s"
                        % (script, speed, key, scenario, short(run[key]), short(value))
                    )
            for key, (value, pin_name) in pinned.items():
                if run[key] != value:
                    problems.append(
                        "script %s, %d Hz: key %s differs from the %s pin %s: got %s, want %s"
                        % (script, speed, key, section, pin_name, short(run[key]), short(value))
                    )
            for key in ("final_digest", "audio_stream", "transcript", "save"):
                if not isinstance(run[key], str) or HEX64.match(run[key]) is None:
                    problems.append("script %s, %d Hz: key %s is not a SHA-256 hex string" % (script, speed, key))
        if not any(p.startswith("script %s" % script) for p in problems):
            info = ", ".join(
                "%d Hz: %d steps, %d mid-step drain stalls"
                % (run["speed_hz"], run.get("info", {}).get("steps", 0), run.get("info", {}).get("backpressure_stops", 0))
                for run in group
            )
            lines.append(
                "%s: %s Hz equal; %d frames, %d audio packets (stream), %d-byte transcript, save image, "
                "completion tick %d and final digest equal the oracle and the %s pins [information: %s]"
                % (
                    script,
                    " and ".join(str(s) for s in speeds),
                    first["video_packets"],
                    first["audio_packets"],
                    first["transcript_bytes"],
                    first["completion_tick"],
                    section,
                    info,
                )
            )
    return lines, problems


def short(value):
    text = json.dumps(value)
    return text if len(text) <= 80 else text[:77] + "..."


def selftest():
    """Seeded failures: each edit of a good set of lines must be reported by key."""
    expected = load_json(EXPECTED_DEFAULT)
    pins = load_json(PINS_DEFAULT)

    def good_line(script, speed):
        scenario, section = SCRIPTS[script]
        record = {"schema": SCHEMA, "script": script, "speed_hz": speed, "info": {"steps": 1, "backpressure_stops": 0}}
        record.update(expected_values(expected, scenario))
        for key, (value, _) in pin_values(pins, section).items():
            record[key] = value
        return record

    def lines_of(records):
        return "\n".join(json.dumps(record) for record in records)

    base = [good_line(s, hz) for s in SCRIPTS for hz in (240, 3)]
    runs = parse_lines(lines_of(base))
    report, problems = judge(runs, expected, pins)
    if problems or len(report) != len(SCRIPTS):
        raise Refused("selftest: the good set is not accepted: %s" % problems)

    def seeded(name, edit, wanted):
        records = copy.deepcopy(base)
        edit(records)
        _, found = judge(parse_lines(lines_of(records)), expected, pins)
        if not any(wanted in problem for problem in found):
            raise Refused("selftest: seed %s not reported by %s: %s" % (name, wanted, found))

    def flip(text):
        return ("0" if text[0] != "0" else "1") + text[1:]

    seeded("final digest", lambda r: r[0].__setitem__("final_digest", flip(r[0]["final_digest"])), "key final_digest")
    seeded("final digest at the slow speed", lambda r: r[1].__setitem__("final_digest", flip(r[1]["final_digest"])), "3 Hz")
    seeded("frame", lambda r: r[0]["video"].__setitem__(1, flip(r[0]["video"][1])), "key video")
    seeded("audio stream", lambda r: r[2].__setitem__("audio_stream", flip(r[2]["audio_stream"])), "key audio_stream")
    seeded("save", lambda r: r[3].__setitem__("save", flip(r[3]["save"])), "key save")
    seeded("transcript", lambda r: r[0].__setitem__("transcript", flip(r[0]["transcript"])), "key transcript")
    seeded("completion tick", lambda r: r[2].__setitem__("completion_tick", r[2]["completion_tick"] + 1), "key completion_tick")
    seeded("stop reason", lambda r: r[0].__setitem__("stop_reason", "BOUND"), "key stop_reason")
    seeded("packet count", lambda r: r[1].__setitem__("video_packets", 4), "key video_packets")
    seeded("one speed only", lambda r: r.pop(1), "at least two distinct speeds")
    seeded("unknown script", lambda r: r[0].__setitem__("script", "other.json"), "unknown")
    seeded("speed differs from the other speed", lambda r: r[1]["audio"].__setitem__(0, flip(r[1]["audio"][0])), "differs between")

    # A pin that differs from the run is reported by the pin's own name.
    scratch = copy.deepcopy(pins)
    scratch["final_state_digest"] = flip(scratch["final_state_digest"])
    _, found = judge(parse_lines(lines_of(base)), expected, scratch)
    if not any("final_state_digest" in problem and "key final_digest" in problem for problem in found):
        raise Refused("selftest: a changed pin is not reported by key final_digest: %s" % found)
    _, found = judge([], expected, pins)
    if not found:
        raise Refused("selftest: an empty run set is accepted")
    return len(base)


def main(argv):
    runs_path = None
    do_selftest = False
    args = list(argv)
    while args:
        name = args.pop(0)
        if name == "--runs" and args:
            runs_path = args.pop(0)
        elif name == "--selftest":
            do_selftest = True
        else:
            print("usage: check-host-title.py --runs FILE | --selftest", file=sys.stderr)
            return 2
    if do_selftest == (runs_path is not None):
        print("usage: check-host-title.py --runs FILE | --selftest", file=sys.stderr)
        return 2
    try:
        if do_selftest:
            selftest()
            print("check-host-title: selftest ok (seeded failures reported by key)")
            return 0
        expected = load_json(os.environ.get("T64_TITLE_EXPECTED") or EXPECTED_DEFAULT)
        pins = load_json(os.environ.get("T64_TITLE_PINS") or PINS_DEFAULT)
        if runs_path == "-":
            text = sys.stdin.read()
        else:
            with open(runs_path, "r", encoding="utf-8") as handle:
                text = handle.read()
        lines, problems = judge(parse_lines(text), expected, pins)
    except Refused as error:
        print("check-host-title: %s" % error, file=sys.stderr)
        return 2
    except OSError:
        print("check-host-title: cannot read the run lines", file=sys.stderr)
        return 2
    for line in lines:
        print("check-host-title: " + line)
    for problem in problems:
        print("check-host-title: MISMATCH " + problem, file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
