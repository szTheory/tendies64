# Phase 3 decision research brief

## Authorized outcome

A project-authored hello ROM executes through HLE-PIF initialization and an
actual pinned open IPL3, emits ISViewer output, and is classified by the real
headless runner with identical state digests across Linux/GCC and macOS/Clang.
All six Phase 3 ROADMAP criteria and eight mapped requirements remain binding.
Primary-document verification opens the phase. Research and reversible choices
are autonomous under AGENTS.md and AUTONOMY.md; no routine owner acknowledgment.

## Current implementation

Phase 2 is complete. C17/CMake/Ninja core: per-instance owned state, bounded
integer scheduler, physical RDRAM/SP/ROM map, MI and delayed PI/SP model DMA,
canonical digest v5, trace, bounded admission. Public API and machine/result
contracts live in core/include/tendies64/t64_machine.h and docs/spec/.
Boot, CPU/RSP instruction execution, virtual CPU addressing, VI/audio/SI, and
real guest result capture do not exist. Native model results are a distinct
schema and are rejected as guest evidence. Both hosted gates pass current source.
A bounded production admission sanitizer campaign passed with a negative control.

## Prior decisions and stops

- C17 and existing allocator/ownership/determinism contracts remain accepted.
  Add the minimum real instruction/device behavior needed for this vertical
  boot slice, then complete CPU/RSP breadth in later phases. No stub guest pass,
  skipped IPL3, host-driven guest time or unsupported hardware-accuracy claim.
- First profile is NTSC cartridge-only 4 MB; CPU/RSP quantum, boot route and
  limitations are identity-bearing. Profile changes require explicit versioned
  fields and corresponding digest/tests; full API/ABI freeze is Phase 10.
- Apache-2.0 public repo; compatible copied helpers limited to ISC/MIT/BSD-3/
  Unlicense with provenance/notice and maintenance owner. No external emulator
  implementation copying. No paid services or new dependency by default.
- No Nintendo PIF/IPL/CIC/ROM bytes; no protected fixtures, owner-held files or
  physical hardware tests. Manuals are rights-bearing: source URL and SHA-256
  evidence may be committed; downloaded document bytes stay private/local.
- nemu64-test rights remain unresolved; do not download/build/use it here.
  The hello fixture must be authored or independently rights-cleared.
- D3 open IPL3 revision/variant and maintenance owner is self-unblockable with
  primary-source evidence. Verify mutable claims (including archive status),
  per-file license and generated artifact rights; do not trust bootstrap dates.
- The supervisor must attest the exact bytes the real runner loads using an
  already-open verified descriptor or private copy (Phase1 WR-06).
- Runtime/model mapping stays project-owned; keep user dirty AGENTS/config and
  untracked cache untouched. Researchers own only assigned decision artifacts.

## Output discipline

One compact recommendation per decision cluster: viable options/tradeoffs,
selected path and reversal evidence, source facts versus software expectations,
minimum implementation seams, meaningful acceptance/negative controls and
actual blockers. Cite primary sources with URL/revision/date and local code
paths. Stop when more research cannot change the recommendation or evidence
plan. Do not write plans or implementation during this fan-out. Avoid repeated
broad discovery; inspect only assigned sources and relevant current contracts.
