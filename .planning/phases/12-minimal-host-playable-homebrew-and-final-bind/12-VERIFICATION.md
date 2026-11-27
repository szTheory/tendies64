---
phase: 12-minimal-host-playable-homebrew-and-final-bind
verified: 2026-11-27T04:32:00Z
status: passed
score: 8/8 must-haves verified
covered_files:
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-01-PLAN.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-01-SUMMARY.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-02-PLAN.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-02-SUMMARY.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-03-PLAN.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-03-SUMMARY.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-04-PLAN.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-04-SUMMARY.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-05-PLAN.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-05-SUMMARY.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-06-PLAN.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-06-SUMMARY.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-07-PLAN.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-07-SUMMARY.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-08-PLAN.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-08-SUMMARY.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-09-PLAN.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-09-SUMMARY.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-10-PLAN.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-10-SUMMARY.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-11-PLAN.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-11-SUMMARY.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-12-PLAN.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-12-SUMMARY.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-13-PLAN.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-13-SUMMARY.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-14-PLAN.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-14-SUMMARY.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-15-PLAN.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-15-SUMMARY.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-16-PLAN.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-16-SUMMARY.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-17-PLAN.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-17-SUMMARY.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-18-PLAN.md
  - .planning/phases/12-minimal-host-playable-homebrew-and-final-bind/12-18-SUMMARY.md
  - core/CMakeLists.txt
  - core/include/tendies64/t64_machine.h
  - core/t64_internal.h
  - core/t64_machine.c
  - core/t64_state.c
  - core/tests/test_api_struct_size.c
  - core/tests/test_api_surface.c
  - docs/abi/FREEZE-CANDIDATE.md
  - docs/abi/header.sha256
  - docs/abi/layout.c
  - docs/ci-reviews/LOG.md
  - docs/claims/phase12.md
  - docs/evidence/phase12-host-manual.md
  - docs/evidence/phase12-libfuturo-fit.md
  - docs/evidence/phase12-qualification.md
  - docs/evidence/sdl3-host-dependency-audit.md
  - docs/spec/api.md
  - docs/spec/machine-contract.md
  - docs/spec/raw-capture.md
  - docs/spec/replay.md
  - docs/spec/result-protocol.md
  - docs/spec/save-media.md
  - docs/spec/state-container.md
  - fixtures/manifest.json
  - fixtures/title/title-spec.md
  - fixtures/title/title.S
  - fixtures/title/title.expected.json
  - fixtures/title/title.ld
  - fixtures/title/title.long.input.json
  - fixtures/title/title.runner-pins.json
  - fixtures/title/title.shifted.input.json
  - fixtures/title/title.short-wrong.input.json
  - fixtures/title/title.short.input.json
  - fixtures/title/title_oracle.py
  - host/CMakeLists.txt
  - host/pump/t64h_frames.c
  - host/pump/t64h_frames.h
  - host/pump/t64h_input.c
  - host/pump/t64h_input.h
  - host/pump/t64h_pump.c
  - host/pump/t64h_pump.h
  - host/pump/t64h_script.c
  - host/pump/t64h_script.h
  - host/sdl/t64h_sdl.c
  - host/sdl/t64h_sdl.h
  - host/sdl/t64h_sdl_main.c
  - host/sdl/t64h_sdl_smoke.c
  - host/tests/t64h_sha256.c
  - host/tests/t64h_sha256.h
  - host/tests/t64h_tests.c
  - runner/CMakeLists.txt
  - runner/t64_real_fixture.c
  - runner/tests/test_title_two_run.c
  - scripts/build-title-fixture.sh
  - scripts/check-abi.py
  - scripts/check-api-init.py
  - scripts/check-api07-evaluation.sh
  - scripts/check-consumer-literals.py
  - scripts/check-host-includes.sh
  - scripts/check-host-isolation.sh
  - scripts/check-host-policy.sh
  - scripts/check-host-title.py
  - scripts/check-host.sh
  - scripts/check-install.sh
  - scripts/check-stale-freeze-text.sh
  - scripts/check-test-inventory.sh
  - scripts/p12-mutants.py
  - scripts/preflight-phase12-regexes.txt
  - scripts/preflight-phase12.sh
  - scripts/qualify-phase12.py
  - scripts/tests/test_phase12_qualification.py
  - tests/installed-consumer/CMakeLists.txt
  - tests/installed-consumer/cases.txt
  - tests/installed-consumer/controls.tsv
  - tests/installed-consumer/main.c
  - tests/installed-consumer/ops.c
  - tests/installed-consumer/t64c_cases.h
  - tests/installed-consumer/t64c_title.h
  - tests/installed-consumer/title.c
  - tools/abi/CMakeLists.txt
  - tools/hostcheck/CMakeLists.txt
covered_digest: "v4:sha256:4f766634ca56024791516419d695777d29ae02f52b56209ee2bb2c12fa9e11a1"
behavior_unverified: 0
overrides_applied: 0
human_verification:
  - test: "Owner checklist docs/evidence/phase12-host-manual.md (physical controller play-through): run `sh scripts/check-host.sh sdl` and `build/host-sdl/t64h_sdl --rom build/fixtures/phase12-title-s32k.z64` with a connected controller and fill the run record and the seven pass-or-fail lines."
    expected: "D-pad and left stick move the square, A plays one tone, B resets the square, Start saves and quits, the window title reads exactly 't64 minimal host (development)'. A relaunch starts with an empty save slot (the shell keeps no host-side save file, DR-19); the restored position is proven headlessly by installed.consumer.title-persist, not by this list."
    why_human: "Owner-held hardware (CONTEXT, ROADMAP D-gates). The file is a template marked 'not run'; no agent may fill, attest or mark it. Per .planning/AUTONOMY.md prompt dispositions this is a non-blocking owner-attested item and never a must-have; no claim of Phase 12 rests on it."
  - test: "Confirm the verifier's NON-AUTHORITATIVE verdict 'holds' on 26 judgment-tier prohibitions that the plans left status: unverified (flag: unverified-prohibition, human review recommended). Per plan: 12-01 x1, 12-02 x1, 12-03 x1, 12-04 x1, 12-05 x1, 12-07 x2, 12-08 x2, 12-09 x3, 12-10 x2, 12-11 x2, 12-13 x1, 12-14 x3, 12-15 x2, 12-16 x1, 12-17 x1, 12-18 x2 (the statements and the evidence are in the Prohibitions section)."
    expected: "Each holds: no oracle material and no other emulator output, no tag/release/package channel/redistribution, no protected ROM, no workstation path or host name in a committed file, no oracle-bound file edited, no SDL token outside host/sdl, no SDL package installed by an agent, no workflow/gate/root CMake edit, no sibling-checkout path or working-tree read for LibFuturo, no bound header, golden or version move except in 12-14, no closed qualifier touched, the owner checklist never filled."
    why_human: "Judgment-tier prohibitions have no authoritative automated verdict (ADR-550 D4, autonomous mode); they are recorded as flagged and never passed silently. This is a confirmation item only: no violation was found and nothing blocks the next phase."
---

# Phase 12: Minimal Host, Playable Homebrew and Final Bind Verification Report

**Phase Goal:** An authored, rights-cleared title runs headlessly and in a minimal host built only on the installed API, the core stays free of the host, and the ABI is bound for good once nothing found in the host work needs amending.
**Verified:** 2026-11-27T04:32:00Z
**Status:** human_needed (zero gaps; the only open items are the not-run owner checklist and a confirmation of 26 flagged judgment-tier prohibitions, both non-blocking under `.planning/AUTONOMY.md` prompt dispositions)
**Re-verification:** No, initial verification
**Verified head:** `411e9cf` (docs-only `[skip ci]` commits above the qualified commit `40b7d2d` and the pushed hosted head `f089171`; `git diff --name-only 40b7d2d HEAD -- . ':!.planning' ':!docs'` is empty, and the only `docs/` files that moved after `40b7d2d` are the three Phase 12 evidence documents and `docs/ci-reviews/LOG.md`. The orchestrator's cold-measurement commit `411e9cf` landed while this verification ran; it touches only `PIN-IMPACT.md`, `docs/ci-reviews/LOG.md` and `docs/evidence/phase12-qualification.md`, and I re-read its diff and recomputed the fingerprint after it)

Evidence tier: code observation and commands re-run by the verifier in this pass, plus the bound qualification record that the verifier re-verified against the tree. The result is an authored-scope software-interface claim: ABI 1.0 and package 1.0.0 are an ABI identity, not a release version. It is not a compatibility, accuracy, real-time, hardware or oracle claim, and no tag, release, binary redistribution or package channel exists.

## Goal Achievement

### Must-have truths

The must-haves are the four ROADMAP Phase 12 success criteria (the contract) plus four cross-cutting truths from 12-CONTEXT.md, AUTONOMY.md and AGENTS.md. API-05, API-06 and API-07 are verified through them (Requirements coverage below).

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| SC1 | An authored title (16x16 square moved by the D-pad or stick, one fixed AI tone on A, one save slot, frame-locked on the VI, no Z, textures, RSP microcode or TLB) has manifest rows and produces expected checkpoint digests headlessly through the runner and through the installed consumer | VERIFIED | See SC1 table |
| SC2 | The host pump is unit-tested in the required gate; scripted input gives byte-identical video, audio and save results to the headless run at two fake-clock speeds; a virtual joystick under SDL dummy drivers exercises button and axis mapping | VERIFIED | See SC2 table |
| SC3 | The SDL3 shell is optional, off by default and in a non-gating lane; the core builds and links with no SDL dependency and a build without the host target succeeds | VERIFIED | See SC3 table |
| SC4 | A written LibFuturo fit evaluation pinned to a commit SHA exists with a "no adapter now" conclusion, and the final ABI bind is made (each defect-driven amendment would be recorded with evidence and a re-run consumer suite) | VERIFIED | See SC4 table |
| X1 | The host is built only on the installed API (find_package, the one public header, C99 headers; no source-tree path), and no host or adapter requirement changed the core API | VERIFIED | See Cross-cutting table |
| X2 | Decision gates and rights hold: D4 (SDL3 link-only, never vendored), D5 (no oracle material), D6 (no tag, release, redistribution; 1.0 is an ABI identity), D7 (neutral window title, no icon/bundle/product name); no protected ROM; the owner checklist is never a must-have | VERIFIED | See Cross-cutting table |
| X3 | The gate and hosted CI are green on the qualified head and stay inside the recorded budget; the final qualification record verifies against the tree | VERIFIED | See Cross-cutting table (the orchestrator's cold measurement of the final head, 606 s of 900 s, is recorded) |
| X4 | Carry-ins are triaged, the "provisional" header text is settled, no overclaim appears in claims or evidence, and API-05, API-06 and API-07 are claimed by plans | VERIFIED | See Cross-cutting table |

**Score:** 8/8 truths verified, 0 present-but-behavior-unverified, 0 overrides. Every behavior-dependent truth (drain order, backpressure stall and resume, held-packet semantics, backward-clock refusal, two-speed equality, input mapping, bound-record failure on a changed ABI) has a behavioral test that I ran and that passed, and I killed three pump mutants and two bound-record mutants of my own (below).

### SC1: authored title, manifest rows, checkpoint digests through the runner and the installed consumer

| Check | Evidence (read or run by me) | Result |
|-------|------------------------------|--------|
| The title is authored, small and inside the scope cap | `fixtures/title/title.S` is 557 lines (522 non-empty per 12-REVIEW). The spec (`fixtures/title/title-spec.md`, TS-1 to TS-16) fixes one 128x96 screen, a 16x16 square (RDP fill list TS-9: `FillRectangle (x,y)..(x+15,y+15)`), movement by D-pad or by stick at an absolute axis of at least 16 (TS-7), one fixed square-wave tone burst of two 2,048-byte descriptors on a press edge of A (TS-8), B reset, and a 16-byte SRAM slot (TS-12). A grep of `title.S` for tlb, SP register bases, texture/tile and Z words finds only the comment that denies them (lines 16-17) | PASS |
| Frame-locked, never an instruction-count delay | `title.S:314-324` ("TS-5 frame lock ... wait for the VI pending bit with Status.IE clear", `wait_vi`: `andi $t1,$t1,T64_MI_VI_PENDING; beq ... wait_vi`, then a write to VI_CURRENT clears it). The header comment (lines 14-18) states no handler exists and the guest never counts instructions to wait. This is the VI interrupt condition read by polling, as the CONTEXT decision allows ("VI interrupt or VI_CURRENT polling"); there is no interrupt handler (info I-1 below) | PASS |
| Manifest rows with the rights state | `fixtures/manifest.json:1205-1262`: `phase12-title-s32k` and `phase12-title-refused-s32k`, `source_rights: authored-here`, `binary_rights: cleared`, `distribution: ["build-only"]`, profile `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k`, toolchain GNU Binutils 2.45, source revision `7b962961...` (I confirmed it is a commit: `git cat-file -t` prints `commit`). `git ls-files` holds no ROM-like file (`.z64 .n64 .v64 .rom .bin .sav .eep .srm .fla .mpk .iso .ndd .d64`: none) | PASS |
| The expected values are independent of the guest | `fixtures/title/title_oracle.py` imports only the Python standard library (argparse, ast, hashlib, io, json, math, os, re, sys, fractions). Commit order: spec and oracle tracer `8090e92`, full model `5eca388`, expectations `a6bb66d` (2026-11-26 11:29 to 11:36), then the guest `8d6d330`, `6db2224`, `7b96296` (11:51 to 11:55): oracle before guest. `harness.title-oracle` passes (selftest plus a byte comparison of `title.expected.json` with a regenerated copy) | PASS |
| Through the runner | `ctest -R runner.title-two-run -V`: the supervised raw-capture run compares three 49,152-byte video checkpoints (`8227044b...`, `548e2016...`, `8227044b...`), two audio packets of 2,048 bytes in sequence order and the 20-byte transcript (`e2af326a...`) with the oracle values: all "equal"; each comparison is followed by a control that changes one expected value and is detected (checkpoint 2 digest, audio packet 2, transcript byte 10). The four runner pins carry the label "runner-recorded pin, not independent". The runner's limits are executable facts of the same case (DR-34): no run without raw capture, no save-out producer together with raw capture, so the save image is compared through the consumer | PASS |
| Wrong-input and refused-frame negative controls | `controls.real.title-wrong-input` (the short-wrong script gives checkpoint 2 `e063ea0a...`, mismatching the short expectation, and equals its own expectation); `controls.real.title-refused-frame` (the refused ROM never completes, ends `capture-error/host-error` at tick 4,000,000, no transcript, no bundle, with six same-setup controls each refused for its own reason) | PASS |
| Through the installed consumer | `ctest -R installed.consumer.title -V`: `title: checks=99 failures=0` (short, long, shifted prefix), `title-persist` and `title-refused: checks=25 failures=0`. The long scenario (12 frames, 4 audio packets, 20-byte transcript, 32,768-byte SRAM save image through `t64_machine_save_export`) equals the independent literals of `t64c_title.h` (generated from `title.expected.json`); the pins are labelled "consumer-recorded pin, not independent". The consumer's own SHA-256 is used. The refused ROM gives only blank 49,152-byte packets, no audio, no transcript, no completion and the blank save image. `tests/installed-consumer/title.c` includes only standard headers, `<tendies64/t64_machine.h>` and `t64c_*` headers and opens no file | PASS |
| Input coverage of the scripts | `title.long.input.json` holds D-pad (256, 1280), a stick above the threshold (x 40; x -100, y -50), a stick inside it (15, -15), A (32768), B (16384) and Start (4096) | PASS |

### SC2: host pump unit-tested in the gate, two-speed equality, virtual joystick

| Check | Evidence | Result |
|-------|----------|--------|
| The pump is pure C on the installed header with an injected clock | `host/pump/t64h_pump.[ch]`: the only includes in `host/pump` are `<stddef.h>`, `<stdint.h>`, `<string.h>`, `<tendies64/t64_machine.h>` and local headers. Time enters through the `t64h_clock_fn` callback only (`pump.c:281`); the bound of a step is `elapsed_ns / 16` carried from the clock origin (`:290-297`); a clock reading behind the previous one is refused (`:282-286`); the drain order is video, audio, text (`:216-231`); a sink-refused packet is held and offered first next time (`:76-85`); a stall is two backpressure stops with nothing delivered (`:353-368`) | PASS |
| Eight `host.pump.*` names run in the required gate | `ctest -N` lists `host.pump.build`, `-clock`, `-drain-order`, `-stall`, `-title-two-speeds`, `-input-map`, `-frames`, `-script`; I ran them: all pass. They are part of the 354-name gate that passed on both hosted jobs (run 45937541960, `100% tests passed out of 354` on gate-macos) | PASS |
| Two speeds give byte-identical results | `ctest -R host.pump.title-two-speeds -V`: for `title.short.input.json` and `title.long.input.json` the 240 Hz and 3 Hz fake-clock runs are "equal; 3 frames / 12 frames, 2 / 4 audio packets (stream), 20-byte transcript, save image, completion tick 3693938 / 13078313 and final digest equal the oracle and the runner / consumer pins". The two speeds really differ in how the pump worked: short 15 steps and 0 mid-step drain stalls at 240 Hz against 1 step and 1 stall at 3 Hz; long 51 steps and 0 stalls against 1 step and 5 stalls. The comparator `scripts/check-host-title.py` runs its own seeded selftest first | PASS |
| The tests can fail (my own mutants, scratch copy of `host/` outside the repository, built against the moved install prefix) | M1: `drain_audio` before `drain_video` in `drain_pass` makes `t64h_tests --case drain-order` fail (`order WRONG`, `failures=1`, exit 1). M2: the D-pad Up and Down rows of the mapping table swapped makes `--case input-map` fail (`pad.buttons is 1024, expected 2048`, `failures=2`). M3: the backward-clock refusal disabled makes `--case clock` fail (`backward_clocks is 0, expected 1`, `failures=4`, exit 1). The unmutated baseline passes each case. In addition the qualification record's `mutants` stage ran the plan's 14-row table: 12 KILLED (8 pump, 4 title guest mutants through the two-speed path), the equivalent control SURVIVED and the non-compiling control INVALID | PASS |
| Input mapping | `host/pump/t64h_input.c`: one mapping table (A, B, left trigger as Z, Start, D-pad), the stick scaled by a divisor to -128..127 with a dead zone, the vertical axis complemented; `host.pump.input-map checks=105 failures=0`. The SDL lane's `mapping rows 30/30` drives the same table through a virtual gamepad | PASS |
| Virtual joystick under SDL dummy drivers | I ran `sh scripts/check-host.sh sdl` (2.0 s): `[host-sdl] sdl 3.4.10`, `video-driver dummy`, `audio-driver dummy`, `mapping rows 30/30`, `title ok`, `blit ok`, `frames 8`, `audio ok`, `[host-sdl] PASS`. `host/sdl/t64h_sdl_smoke.c` creates the pad with `SDL_AttachVirtualJoystick` (15 buttons) and checks the window title. This is host glue only: no rendering, timing or audio claim | PASS |
| Strength of the equality proof | Equal results at two speeds could be a tautology if the pump ignored time; the step and stall counts above show it did not, and mutant P12-P02 (the input hook called only at the end of a large step) is killed by exactly this case | PASS |

### SC3: SDL optional, off by default, non-gating; core links without SDL; build without the host target

| Check | Evidence | Result |
|-------|----------|--------|
| Off by default and optional | `host/CMakeLists.txt:23` `option(T64H_SDL "Build the optional SDL3 shell" OFF)`; `find_package(SDL3 3.2.0 CONFIG REQUIRED)` only inside `if(T64H_SDL)` (`:39-44`); the host is a separate top-level project that finds the library only through `find_package(tendies64 1.0 CONFIG REQUIRED)` | PASS |
| Non-gating lane | The lane is the local script `scripts/check-host.sh sdl`. `grep -rn -i 'sdl\|check-host' .github scripts/gate.sh CMakeLists.txt` prints nothing; `ctest -N \| grep -c -i sdl` is 0; `git diff --name-only 60b9e3c HEAD -- .github scripts/gate.sh CMakeLists.txt scripts/check-ci-yml.sh` is empty | PASS |
| The core has no SDL dependency | `sh scripts/check-host-isolation.sh --build build/gate`: `files=496 archives=2 findings=0` (no SDL token in `core`, `runner`, `tools`, `tests` or the root CMake file; no `SDL_` symbol in the build tree archive or the installed archive; none in `docs/abi/installed-undefined.txt`). My own `nm -P -g build/gate/core/libt64_core.a \| grep -c SDL_` prints 0. `grep -rn -i sdl core/*.c core/*.h core/include` finds none | PASS |
| A build without the host target succeeds | The root `CMakeLists.txt` holds no `host` path (`ISO-ROOT` rule) and the gate build (354 names, passing on both hosted jobs and in the qualification record's gate stage) contains no host target; the `host.pump.build` fixture configures `host/` with `T64H_SDL` OFF; the record's `fresh-archive` stage builds `t64_core` alone from `git archive` | PASS |
| Policy: no vendoring, neutral title, no renderer | `sh scripts/check-host-policy.sh`: `files=16 findings=0` (no FetchContent, ExternalProject, vendor directory, SDL source, install rule, bundle/icon/resource, renderer, GPU path or keyboard event; the window is created with exactly `T64H_SDL_TITLE`). `host/sdl/t64h_sdl.h:29` `#define T64H_SDL_TITLE "t64 minimal host (development)"`; presentation by `SDL_BlitSurfaceScaled(... SDL_SCALEMODE_NEAREST)` (`t64h_sdl.c:111`). `docs/evidence/sdl3-host-dependency-audit.md` records the zlib license and link-only use | PASS |

### SC4: LibFuturo evaluation and the final ABI bind

| Check | Evidence | Result |
|-------|----------|--------|
| Written evaluation, pinned, honest about publication | `docs/evidence/phase12-libfuturo-fit.md`: `Pinned commit: 4177a43e67312e2db88ab21cba13aa7350063105`, stated "local and unpublished" (no remote, no public URL, so a reader cannot verify against it), read through committed objects only; a ten-axis table (3 compatible, 6 convertible with loss, 1 missing); five deltas that all sit on the other project's side; Conclusion "no adapter now" with its reasons; "No LibFuturo support exists, and no claim of it is made". `repo.api07-evaluation` passes (pin equals the PIN-IMPACT pin, the conclusion is present, no support claim, no mention in code); `git grep -i -c libfuturo -- core runner host tools tests` prints nothing | PASS |
| Host-found defect triage decided the amendment slot | PIN-IMPACT "Host-found API defects": 15 rows (C-1 to C-6, H-01 to H-09), each with evidence, "breaks a postcondition: no", 13 accept, 2 document (C-1 done in 12-07; H-03 done in 12-15: `docs/spec/api.md:412-415` now says a masked handler status consumes its event), 0 amend; "12-11 trigger: no". 12-11 closed with a one-line SUMMARY and a planning-only commit (`35a2736` touches `ROADMAP.md` and the SUMMARY). I read the rows for plausibility; none contradicts a header or api.md promise | PASS |
| The bind: ABI 1.0, package 1.0.0 | `core/include/tendies64/t64_machine.h:42-43` `T64_ABI_VERSION_MAJOR UINT32_C(1)`, `MINOR UINT32_C(0)`; `core/CMakeLists.txt:141-160` `VERSION 1.0.0 COMPATIBILITY SameMajorVersion` (a literal, with `project(tendies64 VERSION 0.0.0)` untouched in the root file); the installed `tendies64ConfigVersion.cmake` reads `PACKAGE_VERSION "1.0.0"` | PASS |
| The bind moved nothing but the two version constants and comments | `git diff 60b9e3c HEAD -- core/include/tendies64/t64_machine.h` with comment lines filtered: exactly the two `#define` changes (0.1 to 1.0). The core diffs outside the header (`t64_internal.h`, `t64_machine.c`, `t64_state.c`) read in full: every added or removed line is comment text ("provisional until the Phase 11 freeze candidate" relabelled to "format-versioned, outside the ABI promise" and similar). `docs/abi/public-symbols.txt` and `status-classes.txt` have the same SHA-256 as at the Phase 11 candidate (`c45ff33c...`, `570548fc...`). `grep -n -i provisional core/include/tendies64/t64_machine.h` prints nothing (FN-03 settled); the only remaining hit in `core/include` is the non-installed, non-bound `t64_facade.h` (info I-2) | PASS |
| The bound record equals the tree | I recomputed `shasum -a 256` of the header (`4524c0c3...`, equals the line in `docs/abi/header.sha256`), of `header.sha256` (`b4f5705c...`), `public-symbols.txt`, `status-classes.txt` and `layout.c` (`7897b5f8...`); all five equal the machine-read block of `docs/abi/FREEZE-CANDIDATE.md` (`abi_version: 1.0`, `package_version: 1.0.0`, `bound_minor: 0`, `candidate_amendments: 0`, `amendments: 0`, 35 functions, 39 statuses, 8 stop reasons, 14/165/190 layout, 8 padding holes). `python3 -B scripts/check-abi.py freeze --record docs/abi/FREEZE-CANDIDATE.md --prefix <moved prefix>` prints `abi=1.0 package=1.0.0 ... findings=0`. No bound file changed after the bind commit `03ccb01` (`git diff --name-only 03ccb01 HEAD -- core/include docs/abi/{header.sha256,layout.c,public-symbols.txt,status-classes.txt}` is empty; the record's block lines did not move) | PASS |
| An incompatible change fails the check (my own mutations in a scratch copy of the moved prefix) | Raising `T64_ABI_VERSION_MINOR` to 1 with no amendment row: `check-abi: freeze findings=4` plus the re-bind procedure text. Changing `T64_STATUS_STRUCT_TOO_SMALL` from 35 to 45: `findings=3`, exit 1. The restored copy: `findings=0`. `repo.abi-freeze-selftest` (37 cases) and `repo.abi-freeze-record` pass | PASS |
| Version controls and the installed-consumer suite after the bind | `installed.controls.version-mismatch-minor` (request 1.1), `-patch` (1.0.5) and `-major` (2.0) each fail with their own CMake headline ("compatible with requested version ..."); `doctored-header-abi` still answers status 34; `unit.core.api.abi-major-rule` passes. I ran the whole `install.`, `installed.`, `api04.`, `repo.abi*` and Phase 12 groups against the current build tree: `100% tests passed out of 97`. The fresh-archive and freeze stages of the final record pass | PASS |
| Owner decision recorded for the 1.0 number | `.planning/phases/12-.../12-CONTEXT.md:40` and `.planning/STATE.md:368`: owner answer "Bind ABI 1.0 / package 1.0.0" of 2026-11-26, an ABI identity constant, not a release version; `docs/abi/FREEZE-CANDIDATE.md` "Owner-review item (D6)" states it and lists tags, channel and release naming as unselected. I read these records; I cannot independently attest an owner statement | PASS (recorded) |

### Cross-cutting truths

| # | Check | Evidence | Result |
|---|-------|----------|--------|
| X1 | Host only on the installed API; the core API unchanged by the host work | `host.pump.build` configures `host/` with `CMAKE_PREFIX_PATH` set to the moved prefix alone and asserts no source-tree path or original install path leaks into its build files; `repo.host-include-guard` (rules HOST-INCLUDE, HOST-SDL-INCLUDE, HOST-SDL-IDENT, HOST-PUMP-TIME, HOST-CMAKE) and its selftest pass; `scripts/check-api-init.py` roots now include `host` and `repo.api-init-guard` passes; `check-consumer-includes.sh` `findings=0`. No core production source moved except the three comment-only files above; the digest sweep over the 21 shipped ids against the Wave 0 base recorded in the final record is `1599 shared lines, 0 differences` | PASS |
| X2 | Decision gates and rights | `git tag` lists only `phase09-f2`, `phase09-fh`, `phase09-fr`; `gh release list` is empty; no tag at HEAD. `python3 -B scripts/check-rdp-oracle-authority.py --expect-blocked` exit 0 ("blocked: no oracle operation is authorized"); no oracle-bound file changed since the Wave 0 base. No ROM-like tracked file. `sh scripts/pii-scan.sh --tree` `findings=0` (1528 files), `sh scripts/rights-scan.sh` `findings=0` (tree 1528, history 5607). SDL3 is link-only: no SDL source, no `FetchContent`, no vendor directory (policy guard `findings=0`). The window title is the D7 string; no icon, bundle or product name | PASS |
| X3 | Gate and hosted CI | Re-checked live with `gh`: run 45937541960 (push, head `f089171`) success, gate-macos 04:02:01 to 04:11:20 (559 s, `100% tests passed out of 354`, `[gate] PASS`), gate-linux 04:02:01 to 04:06:47 (286 s). `python3 -B scripts/qualify-phase12.py --verify build/gate/12-qualification-private/20261127T033336673294Z-40b7d2d9/record.json` prints `record verified against the current tree` and `sdl-lane status: pass; sc2_virtual_joystick: closed` (exit 0): `rehearsal: false`, `status: complete`, 15 stages inside their limits (gate 307 s of 600, instrumented sanitizer 682 s of 1200, sweep 0 differences over 1599 lines, freeze 0 findings, mutants 12 killed), commit `40b7d2d`, bases Wave 0 `60b9e3c`, Host `0836e40`, Bind `49d41ac`, Qualification `656dc00`. Claims in the record: `final_abi` true; `hardware_accuracy`, `oracle_comparison`, `real_time`, `release`, `libfuturo_adapter` false. The warm margin is 341 s against 900 s. The cold run of the final head (orchestrator-only, F-142) is now measured: I re-checked run 45939090179 (`workflow_dispatch`, head `ba99773`, code identical to the qualified head) with `gh`: the macOS job log holds "Cache not found for input keys: binutils-macOS-ARM64-...", `100% tests passed out of 354`, `[gate] PASS`, gate-macos 04:17:40 to 04:27:46 (606 s, margin 294 s) and gate-linux 04:17:39 to 04:22:45 (306 s). This replaces the earlier 749 s projection (a measured cold run of the HOST head was 547 s, F-139). The G4 reversal rule (cold above 900 s) is not triggered | PASS |
| X4 | Carry-ins, text, claims, requirements | `sh scripts/check-stale-freeze-text.sh` `files=345 findings=0`; the PIN-IMPACT carry-in table has 11 rows with one disposition each (FN-02, FN-03, FN-04 done; FN-01 and S-05 accepted; OIPL3 no action; T1, T2, T5, T6, DEF-W1-01 deferred with reasons). `sh scripts/check-claims.sh --files docs/claims/phase12.md` prints `claims: files=1 blocks=3 errors=0`; `sh scripts/check-claims.sh` `files=81 blocks=29 errors=0`. The three blocks are tier `unit-tested`, each with a `not_claimed` list covering compatibility, accuracy, hardware, real-time, throughput, latency, A/V sync, SDL rendering correctness and the physical play-through; recorded pins are labelled "not independent". `docs/evidence/phase12-qualification.md` lists nine "Recorded exceptions for owner review", none worded as an approval. Debt-marker scan over the 81 non-planning files changed since the Wave 0 base: no `TBD`, `FIXME`, `XXX`, and no `TODO`/`HACK`/`PLACEHOLDER`. No disabled or skipped Phase 12 test | PASS |

### Required artifacts (mechanical check)

`gsd-tools query verify.artifacts` and `verify.key-links` were run for all 18 plans. 17 plans pass fully on both. One artifact row reports a missing literal:

| Plan | Artifact | Missing literal | Reality |
|------|----------|-----------------|---------|
| 12-09 | `host/sdl/t64h_sdl.c` | `t64 minimal host (development)` | The literal lives in `host/sdl/t64h_sdl.h:29` as `T64H_SDL_TITLE`; `t64h_sdl.c:278` passes the macro to `SDL_CreateWindow`, and the smoke and `repo.host-policy` check the string. Plan-text drift only |

### Data-flow trace (Level 4)

Expected values flow from the committed spec through the stdlib oracle (`title_oracle.py`) into `title.expected.json`, then into generated consumer literals (`t64c_title.h`, `scripts/check-consumer-literals.py`) and into the pump comparator; none comes from the guest, the core or a runner run except the four labelled pins. Outputs under test come from the installed archive through the public header (consumer) or from the pump driving the same archive (host tests). No static fallback, hardcoded-empty prop or disconnected source was found.

### Behavioral spot-checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| Phase 12 names and their guards | `ctest --test-dir build/gate -R '^(host\.pump\.\|repo\.host-\|repo\.api07\|harness\.title-oracle\|harness\.phase12\|unit\.core\.api\.abi)'` | 100% passed out of 19 | PASS |
| Title through the runner, controls and consumer | `ctest -R '^(installed\.consumer\.title\|runner\.title\|controls\.real\.title)'` | 100% passed out of 8 | PASS |
| Whole installed and Phase 12 surface | `ctest -R '^(install\.\|installed\.\|api04\.\|repo\.abi\|repo\.host\|repo\.api07\|repo\.p12\|repo\.stale\|repo\.consumer\|repo\.api-init\|host\.pump\|harness\.title\|harness\.phase12\|runner\.title\|controls\.real\.title\|unit\.core\.api)' -j4` | 100% passed out of 97 (22.4 s) | PASS |
| SDL lane under dummy drivers | `sh scripts/check-host.sh sdl` | ends `[host-sdl] PASS`, SDL 3.4.10, mapping rows 30/30 | PASS |
| Pump mutants (own) | scratch copy of `host/`, build against the moved prefix, run the killing case | M1, M2, M3 each exit non-zero with their own FAIL line | PASS (mutants killed) |
| Bound record mutants (own) | scratch copy of the moved prefix, `check-abi.py freeze` | minor raised `findings=4`; status renumbered `findings=3` | PASS (mutants killed) |
| Record verify | `python3 -B scripts/qualify-phase12.py --verify .../record.json` | `record verified against the current tree`, exit 0 | PASS |
| Claims, oracle authority, PII, rights, stale text, isolation, policy, includes | `check-claims.sh --files docs/claims/phase12.md`; `check-rdp-oracle-authority.py --expect-blocked`; `pii-scan.sh --tree`; `rights-scan.sh`; `check-stale-freeze-text.sh`; `check-host-isolation.sh --build build/gate`; `check-host-policy.sh`; `check-host-includes.sh` | errors=0; exit 0; 0; 0; 0; 0; 0; 0 | PASS |

I did not run the full gate, the sanitizer lanes, TSan or the digest sweep (long native lanes). Their evidence is the bound qualification record, which verifies against the current tree, plus the hosted run of the same source commit. My independent check of the "digest sweep, no digest moved" claim is the full read of the non-comment core and header diff since the Wave 0 base (above): nothing executable changed.

### Probe execution

No probe scripts are declared by the plans and `scripts/*/tests/probe-*.sh` does not exist. The title's own probe against the oracle (plan 12-03) is the registered test set above. Step 7c: not applicable.

### Test quality audit

| Test file | Linked req | Active | Skipped | Circular | Assertion level | Verdict |
|-----------|-----------|--------|---------|----------|-----------------|---------|
| `host/tests/t64h_tests.c` (8 `host.pump.*` names) | API-05 | all | 0 | no (hand-written expectations, an independent SHA-256 `t64h_sha256.c`, fake clock; two-speed output judged by the oracle comparator) | value and behavioral; 12 pump and title mutants killed in the record, 3 more by me | sound |
| `tests/installed-consumer/title.c` | API-06 | all | 0 | no (literals generated from the stdlib oracle; pins labelled) | value (frame, audio, transcript and save digests) | sound |
| `runner/tests/test_title_two_run.c` | API-06 | all | 0 | no | value, with seeded controls | sound |
| `fixtures/title/title_oracle.py` | API-06 | selftest plus `--check` | 0 | no (stdlib only, written before the guest) | value | sound |
| `scripts/check-api07-evaluation.sh` | API-07 | 32-case selftest | 0 | no | rule-level | sound (guards form, not truth of the text) |

No disabled test: no `DISABLED` or `SKIP_RETURN_CODE` on any Phase 12 registration.

### Prohibitions (judgment tier, non-authoritative verdicts)

All 39 `must_haves.prohibitions` of the 18 plans are `verification: judgment`; there are no test-tier entries. 13 are `status: resolved`. **26 are `status: unverified`** (the dispatch brief expected 8; the count of the plan frontmatter is 26, and `12-18-SUMMARY.md` does not list them). They are flagged `unverified-prohibition: human review recommended`; each carries my NON-AUTHORITATIVE LLM-judge verdict. None is passed silently. All 26 come out as "holds". The 13 `resolved` entries were not re-judged one by one; the goals they protect (no push on a red gate, no tag, no production change in 12-01, the reviewer fixes nothing) are covered by the same scans and diffs.

| Plan | Prohibition | Verdict | Evidence |
|------|-------------|---------|----------|
| 12-01 | No oracle material, no tag, no package install, no workstation path, hostname or e-mail in any committed file | holds | Oracle checker `--expect-blocked` exit 0; `git tag` shows only the three Phase 9 tags; `pii-scan --tree` 0 findings; 12-01's non-planning files are `scripts/preflight-phase12*` only |
| 12-02 | No oracle material (D5) and no reference to any other emulator's output | holds | `title_oracle.py` is stdlib-only; no emulator name in `fixtures/title`, `host`, the claims or the evidence documents (my grep; the only hits were "shares" substrings); oracle authority blocked |
| 12-03 | No oracle-bound file is edited; `fixtures/rdp/fill.S` is read only | holds | `git diff --name-only 60b9e3c HEAD -- fixtures/rdp runner/tests/test_rdp_video.c scripts/check-rdp-oracle-authority.py docs/spec/phase09-cases.json docs/evidence/phase07-oracle-authority.json` is empty |
| 12-04 | No edit of `test_rdp_video.c` or any oracle-bound file; no change of `T64_REAL_GUEST_TICKS_LONG_CAP` or the raw-capture limits | holds | The only runner source change is two table rows in `runner/t64_real_fixture.c` (the title ROM rows that use the existing cap) plus a comment; a grep of the runner diff for the cap and capture-limit names finds only uses |
| 12-05 | The consumer includes nothing outside `<tendies64/t64_machine.h>`, the C standard headers and its own headers, and reads no file other than `--rom` | holds | `title.c` and `t64c_title.h` include only standard headers, the one public header and `t64c_*`; no `fopen`/`fread` in `title.c`; `check-consumer-includes.sh` `files=19 findings=0` |
| 12-07 | No edit of the root CMakeLists.txt, the installed header, any golden or any core or runner production source | holds | 12-07's non-planning commits touch `host/`, `scripts/check-host*`, `scripts/check-api-init.py`, `tools/hostcheck/CMakeLists.txt` and `docs/spec/api.md` only; root `CMakeLists.txt` has no diff since the Wave 0 base |
| 12-07 | The pump reads no clock, file, entropy or thread itself; time enters only through the clock callback | holds | I read `t64h_pump.c` in full (one `config.clock(...)` call site per read, no libc time, file or thread call); includes are standard plus the one header; `repo.host-include-guard` rule HOST-PUMP-TIME; my mutant M3 and `host.pump.clock` |
| 12-08 | No SDL include or type in `host/pump` or `host/tests`; no core, runner or header edit | holds | 12-08's files are `host/`, `scripts/` and `tools/hostcheck/CMakeLists.txt`; `repo.host-isolation` and `repo.host-include-guard` (HOST-SDL-INCLUDE, HOST-SDL-IDENT) pass |
| 12-08 | No throughput, latency or real-time statement in any output or record | holds | No such word in `host/pump`, `host/tests`, `scripts/check-host*`; the claims list them as `not_claimed`. The one measured statement (H-09, "ran about 12.5 million guest ticks per CPU second") belongs to 12-09 and is labelled information only (info I-3) |
| 12-09 | No package is installed by this plan; SDL3 is used only if already present | holds | `check-host.sh sdl` installs nothing (its header and body: a scratch `find_package` probe, then configure/build into `build/host-sdl`); SDL 3.4.10 was already present on this host; no SDL source in the tree; the dependency audit records "nothing was installed by the plan". Judgment: no contradicting evidence, I cannot observe a past `brew` invocation |
| 12-09 | No edit of `.github/workflows`, `scripts/check-ci-yml.sh`, `scripts/gate.sh` or the root CMakeLists.txt; no CTest name builds SDL | holds | The `git diff --name-only 60b9e3c HEAD` over those paths is empty; `ctest -N \| grep -c -i sdl` is 0 |
| 12-09 | No lane log with a local path, user name or hostname is committed | holds | The lane prints only sanitized `[host-sdl]` lines; raw logs stay under ignored `build/host-sdl`; `pii-scan --tree` 0 findings; a grep of the Phase 12 evidence, claims and PIN-IMPACT for `/Users/` or `/home/` finds nothing |
| 12-10 | No adapter code, no build target, no core API change and no LibFuturo working-tree read; committed objects only | holds | `git grep -i libfuturo -- core runner host tools tests` prints nothing; 12-10's commits touch the evidence document, the guard script, the inventory and `tools/hostcheck/CMakeLists.txt` (the guard registration); the document's Method section says committed objects only. The no-working-tree-read part rests on that statement |
| 12-10 | No filesystem path of the sibling checkout and no request to an external service carrying project identifiers | holds | The evidence document names no path of the sibling checkout and says the public repository "was not asked again" for that reason; no `/Users/` or `/home/` string in the Phase 12 documents |
| 12-11 | Not triggered: no file other than the SUMMARY changes and nothing is committed by the executor | holds | `12-11-SUMMARY.md` records `commits: 0` and equal head before and after; the one commit that names 12-11 (`35a2736`) changes `.planning/ROADMAP.md` and the SUMMARY only |
| 12-11 | Triggered: no edit of a bound header or golden outside the amendment steps; no ergonomics change that is not a postcondition defect | holds | Not applicable because the slot was not triggered; no bound element moved outside 12-14 |
| 12-13 | No edit of `core/include`, `docs/abi`, any CMake file, the consumer, `host/` or the version-mismatch controls in this plan | holds | The four 12-13 commits change `scripts/check-abi.py` and planning files only |
| 12-14 | No tag, release, upload or package channel; `project(VERSION)` is not touched; no release naming appears in the record | holds | Tags and releases as above; `project(tendies64 VERSION 0.0.0 ...)` unchanged; the record states "No tag or release was made with the bind" and names tags and release naming as unselected |
| 12-14 | No struct, function, status or stop-reason change in the header; only the two ABI constants and comment text move | holds | The non-comment header diff is exactly the two `#define` lines; the symbol and status goldens have the Phase 11 hashes |
| 12-14 | No sealed body and no oracle-bound file is edited; no `docs/spec`, production `.c` or guard-script edit here (12-15) | holds | 12-14's non-planning files are `core/CMakeLists.txt`, the header, `core/tests/test_api_surface.c`, the record, `header.sha256`, `layout.c`, `host/CMakeLists.txt`, `scripts/check-install.sh` (a pin-site message listed for 12-14 in PIN-IMPACT B-23, not a guard script), `tests/installed-consumer/CMakeLists.txt` and `controls.tsv` |
| 12-15 | No edit of the installed header, a golden, the record block, a version number, a control row or a CTest registration | holds | `git diff --name-only 03ccb01 HEAD -- core/include docs/abi/...` is empty; the record's block lines did not move since the bind; 12-15's files are comments, `docs/spec`, record prose and `check-stale-freeze-text.sh` |
| 12-15 | No sealed body and no oracle-bound file is edited; production comment edits stay outside every function body | holds | I read the full diff of `t64_internal.h`, `t64_machine.c` and `t64_state.c`: comment lines only; sweep 0 differences recorded |
| 12-16 | No edit of the installed header, a golden, the record or a version number; no ABI byte moves after the 12-14 bind commit | holds | The same empty diff after `03ccb01`; the 12-16 change in `core/CMakeLists.txt` adds test registrations and a comment, no `VERSION` change |
| 12-17 | No closed qualifier (Phase 7 to 11) or its hermetic test is edited, imported or re-verified | holds | Only `scripts/qualify-phase12.py` and `scripts/tests/test_phase12_qualification.py` match "qualif" in the diff since the Wave 0 base; `qualify-phase12.py` imports only the standard library |
| 12-18 | No tag, release, upload, package channel or binary redistribution | holds | Tags and releases as above; the push of `f089171` was a fast-forward of `main` with no tag; the repository holds no binary |
| 12-18 | The owner checklist is never filled, attested or marked passed by an agent | holds | `docs/evidence/phase12-host-manual.md` reads "human-needed, non-blocking, owner-attested; not run", every Run-record and Checks cell is empty, no date and no PASS mark |

### Requirements coverage

| Requirement | Source plans (frontmatter `requirements`) | Description | Status | Evidence |
|-------------|-------------------------------------------|-------------|--------|----------|
| API-05 | 12-01, 12-07, 12-08, 12-09, 12-11 to 12-18 | A minimal host separate from the core presents raw video and audio and maps a controller: a pure-C host pump unit-tested in the required gate, with an optional SDL3 shell on top (link-only, never vendored, off by default, non-gating lane) | SATISFIED | SC2 and SC3 tables; the shell's physical play-through is the owner checklist and is outside the requirement text's gate |
| API-06 | 12-01 to 12-06, 12-08, 12-11 to 12-18 | A rights-cleared playable title authored in this repository with the pinned Binutils and open IPL3 (route A) runs headlessly through the runner and the installed consumer and in the minimal host, inside qualified scope; libdragon route is research only | SATISFIED | SC1 and SC2 tables; route B absent (12-REVIEW deferred table; `git diff` since the base adds no libdragon line) |
| API-07 | 12-01, 12-10, 12-12 to 12-18 | A written LibFuturo fit evaluation pinned to a commit SHA; any adapter optional and not shaping the core API | SATISFIED | SC4 table; the paper read of Phase 11 and the final text of Phase 12 both pin the same SHA |

REQUIREMENTS.md maps exactly API-05, API-06 and API-07 to Phase 12 (traceability rows 76-78, now `[x]` and "Complete", updated by the 12-18 orchestration). None is orphaned and each is claimed by plans.

### Decision coverage

`gsd-tools query check.decision-coverage-verify` returned `reason: could-not-parse` (the CONTEXT decisions are bullets in a different format), so the gate evaluated nothing and is not a result. I cross-checked the CONTEXT "Locked", "Decided autonomously", "Settled" and "Research facts" entries by hand against the tree: route A and builder copy (SC1), title scope cap (SC1), pump and shell split (SC2, SC3), SDL link-only and neutral title (SC3, X2), "plays means correct" (no throughput claim in host code or claims), evidence tiers (claims and checklist), LibFuturo (SC4), final bind as the last ABI-moving plan (SC4; `03ccb01` is the last commit that moved a bound byte), no core API change by a host requirement (15-row ledger), plan cap and per-wave review (12-REVIEW TITLE, HOST, FINAL), host layout `host/` (X1), checkpoint digests (SC1), input mapping (SC2), SDL lane placement (SC3), host-side save omitted (`host/` opens files for reading only; DR-19 fit-test row), S-05 kept (record `padding_holes: 8`), ABI 1.0 owner confirmation (SC4), keyboard fallback off (policy rule HP-KEYBOARD), G4 exception recorded (PIN-IMPACT gate-growth ledger). No decision was found abandoned.

### Anti-patterns found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| (none) | | No unreferenced `TBD`, `FIXME` or `XXX`; no stub, empty implementation or disabled test in the Phase 12 files | none | none |

## Non-blocking observations (not gaps)

These are recorded so a later reader does not rediscover them. None changes a verdict.

1. **I-1 Frame lock by polling, not by an interrupt handler.** ROADMAP SC1 says "frame-locked on the VI interrupt"; the title polls the MI VI pending bit with `Status.IE` clear and clears it by a VI_CURRENT write. CONTEXT explicitly allows "the VI interrupt or VI_CURRENT polling" and DR-03 records the choice. No instruction-count loop exists.
2. **I-2 Stale facade comment.** `core/include/tendies64/t64_facade.h:8` still calls the public header "a 0.x freeze candidate bound by Phase 11" (12-REVIEW FR-01). The file is not installed and not in the bound record; `repo.stale-freeze-text` exempts it by exact name for the `provisional` rule. A text item for a plan that may edit it (v1.3). 12-REVIEW also counts "nine" `host.pump.*` names in its API-05 row where eight are registered (the claims document and the gate agree on eight): a miscount in prose only.
3. **I-3 Throughput disclosure.** PIN-IMPACT H-09 and the owner checklist say the unthrottled pump ran below real time on the planning host. This is labelled information only and no claim rests on it.
4. **I-4 Runner limits.** The runner cannot show a refused frame or a save image of the title (DR-34); the consumer holds those assertions. The runner covers the short scenario (3 frames) only, by its guest-tick cap.
5. **I-5 Pins of no independent source.** The completion tick, stop reason, final state digest and packet count of each scenario are recorded pins, labelled "runner-recorded pin, not independent" and "consumer-recorded pin, not independent" wherever they are printed. Frames, audio packets, transcript and save image are independent.
6. **I-6 No local Linux evidence for 12-16 and 12-17.** The Docker engine on the planning host did not answer, so the act Linux run and the preflight `linux-compile` step have no local Linux evidence (12-REVIEW FR-03). The hosted `gate-linux` job of runs 45932117914 and 45937541960 (354 of 354) is the Linux evidence.
7. **I-7 Gate growth.** The Phase 12 G4 gate-growth exception stays recorded for owner review: the cold gate-macos run of the final head took 606 s against the 900 s limit (F-142, measured by the orchestrator after the push, replacing the 749 s projection); the warm hosted pair is 559 s and 286 s. The reversal rule is not triggered.
8. **I-8 Recorded exceptions for owner review.** The nine exceptions in `docs/evidence/phase12-qualification.md` (G4 gate growth, the D6 item of the 1.0 numbers, the 12-11 outcome, carry-ins to v1.3, DR-34, plan-count and file-count exceptions, the Docker limitation, the accepted ABI limits S-05 and I-05) are the owner's to accept. None is worded as an approval and none is a must-have. G5 (18 plans, above the 10 to 15 guide) is recorded in ROADMAP.md.
9. **I-9 Deferred, not gaps.** VID-04 stays deferred (oracle request blocked); DEF-W1-01, T1, T2, T5 and T6 are deferred to v1.3 with reasons in PIN-IMPACT; libdragon route B, a GPU renderer, hotplug, remapping, rumble, ports 1-3, host-side save persistence and a LibFuturo adapter are absent by design.

## Human Verification Required

### 1. Owner checklist: physical-controller play-through

**Test:** Follow `docs/evidence/phase12-host-manual.md`: run `sh scripts/check-host.sh sdl`, then `build/host-sdl/t64h_sdl --rom build/fixtures/phase12-title-s32k.z64` with a connected controller, and fill the run record and the seven pass-or-fail lines.
**Expected:** D-pad and left stick move the square, A plays one tone, B resets the position, Start saves and quits, and the window title reads exactly "t64 minimal host (development)". The relaunch line records what the window shows (an empty save slot, by design).
**Why human:** Owner-held hardware and a visual/physical check. The checklist is a template marked "not run"; an agent never fills or attests it. Under `.planning/AUTONOMY.md` it is a non-blocking owner-attested item and never a must-have. No Phase 12 claim rests on it.

### 2. Confirm 26 flagged judgment-tier prohibitions

**Test:** Read the "Prohibitions" table above and confirm that none of the 26 `unverified` plan prohibitions was violated.
**Expected:** Each holds (no oracle material, no tag or release, no protected ROM, no workstation path, no bound byte moved outside 12-14, no SDL outside `host/sdl`, no workflow or gate edit, no LibFuturo working-tree read, no closed qualifier touched, the checklist never filled).
**Why human:** ADR-550 D4 treats a judgment-tier prohibition as non-authoritative until a human resolves it. In autonomous mode the run completes with the items flagged: "complete with 26 flagged prohibitions". There is no gap behind this item.

## Gaps Summary

No gaps. All four ROADMAP success criteria and the four cross-cutting truths are verified against the tree and by commands I ran myself, including three pump mutants (drain order, input mapping, backward clock) and two bound-record mutants (an unrecorded ABI minor and a renumbered status) that the existing tests and checks catch. The phase goal is achieved: an authored, rights-cleared title produces independent-oracle checkpoint digests through the supervised runner and the installed consumer and plays through a minimal host that sees only the installed package; the pump is unit-tested in the required gate with byte-identical results at two fake-clock speeds; the SDL3 shell is optional, off by default and outside every gate, and the core and its archives hold no SDL name; the LibFuturo evaluation is pinned (to a local, unpublished SHA, stated as such) with the conclusion "no adapter now"; and the ABI is bound at 1.0 (package 1.0.0, an identity and not a release) with no amendment, no tag and no executable byte moved.

The overall status is `human_needed` only because the verifier rules forbid reading the not-run owner checklist and the 26 `unverified` judgment-tier prohibitions into a `passed` verdict. Under `.planning/AUTONOMY.md` prompt dispositions (`human_needed`: record as a non-blocking owner-attested item, never stop the run, never a must-have) both are non-blocking, and the orchestrator may record that disposition as it did for Phase 11.

---

_Verified: 2026-11-27T04:32:00Z_
_Verifier: Claude (gsd-verifier)_

## Disposition (orchestrator, 2026-11-27)

The verifier returned `human_needed` for two non-blocking reasons: the owner checklist `docs/evidence/phase12-host-manual.md` (a blank template marked "not run", never fillable by an agent) and 26 plan prohibitions that are judgment-tier and were left `status: unverified` by their plans (12-01, 12-02, 12-03, 12-04, 12-05, 12-07 x2, 12-08 x2, 12-09 x3, 12-10 x2, 12-11 x2, 12-13, 12-14 x3, 12-15 x2, 12-16, 12-17, 12-18 x2). Each has a recorded "holds" verdict with evidence above and zero gaps stand behind it. Per `.planning/AUTONOMY.md` prompt dispositions (`human_needed`: record as a non-blocking owner-attested item, never stop the run, never a must-have) the status is set to `passed`. The owner checklist and the 26 prohibitions are **owner-attested, non-blocking items for review**; they cover no oracle material, no tag/release/package channel/upload, no package install, no protected inputs in evidence or public CI, no workstation paths, and the unchanged bound ABI files after the bind commit. The cold gate-macos measurement of the final head is 606 s of 900 s (F-142), and the nine recorded exceptions for owner review in `docs/evidence/phase12-qualification.md` are not approvals.
