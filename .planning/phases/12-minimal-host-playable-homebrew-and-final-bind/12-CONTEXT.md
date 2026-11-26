# Phase 12: Minimal Host, Playable Homebrew and Final Bind - Context

**Gathered:** 2026-11-26
**Status:** Ready for planning

<domain>
## Phase Boundary

An authored, rights-cleared title runs headlessly (runner and installed consumer) and in a minimal host built only on the installed API; the core stays free of the host; the ABI candidate bound in Phase 11 (ABI 0.1, `docs/abi/FREEZE-CANDIDATE.md`) is bound for good at the end, or each defect-driven amendment is recorded with evidence and a re-run installed-consumer suite. Requirements: API-05, API-06, API-07. No oracle material (D5); no tag, release or binary redistribution (D6); neutral development window title only (D7); SDL3 link-only against a system install, never vendored (D4); the physical-controller play-through is one owner-attested, non-blocking checklist, never a must-have.

</domain>

<decisions>
## Implementation Decisions

### Locked by REQUIREMENTS.md, ROADMAP.md and the v1.2 fan-out (not re-asked)
- API-06 route A: an authored assembly title built with the pinned Binutils 2.45 and the pinned open IPL3 through the existing fixture-builder pattern (copy-paste of the previous builder, no new toolchain, no committed ROM, manifest rows `authored-here / cleared / build-only`); route B (libdragon) is a documented non-goal for v1.2 — **Reversibility:** costly — a toolchain and rights choice that later fixtures build on
- Title scope cap (<= ~600 lines of assembly composed from the existing qualified idioms: `fixtures/rdp/fill.S`, `pad/poll.S`, `audio/pcm.S`, `save/*.S`, `replay/mix.S`): one screen, one 16x16 filled square moved by the D-pad or stick, one fixed AI tone burst on A, one save slot (EEPROM or SRAM) persisting position or score, B resets; frame-locked on the VI interrupt or VI_CURRENT polling, never instruction-count delay loops; no Z, no textures, no RSP microcode, no TLB, no libultra; it uses the one named composite profile that the Phase 10 RUN-07 fixture selects so title and fixture share an identity
- API-05 split: a pure-C `host-pump` with no SDL (drains the video/audio rings in host order, injected monotonic clock, logical input mapping, RGBA8 to pixel buffer, scripted input source) unit-tested in the required gate with a fake clock; plus an optional SDL3 shell of roughly 300 lines, OFF by default, in a separate non-gating (manual or nightly) lane; the core builds and links with no SDL dependency and a build without the host target succeeds
- SDL3: link-only via `find_package(SDL3 CONFIG)` against a system install; no vendoring and no FetchContent (zlib is not on the D4 allowlist: vendoring is an owner stop); SDL version and license recorded in the evidence; presentation by window-surface blit with nearest or integer scale only (no GPU renderer); window title neutral ("t64 minimal host (development)"), no icon, no bundle, no installer, no product name in user-visible strings (D7)
- "Plays" means correct, not real-time: no throughput claim, no A/V sync or latency number, underrun policy is play silence and count it, no vsync dependence; the pacing is a pure function of the injected clock; a screenshot proves nothing, digest equality does
- Evidence tiers: scripted input through the host-pump with a fake clock at two speeds gives byte-identical video, audio and save results to the headless run (the claim); SDL glue smoke under dummy drivers with a virtual joystick is self-checking host-glue only and not rendering correctness; the owner-attested physical checklist (`docs/evidence/phase12-host-manual.md`, human-needed, non-blocking per AUTONOMY dispositions) records date, OS, SDL version, controller model, build id and pass/fail lines
- API-07: a written LibFuturo fit evaluation pinned to a commit SHA of the LibFuturo project (the Phase 11 paper read recorded its local SHA in PIN-IMPACT; re-take the pin at plan time and state whether it is published), expected conclusion "no adapter now", no adapter code, no core API change, no claim that LibFuturo support exists
- The final ABI bind is the last plan: bind the freeze record at major 1 (or keep 0.x with a recorded reason) only if nothing found in the host work needs amending; each defect-driven amendment is a numbered plan with evidence, a re-run of the installed-consumer suite and an amendment-log row in `docs/abi/FREEZE-CANDIDATE.md` (the check requires ABI minor equals 1 plus the amendment rows); the "provisional until the Phase 11 freeze candidate" header comments are text that the final bind must settle (FN-03) and it moves the bound header hash by design — **Reversibility:** one-way — a major-1 ABI bind is a published contract
- No host or adapter requirement may change the core API unless it is a defect of the postconditions; any API defect found by the host goes through the amendment rule, never an edit of the bound header outside it
- Plans are capped at 4 tasks and about 45 minutes; no decimal plans (a defect fix is a new numbered plan); independent review per wave in scratch clones plus one final audit; parallelization false; `git.create_tag` false; new CTest names stay cheap with COST (gate-macos warm spread is ~105 s; cold measured 729 s of 900 s); the SDL lane, sanitizer and fresh-archive lanes stay out of the hosted gate

### Decided autonomously
- [Host layout] Q: "Where do the pump and the SDL shell live?" → Selected: "`host/` top-level directory with `host/pump` (pure C, consumes only the installed `tendies64/` headers, guarded by the consumer include guard) and `host/sdl` (optional target, OFF by default); not under tools/ (the root glob adds every tools/*/CMakeLists.txt)" — layout is cheap to move
- [Title builder] Q: "New builder or extend an existing one?" → Selected: "copy `scripts/build-pad-fixture.sh` into a new builder, new manifest rows, a spec-derived oracle script for expected checkpoint digests (the `poll_oracle.py` / `mix_oracle.py` pattern), a wrong-input negative control and a refused-packet control (a blank screen must be distinguishable)" — copy-paste is the project default
- [Checkpoint digests] Q: "How are long runs checked?" → Selected: "checkpoint digests, not long raw captures (raw-capture limit about 800 frames)" — test design
- [Input mapping] Q: "Stick and button mapping policy?" → Selected: "stick scaled to -128..127 with a documented deadzone, D-pad and buttons to the standard controller mask, port 0 only, hotplug unsupported" — mapping table is test-pinned and replaceable
- [SDL lane] Q: "How is the SDL shell exercised?" → Selected: "manual/nightly lane with SDL_VIDEO_DRIVER and SDL_AUDIO_DRIVER set to dummy and a virtual joystick; verify driver hint strings and headless gamepad init at plan time on both runners; skip with a recorded reason if SDL3 is not installable; the required gate never builds SDL" — lane placement is reversible
- [Save persistence] Q: "Host-side save files?" → Selected: "omit unless one CLI-provided path with atomic write fits one task; the headless run already proves save bytes" — scope cap
- [Throwaway measurement] Q: "Real-time feasibility?" → Selected: "one throwaway frames-per-second measurement for the plan, no claim made from it" — information only
- [LibFuturo re-pin] Q: "Which LibFuturo commit?" → Selected: "re-read the sibling project's committed objects at plan time, record the SHA and whether it is published; if no public URL exists the pin is a local SHA stated as such" — documentation
- [Review] Q: "Review cadence?" → Selected: "per wave in scratch clones plus the final audit before the bind, as in Phases 10 and 11" — process choice

### Claude's Discretion
- Exact host-pump function names, the title's sprite colors and tone frequency, plan decomposition (estimate 8-12 plans), and whether the SDL shell gets its own CI job or only a documented manual command

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `tests/installed-consumer/` (consumer project, `t64c_*.h` helpers, hostloop.c is the host-shaped loop to build on), `scripts/check-install.sh`, the installed package `tendies64`
- Builders and fixtures: `scripts/build-pad-fixture.sh`, `fixtures/{rdp,pad,audio,save,replay}/*.S`, the `t64.input` replay v1 format, `runner/t64_real.c` (public-API client and reference), `fixtures/composite/` (RUN-07 composite and its oracle)
- Freeze machinery: `docs/abi/FREEZE-CANDIDATE.md`, `scripts/check-abi.py freeze`, `scripts/qualify-phase11.py`, `docs/spec/api.md`

### Established Patterns
- Authored guests only; independent oracle literals; manifest rows with rights state; new semantics get new registered identities; evidence tiers per claim; one qualifier per phase as a thin delta of the previous

### Integration Points
- The host consumes only `include/tendies64/` (the consumer include guard rejects anything else); root CMake globbing of `tools/*/CMakeLists.txt`; hosted CI jobs (gate-macos 15 min limit, cold 729 s)

</code_context>

<specifics>
## Specific Ideas

Full findings, SDL and libdragon facts, LibFuturo comparison table and the adversarial scope bounds are in `.planning/research/v1.2-fanout/lens-host.md`; the researcher and planner must read it and re-verify mutable facts (SDL3 availability and version, dummy driver hint semantics, headless gamepad init on both runners, the LibFuturo SHA). Phase 11 left these Phase 12 text items: FN-02 (api.md advance row INTERNAL_DEFECT source), FN-03 (header "provisional" comments, settled by the bind), S-05 (explicit reserved members for padding holes as an amendment question), the openipl3 admission narrowing (documented in 11-29).

</specifics>

<deferred>
## Deferred Ideas

- libdragon route B, a GPU renderer, hotplug UX, remapping UI, config files, rumble/controller pak, ports 1-3, fullscreen/scaling filters, rewind, save-state UI, host-side save persistence beyond one atomic path
- A LibFuturo adapter (optional, must not shape the core API); VID-04 oracle evidence (D5); any release, tag or packaging (D6); real microcode (v1.3)

</deferred>
