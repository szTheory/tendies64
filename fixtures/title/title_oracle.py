#!/usr/bin/env python3
"""Independent oracle for the Phase 12 title (API-06, plan 12-02).

Every constant below is copied from fixtures/title/title-spec.md (cited as
"title-spec.md TS-n" at the constant) or from the document it names there
(docs/spec/vi.md for the field clock). Nothing in this file reads a ROM, the
core, the runner, a build output or a manifest row: it imports only the
standard library, reads only the input scripts that sit beside it and the
constants it carries, and a selftest proves the import set. The oracle was
written before the guest source exists, so no expected value comes from the
producer under test (guardrail G6). Every figure is a named software-profile
policy value of an authored title; no hardware accuracy is claimed.

Field clock (title-spec.md TS-4). The VI enable tick T0 is the origin of the
field boundaries B(k) = T0 + ceil(k * 812175 * 62500000 / 48681812), evaluated
in exact Fraction arithmetic with the ceiling rule of docs/spec/vi.md. T0 is a
NOMINAL value (ORIGIN_TICKS), not a measurement, and the model assumes that
the guest has programmed the VI and entered its polling loop before the first
boundary. A guest whose real T0 is within 100,000 ticks of the nominal value
gives the same expectations, because every input stamp must lie at least
200,000 ticks from every boundary (the oracle exits 2 otherwise). Plan 12-03
measures the real T0.

Usage:
  python3 -B fixtures/title/title_oracle.py --selftest
  python3 -B fixtures/title/title_oracle.py --print
  python3 -B fixtures/title/title_oracle.py --write FILE
  python3 -B fixtures/title/title_oracle.py --check FILE
  python3 -B fixtures/title/title_oracle.py --selftest --check FILE

--print writes the expectation document to standard output. --write FILE
writes it (the committed fixtures/title/title.expected.json) and --check FILE
regenerates the same text and exits 1, naming the first differing byte, when
FILE is not byte for byte equal to it, so the committed file cannot drift
from this script. --selftest runs the built-in cases (hand-written literals,
never values taken from the functions under test) and exits 1 on any FAIL.
Exit status 2 means a refusal: an invalid argument or script, or a model case
the oracle does not cover.
"""

from __future__ import annotations

import argparse
import ast
import hashlib
import json
import math
import os
import sys
from fractions import Fraction

SPEC = "fixtures/title/title-spec.md"
HERE = os.path.dirname(os.path.abspath(__file__))

# ---- title-spec.md TS-3 screen, TS-9 render ---------------------------------

WIDTH = 128
HEIGHT = 96
BYTES_PER_PIXEL = 4
FRAME_BYTES = WIDTH * HEIGHT * BYTES_PER_PIXEL  # 49,152 (DR-02)
BACKGROUND = (0x20, 0x30, 0x60, 0xFF)  # SetFillColor 0x203060ff (TS-9 step 4)
SQUARE = (0xF0, 0xC0, 0x20, 0xFF)  # SetFillColor 0xf0c020ff (TS-9 step 6)
SQUARE_SIZE = 16

# ---- title-spec.md TS-7 input rule ------------------------------------------

START_POSITION = (56, 40)
MAX_X = 112  # WIDTH - SQUARE_SIZE
MAX_Y = 80  # HEIGHT - SQUARE_SIZE

# ---- title-spec.md TS-4 time base (docs/spec/vi.md clock policy) ------------

TICK_RATE = 62_500_000
VI_CLOCK = 48_681_812
H_SYNC = 3093
V_SYNC = 525
ORIGIN_TICKS = 550_000  # nominal T0, the VI enable tick (an assumption)
BOUNDARY_MARGIN = 200_000  # DR-05, TS-15
SAVE_SIZE = 32_768  # docs/spec/save-media.md, SRAM on the -s32k profile


class Refused(Exception):
    """The input is invalid or the model does not cover the case (exit 2)."""


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


# ---- field boundaries (TS-4) -------------------------------------------------


def boundary(k: int) -> int:
    """Tick of field boundary k (k = 0 is the VI enable tick T0)."""
    cycles = Fraction(k * (H_SYNC + 1) * V_SYNC, 2)
    return ORIGIN_TICKS + math.ceil(cycles * TICK_RATE / VI_CLOCK)


# ---- frame model (TS-9) -------------------------------------------------------


def render(x: int, y: int) -> bytes:
    """The 49,152-byte frame with the square at (x, y), R,G,B,A byte order."""
    if not (0 <= x <= MAX_X and 0 <= y <= MAX_Y):
        raise Refused("square position (%d, %d) is outside the screen" % (x, y))
    background = bytes(BACKGROUND)
    square = bytes(SQUARE)
    plain_row = background * WIDTH
    square_row = background * x + square * SQUARE_SIZE + background * (WIDTH - x - SQUARE_SIZE)
    rows = [square_row if y <= py < y + SQUARE_SIZE else plain_row for py in range(HEIGHT)]
    return b"".join(rows)


BLANK_FRAME = bytes(FRAME_BYTES)  # the frame of a refused packet (TS-13)


# ---- the expectation document -------------------------------------------------


def constants_block() -> dict:
    return {
        "background_rgba": bytes(BACKGROUND).hex(),
        "boundary_margin_ticks": BOUNDARY_MARGIN,
        "field_cycles": (H_SYNC + 1) * V_SYNC // 2,
        "frame_bytes": FRAME_BYTES,
        "h_sync": H_SYNC,
        "height": HEIGHT,
        "max_x": MAX_X,
        "max_y": MAX_Y,
        "origin_ticks_nominal": ORIGIN_TICKS,
        "save_bytes": SAVE_SIZE,
        "square_rgba": bytes(SQUARE).hex(),
        "square_size": SQUARE_SIZE,
        "start_x": START_POSITION[0],
        "start_y": START_POSITION[1],
        "tick_rate": TICK_RATE,
        "v_sync": V_SYNC,
        "vi_clock": VI_CLOCK,
        "width": WIDTH,
    }


def expected_document() -> dict:
    return {
        "blank_bytes": len(BLANK_FRAME),
        "blank_sha256": sha256(BLANK_FRAME),
        "boundaries": [boundary(k) for k in range(0, 15)],
        "constants": constants_block(),
        "neutral_frame": {
            "sha256": sha256(render(*START_POSITION)),
            "x": START_POSITION[0],
            "y": START_POSITION[1],
        },
        "schema": "t64.title-oracle",
        "schema_version": 1,
        "spec": SPEC,
    }


def document_text(document: dict) -> str:
    return json.dumps(document, indent=2, sort_keys=True) + "\n"


def expected_text() -> str:
    return document_text(expected_document())


# ---- selftest -------------------------------------------------------------------

# Hand-computed once, outside this file, by integer ceiling division and by bc:
# ceil(k * 812175 * 62500000 / 48681812) for k = 1 to 12 (k = 1 is the figure
# 1,042,709 that docs/spec/vi.md gives for a standard field).
FIELD_TICKS_LITERAL = (
    1042709, 2085417, 3128126, 4170834, 5213543, 6256251,
    7298960, 8341668, 9384377, 10427085, 11469794, 12512502,
)  # fmt: skip
# SHA-256 of 49,152 zero bytes, computed once outside this file (shasum).
BLANK_SHA256_LITERAL = "2aae7dc846aaf25f1cadf55f1666862046c6db9d65d84bdc07fa039dac405606"

# Standard-library modules the oracle may import (closed list).
ALLOWED_IMPORTS = frozenset(
    {"__future__", "argparse", "ast", "contextlib", "fractions", "hashlib", "io", "json",
     "math", "os", "re", "sys"}
)  # fmt: skip
# Substrings that no non-docstring string constant of this file may hold
# (assembled so that this definition does not hold them itself).
FORBIDDEN_FRAGMENTS = (".z" + "64", "mani" + "fest", "bui" + "ld/", "run" + "ner/", "co" + "re/")


class Selftest:
    def __init__(self) -> None:
        self.failures = 0
        self.cases = 0

    def case(self, name: str, ok: bool, detail: str = "") -> None:
        self.cases += 1
        if ok:
            print("PASS %s" % name)
        else:
            self.failures += 1
            print("FAIL %s: %s" % (name, detail))


def source_imports() -> set[str]:
    with open(os.path.abspath(__file__), "r", encoding="utf-8") as handle:
        tree = ast.parse(handle.read())
    found: set[str] = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            found.update(alias.name.split(".")[0] for alias in node.names)
        elif isinstance(node, ast.ImportFrom):
            found.add((node.module or "").split(".")[0])
    return found


def source_strings() -> list[str]:
    """Every string constant of this file that is not a docstring."""
    with open(os.path.abspath(__file__), "r", encoding="utf-8") as handle:
        tree = ast.parse(handle.read())
    docstrings = set()
    for node in ast.walk(tree):
        if isinstance(node, (ast.Module, ast.FunctionDef, ast.ClassDef)):
            body = node.body
            if body and isinstance(body[0], ast.Expr) and isinstance(body[0].value, ast.Constant):
                docstrings.add(id(body[0].value))
    return [
        node.value
        for node in ast.walk(tree)
        if isinstance(node, ast.Constant) and isinstance(node.value, str)
        and id(node) not in docstrings
    ]


def selftest_tracer(test: Selftest) -> None:
    for k, literal in enumerate(FIELD_TICKS_LITERAL, start=1):
        test.case(
            "boundary-%d" % k,
            boundary(k) == ORIGIN_TICKS + literal,
            "B(%d) = %d, literal %d" % (k, boundary(k), ORIGIN_TICKS + literal),
        )
    test.case("boundary-origin", boundary(0) == ORIGIN_TICKS, "B(0) = %d" % boundary(0))
    test.case(
        "blank-digest",
        sha256(bytes(49152)) == BLANK_SHA256_LITERAL
        and expected_document()["blank_sha256"] == BLANK_SHA256_LITERAL,
        "blank digest %s" % expected_document()["blank_sha256"],
    )
    frame = render(56, 40)
    pixel = lambda px, py: tuple(frame[(py * WIDTH + px) * 4 : (py * WIDTH + px) * 4 + 4])  # noqa: E731
    test.case("neutral-frame-size", len(frame) == 49152, "length %d" % len(frame))
    test.case("neutral-frame-corner", pixel(0, 0) == (0x20, 0x30, 0x60, 0xFF), str(pixel(0, 0)))
    test.case("neutral-frame-square-first", pixel(56, 40) == (0xF0, 0xC0, 0x20, 0xFF), str(pixel(56, 40)))
    test.case("neutral-frame-square-last", pixel(71, 55) == (0xF0, 0xC0, 0x20, 0xFF), str(pixel(71, 55)))
    test.case(
        "neutral-frame-square-edges",
        pixel(55, 40) == (0x20, 0x30, 0x60, 0xFF)
        and pixel(72, 55) == (0x20, 0x30, 0x60, 0xFF)
        and pixel(56, 39) == (0x20, 0x30, 0x60, 0xFF)
        and pixel(71, 56) == (0x20, 0x30, 0x60, 0xFF),
        "an edge pixel is inside the square",
    )
    square_pixels = sum(
        1 for index in range(0, len(frame), 4) if tuple(frame[index : index + 4]) == (0xF0, 0xC0, 0x20, 0xFF)
    )
    test.case("neutral-frame-square-area", square_pixels == 256, "%d square pixels" % square_pixels)
    test.case("neutral-frame-not-blank", sha256(frame) != BLANK_SHA256_LITERAL, "frame equals blank")
    imports = source_imports()
    test.case(
        "imports-standard-library-only",
        imports <= ALLOWED_IMPORTS,
        "unexpected imports %s" % sorted(imports - ALLOWED_IMPORTS),
    )
    hits = [s for s in source_strings() for fragment in FORBIDDEN_FRAGMENTS if fragment in s]
    test.case("no-producer-paths", not hits, "string constants name producer files: %s" % hits[:3])


def run_selftest() -> int:
    test = Selftest()
    selftest_tracer(test)
    print("selftest: %d cases, %d failed" % (test.cases, test.failures))
    return 1 if test.failures else 0


# ---- command line -----------------------------------------------------------------


def check_file(path: str, text: str) -> int:
    with open(path, "rb") as handle:
        committed = handle.read()
    data = text.encode("ascii")
    if committed == data:
        print("title expectation equals the oracle (%d bytes)" % len(data))
        return 0
    index = 0
    while index < min(len(committed), len(data)) and committed[index] == data[index]:
        index += 1
    print(
        "title expectation differs from the oracle at byte %d (file %d bytes, oracle %d bytes)"
        % (index, len(committed), len(data)),
        file=sys.stderr,
    )
    return 1


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--selftest", action="store_true", help="run the built-in cases")
    parser.add_argument("--print", action="store_true", dest="show", help="print the expectation document")
    parser.add_argument("--write", metavar="FILE", help="write the committed expectation")
    parser.add_argument("--check", metavar="FILE", help="compare FILE with the regenerated text")
    try:
        args = parser.parse_args(argv)
    except SystemExit as error:
        return 2 if error.code else 0
    if args.write is not None and args.check is not None:
        print("title oracle refused: --write and --check are exclusive", file=sys.stderr)
        return 2
    if not (args.selftest or args.show or args.write is not None or args.check is not None):
        print("title oracle refused: choose --selftest, --print, --write or --check", file=sys.stderr)
        return 2
    status = 0
    try:
        if args.selftest:
            status = max(status, run_selftest())
        if args.show:
            sys.stdout.write(expected_text())
        if args.write is not None:
            with open(args.write, "wb") as handle:
                handle.write(expected_text().encode("ascii"))
        if args.check is not None:
            status = max(status, check_file(args.check, expected_text()))
    except (Refused, ValueError, KeyError, TypeError, OSError) as error:
        print("title oracle refused: %s" % error, file=sys.stderr)
        return 2
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
