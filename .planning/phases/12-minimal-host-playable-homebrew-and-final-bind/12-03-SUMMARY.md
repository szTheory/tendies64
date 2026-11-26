---
phase: 12-minimal-host-playable-homebrew-and-final-bind
plan: 03
subsystem: fixtures
tags: [title, authored-guest, mips-assembly, fixture-builder, manifest, probe, frame-lock, oracle-equality]

requires:
  - phase: 12-02
    provides: fixtures/title/title-spec.md (TS-1 to TS-16), the independent oracle fixtures/title/title_oracle.py, the four input scripts and fixtures/title/title.expected.json
  - phase: 12-01
    provides: decision record DR-01 to DR-04 (identity, screen, frame lock, behavior), registration map R-01 to R-05, the standing rules and the sampling pin guards
provides:
  - fixtures/title/title.S, the authored title guest (557 lines, 522 non-empty), final at source revision 7b962961fcb8e1c1600dc84dae8a67f0c88dce26
  - fixtures/title/title.ld (the composite layout plus a data-area assertion)
  - scripts/build-title-fixture.sh (copy of the composite builder, schema t64.phase12-title-fixture-build.v1, --build and --qualify)
  - CMake target t64-phase12-title-fixture building phase12-title-s32k and phase12-title-refused-s32k (build-only ROMs)
  - fixtures/manifest.json rows phase12-title-s32k and phase12-title-refused-s32k (commit 54731f4c98cb0203567708eeec1ff0bc04644432)
  - PIN-IMPACT "Title expectations" rows: title source revision, built identities, manifest rows commit, measured VI enable tick and poll timing
affects: [12-04, 12-05, 12-06, 12-08]

actuals:
  tokens: 10302
  tasks: 4
  commits: 6
plan_head_before: 19638db2d3538458a539e3f49f85b090076c36c8
plan_head_after: c47bc55e4abe70d9c59385fde82a32a230cfb706

tech-stack:
  added: []
  patterns:
    - "an authored guest proven equal to a spec-derived oracle by a throwaway probe against the installed prefix before its source is frozen; no expected value comes from the probe"
    - "measurement by instruction-count single-stepping with the trace enabled (16 instructions per advance keeps the 256-record trace ring from dropping)"
    - "fixture source and manifest rows in two commits, so source.revision names a commit that already holds the source"

key-files:
  created:
    - fixtures/title/title.S
    - fixtures/title/title.ld
    - scripts/build-title-fixture.sh
  modified:
    - core/CMakeLists.txt
    - runner/CMakeLists.txt
    - fixtures/manifest.json
    - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/PIN-IMPACT.md

key-decisions:
  - "The measured VI enable tick is 546,614 (nominal 550,000, delta -3,386) and the controller is sampled at B(p-1) + 14,860 or 14,861 ticks, both inside the TS-4 bands, so title-spec.md, the input scripts and title.expected.json stand unchanged (no re-derivation)"
  - "The guest data areas sit at RDRAM 0x4000 to 0x57ff (RDP list, PIF frame, PIF reply, slot, tone table) and title.ld asserts the payload ends below 0x80004000; the static RDP list is copied once and only the square's two FillRectangle words are rewritten per field"
  - "A stale VI pending bit is cleared by a VI_CURRENT write just before the STATUS write, so poll 1 always follows the first pending bit after the enable"
  - "The refused-frame variant changes only the SetOtherModes word of the .rodata list (.ifdef T64_TITLE_REFUSED); the core refuses FillRectangle outside fill cycle type (RENDER_RANGE), so the guest never leaves its first DP-done wait"
  - "Manifest rows use expected.kind spec-derived with revision sha256 of title.expected.json; the recipe carries the builder source-content SHA-256 as the composite rows do"

patterns-established:
  - "Title guest register map: $s0 ISViewer, $s1 SI, $s2 RDRAM KSEG1, $s3 PI, $s4 AI, $s5 x, $s6 y, $s7 press count, $fp previous A, $gp polls"

requirements-completed: []

coverage:
  - id: D1
    description: "Authored title guest, linker script, builder copy and fixture target build both ROMs from tracked sources with no committed ROM, payload or build output"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "cmake --build --preset gate --target t64-phase12-title-fixture; test -s build/fixtures/phase12-title-s32k.z64 and phase12-title-refused-s32k.z64; git ls-files --error-unmatch on the three sources; test -z git ls-files build/*, *.z64, *.payload.bin; sh scripts/rights-scan.sh (findings=0)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Guest equal to the independent oracle on every scenario (frames, audio, save, transcript; persist from the long run's save; refused ROM blank with no completion)"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "sh build/12-03-probe/probe-check.sh --scenarios all (throwaway, outside git): compared 41 keys, 0 mismatches"
        status: pass
    human_judgment: false
  - id: D3
    description: "Two manifest identity rows in a second commit, qualified by the builder, with governance checks and no oracle-authority rebind"
    requirement: "API-06"
    verification:
      - kind: integration
        ref: "sh scripts/build-title-fixture.sh --qualify; ctest -R '^(gov\\.manifest\\.|gov\\.rights\\.|harness\\.rdp-oracle-authority$)' (7 of 7 passed); commit-shape check on 54731f4 (only fixtures/manifest.json)"
        status: pass
    human_judgment: false

duration: 16min
completed: 2026-11-26
status: complete
---

# Phase 12 Plan 03: Authored Title Guest Summary

**A 522-line authored MIPS III title (128x96 polled frame lock, D-pad and stick movement, A tone burst, SRAM slot save and boot restore, ISViewer transcript, refused-frame defsym) built by a copy of the composite builder, equal to the independent oracle on all 41 compared keys of seven probe runs, frozen at 7b962961fcb8e1c1600dc84dae8a67f0c88dce26, with two build-only manifest rows committed separately and no oracle-authority rebind.**

## Performance

- **Duration:** 16 min
- **Started:** 2026-11-26T16:46:00Z
- **Completed:** 2026-11-26T17:02:17Z
- **Tasks:** 4 of 4
- **Files:** 3 created, 4 modified (no runner, consumer or host source, no oracle-bound file, no expected value)

## Accomplishments

- `fixtures/title/title.S` implements TS-3 to TS-13 from composed composite idioms: Status.IE cleared with no handler, ISViewer handshake, RDP fill list in uncached RDRAM with DPC START/END and the DP-done wait (MI DP mask set first, cleared by MI_MODE 0x0800), the TS-3 VI block with STATUS last, the MI VI pending poll cleared by VI_CURRENT, one RD64B poll per field after a one-time WR64B, the TS-7 input rule (B reset, D-pad or stick threshold 16, 2 pixels per axis, clamp), the A press edge with the press counter and a two-descriptor burst gated on AI busy/full, the TS-12 boot load and the TS-11 end sequence (slot by PI DMA, end line, `break` at `t64_title_completion`). No `eret`, `syscall`, exception vector, TLB write, Z, texture, RSP microcode or instruction-count delay loop.
- `scripts/build-title-fixture.sh` is the composite builder with the listed deltas only; the `t64-phase12-title-fixture` target sits beside the composite target (outputs `.z64 .elf .payload.bin .json` per stem, `add_dependencies` on `t64-boot-fixtures` and, in `runner/CMakeLists.txt`, on `t64-rdp-fill-fixture` for the packer race).
- Built identities: title ROM `dfe77f58...` (7,568 bytes, payload 3,472 bytes), refused ROM `7989af8a...` (7,568 bytes); the payloads differ only in the SetOtherModes word; completion PC `0x80000820`.
- Two manifest rows (authored-here, cleared, build-only, GNU Binutils 2.45, profile `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k`, `source.revision` 7b962961fcb8e1c1600dc84dae8a67f0c88dce26, expected spec-derived) in commit 54731f4, which changes only `fixtures/manifest.json`.

## Task Commits

1. **Task 1: Title tracer (boot, VI setup, frame lock, one rendered field, builder, target)** - `8d6d330` (feat)
2. **Task 2: Input, movement, reset, A tone burst, boot slot load, transcript routine** - `6db2224` (feat)
3. **Task 3: Start save, completion and refused variant (final title source)** - `7b96296` (feat); source revision recorded in PIN-IMPACT by `d9a22e3` (docs)
4. **Task 4: Manifest rows in a second commit** - `54731f4` (feat); rows commit recorded in PIN-IMPACT by `c47bc55` (docs)

**Plan metadata:** the commit of this summary with STATE and ROADMAP follows.

`commits: 6` is `git rev-list --count 19638db2d3538458a539e3f49f85b090076c36c8..c47bc55e4abe70d9c59385fde82a32a230cfb706`, read from the persisted ledger file of the plan.

## Probe results (throwaway, never committed, never an expectation)

The probe (`build/12-03-probe/probe.c`, C99, one plain `cc -std=c99` line against `build/gate/install-check/moved prefix/include` and `lib/libt64_core.a` plus the consumer's own `t64c_sha256.c`) and its driver `build/12-03-probe/probe-check.sh` live outside git. Scenario events come from the committed input scripts through a `python3 -I -B -c` line into `build/12-03-probe/events.h`; the comparison is `python3 -I -B build/12-03-probe/compare.py` against `fixtures/title/title.expected.json`.

- **Task 1:** first checkpoint digest of the short scenario `8227044b9a781c302a8d3130fc67645289acd2c5bc9fd79805f3be6fedca79d3` equals `title.expected.json` short frame 1 `8227044b9a781c302a8d3130fc67645289acd2c5bc9fd79805f3be6fedca79d3`.
- **Task 2** (`--scenarios short,short-wrong,long,shifted --keys frames,audio,boot-transcript`, 12 keys, 0 mismatches):

  | Scenario | Probe frames | Frames | Audio stream digest | Boot transcript |
  |----------|--------------|--------|---------------------|-----------------|
  | short | 3 | all checkpoint digests equal | `0f662b9b3a9da8502a01bc60abefc2bb48f4ada04f0048d91fef9cd97c0aa07a` (2 packets), equal | `''` equal |
  | short-wrong | 3 | all checkpoint digests equal | `0f662b9b...` (2 packets), equal | `''` equal |
  | long | 12 | all checkpoint digests equal | `42eac0cf13ac8c14c13e6171d3cfab5925274eeeaab6eb81fac279709d14cfd8` (4 packets), equal | `''` equal |
  | shifted | 13 | all checkpoint digests equal | `42eac0cf...` (4 packets), equal | `''` equal |

  Restore probe: a 32,768-byte image the probe builds from the TS-12 layout (magic `T64T`, x 60, y 44, count 2; never from a guest run) gives 3 frames equal to the persist expectation (restored first frame `ef24ff37...` equal) and the boot line `L 3c 2c 00000002` equal.
- **Task 3** (`--scenarios all`, 41 keys, 0 mismatches): short, short-wrong, long and shifted all equal for frames (exact counts 3, 3, 12, 13), audio, save (`339a7e8f...` short pair, `92d0f715...` long pair) and transcript (`S 38 28 00000001 04`, same, `S 3c 2c 00000002 0d`, `S 3c 2c 00000002 0e`), each completing; persist (the long run's exported save imported into a fresh instance, no events) equal for 3 frames, 0 audio packets, the boot line and the unchanged save, no completion; refused: all 4 frames blank, no audio, no transcript, blank save, no completion by tick 5,000,000.
- **Measurement** (`--measure`, instruction-count single steps of 16 with the trace enabled): VI enable (retired `STATUS` store) at tick 546,614, equal to the first packet's start tick; RD64B issued 3,984 or 3,985 ticks after each real boundary and the SI execute 10,876 ticks later, so sampling at B(p-1) + 14,860 or 14,861 (polls 1 to 8 of long). Completion ticks: short 3,693,938, long 13,078,313, shifted 14,121,022.

## Deviations from Plan

### Auto-fixed Issues

None.

### Other deviations (recorded for transparency)

- **[Rule 2 - correctness guard] `title.ld` gained an `ASSERT` that the payload ends at or below 0x80004000** (the start of the guest data areas). The plan said "same layout, title symbols"; the layout is unchanged and the assertion only fails the link if the payload ever grew into the data areas. Files: `fixtures/title/title.ld`. Commit `8d6d330`.
- **A VI_CURRENT write precedes the STATUS write** (not listed in TS-3): it clears a pending VI bit that might predate the enable, so poll 1 cannot happen one field early. It changes no expectation (TS-5 already clears the bit by a VI_CURRENT write).
- **`t64_title_completion` existed from Task 1** as an unreachable `break`, because the copied builder refuses an ELF without the completion symbol; Task 3 placed the end sequence in front of it. The refused defsym had no effect until Task 3, so the two ROMs of Tasks 1 and 2 differed only in the ELF symbol table.
- **Sampling pin guards:** the standing rule asks for them after every task that touches CMake or the manifest. They ran after Task 3 (with Task 1's CMake edits in the tree, together with `harness.title-oracle`, 6 of 6) and again at the final head (5 of 5); `harness.rdp-oracle-authority` also ran at once after the manifest edit. They were not run separately right after the Task 1 commit.
- **Extra checks, run for safety:** `ctest -R '^(gov\.|harness\.|repo\.)'` (63 of 63 passed) because the new manifest rows are read by several evidence checkers, and `sh scripts/preflight-phase12.sh --no-replay` (PASS, 62 s).
- **The probe gained modes outside the plan's list:** `--measure` (trace single-stepping for the T0 and poll-delay measurement) and a `restore` scenario (the slot-image probe of Task 2), which `all` also runs.
- `requirements-completed` is empty on purpose: API-06 needs the runner rows (12-04) and the consumer cases (12-05), not only the guest.

## Authentication Gates

None.

## Issues Encountered

- The first measure attempt stepped 100 instructions per advance and the 256-record trace ring dropped records (more than one record per retired instruction); 16 instructions per advance dropped none.
- `objdump` prints immediates in decimal (`li t1,771`), so the probe driver finds the watched PCs by opcode word, not by mnemonic text.
- `build/gate/Testing/Temporary/LastTestsFailed.log` (dated 00:27, before this plan) lists `unit.core.cpu-time` and `unit.core.sched.matrix` from an earlier run; no test of this plan failed.
- Host load reached 13 once during the full gate build; the CTest runs started at load 6 to 7 and no limit was widened.

## Verification Results

| Check | Result |
|-------|--------|
| `cmake --build --preset gate --target t64-phase12-title-fixture` (each task) | both ROMs built, non-empty |
| `git ls-files --error-unmatch` title.S, title.ld, builder | tracked |
| `test -z "$(git ls-files -- 'build/*' '*.z64' '*.payload.bin')"`; probe untracked | empty; `build/12-03-probe` untracked |
| `sh scripts/rights-scan.sh` | tree=1476 history=5344 findings=0 |
| `grep -c -i -E 'eret\|syscall\|0x80000180' fixtures/title/title.S` | 0 |
| `grep -c -i -E 'mtc0.*(entryhi\|entrylo\|index)\|tlbw' fixtures/title/title.S` | 0 |
| `grep -c . fixtures/title/title.S` | 522 (limit 600) |
| probe-check Task 1 / Task 2 / Task 3 (`all`) | 1 / 12 / 41 keys, 0 mismatches |
| sampling set + `harness.title-oracle` (Task 3) | 6 of 6 passed |
| `sh scripts/build-title-fixture.sh --qualify` | both manifest identities match |
| `ctest -R '^(gov\.manifest\.\|gov\.rights\.\|harness\.rdp-oracle-authority$)'` | 7 of 7 passed, no rebind |
| Task 4 commit-shape check on `54731f4` | only `fixtures/manifest.json` |
| `grep -c '"phase12-title' fixtures/manifest.json` | 2 |
| both rows' `source.revision` = 7b96296..., which contains `fixtures/title/title.S` | yes (`git ls-tree`) |
| `git cat-file -e 7b962961fcb8e1c1600dc84dae8a67f0c88dce26` | exists |
| sampling set at the final head | 5 of 5 passed |
| `sh scripts/preflight-phase12.sh --no-replay` | PASS |
| `sh scripts/pii-scan.sh --tree` before each docs commit | findings=0 |

## Known Stubs

None.

## Threat Flags

None. T-12-07 is mitigated by the tracked-output check and the rights scan, T-12-08 by the probe rule (outside git; every expected value comes from `title.expected.json`), T-12-09 by appending rows only and running `harness.rdp-oracle-authority` at once, T-12-10 by the grep gates and composite-only idioms.

## Next Phase Readiness

Plan 12-04 (runner rows and the title two-run test) can start: the guest is frozen at 7b962961fcb8e1c1600dc84dae8a67f0c88dce26, the ROM identities are in the manifest and PIN-IMPACT, and the short scenario completes at tick 3,693,938, below the runner guest cap. Nothing here is orchestrator-only or owner-gated and no push is named. Local commits since `origin/main` (`dc247fa`) now include this plan's six commits and this summary.

## Self-Check: PASSED

- FOUND: fixtures/title/title.S, fixtures/title/title.ld, scripts/build-title-fixture.sh, the t64-phase12-title-fixture target, the two manifest rows, the PIN-IMPACT rows
- FOUND commits: 8d6d330, 6db2224, 7b96296, d9a22e3, 54731f4, c47bc55 (all ancestors of HEAD)
