---
phase: 07-rdp-reference-renderer-and-vi-scanout
reviewed: 2026-11-17
depth: standard
reviewer: orchestrator test-only diff and evidence recheck
files_reviewed: 3
files_reviewed_list:
  - core/tests/test_vi.c
  - core/tests/test_profile.c
  - docs/spec/rdp-vi-cases.json
findings:
  critical: 0
  warning: 0
  info: 0
  total: 0
  necessity: 0
status: clean
---

# Plan07-09 test repair recheck

## Narrative Findings (AI reviewer)

The orchestrator rechecked the test-only changes after independent production
review. Literal H4/V8 to H5 timing uses cumulative doubled phase40/88/136
and guest deadlines26/57/88; the registered regression checks accepted packets
at26/57, while the separate frozen-helper reproduction also checks88. The
V_SYNC8 to10 control uses26/58. Both follow integer ceiling conversion with
62500000/97363624, independently of production scheduling helpers.

Additional reached cases check immediate INTR4 to2 at tick20 and comparison
at21, no same-tick retroactive comparison after CURRENT acknowledgment,
serrate enable/disable before/on/after the field latch, and all four decoded
H/V endpoints with nonzero starts. Original INTR failures were the absent
target2 comparison at21 and the stale target4 comparison at42, not proof of
a retroactive same-tick interrupt.

The full320x240 interlace test checks both parities and source spans306560/
307200bytes. Distinct first/last-row colors prevent a repeated early row from
satisfying the output oracle. Progressive Q10 offset1024 samples rows1..240,
with exact-fit154240bytes ending at RDRAM and an eight-byte-beyond control.
Setup/old-packet acceptance precedes the intended new-field assertions.
The earlier malformed Q10 fixture and post-drain metadata reads were corrected
as test bugs; they are not production RED evidence.

V7 and legacy VI-v2 profile hash/serialized-length goldens came from public
APIs linked with the retained pre-Plan09 header/library. V7 is1231bytes with
hash56f25e100d3c92792f836b0664e81697919487b4fdae49404476a85135ae5969;
VI-v2 is1038bytes with
hash5612696247abb5dae1c2abb6c15d7fd6c5632136210be7b21a1085590af1c94e.
The current profile owner passes with these and the preceding goldens.

## Reached verification and limits

- Registered VI inventory:29cases; final normal and ASan/UBSan runs each
  report1813checks and zero failures. Final logs are
  `build/gate/07-09-review-final-v2-vi-timing-LastTest.log` and
  `build/asan/07-09-review-final-v2-owners-LastTest.log`.
- Profile/accepted runner/wrong-pixel/RDP-fill owners pass4/4; profile reports
  1122checks. Earlier focused ASan VI/profile/fill owners pass3/3. The final
  test-only row-color addition received its own normal/instrumented VI recheck.
- Changed production VI and both test translation units pass existing GCC13
  warnings-as-errors checks; only core uses the core frame-size limit. The final
  row-color test also passes its focused GCC check. Source/body guard and all
  51mutation controls pass without weakening the reviewed roster.
- Original library failures and invalid attempts are retained separately in
  `build/gate/07-09-review-repro/REPAIR-EVIDENCE.md`; the minimal parent proof
  and independent arithmetic are in `build/gate/07-09-parent-repro/`.

This closes the initial compatibility-test warning and test coverage for the
production repairs. It is a scoped orchestrator recheck, not a second full
independent production audit. The integrated gate passed152/153 tests but
stopped on private workstation paths in the unpublished evidence commit.
The working document is redacted and its tree privacy scan passes; the
unreached core-lint tail was then executed and passed. The owner subsequently approved amending only that unpublished commit;
reachable-history privacy and the complete final gate now pass. Public history
was unchanged. Original failed results remain retained. No full ASan
inventory or hardware/VID-04 qualification is claimed.
