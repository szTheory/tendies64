---
phase: 10-carry-in-closure-before-the-freeze
plan: 14
subsystem: fixtures
tags: [run-07, composite-guest, authored-rom, manifest-rows, runner-admission, twin-oracle, gate-02, per-row-binding, raw-capture, save-out, si-v2-s32k]

requires:
  - phase: 10-carry-in-closure-before-the-freeze
    provides: the four si-v2 ids with RDP depth active on descriptor 13 (plans 10-09 to 10-12), the per-row oracle pins and the Phase 9 claim binding to the pinned manifest view (plan 10-03), the Wave 3 base 1cded8d (plan 10-13), the preflight (plan 10-01)
provides:
  - fixtures/composite/composite.S, composite.ld, composite.input.json and composite_oracle.py (one authored guest covering video with a Z test, audio, input and save, and its independent expected-value script)
  - scripts/build-composite-fixture.sh and the CMake target t64-phase10-composite-fixture (outputs for two stems)
  - manifest rows phase10-composite-s32k and phase10-composite-wrong-s32k and the matching runner admission rows on ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k (the one manifest-moving commit of the phase)
  - PIN-IMPACT.md section 11 (row hashes, unchanged pins, supervised runs, capture sizes, consumer statuses before and after)
affects: [10-15, 10-17, 10-18, 10-19, phase-11]

actuals:
  tokens: 14253
  tasks: 4
  commits: 3

tech-stack:
  added: []
  patterns:
    - "a guest that must satisfy a legacy-shaped runner observation switches the VI off after a few fields and keeps every device phase short, so the raw capture is 2.4 KB against a 68 KiB per-file limit"
    - "a twin oracle restates the authored constants, the sample policy of the triangle, the Z exponent table and the strictly-after input rule from the spec text, and refuses (exit 2) any input whose stamp is too close to a measured poll tick"
    - "a builder --qualify that compares the built ROM identity with the manifest rows, so a guest edit that was not carried into the rows fails a cheap check"

key-files:
  created:
    - fixtures/composite/composite.S
    - fixtures/composite/composite.ld
    - fixtures/composite/composite.input.json
    - fixtures/composite/composite_oracle.py
    - scripts/build-composite-fixture.sh
  modified:
    - core/CMakeLists.txt
    - runner/CMakeLists.txt
    - fixtures/manifest.json
    - runner/t64_real_fixture.c
    - docs/spec/result-protocol.md
    - .planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md

key-decisions:
  - "The guest and the rows are two commits, not one: the manifest schema requires source.revision to be a 40-hex commit that holds fixtures/composite/composite.S, and a commit cannot name itself, so the sources are 419275d (no manifest row, no runner row) and the rows are f027b05, the only commit that touches fixtures/manifest.json since the Wave 3 base"
  - "Both runner rows use the long guest-tick cap, not the default: IPL3 hands over near tick 539,200 and the guest completes at tick 599,357, 643 ticks below the default 600,000, because each SI DMA models thousands of ticks (the pad-poll and replay-mix rows use the long cap for the same reason)"
  - "The three controller polls execute near ticks 555,600, 573,600 and 591,600 (measured once with 116 probe stamps); the input script stamps at 564,000 and 582,000, at least 8,200 ticks from every poll, and the oracle refuses a stamp within 2,000 ticks of a nominal poll tick"
  - "The video window is the legacy 4x3 class that the non-audio observation shape requires: on the filtered VI the V span counts half-lines, so V_START is 6, not 3, and V_SYNC is 128 as in the audio fixture"

patterns-established:
  - "Plan 10-15 passes --input-size and --input-sha256 with any non-default input script (the supervisor assumes the size and hash of the empty default otherwise and refuses at tick 0)"
  - "A new pinned runner row changes the pass-rule sentence of docs/spec/result-protocol.md (runner.fixture-transcript-pin compares it with the table): count and distinct lengths are edited with the row"

requirements-completed: [RUN-07, GATE-02]

coverage:
  - id: D1
    description: "One authored composite guest runs end to end through the real supervised runner on ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k: a Z-tested 4x3 frame (the second, farther triangle is rejected everywhere), two 16-byte AI DMAs, three SI polls driven by composite.input.json and a 16-byte SRAM record written and read back"
    requirement: "RUN-07"
    verification:
      - kind: integration
        ref: "t64-supervisor --real --fixture build/fixtures/phase10-composite-s32k.z64 --input fixtures/composite/composite.input.json --raw-capture-dir <dir>: outcome pass, positive-match, guest tick 599,357, 188-byte transcript equal to the oracle; the same run with --artifact-dir, --artifact-max-bytes 32768, --save-export-leaf composite.sra, --save-kind sram32k and --save-format raw-v1: outcome pass, the same state digest 97f27027..."
        status: pass
    human_judgment: false
  - id: D2
    description: "The raw capture holds one video window class (three frames of 4x3, 48 bytes) and exactly two 16-byte audio packets, and every file is far below the 69,632-byte child limit; the save-out run exports a 32,768-byte image"
    requirement: "RUN-07"
    verification:
      - kind: integration
        ref: "raw bundle: manifest.json 2,273 bytes, data 176 bytes, total 2,449 bytes; exported composite.sra 32,768 bytes with SHA-256 f0b7fe88... equal to composite_oracle.py save_sha256"
        status: pass
    human_judgment: false
  - id: D3
    description: "An independent twin script computes the expected 48 video bytes, the two audio packets, the 32 KiB save image and the transcript from the authored constants and the input script, never from runner or core output, and states the same for the empty input script; it imports only the standard library"
    requirement: "RUN-07"
    verification:
      - kind: other
        ref: "python3 -B fixtures/composite/composite_oracle.py --input fixtures/composite/composite.input.json exits 0 with the keys video, audio, save_sha256, transcript (and empty); the run outputs equal it byte for byte; the empty-script run prints the oracle's empty transcript; the wrong ROM prints the oracle's --variant wrong SR line and fails the pin; a stamp within 2,000 ticks of a poll and a bad schema exit 2; grep of the imports shows argparse, hashlib, json, sys, fractions only"
        status: pass
    human_judgment: false
  - id: D4
    description: "The manifest gains exactly the two rows, appended, in one commit, and harness.rdp-oracle-authority and harness.phase09-qualification pass with no oracle rebind and PINNED_REQUEST_SHA256 unchanged (GATE-02 proven on a real row); the closed Phase 9 records are byte-identical to the Wave 0 and Wave 3 bases"
    requirement: "GATE-02"
    verification:
      - kind: other
        ref: "json comparison against git show 1cded8d:fixtures/manifest.json (39 to 41 entries, the 39 equal in canonical JSON and position); git log 1cded8d..HEAD -- fixtures/manifest.json lists exactly f027b05; git diff --quiet of docs/claims/phase09.md and docs/evidence/phase09-qualification.md against both bases; ctest -R '^harness\\.(rdp-oracle-authority|phase09-qualification|audio-qualification|rsp-evidence-ownership)$' 4 of 4 on the committed head"
        status: pass
    human_judgment: false
  - id: D5
    description: "Every consumer of the manifest and of the runner table stays green on the committed head: the full gate preset (248 of 248), the preflight on the clean tree (Linux compile with the act image cc, 11 post-Gate steps replayed over the new manifest) and the two scripts no gate test runs keep their exit statuses"
    requirement: "GATE-02"
    verification:
      - kind: integration
        ref: "ctest --preset gate: 100 percent passed out of 248 (95 s); sh scripts/preflight-phase10.sh: PASS in 76 s; check-rsp-evidence.py --check 1 before and after, check-base-evidence.py --check-current 0 before and after (main tree), 2 before and after (worktree)"
        status: pass
    human_judgment: false
  - id: D6
    description: "No protected material enters the guest, the raw-capture exclusion is not relaxed and the Phase 9 pins are untouched"
    requirement: "RUN-07"
    verification:
      - kind: other
        ref: "git diff --quiet 6a6afc8..HEAD over runner/t64_real.c, runner/t64_supervisor.c and scripts/check-rdp-oracle-authority.py; sh scripts/rights-scan.sh --tree and sh scripts/pii-scan.sh --tree: findings=0; the guest embeds only the pinned openIPL3 asset the builders already use"
        status: pass
    human_judgment: true
    rationale: "That the guest is authored here and carries no protected bytes is a rights judgment; the scans and the diff show no finding and no change to the runner, supervisor or checker, but a scan cannot prove authorship"

duration: 27min
completed: 2026-11-25
status: complete
commits: 3
plan_head_before: 6a6afc8392e374c7a59abc28ee41d8ab16ee9901
plan_head_after: ffc08c1285c5008cd2f93be8cdfa8408808be592
---

# Phase 10 Plan 14: Composite guest, rows and the one manifest-moving commit Summary

**One authored MIPS guest draws two Z triangles over a 4x3 window (the farther one is rejected), plays two 16-byte AI packets, reads three SI polls from an input script and writes and reads back a 16-byte SRAM record on `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k`; it passes the real supervised runner in a raw-capture run (2.4 KB capture) and a save-out run (32,768-byte image) with the same state digest, matches an independent twin oracle byte for byte, and its two manifest rows landed in one commit with no oracle rebind and `PINNED_REQUEST_SHA256` unchanged.**

## Performance

- **Duration:** 27 min (2026-11-25T02:19Z to 02:46Z), including a 95 s full gate ctest run and a 76 s preflight
- **Tasks:** 4 (Task 1 tracer, Tasks 2 to 4 auto)
- **Files:** 5 created, 6 modified (about 1,230 added lines, 57,013 characters)
- **Host:** 1-minute load 12 to 15 (other sessions on the same machine), so seconds are upper bounds

## Accomplishments

- **Task 1 (tracer).** `fixtures/composite/composite.S` was built as one Z-tested 4x3 frame with one transcript line, admitted with provisional rows and run once through the supervisor. The first draft failed for two reasons that the plan text did not predict and that the tracer gate caught: the non-raw run ended `video-drain-error` because on the filtered VI the vertical span counts half-lines (`V_START` 6, not 3, gives three rows), and the raw run ended `capture-incomplete-streams` because a bundle needs an audio stream, so the audio phase moved into the tracer. After both, the raw run passed with a 2,272-byte capture, then the verify commands were re-run (`⚡ Tracer verified end-to-end — expanding`).
- **Task 2.** Added the SI polls (the mix idiom, three polls), the SRAM record and readback by PI DMA (the wrong variant flips the low bit of the last record word), the VZ and AI lines, `composite.input.json` (two stamps) and `composite_oracle.py`. The poll ticks were measured by a dense probe script (stamps 400 ticks apart): poll 1 in (555,400, 555,800], poll 2 in (573,400, 573,800], poll 3 in (591,400, 591,800]. Both supervised runs pass; the oracle equals the runs for the video bytes, both audio packets, the save SHA-256 and the 188-byte transcript, for the wrong ROM and for the empty input script.
- **Task 3.** Final rows and the unnamed-consumer fix (`runner.fixture-transcript-pin`, below). The 18 tests of the plan regex pass, plus the runner table controls, the rights scan and the PII scan (findings 0).
- **Task 4.** Sources committed (`419275d`), rows committed (`f027b05`, the one commit that touches `fixtures/manifest.json` since the Wave 3 base), the two non-gate consumers measured before and after (unchanged), the four manifest-adjacent CTests 4 of 4, the full preflight PASS (76 s, 227 units compiled with the act image cc, 11 post-Gate steps replayed with the real `qualify-audio.py` and `qualify-phase09.py` emit steps), and the full gate preset 248 of 248. PIN-IMPACT.md section 11 records the lot (`ffc08c1`).

## Task Commits

1. **Tasks 1 to 3 (working tree, one slice by the plan):** the guest sources, builder, CMake target, oracle and input script - `419275d` (feat)
2. **Task 4:** manifest and runner rows, the pass-rule sentence and PIN-IMPACT section 11 - `f027b05` (feat); the one manifest-moving commit
3. **Task 4 record:** consumer statuses after the rows commit and the preflight result - `ffc08c1` (docs)

**Plan metadata:** this SUMMARY and the STATE and ROADMAP updates follow in docs(10-14) commits.

## Files Created/Modified

- `fixtures/composite/composite.S`, `composite.ld` - the authored guest (505 lines) and its linker script (copy of `pcm.ld`)
- `fixtures/composite/composite.input.json` - two controller stamps, at ticks 564,000 and 582,000
- `fixtures/composite/composite_oracle.py` - the independent expected values (standard library only)
- `scripts/build-composite-fixture.sh` - copy of `build-save-fixtures.sh` for the two stems; `--qualify` also checks the built ROMs against the manifest rows
- `core/CMakeLists.txt`, `runner/CMakeLists.txt` - the target `t64-phase10-composite-fixture` and its packer ordering edge
- `fixtures/manifest.json`, `runner/t64_real_fixture.c` - two rows each (appended)
- `docs/spec/result-protocol.md` - the pass-rule sentence (17 pinned rows, 188 added to the lengths)
- `.planning/phases/10-carry-in-closure-before-the-freeze/PIN-IMPACT.md` - section 11

## Decisions Made

See key-decisions. The ones plan 10-15 needs: the guest completes at tick 599,357 on the long cap; the supervisor needs `--input-size` and `--input-sha256` for `composite.input.json`; the save-out options are `--save-kind sram32k --save-format raw-v1 --save-export-leaf <name>.sra --artifact-dir <dir> --artifact-max-bytes 32768`; both runs end with state digest `97f270274d456e883ceaa11f100f92c7fbf3150e89031fa51164e79991f1267c`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Premise correction] The rows commit is separate from the sources commit**
- **Found during:** Task 1 (the manifest schema) and Task 4
- **Issue:** the plan stages every file of Tasks 1 to 3 in one commit, but `fixtures/manifest.json` requires `source.revision` to be a 40-hex commit that holds the source path, and a commit cannot contain its own hash (the Phase 9 rows name the source commit `faa2a38` or `c9be030`, committed before the rows).
- **Fix:** sources first (`419275d`, no manifest row, no runner row), then the rows with `source.revision` set to it (`f027b05`). The one-manifest-commit requirement holds: `git log 1cded8d..HEAD -- fixtures/manifest.json` lists exactly `f027b05`, and the guest source was frozen before the rows were computed from it.
- **Commit:** `419275d`, `f027b05`

**2. [Rule 1 - Premise correction] The runner rows use the long cap**
- **Found during:** Task 2
- **Issue:** the plan says cap 0 (default 600,000 ticks); the guest completes at tick 599,357 (boot 539,200 plus three SI DMAs that model thousands of ticks each), 643 ticks below the cap, and a run with `--bound` above the cap is refused at tick 0.
- **Fix:** `T64_REAL_GUEST_TICKS_LONG_CAP` in both rows, as the pad-poll and replay-mix rows have.
- **Commit:** `f027b05`

**3. [Rule 3 - Blocking] A consumer of the runner table that the plan did not name**
- **Found during:** Task 3
- **Issue:** `runner.fixture-transcript-pin` failed: the pass-rule sentence of `docs/spec/result-protocol.md` says how many rows carry their own pin and which lengths they have, and the two new pinned rows changed both.
- **Fix:** the sentence now says 17 rows and adds 188 bytes (text only; the document is not a closed Phase 9 record and no pin binds it; `check-rsp-evidence.py` hashes it but already exits 1).
- **Files modified:** `docs/spec/result-protocol.md`
- **Verification:** the control passes; the full gate preset passes 248 of 248
- **Commit:** `f027b05`

**4. [Rule 1 - Premise correction] The tracer needs the audio stream, and the 4x3 window needs six half-lines**
- **Found during:** Task 1
- **Issue:** the plan runs the tracer (video only) through a raw capture and expects a pass; a raw bundle without audio ends `capture-incomplete-streams`. Separately the legacy 4x3 observation shape of the non-raw path needs `V_START` 6 on the filtered VI (half-lines), not the 3 of `fill.S`.
- **Fix:** the AI phase is part of the tracer; the VI registers follow `pcm.S` (`V_SYNC` 128) with `V_START` 6.
- **Commit:** `419275d`

**5. [Rule 2 - Added check] `--qualify` compares the built ROMs with the manifest rows**
- **Found during:** Task 1
- **Issue:** the plan's verify says `--qualify` fails on a hash or size different from the manifest row, but the copied save builder's `--qualify` only checks that files exist, and its identity check covers only the old 09-04 stems.
- **Fix:** the copy drops the 09-04 check and adds `check_manifest_identities` to `--qualify` (the build step stays independent of the rows, so a guest edit can rebuild before the rows are updated).
- **Commit:** `419275d`

**6. [Rule 1 - Premise correction] `.4byte` instead of `.word` for the RDP command list**
- **Found during:** Task 1
- **Issue:** the MIPS assembler aligns each `.word` with a variable-size fragment, so the `.if` that pins the list length was a non-constant expression.
- **Fix:** `.4byte` for the command, sample and audio words (no automatic alignment), which keeps the length assertion.
- **Commit:** `419275d`

**Total deviations:** 6 (4 Rule 1 premise corrections, 1 Rule 3 blocker, 1 Rule 2 added check). **Impact on plan:** none on scope or evidence; deviation 1 keeps the one-manifest-commit requirement and the 40-hex rule of the schema.

## Issues Encountered

- A probe run with a non-default input script ended `refusal` at tick 0 before `--input-size` and `--input-sha256` were passed; the supervisor assumes the empty default's size and hash otherwise. This is information for plan 10-15, not a defect.
- `harness.rsp-evidence-ownership` and the two scripts of the section 9 audit (rows e and f) are not exercised by the manifest binding; their statuses are recorded in PIN-IMPACT.md section 11.4 and are unchanged.
- `build-composite-fixture.sh` writes only the `phase10-composite*` outputs; the Phase 9 save builder re-ran twice because it also lists `fixtures/manifest.json` as an input, and its identity check passed both times.

## Authentication Gates

None.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None. The guest has no placeholder data; every printed value is derived from a draw, a DMA, a poll or a readback.

## Threat Flags

None new. T-10-32 mitigated (authored-here rows, the rights and PII scans and the governance CTests pass with findings 0; the guest embeds only the pinned openIPL3 asset). T-10-33 mitigated (`harness.rdp-oracle-authority` passed right after the edit and on the committed head; `PINNED_REQUEST_SHA256` unchanged). T-10-50 mitigated (the closed Phase 9 records are byte-identical to both bases; `harness.phase09-qualification` passes; the post-Gate replay of `qualify-phase09.py --emit-hosted` passes). T-10-34 mitigated (largest capture file 2,273 bytes of 69,632). T-10-35 mitigated (`runner/t64_real.c` and `runner/t64_supervisor.c` are unchanged).

## Next Phase Readiness

- Plan 10-15 can build the two-run test on `phase10-composite-s32k`: expected outputs in `composite_oracle.py` (`--input fixtures/composite/composite.input.json`; add modes there, the ROMs do not move), the exclusion untouched, `ctest` names unchanged by this plan. Run options and values: see Decisions Made.
- Orchestrator items: (1) nothing is pushed (plan 10-18 pushes); the local commits are `419275d`, `f027b05`, `ffc08c1`, this SUMMARY and the STATE and ROADMAP updates; (2) `requirements.ready-ids` will keep RUN-07 and GATE-02 unchecked while plans 10-15 to 10-19 declare them; (3) the deviation 1 two-commit shape should be read by the Wave 3 review (plan 10-18) as one manifest-moving commit, as `git log WAVE3_BASE..HEAD -- fixtures/manifest.json` shows; (4) the guest completes 643 ticks below the default cap, so the long cap in the rows is load-bearing: a later edit that shortens the guest could use the default cap, never the reverse; (5) the poll tick bands in the oracle were measured once, so a later change of SI or PIF timing moves them and the oracle then refuses the input (exit 2) instead of giving a wrong value.

---
*Phase: 10-carry-in-closure-before-the-freeze*
*Completed: 2026-11-25*

## Self-Check: PASSED

The five created files and the six modified files exist; `419275d`, `f027b05` and `ffc08c1` are ancestors of HEAD; `composite.S` carries `t64_save_completion` and `runner/t64_real_fixture.c` the row id `phase10-composite-s32k`; `runner/t64_real.c`, `runner/t64_supervisor.c` and `scripts/check-rdp-oracle-authority.py` are unchanged against the plan start; the Task 4 verify commands passed (`grep -c fixtures/manifest.json` of the last task commit was 1 at the time, the four CTests 4 of 4, the preflight PASS, the Wave 3 base check: exactly one manifest commit and the closed records unchanged); `git rev-list --count 6a6afc8392e374c7a59abc28ee41d8ab16ee9901..ffc08c1285c5008cd2f93be8cdfa8408808be592` is 3.
