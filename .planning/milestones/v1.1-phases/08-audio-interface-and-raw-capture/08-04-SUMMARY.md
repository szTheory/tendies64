---
phase: 08-audio-interface-and-raw-capture
plan: "04"
subsystem: runner
status: complete
tags: [raw-capture, filesystem, no-clobber, io]
requires:
  - phase: 08-03
    provides: Qualified focused packet and drain behavior
provides:
  - Actual general packet-to-file A/V capture with exact manifest and byte accounting
  - Private descriptor-relative staging and native macOS atomic no-replace publication
  - Reached cap IO incomplete-output termination and race controls
affects: [08-05, 09]
actuals:
  tokens: 52338
  tasks: 2
  commits: 2
tech-stack:
  added: []
  patterns: [retained parent FD, manifest-last staged bundle]
key-files:
  created: [runner/t64_raw_capture.c, runner/t64_raw_capture.h, docs/spec/raw-capture.md]
  modified: [runner/t64_real.c, runner/t64_supervisor.c, runner/t64_result_check.c, runner/tests/test_audio_capture.c, runner/CMakeLists.txt, docs/spec/audio-cases.json]
requirements-completed: [RUN-06, AUD-02, AUD-03]
requirements-contributed: [RUN-06, AUD-02, AUD-03]
key-decisions:
  - Native no-replace operation is required; unsupported hosts fail before guest startup.
  - Killed producers have unknown guest state in supervisor reports; private staged bytes are separate evidence.
coverage:
  - id: D1
    description: General actual raw A/V files identities metadata and byte accounting
    requirement: RUN-06
    verification:
      - kind: integration
        ref: scripts/check-audio-cases.py --build build/gate --suite capture --run
        status: pass
      - kind: integration
        ref: scripts/check-audio-cases.py --build build/asan --suite capture --run
        status: pass
    human_judgment: false
  - id: D2
    description: Native macOS no-clobber IO cap termination and parent-race controls
    requirement: AUD-02
    verification:
      - kind: integration
        ref: scripts/check-audio-cases.py --build build/gate --suite capture-errors --run
        status: pass
      - kind: integration
        ref: scripts/check-audio-cases.py --build build/asan --suite capture-errors --run
        status: pass
    human_judgment: false
completed: 2026-11-18
plan_head_before: 620f129e0f207d4ff412221b64b2fafd8749cd58
plan_head_after: 463d26f77b62d6c1d4469a7411c6e04492cf49a6
---

# Phase8 Plan04: actual raw A/V capture

A real supervised authored guest now writes general observer packets into an
opt-in raw A/V bundle. The helper retains parent/stage directory FDs, rejects
unsafe paths, bounds total output including manifest, writes metadata last and
commits atomically without replacing an existing target. Source review and whole
phase/platform qualification remain05; this is focused native evidence only.
Task1 `b588325`; Task2 `463d26f`. Nine files changed. Actuals are diff chars/4.
The new small runner helper/header and closed result vocabulary are necessary
roster extensions; no dependencies or production threads were added.

## Reached behavior

The accepted bundle contains29 actual12x7 RGBA packets and two stereo PCM
packets. Files match original observer bytes and independent PCM literals;
profile/content/input/core/runner identities, metadata, modes, lengths and
manifest self-size are checked. The measured complete bundle is16,581 bytes.
Cap16,580 refuses;16,581 and16,582 publish identical output;256MiB is accepted,
cap1 reports a specific partial-progress error and invalid caps refuse prestart.

Second geometry uses the unchanged accepted `rdp-fill` guest: an actual4x3,
stride16,48-byte API packet is written and compared in a private test-only stage.
Finalization correctly refuses absent audio. This proves the second general
writer geometry; it is not a successful second A/V bundle or a new fixture grant.

Interrupted/partial writes complete exactly; write, close, flush and rename
faults reach specific errors. Original producer and supervisor reports agree on
outcome/detail/tick/digest/stop, with stream-write failures at tick539440 and
final-rename refusal at551001. Existing file/directory/symlink targets preserve
bytes/mode/inode; dot-dot and intermediate symlink parents refuse. At precommit,
replacing the opened parent pathname with an outside symlink still publishes
only under the renamed original directory; outside sentinel is untouched.

Actual SIGKILL at the shared precommit barrier leaves a mode0700 private intact
stage and no final target. Supervisor retains wait status9/runner-signalled;
there is no invented producer report or known guest state for the killed process.
Its staged manifest and bytes are compared separately to the completed baseline.
Two real contenders reach the same barrier: exactly one publishes the complete
byte-identical baseline; loser reports the no-replace failure at actual completion
and cannot clobber the winner. Local runtime evidence is macOS only. Linux
implementation exists but execution remains required in05's existing hosted job.

## Exact checks and evidence

All focused checks pass after final source edits:

- Normal capture: `build/gate/08-04-final-normal-capture-capture-1795038298045361000-62170-capture-junit.xml`.
- Normal errors: `build/gate/08-04-final-normal-capture-errors-capture-errors-1795038304401178000-62308-capture-errors-junit.xml`.
- ASan/UBSan capture: `build/asan/08-04-final-asan-capture-capture-1795038315187682000-62676-capture-junit.xml`.
- ASan/UBSan errors: `build/asan/08-04-final-asan-capture-errors-capture-errors-1795038351207604000-95006-capture-errors-junit.xml`.
- Normal tracer: `build/gate/08-04-final-normal-tracer-tracer-1795038381146674000-9291-tracer-junit.xml`.

Each prefix retains commands/discovery/source guard, owning LastTest and unique
JUnit; raw attempts remain private under `build/gate/08-04-evidence/` and related
case roots. Parent parsed all five nonempty JUnits without failure/error/skip,
freshly ran source/body/object guard, checked reachable commits and owner edits.
No raw/generated asset was committed. Whole-phase requirements remain pending.

## Corrections and review handoff

Valid video sequence0 was initially rejected. A capture-cap pre-copy path omitted
its specific detail, and closed result vocabulary lacked capture labels; these
were corrected. A table tests all concrete capture host-error labels and rejects
unknown details or pass outcomes. Early usage/report-invalid originals remain
retained and do not count as semantic capture proof.

One collision test forgot to forward capture; argv assertions now require exact
target/cap/fault before launch. Evidence-root and baseline-winner filtering test
errors were fixed from retained originals. The synthetic interruption return was
replaced with actual process termination. Parent identified and filled the
missing intermediate-symlink/parent-substitution controls before closeout.

The final inventory has expanded explicit fault/vocabulary labels; host-error
progress/digest assertions are shared across actual error runs. Plan05 must audit
all original plan contracts against actual dispatcher/CTest reachability before
freezing its independent denominator; do not infer coverage from list-cases alone.
The result schema/spec, capture helper and all initial AI code still require the
planned independent full source review before final qualification.

## Next action and self-check

Plan05 Task1 adds evidence controls and safe CI projection; then parent source
review/fixes precede Task2 full native/compiler/hosted qualification. Owner edits
remain unstaged and unchanged. Oracle/VID-04, rights, expired windows and release
stops persist. No user action is needed.

## Self-Check: PASSED
