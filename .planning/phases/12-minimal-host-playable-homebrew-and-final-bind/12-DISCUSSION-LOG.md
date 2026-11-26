# Phase 12: Minimal Host, Playable Homebrew and Final Bind - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-11-26
**Phase:** 12-Minimal Host, Playable Homebrew and Final Bind
**Areas discussed:** none asked (hard-to-reverse choices already locked upstream)

---

## Locked upstream

| Question | Options considered | Chosen |
|----------|--------------------|--------|
| Homebrew route | A authored assembly in-tree; B libdragon toolchain build | A (lens-host Q1) |
| Host shape | SDL-only host; no-window raw player; pump plus optional SDL shell | pump plus optional shell |
| SDL sourcing | vendored copy; FetchContent; system install link-only | link-only system install (D4) |
| Presentation | SDL_Renderer/GPU; window-surface nearest blit | surface blit |
| LibFuturo | adapter now; written evaluation only | written evaluation, no adapter |
| Final bind | bind at Phase 11; candidate then final bind at end of Phase 12 | end of Phase 12 |

## Decided autonomously

[auto] [Host layout] — Q: "Where do the pump and shell live?" → Selected: "host/ top-level with pump and sdl" (reversible: directory layout)
[auto] [Title builder] — Q: "New builder or extend one?" → Selected: "copy build-pad-fixture.sh" (reversible: script copy)
[auto] [Checkpoint digests] — Q: "Long runs?" → Selected: "checkpoint digests" (test design)
[auto] [Input mapping] — Q: "Mapping policy?" → Selected: "documented deadzone, port 0 only" (test-pinned table)
[auto] [SDL lane] — Q: "How is the shell exercised?" → Selected: "manual/nightly, dummy drivers, virtual joystick" (lane placement)
[auto] [Save persistence] — Q: "Host-side saves?" → Selected: "omit unless one task" (scope cap)
[auto] [Throwaway measurement] — Q: "Real-time?" → Selected: "one measurement, no claim" (information only)
[auto] [LibFuturo re-pin] — Q: "Which commit?" → Selected: "re-read at plan time" (documentation)
[auto] [Review] — Q: "Cadence?" → Selected: "per wave plus final audit" (process choice)

## Claude's Discretion

Function names, sprite colors, tone frequency, plan decomposition, SDL CI job shape.

## Deferred Ideas

Route B, GPU renderer, UX features, adapter, VID-04, release, real microcode.
