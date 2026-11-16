---
phase: 03-first-headless-rom-through-open-ipl3
plan: "02"
subsystem: fixtures
tags: [mips, open-ipl3, rom, reproducible-build]
requires:
  - phase: 03-01
    provides: qualified pinned open IPL3 asset, boot contract and Binutils toolchain
provides:
  - Authored flat hello and six deterministic control ROM fixtures
  - Checked C17 cartridge packer and headless fixture build/qualification commands
  - Rights and source, payload, and whole-ROM identities for the fixture inventory
affects: [03-03, 03-06, 03-11, boot-fixtures]
actuals:
  tokens: 18457
  tasks: 1
  commits: 5
plan_head_before: 3fc1de27903788ae59964335e6542560a3a3c2c6
plan_head_after: 4a1a4900247d3d3830a4131bd3626ab8cc17c31e
tech-stack:
  added: []
  patterns:
    - Separate whole source asset, embedded IPL3 payload, flat app payload and authored cartridge identities
    - Keep host-specific tool hashes in local build metadata, outside guest/profile identity
key-files:
  created:
    - fixtures/hello/hello.S
    - fixtures/hello/hello.ld
    - tools/fixture/t64_rom_pack.c
    - scripts/build-boot-fixtures.sh
    - .planning/phases/03-first-headless-rom-through-open-ipl3/03-02-task1-red.json
    - .planning/phases/03-first-headless-rom-through-open-ipl3/03-02-task1-green.json
  modified:
    - fixtures/manifest.json
key-decisions:
  - "Keep the pinned 4096-byte source asset, its 4032-byte executable payload and each authored whole ROM independently identified."
  - "Set header offset 0x10 to the checked nonzero 184-byte flat app length expected by loader_compat; keep the whole cartridge under one MiB."
  - "Pin the source files to a commit that contains them and their exact content hash; keep per-host binary identities in ignored build metadata."
patterns-established:
  - "Treat allowed header rewrites as explicit provenance: the base header is authored from zero, and the upstream IPL3 payload is copied byte-for-byte."
  - "Record authored Apache-2.0 source and embedded Unlicense bytes separately; generated ROMs remain build-only."
requirements-completed: [BOOT-02]
coverage:
  - id: D1
    description: "Rights-cleared hello and control ROMs build reproducibly from pinned tools and IPL3 bytes."
    requirement: BOOT-02
    verification:
      - kind: integration
        ref: "sh scripts/build-boot-fixtures.sh --qualify, --build, and --selftest"
        status: pass
    human_judgment: false
duration: 42min
completed: 2026-11-10
status: complete
---

# Phase 3 Plan 02: Build the Authored Flat Hello Summary

**An authored MIPS III hello and six control ROMs now build reproducibly from the qualified open IPL3 payload, with separate rights and byte identities.**

## Performance

- **Duration:** 42 minutes, measured from the first persisted task artifact
- **Started:** 2026-11-10T22:32:53-05:00
- **Completed:** 2026-11-10T23:15:00-05:00
- **Tasks:** 1
- **Files modified:** 11, including source, evidence and sequential planning records

## Accomplishments

- Added a freestanding MIPS III assembly hello that writes the exact 30-byte independent transcript through the pinned ISViewer contract, flushes, then reaches an authored BREAK site. Added empty BREAK, wrong-text, empty-loop, incomplete-loop, unsupported-opcode and output-overflow controls.
- Added a strict C17 packer with checked flat payload ranges, entry validation and a one-MiB whole-cartridge limit. The generated ROM header is authored from zero; it points to entry `0x80000400` and stores the checked 184-byte flat payload length at offset `0x10`, avoiding the pinned loader's one-MiB fallback.
- Recorded separate identities for the 4096-byte upstream IPL3 asset, its copied 4032-byte payload, the flat app and every whole ROM. The manifest records the permitted header transformations, rights, source commit/content hash, recipe, owner and build-only distribution.
- Reused the already-qualified GNU Binutils 2.45 tools and host system zlib. Host-specific binary hashes stay in ignored build metadata and do not enter guest/profile identity. No SDK, custom assembler, GCC runtime, Nintendo boot bytes or `nemu64-test` assets were added.

## Task Commits

1. **RED:** `f065686` — failing flat-payload range assertions; evidence was accepted by `gsd_run check tdd-red-evidence` as `RED_EVIDENCE_OK`.
2. **GREEN:** `e6f0655` — authored hello, packer and fixture build commands.
3. `d5f3482` — corrected the selftest's installed-payload verification path.
4. `47c756d` — corrected the qualification path to retain the installed payload identity.
5. `4a1a490` — registered fixture identities and the passing evidence record.

The RED report ran four TAP assertions: valid and exact-limit payload checks failed on the planned range behavior, while zero and overflow refusal checks passed. The observed failures were assertions, not compilation or discovery failures. GREEN expanded this to nine passing packer assertions and the full fixture controls. No separate refactor was needed.

## Files Created/Modified

- `fixtures/hello/hello.S` — authored guest code and six assembly-selected controls.
- `fixtures/hello/hello.ld` — fixed flat payload entry and bounds.
- `tools/fixture/t64_rom_pack.c` — checked header, IPL3 and flat-payload packer.
- `scripts/build-boot-fixtures.sh` — pinned tool/input checks and `--qualify`, `--build`, `--selftest` flows.
- `fixtures/manifest.json` — separate rights and identities for the source asset, embedded payload, hello ROM and six controls.
- `03-02-task1-red.json` and `03-02-task1-green.json` — actual RED and passing gate evidence.

## Decisions Made

- Keep upstream source asset, embedded IPL3 payload and authored cartridge as distinct identities; the source header is not copied into the authored ROM.
- Use header offset `0x10` for the checked loader-compatible flat payload byte length. This is not a CRC1 claim; CRC2 remains zero without a CIC checksum claim.
- Keep authored source under the project Apache-2.0 license and the embedded upstream IPL3 under its pinned Unlicense record. Generated bytes remain ignored local build artifacts.
- Pin authored source to commit `47c756d` plus its content digest. Later planning commits may descend from that source pin without changing the bytes it identifies.

## Automated Checks

- `sh scripts/build-boot-fixtures.sh --qualify` — passed tool, source, asset, payload, ROM and manifest identity checks.
- `sh scripts/build-boot-fixtures.sh --build` — passed and produced the seven ignored ROM artifacts.
- `sh scripts/build-boot-fixtures.sh --selftest` — packer TAP 9/9 passed. It rejected truncated and wrong-hash boot assets, zero-length and over-limit payloads, an unaligned entry, incorrect clock and CRC2 fields, and a wrong assembler. Two clean builds produced identical bytes and hashes for all seven variants; final source/tool/boot/payload/manifest checks passed.
- `build/gate/tools/manifest/t64-manifest-check --schema fixtures/manifest.schema.json --root "$PWD" fixtures/manifest.json` — passed with 12 entries and no errors.

The hello cartridge is 4280 bytes and contains a 184-byte flat app payload. The independent expected transcript has length 30 and SHA-256 `86594505bea53c9843f9eaf07e4cf0121cbe0055670324674fd8385c88ffd18e`. These are fixture construction and byte-level checks; **the guest was not executed, and this plan makes no claim that the hello runs.** BOOT-02 is recorded complete. BOOT-04 remains pending until a later integration slice executes the ROM through the headless guest path.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Verify the installed payload after staging moves**

- **Found during:** Task 1 selftest and qualification
- **Issue:** Both verification flows kept reading the extracted IPL3 payload through a temporary staging path after `install_generated_outputs` had moved the file to its stable ignored build location.
- **Fix:** Point manifest verification at `build/fixtures/ipl3_compat.payload.bin` after installation.
- **Files modified:** `scripts/build-boot-fixtures.sh`
- **Verification:** The complete `--qualify`, `--build` and `--selftest` sequence passed, including all negative controls and deterministic comparisons.
- **Committed in:** `d5f3482`, `47c756d`

**Total deviations:** 1 auto-fixed bug. The fix stayed within the fixture build script and did not expand CPU admission or guest execution scope.

## Issues Encountered

The first full selftest exposed the stale temporary path; qualification then exposed the same lifecycle issue in the shared build flow. Both call paths now use the installed payload location for identity checks.

## User Setup Required

None. Qualified tools and the cleared IPL3 asset were already present locally. Generated artifacts remain ignored and no external service is needed.

## Next Phase Readiness

The authored fixture set, independent transcript oracle, source pins and local build checks are ready for subsequent Phase 3 integration. The current core still accepts only the exact original 4096-byte asset for its partial tracer; authored-ROM admission and guest execution belong to later CPU/warm integration work. No Phase 3 completion or hello execution is claimed.

---
*Phase: 03-first-headless-rom-through-open-ipl3*
*Completed: 2026-11-10*

## Self-Check: PASSED

- Required source, manifest, RED evidence, GREEN evidence and summary files exist.
- Task commits `f065686`, `e6f0655`, `d5f3482`, `47c756d` and `4a1a490` are ancestors of the plan head recorded above.
