---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-17
depth: standard
reviewer: orchestrator test-only diff and evidence recheck
files_reviewed: 3
files_reviewed_list:
  - core/tests/test_vi.c
  - core/tests/test_video_output.c
  - docs/spec/rdp-vi-cases.json
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Plan07-10 test repair recheck

## Narrative Findings (AI reviewer)

The orchestrator checked the test-only diff after independent production
review. One-visible-row AA has partial foreground coverage2 and no full
background on row0; only the in-RDRAM row1 supplies the full background.
Literal(2F+6B)/8 yields RGBA(80,110,160,255), and WIDTH5 RGBA8888 reaches
40source bytes. A one-column dither witness repeats the top red row twice and
the next blue row once, giving RGBA(170,0,85,255) and4source bytes. At the
actual RDRAM end, invalid neighbors retain the valid-center fallback. The
older3x3 dither fixture now reads row3/column2, independently yielding24bytes
rather than18; its pixel expectations remain unchanged.

The frozen-helper old-library reproduction has7failures/48checks; the
repaired normal and ASan/UBSan versions each pass48checks. Registered old-core
AA and dither cases also reach the intended failures. The edge control passes.
Original commands/outcomes, invalid attempts and immutable archive/header/
fixture hashes remain in `build/gate/07-10-review-repro/REPAIR-EVIDENCE.md`
and `build/gate/07-10-parent-repro/`.

Whole/split now compares one public request to tick166 against requests83,
165,166, checking both packets, fields/sequences, progressive parity0, actual
metadata/bytes and equal guest digests before/after drains. It no longer uses
the same field-by-field helper on both sides. The earlier parity expectation
mistake remains a test failure, not a production defect.

Every unexpected filter fixture setup/allocation/configuration/advance return
records a failed assertion. The valid-case forced-advance-failure control exits1
with exactly one counted failure on the current binary, so setup cannot silently
skip its pixel/metadata assertions. This is an authored harness control, not
production RED evidence.

## Reached verification and limits

Final normal exact suite checkers pass: VI filters745checks, output381checks
and real supervised cadence; normal baseline6/6, instrumented owners9/9.
GCC13 strict production and changed-test compiles pass. Source guard80 and all
52mutation controls pass. Complete final gate passes156tests and lint in61s.
Original implementation and review-repair archives remain retained and checked;
review normal94/instrumented35entries bind exact source, binaries and logs.

This scoped orchestrator recheck resolves WR-01/02; the separate independent
production recheck resolves CR-01. The evidence-note profile-ID typo is fixed.
The earlier128tick runner attempt hit the fixed trace observer capacity before
the no-progress guard. That log is not a runner resume counterexample; the
static guard concern remains unproven and is not promoted to a blocker. No
resource bound was widened. This is not full ASan inventory, hardware accuracy
or VID-04 acceptance.
