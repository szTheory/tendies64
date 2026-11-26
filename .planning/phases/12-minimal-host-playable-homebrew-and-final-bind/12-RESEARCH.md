# Phase 12: Minimal Host, Playable Homebrew and Final Bind - Research

**Researched:** 2026-11-26
**Domain:** C-ABI host (pure-C pump + optional SDL3 shell) over the installed `tendies64` package; an authored MIPS assembly title; LibFuturo fit evaluation; final ABI bind
**Confidence:** MEDIUM-HIGH (title, pump equality and SDL facts were run this session on a throwaway prototype; bind and gate-budget numbers are estimates)

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

#### Locked by REQUIREMENTS.md, ROADMAP.md and the v1.2 fan-out (not re-asked)
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

#### Decided autonomously (CONTEXT)
- [Host layout] `host/` top-level directory with `host/pump` (pure C, consumes only the installed `tendies64/` headers, guarded by the consumer include guard) and `host/sdl` (optional target, OFF by default); not under tools/ (the root glob adds every tools/*/CMakeLists.txt) — layout is cheap to move
- [Title builder] copy `scripts/build-pad-fixture.sh` into a new builder, new manifest rows, a spec-derived oracle script for expected checkpoint digests (the `poll_oracle.py` / `mix_oracle.py` pattern), a wrong-input negative control and a refused-packet control (a blank screen must be distinguishable) — copy-paste is the project default
- [Checkpoint digests] checkpoint digests, not long raw captures (raw-capture limit about 800 frames) — test design
- [Input mapping] stick scaled to -128..127 with a documented deadzone, D-pad and buttons to the standard controller mask, port 0 only, hotplug unsupported — mapping table is test-pinned and replaceable
- [SDL lane] manual/nightly lane with SDL_VIDEO_DRIVER and SDL_AUDIO_DRIVER set to dummy and a virtual joystick; verify driver hint strings and headless gamepad init at plan time on both runners; skip with a recorded reason if SDL3 is not installable; the required gate never builds SDL — lane placement is reversible
- [Save persistence] omit host-side save files unless one CLI-provided path with atomic write fits one task; the headless run already proves save bytes — scope cap
- [Throwaway measurement] one throwaway frames-per-second measurement for the plan, no claim made from it — information only
- [LibFuturo re-pin] re-read the sibling project's committed objects at plan time, record the SHA and whether it is published; if no public URL exists the pin is a local SHA stated as such — documentation
- [Review] per wave in scratch clones plus the final audit before the bind, as in Phases 10 and 11 — process choice

### Claude's Discretion
- Exact host-pump function names, the title's sprite colors and tone frequency, plan decomposition (estimate 8-12 plans), and whether the SDL shell gets its own CI job or only a documented manual command

### Deferred Ideas (OUT OF SCOPE)
- libdragon route B, a GPU renderer, hotplug UX, remapping UI, config files, rumble/controller pak, ports 1-3, fullscreen/scaling filters, rewind, save-state UI, host-side save persistence beyond one atomic path
- A LibFuturo adapter (optional, must not shape the core API); VID-04 oracle evidence (D5); any release, tag or packaging (D6); real microcode (v1.3)
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| API-05 | Pure-C host pump (ring draining, injected clock, input mapping, scripted input) unit-tested in the required gate; optional SDL3 shell, link-only, off by default, non-gating lane | Pump rules and tick arithmetic (Pattern 4), verified two-speed equality (Pattern 4), SDL facts (Pattern 5), lane cost analysis, Validation map |
| API-06 | Rights-cleared authored title (route A) headless through runner and installed consumer and in the host, inside qualified scope | Title design verified by prototype (Pattern 2), the two runner limits that bound it (Pitfalls 1-2), registration checklist (Pattern 3) |
| API-07 | Written LibFuturo fit evaluation pinned to a commit SHA, no adapter | Refreshed comparison table and pin facts (Pattern 7) |

The final bind is API-03's tail (already marked complete by Phase 11 for the candidate); it is owned by Success Criterion 4 of this phase.
</phase_requirements>

## Project Constraints (from CLAUDE.md / AGENTS.md / AUTONOMY.md)

- Treat ROMs, IPL/PIF data, saves and protected inputs as rights-bearing; a hash proves identity, not rights; authored guests only; no commercial content; D5 (oracle) not involved [CITED: AGENTS.md].
- Evidence tiers per claim; no hardware-accuracy, compatibility or real-time claim; a screenshot proves nothing [CITED: AGENTS.md; CONTEXT].
- Core owns no files, clocks, entropy or threads; the host supplies them (API-04) [CITED: AGENTS.md, REQUIREMENTS API-04].
- Explicit owner stops: protected content, owner-held hardware (the physical controller), PII or private paths, D2/D4/D5/D6/D7/D8, release channel and versioning [CITED: .planning/AUTONOMY.md section 4 and dispositions].
- v1.2 execution rules: scripted preflight under ~90 s cited by every plan, review per wave in scratch clones, 4 tasks and ~45 min per plan, no decimal plans, `parallelization` false, local-first CI (`act` for Linux) [CITED: AUTONOMY.md "v1.2 execution rules"].
- `git.create_tag` is false; `commit_docs` true but this research file was written without a commit (orchestrator instruction: read-only repo) [VERIFIED: .planning/config.json].

## Summary

The phase is feasible inside qualified scope and every load-bearing mechanism was exercised this session with throwaway code that is not committed: a frame-locked 16x16-square title (VI pending-bit polling, SI poll, RDP fill list, AI burst) ran on the installed package; scripted input at four slice rates (240, 60, 10 and 3 Hz of fake clock) gave byte-identical video, audio and final machine digest; SDL3 dummy drivers plus a virtual gamepad initialise on macOS (3.4.10) and Linux (Ubuntu 26.04, 3.4.2). Three limits the CONTEXT did not know shape the title and its tests (Pitfalls 1 to 3): the supervisor caps each raw-capture file at 69,632 bytes, the runner caps a guest at 4,000,000 ticks, and `ubuntu-24.04` has no SDL3 apt package.

The title therefore needs a small screen (128x96, 49,152-byte packets) and a runner scenario of at most three standard-NTSC fields; longer scenarios (10 to 12 fields) run through the installed consumer and the pump, which have neither limit. Expected values come from an independent oracle script written before the guest. The pump is a standalone C99 project built against the installed prefix (so "installed API only" is proven by construction), registered through script-only CTest names, with the SDL shell and its lane entirely outside the gate. The final bind is a mechanical but wide re-pin (header text, ABI constants, package version and compatibility, goldens, check-abi.py, controls, qualifier); no API defect was found in this research, so the default plan is zero amendments with one reserved amendment slot.

**Primary recommendation:** Author the oracle first, then a 128x96 title (about 450 lines) on profile `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k`; copy `scripts/build-composite-fixture.sh` (not the pad builder); pin runner, consumer and pump to the same oracle JSON; build `host/` as an installed-package consumer with its own include guard; keep SDL as a local script lane (no workflow file); bind ABI 1.0 only after the host plans pass, with `check-abi.py` generalised in the bind plan itself.

## Where this research contradicts or sharpens CONTEXT (read first)

| # | CONTEXT says | Finding | Consequence |
|---|--------------|---------|-------------|
| 1 | "checkpoint digests, not long raw captures (raw-capture limit about 800 frames)" | Through the runner the binding limits are different: `RLIMIT_FSIZE` = 4096 + 65536 = 69,632 bytes per file when `--raw-capture-dir` is used, so a 320x240 RGBA packet (307,200 bytes) cannot be captured at all; and the runner ceiling is 4,000,000 guest ticks = 3 packets at standard NTSC (Pitfalls 1, 2) | Screen 128x96; runner scenario <= 3 fields; long scenarios via consumer and pump |
| 2 | Copy `scripts/build-pad-fixture.sh` | `scripts/build-composite-fixture.sh` is the newer descendant: it adds `check_manifest_identities` (ROM identity equals manifest row) and a defsym loop | Copy the composite builder |
| 3 | "frame-locked on the VI interrupt" | No authored fixture takes a CPU interrupt: a grep of `fixtures/*/*.S` for `eret`, `syscall` and `0x80000180` matched no file, and every idiom clears Status.IE and polls MI [VERIFIED: grep run this session; fixtures/composite/composite.S:148-152]. Polling the VI bit of MI_INTERRUPT with IE cleared works (prototype); an exception handler would be a new unqualified idiom | Frame-lock = poll MI pending bit 0x08 (or VI_CURRENT), never a handler |
| 4 | `host/pump` "guarded by the consumer include guard" | `scripts/check-consumer-includes.sh` hard-codes one directory (`CONSUMER_DIR=tests/installed-consumer`) | Add a sibling guard copy for `host/pump` and `host/tests`; extend `check-api-init.py` roots |
| 5 | Scripted source "reuses the t64.input v1 / replay format" | The runner parser (`runner/t64_input.c`) is outside the include guard; the format is flat and small | Pump tests carry a ~120-line strict parser of the same `t64.input` v1 subset |
| 6 | SDL lane on "both runners" | `ubuntu-24.04` (the only allowed Linux label) has no `libsdl3-dev` candidate; macOS hosted has Homebrew but no SDL | Hosted Linux SDL lane would need a source build; recommend local script lane, macOS/brew optional |
| 7 | "The final ABI bind is the last plan" | The qualification record must bind the post-bind tree (header hash and build id move) | Read "last plan" as the last ABI-moving plan; qualification and evidence follow it (Open Question 1) |
| 8 | G4 "Phases 11 and 12 follow G4 as written" | Phase 12 adds roughly 16 native seconds of required evidence (Validation Architecture) with nothing removable | Record a Phase 12 G4 exception with a reversal rule, as Phase 11 did |
| 9 | LibFuturo lens table (2026-11-23, header 158 lines) | The committed header at the same SHA is 347 lines and now has snapshots, real cores and cadence fields | Table refreshed (Pattern 7); conclusion unchanged |

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Deterministic guest execution, rings, save bytes | Core library (`libt64_core.a`) | none | Owns no clock, file or thread (API-04) |
| Clock injection, tick arithmetic, drain order, backpressure loop | Host pump (pure C) | none | Policy lives outside the core; unit-testable with a fake clock |
| Logical input mapping, scripted input | Host pump | SDL shell (physical sampling only) | Mapping table is test-pinned; SDL only reads devices |
| Presentation, audio device, window, gamepad | SDL3 shell (optional) | none | Never in the gate; link-only system SDL3 |
| Reference headless run, raw capture, save-out | Runner/supervisor | none | In-tree reference client; fixture table is hard-coded |
| API proof from an installed package | Installed consumer and `host/` project | none | Built only against the moved prefix |
| Title behavior, expected values | Authored guest ROM + independent oracle script | none | Oracle never reads the ROM, core or runner |
| ABI identity and bind | Header + four goldens + record + `check-abi.py` | qualifier | One-way contract |

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| GNU Binutils (pinned) | 2.45 | assemble and link the title | Pinned receipt `build/tools/qualified-host-tools.json`; builders verify tool hashes [VERIFIED: scripts/build-composite-fixture.sh prerequisites, 2.45 strings] |
| Open IPL3 asset (Unlicense) | sha256 `f522db2e...a068` | boot code embedded by the packer | Existing pinned asset [VERIFIED: scripts/build-pad-fixture.sh ASSET_SHA256] |
| `t64_rom_pack` | in-tree | pack payload with IPL3 | Existing tool, source pin checked by builders |
| `tendies64` package | ABI 0.1 now, package 0.1.0 | the only API the host may use | Installed header + archive [VERIFIED: docs/abi/FREEZE-CANDIDATE.md] |
| CMake / Ninja | >= 3.25 (project minimum); 4.4.3 local, 4.5.0 hosted macOS | standalone `host/` project | `cmake_minimum_required(VERSION 3.25)` [VERIFIED: CMakeLists.txt:1; runner-images README for 4.5.0] |
| Python 3 (stdlib only) | system | oracle script | Existing oracles are stdlib-only |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| SDL3 | `>= 3.2.0` required (hint and virtual-joystick APIs are "since 3.2.0"); 3.4.10 installed locally, Homebrew stable 3.4.20, Ubuntu 25.10 3.2.20, Ubuntu 26.04 3.4.2 | optional shell only | `T64H_SDL=ON`, never in the gate [VERIFIED: `brew info sdl3`; docker apt-cache policy; SDL_hints.h] |
| copy of `t64c_sha256.c/.h` | in-tree | digest in host tests | Copy-paste (project default), not a dependency |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Source-built SDL3 on ubuntu-24.04 | pinned tarball built in a lane | Reproducible but minutes of CI, and "link-only against a system install" is stretched; only if a hosted Linux lane is ever required |
| Hosted SDL workflow file | local `scripts/check-host.sh sdl` | Workflow needs a `check-ci-yml.sh` edit (Pitfall 9); local script costs nothing bound |
| 320x240 screen | 128x96 | 320x240 cannot go through raw capture (Pitfall 1) |

**Installation (local only):** `brew install sdl3` (macOS) or `apt install libsdl3-dev` (Ubuntu 25.10+). No npm/pip/cargo package is installed by this phase.

## Package Legitimacy Audit

| Package | Registry | Age | Downloads | Source Repo | Verdict | Disposition |
|---------|----------|-----|-----------|-------------|---------|-------------|
| `sdl3` | Homebrew core formula, License: Zlib [VERIFIED: `brew info sdl3`] | 3.2.0 is the first stable API (hint docs say "since 3.2.0") | 89,792 installs / 30 days [VERIFIED: `brew info sdl3`] | github.com/libsdl-org/SDL [CITED: lens-host.md, 2026-11-23] | Not covered by the npm/PyPI/crates seam (system package); not run | Approved link-only; a dependency audit document in the style of `docs/evidence/zlib-host-prerequisite-audit.md` is a plan task (AUTONOMY package/provenance audit for a new package source) |
| `libsdl3-dev` | Ubuntu archive | no candidate on 24.04; 3.2.20 on 25.10; 3.4.2 on 26.04 [VERIFIED: docker `apt-cache policy` run this session] | n/a | same | n/a | Lane skip with recorded reason on 24.04 |

**Packages removed due to SLOP verdict:** none. **Flagged SUS:** none. SDL3 stays out of `provenance/copies.json` (nothing is copied) [CITED: lens-host.md, docs/policy/copy-provenance.md "Pre-vetted, not yet allowed" lists Zlib, so copying would be a D4 stop].

## Architecture Patterns

### System Architecture Diagram

```
 t64.input v1 script ──► [scripted source] ┐
 SDL gamepad (opt) ───► [SDL sample] ─────► [logical pad: buttons u16, x/y -128..127]
                                              │ lazily queued, stamp >= guest tick
 injected clock (ns) ──► [pump: ticks = ns/16] ▼
                              │        t64_machine_queue_input / t64_machine_advance(BOUND)
                              ▼                        │ RESOURCE_LIMIT moved: continue
                       [pump advance loop] ◄───────────┘ RESOURCE_LIMIT stuck: drain video, audio, text, retry once
                              │ terminal stop (GUEST_COMPLETION / CPU_UNSUPPORTED): stop queueing
                              ▼
        video packet (RGBA bytes + info) ─► tight pixel buffer ─► SDL surface blit (opt) | digest (tests)
        audio packet (S16BE, rational rate) ─► SDL_AudioStream (opt) | digest (tests)
        text (ISViewer) ─► log;   save: t64_machine_save_export at exit (host policy)
 Title ROM (authored .S + pinned IPL3, build-only) ─► admit ─► machine (profile ...-si-v2-s32k)
 Expected values: title_oracle.py ─► title.expected.json ─► runner test | consumer literals | pump test
```

### Recommended Project Structure
```
fixtures/title/        title.S, title.ld, title_oracle.py, title.expected.json,
                       title.short.input.json (<=3 fields), title.long.input.json, title.shifted.input.json,
                       title.runner-pins.json (label "runner-recorded pin, not independent")
scripts/               build-title-fixture.sh (copy of build-composite-fixture.sh), check-host.sh,
                       check-host-includes.sh (copy of the consumer guard), preflight-phase12.sh (+regexes),
                       qualify-phase12.py (thin delta of qualify-phase11.py)
host/CMakeLists.txt    standalone project: find_package(tendies64 CONFIG REQUIRED); option T64H_SDL OFF
host/pump/             t64h_*.c/.h  pure C99: includes only <tendies64/t64_machine.h> and libc
host/tests/            fake-clock tests, t64h_sha256 copy, strict t64.input v1 reader, literals header
host/sdl/              optional shell (~300 lines) + smoke.c (virtual joystick), only when T64H_SDL=ON
tools/hostcheck/CMakeLists.txt   script-only CTest registrations (picked up by the root tools glob, no root edit)
docs/evidence/         phase12-host-manual.md, phase12-libfuturo-fit.md, sdl3-host-dependency-audit.md,
                       phase12-qualification.md; docs/claims/phase12.md
```

### Pattern 1: The authored-title build chain (re-verified against the current tree)
**What:** Same chain as every fixture: `as -32 -mips3 -EB`, `ld -m elf32ebmip -EB -T x.ld`, `objcopy -O binary`, `t64_rom_pack --pack --boot ipl3 --payload bin --entry 0x80000400 --clock 0xf --crc2 0x0`, entry/clock/crc2 symbol checks, completion symbol must be a `break` [VERIFIED: scripts/build-pad-fixture.sh:112 `[ "$entry" = 0x80000400 ]`; fixtures/composite/composite.ld `__t64_rom_clock = 0x0000000f;`, `__t64_rom_crc2 = 0x00000000;`]. The scratch prototype built and ran through exactly these commands.
**Template:** `scripts/build-composite-fixture.sh` (210 lines): diff against the pad builder shows only the source list, schema string `t64.phase10-composite-fixture-build.v1`, a defsym loop (`--defsym NAME=1`), completion symbol `t64_save_completion`, and `check_manifest_identities` run by `--qualify` [VERIFIED: diff run this session]. The title builder needs its own stems (`phase12-title-s32k`, `phase12-title-refused-s32k`), its own schema string, and `source_content_sha256` over its own sources.
**The five idioms are already composed in one guest:** the `composite.S` header says its register setup, interrupt mask, RDP command copy, VI window and ISViewer print follow `fixtures/rdp/fill.S`, its audio setup follows `fixtures/audio/pcm.S`, its SI poll follows `fixtures/replay/mix.S` (itself from `fixtures/pad/poll.S`) and its PI DMA follows `fixtures/save/sram.S` [VERIFIED: fixtures/composite/composite.S:1-8]. The title is a loop around those blocks, so start from `composite.S`, not from five separate files.
**Linker script:** copy `fixtures/composite/composite.ld` (text at 0x80000400, `.rodata` after) [VERIFIED: fixtures/composite/composite.ld].
**Authoring-once rule:** a source edit re-pins the manifest row, the runner table row, the consumer literals and the pump expectations (`composite.S` header says "Editing this file re-pins every composite row"); finish and review the guest before pinning anything.

### Pattern 2: Title design that fits qualified scope (prototype-verified)
- **Profile:** `ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k` (descriptor 13, SRAM 32K, hash `9dfb90d8...`) [VERIFIED: fixtures/manifest.json:1160 `"profile": "ntsc-cart-4m-openipl3-rdram-b-t1-si-v2-s32k"`; runner/t64_real_fixture.c:156-157]. Z stays unused.
- **Screen and VI:** 128x96 RGBA8888, packet 49,152 bytes, stride 512: ORIGIN = a framebuffer address (prototype 0x100000), WIDTH 128, X/Y_SCALE 0x400, H_START 128 (start 0, end 128), V_START 192 (half-lines), H_SYNC 3093, V_SYNC 525 (standard field, 1,042,709 ticks [VERIFIED: docs/spec/vi.md:114-120]), STATUS 0x303 (mode 3 plus AA bits 3 = nearest pass-through; output bytes `20 30 60 ff` equalled the fill color 0x203060ff in the prototype). `T64_VIDEO_MAX_WIDTH/HEIGHT` are 320/240 [VERIFIED: core/t64_internal.h:160-161].
- **Frame lock (verified both ways):** wait until `MI_INTERRUPT` (0xa4300008) bit 0x08 is set, then write VI_CURRENT (0xa4400010) to clear; VI_INTR = 2. A VI_CURRENT poll variant also frame-locked. Interrupts stay masked exactly as `composite.S` lines 148-152 do. The packet at boundary B(k+1) shows what the guest rendered after pending at B(k); input stamped before the poll at B(k)+~4k ticks affects that render (observed: stamp at 3,145,000 moved the square in the packet ending 4,713,156).
- **Render per field:** a 16-word RDP list rebuilt in uncached RDRAM: SetColorImage (RGBA, 32 bit, width), SetScissor, SetOtherModes `0x2f300000,0` (fill), SetFillColor, FillRectangle (background), SetFillColor, FillRectangle (square), SyncFull; kick DPC START/END; wait MI bit 0x20 (set DP mask first with `sw 0x0800, 12(MI)`), clear with MI_MODE write 0x0800 — all idioms of `fixtures/rdp/fill.S` and `composite.S`. FillRectangle takes inclusive lower-right 10.2 coordinates (`0x3600c008` is (3,2) in fill.S) [VERIFIED: fixtures/rdp/fill.S:130-131 `.word 0x3600c008, 0x00000000`].
- **Input:** SI RD64B per field (frame literal and poll loop as in `composite.S` 286-344); bytes: buttons high, buttons low, x, y. Button bits: A 15, B 14, Z 13, Start 12, Up 11, Down 10, Left 9, Right 8 [VERIFIED: docs/spec/si-pif-controller.md:150-153 "bit 15 is A, 14 B, 13 Z, 12 Start, 11 Up, 10 Down, 9 Left, 8 Right"]. Add stick (|axis| >= deadzone) cheaply; it exercises the pump mapping end to end.
- **Audio:** AI enable, DACRATE 1103, BITRATE 15 (valid: 1104 >= 66*16 [VERIFIED: docs/spec/audio-interface.md:40-41]); two descriptors of about 2 KiB so the packets complete within 1.5M ticks (one sample event per frame at about 1,417 ticks; 4,410 frames would need 6.2M ticks, beyond the runner cap). Observed packet rate `48681812/1104`.
- **Save and quit:** 16-byte SRAM slot (magic, x, y, press count) via the `PI_DMA` macro of `composite.S` 113-131; load at boot if magic matches; Start writes the slot, prints a short ISViewer transcript (`print_line`/`emit_hex_byte` idiom, <= 4096 bytes) and executes `break` at a linked completion symbol. One ROM then serves the interactive host (never presses Start) and the headless runs (script ends with Start). B resets the position.
- **Size:** `composite.S` is 505 lines with data; the title fits under the 600-line cap [prototype 240 lines without save, print and stick].
- **Refused-packet control:** a defsym variant that writes a one-cycle SetOtherModes (`0x2f000000`) instead of fill: run on the prototype it produced all-zero frames and the guest never left its DP wait (no status, no stop); the guest bound or cap ends the run. Expected frame digests must differ from the digest of an all-zero frame and the oracle prints that digest so the comparator can name "blank".
- **Prototype cost (information only, no claim):** about 0.12 to 0.18 s native per standard field on macOS arm64 (12 fields 2.2 s including probe scanning); an idle spin ROM alone costs about 0.15 s per field, so guest polling, not RDP or VI, dominates. 128x96 is not faster than 320x240 per field for that reason.

### Pattern 3: Registration checklist for a new fixture (all sites found by grep of `phase10-composite-s32k`)
1. `fixtures/title/*` sources; `fixtures/manifest.json` rows (`authored-here / cleared / build-only`, toolchain `GNU Binutils 2.45`, `source.revision` = a commit that already holds the source, so sources and rows are two commits as in 10-14) [VERIFIED: rows 1148-1190 shape].
2. `scripts/build-title-fixture.sh`; `core/CMakeLists.txt` fixture target beside lines 389-419 (outputs `.z64 .elf .payload.bin .json`, depend on manifest, sources, packer); `runner/CMakeLists.txt` `add_dependencies(... t64-rdp-fill-fixture)` line (the packer is rebuilt by the RDP fill target; building concurrently races, hosted run 44631218639) [VERIFIED: runner/CMakeLists.txt:631-641].
3. Runner: a row in `runner/t64_real_fixture.c` (id, sha256, size, profile, profile sha256, completion PC, pass-eligible, tick cap, transcript bytes and sha256) — **a production runner edit**; adding rows needs no count edit (`pinned >= 6`, generic row checks in `runner/tests/test_phase09_runner.c:1604-1670`).
4. Runner test: copy `runner/tests/test_composite_two_run.c` (1281 lines) to a title test: raw-capture run, save-out run, equal final digest, wrong-input and refused-frame controls; CTest registration with a helper like `t64_phase10_composite_case` (literal TIMEOUT numbers only: `harness.phase09-qualification` scans every CMakeLists.txt).
5. Consumer: add `title` (and `title-persist`) to `tests/installed-consumer/cases.txt`, a literals header (`t64c_title.h`), a `T64_CONSUMER_COST_<case>` variable in `tools/abi/CMakeLists.txt` (a case with no cost is a configure error), and extend `scripts/check-consumer-literals.py`. The consumer reads only `--rom`; the title ROM needs its own path variable, not `T64_CONSUMER_ROM`.
6. Inventory: a `PHASE12_TESTS` block in `scripts/check-test-inventory.sh` (and add it to the concatenation at line 456), label `phase12` on every name, rows in `scripts/preflight-phase12-regexes.txt` (a test with `FIXTURES_REQUIRED t64_install` pulls `install.setup` into any `ctest -N -R` count that matches it, so count rows include it), COST and TIMEOUT per F-108.
7. Rights: nothing committed beyond text; `scripts/rights-scan.sh` default-denies ROM magic and `.z64`; rows are build-only [CITED: docs/policy/rights-and-fixtures.md "Automated enforcement"]. `docs/claims/phase12.md` blocks and `docs/evidence/phase12-qualification.md`.
8. **Oracle-authority behavior with appended rows:** the check pins each of 39 existing rows by canonical hash looked up by id; rows appended later "are outside this request and need no rebind" [VERIFIED: scripts/check-rdp-oracle-authority.py:386-409, 39 bound rows]. The 13 bound inputs are the RDP/VI core tests, `fixtures/rdp/fill.S`, the two case files and `check-rdp-vi-cases.py` [VERIFIED: docs/evidence/phase07-oracle-authority.json]; Phase 12 must not edit them. STATE.md approvals: manifest-row rebinds are identity-only and orchestrator-only; an appended row needs none.

### Pattern 4: Host-pump rules against the frozen-candidate API (all run on a scratch probe)
- **Tick arithmetic:** 62,500,000 ticks per second = exactly one tick per 16 ns, so `bound = ns / 16` (u64, no overflow below 2^64 ns, no rounding carry). The rate appears only inside policy strings (`vi_clock_policy` = `rsp62500000-ntsc48681812-halfline-rational-v2`, `ai_policy` = `fetched-rdram-stereo-s16be-rational-62500000-v1`) and `guest_tick_unit` = `model-rsp-cycle`; no numeric member or api.md sentence gives it to a host [VERIFIED: probe printed the descriptor; `git grep` found 62,500,000 only in vi.md, audio-interface.md, si-pif-controller.md]. Make it a pump config constant and add one sentence to `docs/spec/api.md` section 8.3 (api.md is not one of the four goldens, so no amendment).
- **Advance loop:** one advance dispatches at most the profile budget (4096 events, `advance_event_budget=4096`, `event_queue_capacity=256`); the title run needed 3,188 advances for 12.5M ticks (about 255 per 1M ticks). `RESOURCE_LIMIT` with the tick moved is a budget stop (continue); `RESOURCE_LIMIT` with no movement is ring backpressure: drain then retry once; a second no-move is a permanent stall (hostloop.c treats it so). Bounds behind the guest tick are `TIME_REVERSED`, so a stalled advance must not be followed by a bound below `result.tick`.
- **Drain order:** video, then audio, then ISViewer text (the order `hostloop.c` uses and `docs/spec/api.md` sections 9.1 to 9.3 describe); results are independent of cadence (`installed.consumer.hostloop-cadence`). Rings hold exactly two packets each; undrained, the composite stalls at tick 540,952 and the 320x240 prototype title at tick 3,670,447 (both reproduced; the stall tick is guest-dependent, so a pump test pins it per fixture, never globally).
- **Sizing without trial and error:** `t64_machine_output_status` (48 bytes) gives queued, free and next-packet bytes; buffers up to 307,200 bytes (the public header does not state the maximum; use `video_next_bytes`).
- **Pixels:** the packet is already tightly addressed RGBA bytes in memory order R,G,B,A (first bytes `20 30 60 ff` equal the fill color); conversion is a row copy honoring `stride_bytes`. SDL's `SDL_PIXELFORMAT_RGBA32` is byte-order R,G,B,A.
- **Audio:** S16BE stereo, rate `rate_numerator/rate_denominator` (48,681,812/1,104 observed); the pump hands the shell raw packets; the shell converts with `SDL_AudioStream` (`SDL_SetAudioStreamFormat` from S16BE worked in both probes). Underrun policy: silence, counted.
- **Input timing:** `t64_machine_queue_input(tick, port 0, buttons, x, y)` needs `tick >= guest tick`; the controller latch is "last input stamped strictly before E" [VERIFIED: docs/spec/api.md:538-545]. Scripted events are queued lazily (only events with tick <= the coming bound; 256-entry queue shared with the scheduler) and before the advance that contains them. Live samples are stamped at the completed tick of the previous advance. After `GUEST_COMPLETION` a queue call answers `NOT_READY` (7) while advance answers OK with the same reason and tick (probe; composite completes at tick 599,357 as its pins say) — the pump must stop queueing at a terminal stop.
- **Two-speed equality (the claim), measured:** the 128x96 title run to 12,500,000 ticks with slice rates 240, 60, 10 and 3 Hz gave identical video stream hash, audio stream hash and final machine digest, with 0, 0, 4 and 5 mid-slice drain stalls. The unit test uses the real core, not a mock.
- **What a fake-clock test needs:** a clock function pointer (context, returns ns), a scripted event table, sinks that hash packets, the title ROM path, and assertions on tick conversion (0, 15, 16 ns), backward clock (refused, counted, no advance), carry across steps, drain order, stall detector (a host that never drains must fail with "stall without progress", the seeded failure of `hostloop`), terminal handling, queue deferral.
- **Responsiveness for the SDL shell:** 1M ticks take about 150 ms of CPU, so the shell calls a step with a per-call tick ceiling (for example 64k ticks) and pumps SDL events between calls; the pump API should therefore take a maximum-advance budget argument.

**Candidate API ergonomics defects the host work may expose (budget one reserved amendment plan, expect none):**
| # | Observation | Disposition recommended |
|---|-------------|-------------------------|
| C-1 | Numeric tick rate not given to hosts | Documentation in api.md (free); a header comment or new function is an amendment: not recommended |
| C-2 | `write_generation` does not count guest writes (the header says so; the composite wrote SRAM and `write_generation` stayed 0 in the probe) | Documented behavior, not a defect; hosts export and compare bytes |
| C-3 | `T64_STOP_FRAME_EVENT_COUNT` unreachable (documented, FN/11-29) | Keep; pump slices by ticks |
| C-4 | Maximum video packet size absent from the public header | Use output_status; optional header constant would be an amendment: not recommended |
| C-5 | `RESOURCE_LIMIT` has two meanings (budget vs backpressure) distinguishable only by tick movement | Documented in api.md 8.1; pump encodes the rule |
| C-6 | I-05 bus-read `value_out` span unchecked; S-05 eight padding holes | Decision items of the bind (Pattern 6) |

### Pattern 5: SDL3 facts and the non-gating lane (verified here)
- **Package config:** `find_package(SDL3 CONFIG REQUIRED)` and target `SDL3::SDL3` configured and built with CMake 4.4.3 against the Homebrew prefix with no extra path; installed license text is zlib, "Copyright (C) 1997-2026 Sam Lantinga" [VERIFIED: probe build; /opt/homebrew/opt/sdl3/LICENSE.txt]. Use `find_package(SDL3 3.2.0 CONFIG REQUIRED)`.
- **Hints and env vars (same strings):** `SDL_HINT_VIDEO_DRIVER` = `"SDL_VIDEO_DRIVER"`, `SDL_HINT_AUDIO_DRIVER` = `"SDL_AUDIO_DRIVER"` (comma list, set before init, since 3.2.0), `SDL_HINT_VIDEO_DUMMY_SAVE_FRAMES` = `"SDL_VIDEO_DUMMY_SAVE_FRAMES"` ("1" saves `SDL_windowX-Y.bmp` frames; observed `SDL_window2-00000001.bmp`, 230,454 bytes, 24-bit), `SDL_HINT_AUDIO_DISK_OUTPUT_FILE` = `"SDL_AUDIO_DISK_OUTPUT_FILE"`, and `SDL_AUDIO_DUMMY_TIMESCALE`/`SDL_AUDIO_DISK_TIMESCALE` (the dummy and disk audio drivers simulate real time; the scale hints speed them) [VERIFIED: /opt/homebrew/include/SDL3/SDL_hints.h:459, 472, 486, 499, 3724, 3737]. Setting them as environment variables worked and avoids test-only branches in the shell.
- **Headless init:** macOS 3.4.10: `SDL_Init(VIDEO|AUDIO|JOYSTICK|GAMEPAD|EVENTS)` with dummy drivers, `SDL_CreateWindow`, `SDL_GetWindowSurface` (XRGB8888, pitch 1280), `SDL_BlitSurface` from an RGBA32 surface, `SDL_OpenAudioDeviceStream`, `SDL_AttachVirtualJoystick` (type GAMEPAD, 6 axes, 15 buttons) then `SDL_OpenGamepad`, `SDL_SetJoystickVirtualButton/Axis` and read-back through the gamepad API all succeeded. Linux (Ubuntu 26.04 container, SDL 3.4.2, no DISPLAY): identical output. **Without the hints, Linux headless `SDL_Init` fails ("No available video device")**, so the lane must set both before init [VERIFIED: runs this session]. The API is `SDL_AttachVirtualJoystick(const SDL_VirtualJoystickDesc *)` (SDL3; `...Ex` does not exist).
- **Shell calls:** `SDL_GetTicksNS()` is the injected clock source; `SDL_BlitSurfaceScaled(..., SDL_SCALEMODE_NEAREST)` for integer scale; `SDL_GetWindowTitle` can assert the D7 title string [VERIFIED: SDL_timer.h:213, SDL_surface.h:90, 1450].
- **Hosted availability:** `macos-26` image lists Homebrew 7.0.14, CMake 4.5.0, Ninja 1.13.2 and no SDL (so `brew install sdl3` is a lane step) [CITED: https://github.com/actions/runner-images/blob/main/images/macos/macos-26-arm64-Readme.md]; `ubuntu-24.04` has no SDL3 apt candidate [VERIFIED: docker `apt-cache policy libsdl3-dev` returned no candidate; `libsdl2-dev` 2.30.0 exists].
- **Cheapest non-gating lane:** a local script subcommand `sh scripts/check-host.sh sdl` (precedent: `check-install.sh sanitize|tsan|fresh-archive` are "LOCAL or manual lane, never in the gate or the hosted workflow") costs no bound source. A hosted workflow file is not free: `scripts/check-ci-yml.sh` checks every file of `.github/workflows` against the two-gate-job structure and special-cases only `rsp-stress.yml` by SHA-256 (`CI-STRESS-WORKFLOW`); a new file needs a new case clause with a pin, a seeded mutation per rule (the selftest fails with "no seeded mutation asserts this rule"), `CI_SELFTEST_MIN_CASES` (138) review in the qualifier, and a first hosted run — about one plan. `ci.yml` and `gate.sh` stay untouched either way [VERIFIED: scripts/check-ci-yml.sh:56-70, ALL_RULES; scripts/qualify-phase11.py `CI_SELFTEST_MIN_CASES = 138`]. Recommendation: local script lane plus a documented manual command; no workflow in Phase 12.
- **Local-lane hygiene:** the disk audio driver prints its output path to stderr; raw lane logs contain local paths and must not be committed (PII scan rule); commit only sanitized lines (SDL version, driver names, frames presented, pass/fail).

### Pattern 6: Final bind mechanics (what `check-abi.py freeze` and the record require)
- **Rule as coded:** `repo.abi-freeze-record` recomputes header manifest, public symbols (35), layout (14 structs, 165 members, 190 constants), status classes (39 + 8), padding holes (8) from the installed prefix and fails on any difference; `FREEZE_CANDIDATE_MINOR = 1`; `minor != FREEZE_CANDIDATE_MINOR + rows` fails ("needs N amendment row(s)"); any header edit fails `header_sha256` until `freeze --write` regenerates `header.sha256` and the block [VERIFIED: scripts/check-abi.py:695, 886-890, 957-962].
- **Record text for the bind:** "the final bind ... either confirms this candidate or lists its amendments ... and it may raise the major version only when no amendment remains open" [VERIFIED: docs/abi/FREEZE-CANDIDATE.md:116-120].
- **0.x continuation:** only the header comment settle moves the hash; `freeze --write` and record text change; no constant moves. Its drawback: under `SameMinorVersion` any later additive minor forces every consumer to rebuild.
- **Major 1 (recommended once nothing is open):** (1) header: `T64_ABI_VERSION_MAJOR` 0→1, `MINOR` 1→0; (2) `core/CMakeLists.txt:160` `VERSION 0.1.0 COMPATIBILITY SameMinorVersion` → `1.0.0 SameMajorVersion` and its comment at 141; (3) `scripts/check-abi.py`: `FREEZE_CANDIDATE_MINOR` becomes a bound base (suggest a new record key `bound_minor`, so `minor == bound_minor + rows after the bind`; earlier amendment rows must not be counted, because `AMENDMENT_ROW_RE` counts every dated table row) and the seed `freeze-amendment-missing` (anchor `UINT32_C(1)`→`UINT32_C(2)`) moves; (4) `docs/abi/layout.c` lines 245-246 pins regenerate (`layout --write`); (5) `core/tests/test_api_surface.c:21-22` (`T64_CHECK_INT(T64_ABI_VERSION_MAJOR, 0)`, `MINOR, 1`); (6) `tests/installed-consumer/CMakeLists.txt` `T64C_REQUIRED_VERSION 0.1` and `scripts/check-install.sh` lines 211 and 985; (7) `controls.tsv` rows `version-mismatch-minor` (request 0.2) and `-patch` (0.1.5) become requests of a higher minor (1.1) and a higher major (2) plus one positive control; (8) `docs/spec/api.md` sections 2.1 to 2.3, 13, 14 and 15, and `docs/abi/FREEZE-CANDIDATE.md` status, block (`abi_version: 1.0`, `package_version: 1.0.0`) and wording; (9) a new unit case for the live branch `caller_minor <= library minor` of `abi_version_is_accepted` (`core/t64_machine.c:2718-2729`: today only the `caller_major == 0` branch runs; at 1.0 "older minor accepted" is untestable until a first minor exists — record that gap); (10) `repo.stale-freeze-text` gains a deny phrase for "provisional until the Phase 11" so the settle cannot regress.
- **Header settle (FN-03):** 13 comments contain "provisional" (lines 63, 131, 137, 142, 174, 179, 189, 362, 677, 720, 877, 914, 963), the banner (lines 2-4) and the sentence at 713 ("the API is Phase 11"); 15 sites. Production C comments with the same phrase (`core/t64_state.c` 4, `core/t64_internal.h` 2, `core/t64_machine.c` 1, `core/t64_facade.h` 1) and about 15 lines in `docs/spec/*` are optional; any `core/*.c` comment edit moves the build id and must not touch a sealed body (`repo.rsp-no-hle` seals bodies). FN-02: add the CP0 corrupt-phase `INTERNAL_DEFECT` source to the `t64_machine_advance` row of api.md section 12 (free).
- **S-05 (eight holes):** inserting explicit `uint32_t reserved` members into the holes keeps every offset and size on the bound targets but edits six `T64_*_INIT` macros (positional lists), `layout.c` and the block; after the bind it would not be additive ("never inserted", api.md section 13), so it is a before-the-bind decision. Recommended default: keep the holes (copy-out builds zeroed locals; the record already binds the eight rows); revisit only if the owner wants FFI-clean structs. I-05 (`value_out` span) also stays an accepted limit.
- **"What moves" for a final qualification:** `docs/abi/header.sha256`, the block (`header_sha256`, `layout_sha256`), `T64_BUILD_ID` (header and CMake are build inputs), installed prefix and `tendies64ConfigVersion.cmake` in every build tree, qualifier constants (`qualify-phase11.py` `FREEZE_GOLDENS`, `CLAIMS["final_abi"]`).

### Pattern 7: LibFuturo fit (refreshed against committed objects only)
- **Pin:** the sibling checkout HEAD is `4177a43e67312e2db88ab21cba13aa7350063105` (2026-11-23, "docs: finalize browser-host v2.4 state"), identical to the Phase 11 read; 0 commits since; 1,169 commits; tag `v1.6`; **no remote configured**; the working tree has uncommitted edits that were not read (committed objects via `git show` only; path deliberately not recorded). Published status: the lens (2026-11-23) found a public GitHub repository of the same name whose history was 2 commits on main; this local history is a different, longer one, so the evaluation pin stays "a local, unpublished SHA". The public site was not re-queried this session (a request would carry project identifiers to an external service); plan time may re-check with the owner's authorization.
- **Refreshed table** (header 347 lines, `docs/contract.md` 430 lines, Apache-2.0 LICENSE):

| Axis | LibFuturo at `4177a43` | Tendies64 (candidate ABI 0.1) | Fit |
|------|------------------------|-------------------------------|-----|
| Instance | `lf_create(profile_id, options)`, `lf_destroy(&owned)`, `void *allocate(ctx, bytes)` | create-info with allocator returning storage records, `struct_size`, ABI handshake | compatible shape, convert |
| Content | `lf_load(content, configuration_id)`, transactional (LC-LOAD-01); no unload or reset | admit, reset, unload | adapter maps; no conflict |
| Input | one `uint32 buttons` sampled at advance start (LC-INPUT-01, `LF_INPUT_AT_ADVANCE_START`) | tick-stamped port-0 events, buttons + signed stick | lossy: tick stamps and stick packable, timing not |
| Time | `work_units` of a profile unit (`LF_TIME_FRAME`, NES half-master tick), overshoot, exact `frame_rate` or 0/0 | guest ticks (62.5 MHz), VI fields not frame-exact | needs new enum value; cadence 0/0 |
| Video | GRAY8, RGB24, INDEX8 into a caller span per run | RGBA8 packets up to 320x240 from a 2-packet ring | RGB24 drops alpha; ring-to-span adapter |
| Audio | S16 native interleaved, one rate per profile or run | S16BE, rational rate 48,681,812/(DACRATE+1) | byteswap + rate conversion |
| Media | `lf_copy_media`/`lf_import_media` with `guest_generation` | save export/import, `write_generation` host-only | comparable, not shared |
| Snapshot | opaque, exact-qualified fixtures, same session, local macOS arm64 | state container bound to build identity, composite family only | comparable, not shared |
| Errors | 6 statuses, no classes; postconditions per operation | refused / guest-stop / defect classes, poison | coarser; no poison shape |
| ABI promise | "experimental ... no stable binary or plugin ABI" | candidate, final bind this phase | neither side could rely on the other |
- **Conclusion to write:** no adapter now; deliverable is one evidence document plus a cheap guard (`repo.api07-evaluation`: 40-hex SHA present, conclusion text present, no `libfuturo` token in `core/ runner/ host/ tools/ tests/` outside the document).

### Anti-Patterns to Avoid
- Instruction-count delay loops in the title (decision); an exception handler for the VI interrupt (unqualified idiom).
- Editing `ci.yml`, `check-ci-yml.sh` or `gate.sh` for the SDL lane; building `host/` from the root CMake (the glob and bound files make that a wider change).
- Pinning manifest, runner and consumer rows before the guest source is final.
- Declaring a runner pass from a screenshot or from agreement of two outputs both produced by the code under test (G6); the title expectations come from the oracle.
- Using wall time anywhere in the pump other than through the injected clock.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Window, audio device, gamepad | platform code | SDL3 (link-only) | Verified headless support on both OSes |
| Audio format and rate conversion | a resampler | `SDL_AudioStream` (shell only) | No A/V claim; conversion is not core work |
| Integer-scaled blit | pixel loops in the shell | `SDL_BlitSurfaceScaled` nearest | Matches the no-GPU decision |
| ROM packing, IPL3 embedding | a new packer | `t64_rom_pack` via the builder | Pinned and checked |
| Expected values | reading the core or the runner | `title_oracle.py` (stdlib) | G6: no evidence from the producer under test |
| Install and relocation harness | new scripts | patterns of `check-install.sh` | Copy-paste default |
| Input script grammar | a new format | `t64.input` v1 (`schema`, `schema_version` 1, `events[{buttons, port, tick (decimal string), x, y}]`) | Existing files `mix.input.json`, `composite.input.json` |

**Key insight:** the risk in this phase is registration breadth (a fixture touches about 12 sites) and bound-source churn, not algorithms.

## Runtime State Inventory (final-bind rename of 0.1 to 1.0 only)

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None: no database or datastore holds the version | none |
| Live service config | None | none |
| OS-registered state | None (no tasks, units or plists for this project) | none |
| Secrets/env vars | `T64C_REQUIRED_VERSION` cache variable of the consumer; no secret | code edit (CMake default and `check-install.sh` messages) |
| Build artifacts | Installed prefixes under `build/gate/install-check`, fresh-archive prefix, each carrying `tendies64ConfigVersion.cmake` 0.1.0; consumer build trees cache the requested version | rebuild and reinstall; `install.setup` clears and recreates its prefix |

## Common Pitfalls

### Pitfall 1: Raw capture cannot hold a large frame
**What goes wrong:** a 320x240 RGBA packet (307,200 bytes) written by the runner under the supervisor ends in `SIGXFSZ`; fails closed, publishes nothing.
**Why:** `RLIMIT_FSIZE` is `T64_REAL_RESULT_MAX_BYTES + FSIZE_SLACK` = 4096 + (64ull << 10) = 69,632 bytes unless the artifact pair is used (and the pair is refused together with `--raw-capture-dir`) [VERIFIED: runner/t64_supervisor.c:2189-2192, 47; runner/t64_real_result.h:9; docs/spec/raw-capture.md "Known limit"].
**Avoid:** title screen <= 17,408 pixels; use 128x96 (49,152 bytes). **Warning sign:** `runner-signalled` results.

### Pitfall 2: The runner stops a guest at 4,000,000 ticks
**What goes wrong:** the title cannot show more than three standard-NTSC fields through the runner (first packet about 1.59M, then 2.63M and 3.67M in the prototype).
**Why:** `#define T64_REAL_GUEST_TICKS_LONG_CAP UINT64_C(4000000)` and `T64_REAL_GUEST_TICKS_CEILING` equal it; rows may not exceed the ceiling [VERIFIED: runner/t64_real_fixture.h:14-16; runner/t64_supervisor.c:968; runner/tests/test_phase09_runner.c:1643].
**Avoid:** runner scenario <= 3 fields (poll 1 neutral, poll 2 right + A, poll 3 B, poll 4 Start; quit by about 3.7M); everything longer goes through consumer and pump. Raising K3 is a runner policy edit with test pins; a shorter field period (V_SYNC 263 gives about 522k ticks) is a fallback lever, not recommended (host pacing story).

### Pitfall 3: SDL3 is not installable on the allowed Linux runner
**What goes wrong:** a hosted Linux SDL step fails at `apt install`. **Why:** no `libsdl3-dev` candidate on Ubuntu 24.04. **Avoid:** record "skip: SDL3 not installable on ubuntu-24.04" and run the lane locally (macOS brew, Linux 25.10+ or 26.04 containers).

### Pitfall 4: Headless SDL without hints fails
`SDL_Init` returned "No available video device" on Linux with no DISPLAY; set `SDL_VIDEO_DRIVER=dummy` and `SDL_AUDIO_DRIVER=dummy` before init in the lane script.

### Pitfall 5: A refused RDP packet looks like a quiet success
All frames are zero and the guest stalls at its DP wait; no status is produced. **Avoid:** the oracle prints the blank-frame digest, the comparator names "blank", the refused-frame control must end non-pass at the tick cap.

### Pitfall 6: Input stamped inside the poll window
The controller latch uses "strictly before E" and the poll time is only known to about 20k ticks (enable tick plus guest setup, measured once, like `POLL_TICKS` in `composite_oracle.py`). Place stamps at field midpoints (>= 100k ticks from any boundary plus 4k) and refuse ambiguous scripts in the oracle (exit 2), as `composite_oracle.py` does.

### Pitfall 7: Treating budget stops as errors, and draining only once per slice
About 250 `RESOURCE_LIMIT` budget stops occur per 1M ticks; with slices above 2 fields the rings fill mid-slice, so the drain must live inside the advance loop (measured: 4 and 5 mid-slice stalls at 10 and 3 Hz).

### Pitfall 8: Queueing past completion or past QUEUE_FULL
`queue_input` after `GUEST_COMPLETION` is `NOT_READY`; the 256-entry queue is shared with scheduler events; queue lazily.

### Pitfall 9: A new workflow file breaks `repo.ci-yml`
See Pattern 5; the cost is a `check-ci-yml.sh` edit, its selftest and a qualifier review.

### Pitfall 10: Test names that need the install fixture inflate regex counts
`FIXTURES_REQUIRED t64_install` makes CTest add `install.setup` to matching `-R` results; Phase 11 preflight rows already account for it (`^installed\.consumer\.` counted 28 with it).

### Pitfall 11: Gate cost grows with names, not only seconds
`test-inventory-selftest` was 75 s hosted for 324 names (about 0.23 s per name) [CITED: PIN-IMPACT.md Gate-growth ledger]; every new name adds to it.

## Code Examples

### Tick conversion and the drain loop (pump rule, shown as C99 sketch)
```c
/* 62,500,000 ticks/s == 1 tick per 16 ns exactly; never read a wall clock here. */
static uint64_t t64h_bound_from_ns(uint64_t now_ns, uint64_t origin_ns) { return (now_ns - origin_ns) / 16u; }

for (;;) {                                   /* inside one pump step */
  t64_stop_result r = T64_STOP_RESULT_INIT;
  t64_stop_request q = T64_STOP_REQUEST_INIT(T64_STOP_GUEST_TICK, bound);
  st = t64_machine_advance(m, &q, &r);       /* OK for every stop reason */
  moved = r.tick > tick;  tick = r.tick;
  if (r.reason == T64_STOP_REASON_RESOURCE_LIMIT) {
    if (!moved) { if (stalled) return T64H_STALL; stalled = 1; drain_video(); drain_audio(); drain_text(); }
    else        { stalled = 0; }
    continue;                                 /* budget stop or drained: advance again */
  }
  break;                                      /* BOUND, GUEST_COMPLETION, CPU_UNSUPPORTED, QUIESCENT */
}
```

### Frame-lock excerpt that ran on the prototype (MIPS, guest)
```asm
wait_vi:  lw   $t2, 8($s4)        # MI_INTERRUPT (0xa4300008)
          andi $t2, $t2, 0x08     # VI pending
          beq  $t2, $zero, wait_vi
          nop
          sw   $zero, 0x10($s3)   # write VI_CURRENT: clear pending
```

### Headless SDL smoke environment (macOS and Linux, verified)
```sh
SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ./sdl-smoke   # then SDL_AttachVirtualJoystick(type GAMEPAD)
```

### Oracle derivation sketch (independent, stdlib only)
```python
# field boundary k, exact: ceil(N_k * 62_500_000 / 48_681_812) with N_k = k * (H_SYNC+1) * V_SYNC / 2 (Fraction)
# frame(k) = background everywhere, 16x16 square at (x_k, y_k); x_k, y_k from the input rule at poll k
# digest(k) = sha256(frame bytes); blank_digest = sha256(b"\0" * 49152); transcript, audio, save from the same rule
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| SDL2 `SDL_VIDEODRIVER` env | SDL3 `SDL_VIDEO_DRIVER` / `SDL_AUDIO_DRIVER` | SDL 3.2.0 | Lane scripts use the SDL3 names (verified working) |
| `SDL_JoystickAttachVirtual` | `SDL_AttachVirtualJoystick(const SDL_VirtualJoystickDesc *)` | SDL 3.2.0 | `SDL_INIT_INTERFACE(&desc)` first |

**Deprecated/outdated:** the lens-host.md LibFuturo table (pre-snapshot header); `docs/spec/raw-capture.md` statement that "the current real runner accepts only that authored guest" is stale in detail (the fixture table has 30+ rows) but its 68 KiB limit still holds.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Hosted macOS runs new tests at 2.8 to 4.3 times native (from Phase 10/11 ledgers), so about 16 native seconds is +15 to +25 s wall | Gate budget | Margin to 900 s overstated; a measured run in the review plan replaces it |
| A2 | `SDL_AUDIO_DUMMY_TIMESCALE`/`SDL_AUDIO_DISK_TIMESCALE` set high or low speeds the dummy audio drain enough for a smoke run | Pattern 5 | Lane takes longer; only the doc text was read, not run |
| A3 | A shorter field period (V_SYNC 263 about 522k ticks) would work as a fallback lever; arithmetic from the vi.md formula, not run | Pitfall 2 | Fallback unusable; no effect on the recommended path |
| A4 | `brew install sdl3` works on the hosted macOS image (Homebrew 7.0.14 listed, no SDL) | Pattern 5 | Optional hosted lane unavailable; local lane unaffected |
| A5 | The owner reads an ABI/package version 1.0.0 as an interface identity, not a release version (D6 reserves "release channel and versioning") | Pattern 6, Open Questions | One-way bind blocked until the owner confirms; default is to proceed under CONTEXT |
| A6 | Title instructions stay inside the qualified CPU opcode set (the prototype ran, but only the listed opcodes) | Pattern 2 | `CPU_UNSUPPORTED` at author time, caught by the first run |

## Open Questions

1. **What does "the final bind is the last plan" mean for the qualifier?** The qualification record must bind the post-bind tree. Recommendation: the bind plan is the last plan that moves ABI-visible bytes; qualification, claims and evidence are two further docs-and-records plans (as 11-24 and 11-25 were). Planner to confirm with the orchestrator.
2. **Is the ABI major 1 a D6 "versioning" decision?** Recommendation: proceed (CONTEXT locks it) while writing "interface identity, no release, no tag, `project(VERSION)` stays 0.0.0" into the record; if the owner objects, the 0.x continuation is the recorded fallback.
3. **Phase 12 G4 exception wording and reversal rule** (cold gate-macos above 900 s, or margin under 120 s, moves the long title scenario to a local lane).
4. **Keyboard fallback in the shell?** Not required by CONTEXT (controller mapping only); a 30-line keyboard map helps owners without a pad but widens test tables. Claude's discretion; default off.
5. **Is the LibFuturo public repository still the 2-commit history?** Not re-queried (privacy rule); the local pin statement does not depend on it.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Pinned Binutils 2.45 (`build/tools/install`) | title build | yes | 2.45 | rebuild by `build-boot-fixtures.sh` |
| `build/gate` tree, installed prefix, composite ROM | scratch experiments, check-install | yes (header hash equals `docs/abi/header.sha256` entry `46d07b8f...`) | candidate 0.1 | `sh scripts/check-install.sh setup` |
| CMake / Ninja | host project | yes | 4.4.3 / present | none needed |
| SDL3 | shell, lane | yes (Homebrew) | 3.4.10 installed, 3.4.20 stable | skip with recorded reason |
| Docker + `act` | Linux checks, apt probes | yes (Docker server running; `act` present) | Docker client 29.5.2 | none |
| Physical controller | owner checklist | no (owner-held) | n/a | human_needed, non-blocking |
| `gsd_run` research seam | research caching | not used (facts came from local primary sources and headers) | n/a | n/a |

**Missing with no fallback:** none. **Missing with fallback:** SDL3 on ubuntu-24.04 (skip), physical controller (checklist).

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | CTest via `cmake --preset gate` / `ctest --preset gate`; Python and shell checkers; C test programs (`T64_CHECK_*`) |
| Config file | `CMakePresets.json` (presets `gate`, `asan`); `scripts/check-test-inventory.sh` (inventory); `scripts/preflight-phase12-regexes.txt` (new) |
| Quick run command | `sh scripts/preflight-phase12.sh --no-replay` then `ctest --test-dir build/gate -L phase12 -j4` |
| Full suite command | `./scripts/gate.sh` (hosted `gate-macos` 15 min, `gate-linux` 10 min limits) |

### Phase Requirements to Test Map (proposed names; every name label `phase12`, COST = measured native seconds, TIMEOUT per F-108)
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| API-06 | Oracle regenerates `title.expected.json` byte for byte; blank digest printed | unit | `ctest -R '^harness\.title-oracle$'` | Wave 0 |
| API-06 | Runner: raw-capture run (<= 3 packets, audio, transcript) and save-out run vs oracle, equal final digest | integration | `ctest -R '^runner\.title-two-run$'` | Wave 0 |
| API-06 | Wrong-input (`shifted` script) and refused-frame (blank, non-pass) controls fail as expected | control | `ctest -R '^controls\.real\.title-(wrong-input|refused-frame)$'` | Wave 0 |
| API-06 | Installed consumer: short and long scenario equal the oracle literals; persistence round trip via save import | integration | `ctest -R '^installed\.consumer\.title'` | Wave 0 |
| API-06 | Manifest rows, rights, ROM identity | policy | existing `gov.manifest.*`, `gov.rights.repo`, builder `--qualify` | exists |
| API-05 | Pump builds against the moved prefix with warnings as errors | build | `ctest -R '^host\.pump\.build$'` | Wave 0 |
| API-05 | Fake clock: ns/16, carry, backward clock, bounds | unit | `ctest -R '^host\.pump\.clock$'` | Wave 0 |
| API-05 | Drain order, backpressure drain-and-retry, never-drain detector ("stall without progress") | unit | `ctest -R '^host\.pump\.(drain-order|stall)$'` | Wave 0 |
| API-05 | Input mapping table (stick scale, deadzone, buttons, port 0) and scripted source (lazy queue, QUEUE_FULL, terminal) | unit | `ctest -R '^host\.pump\.(input-map|script)$'` | Wave 0 |
| API-05 | RGBA frame copy honoring stride; audio byteswap helper; underrun counter | unit | `ctest -R '^host\.pump\.frames$'` | Wave 0 |
| API-05 | Title through the pump at two fake-clock speeds equals oracle and runner pins | integration | `ctest -R '^host\.pump\.title-two-speeds$'` | Wave 0 |
| API-05 | Pump sources include only the public header; init forms used; no SDL token in core, runner, tools; D4/D6/D7 policy scan | policy | `ctest -R '^repo\.host-(include-guard|include-guard-selftest|isolation|policy|policy-selftest)$'` | Wave 0 |
| API-05 | SDL glue smoke (dummy drivers, virtual joystick, window title, N frames) | local lane | `sh scripts/check-host.sh sdl` (not a CTest name) | Wave 0 |
| API-07 | Evaluation document has a 40-hex SHA, the "no adapter" conclusion, and no `libfuturo` token in code roots | policy | `ctest -R '^repo\.api07-evaluation$'` | Wave 0 |
| Bind | Bound record, goldens, version constants, controls, stale-text guard | policy | existing `repo.abi-*`, `install.abi-*`, `installed.controls.*`, `repo.stale-freeze-text` (extended) | exists |
| Phase | Qualification inventory and record | harness | `ctest -R '^harness\.phase12-qualification$'` | Wave 0 |

### Sampling Rate
- **Per task commit:** `sh scripts/preflight-phase12.sh --no-replay` (about 25 to 70 s) plus the plan's own names.
- **Per wave merge:** full local gate in a scratch clone (`git archive`/clone, not the shared tree), as in the Phase 10/11 review plans.
- **Phase gate:** full suite green, native qualification record, hosted pair, orchestrator cold run, before `/gsd-verify-work`.

### Wave 0 Gaps
- [ ] `fixtures/title/title_oracle.py`, input scripts, `title.expected.json` (oracle before the guest)
- [ ] `scripts/preflight-phase12.sh` and `preflight-phase12-regexes.txt`; `PHASE12_TESTS` block and label in `scripts/check-test-inventory.sh`
- [ ] `host/` scaffold, `scripts/check-host.sh`, `scripts/check-host-includes.sh` (+selftest), `scripts/check-api-init.py` roots
- [ ] `tools/hostcheck/CMakeLists.txt` registrations; `scripts/qualify-phase12.py` and its test (thin delta of the Phase 11 qualifier)
- [ ] Mutant rows (suggest `scripts/p12-mutants.py`, copy of `p10-mutants.py`): at least 8 pump mutants (divisor 15, queue after advance, skip text drain, drop first packet on stall, ignore stride, deadzone off by one, axis sign, completion treated as resumable) and 4 title mutants (D-pad bit swap, step 3, wrong tone sample, wrong save offset), each reddening a named test
- Framework install: none.

### CI Budget (estimates, not measurements; replaced by the review plan's measurement)
New names about 24; native seconds roughly: `host.pump.build` 2.5, `installed.consumer.title*` 2.9, `host.pump.title-two-speeds` 1.8, `runner.title-two-run` 1.6, two controls 1.6, `repo.host-include-guard-selftest` 1.0, `host.pump.drain-order` 0.8, `installed.consumer.title-persist` 0.9, others under 0.7 each: about 16 s native, 50 to 60 s summed hosted, about +15 to +25 s of `gate-macos` wall including +5 to +6 s of inventory selftest. Baseline: cold 729 s with margin 171 s (run 45795103375, 329 names), warm 487 to 592 s [CITED: docs/ci-reviews/LOG.md F-136]. No `ci.yml`, `check-ci-yml.sh`, `gate.sh` edit. SDL, sanitizer, TSan and fresh-archive lanes stay local.

## Security Domain

### Applicable ASVS Categories (level 1)

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | none |
| V3 Session Management | no | none |
| V4 Access Control | no | none |
| V5 Input Validation | yes | bounded strict parser for `t64.input` v1 in the pump (size cap, depth cap, closed keys, decimal-string ticks, port 0, axis -128..127); ROM and script files read through bounded reads; tick arithmetic on u64 only |
| V6 Cryptography | no new use | SHA-256 only as a digest in tests (copy of the existing implementation); no keys |
| V12 Files | yes | CLI paths are host input; no path is passed to the core; any save write (if included) is one atomic path |
| V14 Config / supply chain | yes | SDL3 link-only from a system package, audit document, no FetchContent/ExternalProject/vendored sources (policy scan test) |

### Known Threat Patterns
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Hostile or oversized input script | Tampering / DoS | size and event-count caps, strict grammar, refuse on first violation |
| Oversized packet vs host buffer | Tampering | size from `output_status.video_next_bytes`, cap at 307,200, `BUFFER_TOO_SMALL` handling |
| Local paths or user names in lane logs | Information disclosure | PII scan; commit sanitized lines only (SDL's disk driver logs its output path) |
| Product name or branding in a window title or strings | Repudiation / policy (D7) | exact-string test of the title "t64 minimal host (development)"; scan for the product name in `host/` |
| Vendoring SDL by accident | Policy (D4) | scan for SDL sources, `FetchContent`, `ExternalProject`, `third_party` |
| Binary redistribution | Policy (D6) | no upload step, no `.app`, no installer; `git.create_tag` false |

## Plan Sizing, Ordering and Owner Stops

**Estimate: 12 plans plus one reserved amendment slot (G5 ceiling about 15).** Prior sizes: Phase 10 plans 12 to 44 min, Phase 11 plans 10 to 52 min [CITED: STATE.md performance table]; the guest-authoring and bind plans are the ones that can exceed 45 minutes.

| Plan | Content | Risk |
|------|---------|------|
| 12-01 | Phase 12 PIN-IMPACT (decision record DR list, wave bases, G4 exception, LibFuturo SHA), preflight copy, `PHASE12_TESTS` skeleton | low |
| 12-02 | API-07 evaluation document and guard; SDL3 dependency audit document | low |
| 12-03 | Oracle, input scripts, `title.expected.json`, `harness.title-oracle` (independent, before the guest) | medium |
| 12-04 | Title guest, linker script, builder, fixture target, manifest rows (two commits) | high (assembly, 45 min) |
| 12-05 | Runner row, runner test (copy), wrong-input and refused-frame controls, runner pins | medium |
| 12-06 | Consumer `title` and `title-persist`, literals header and checker, cases, COST | medium |
| 12-07 | `host/` scaffold, pump core, clock and drain tests, guards, registrations | medium |
| 12-08 | Pump input, scripted source, frames, two-speed equality, stall detector | medium |
| 12-09 | SDL shell, local lane script, smoke, policy scans, owner checklist template | medium; split if over 45 min |
| 12-10 | Wave review in a scratch clone (gate, mutants, sweep 0 differences, includes guard) and amendment triage | medium |
| 12-11.. | Reserved: one numbered amendment plan per defect with evidence, row and suite re-run | n/a |
| 12-12 | Final bind (Pattern 6): header and doc settle, major-1 mechanics or 0.x record, decisions S-05 and I-05 recorded | high (wide re-pin) |
| 12-13 | `qualify-phase12.py`, final audit, native record | medium |
| 12-14 | Claims, evidence documents, push, hosted pair (cold run is orchestrator-only) | low |

**Ordering hazards:** oracle before guest (independence); guest frozen before any pin; title before pump equality; every host-found API defect becomes its numbered amendment plan before 12-12; qualification after the bind; cold run after the final push. The ledger must also record the 12-10 measurement of the true native seconds.

**Owner stops and dispositions:** D4 (system SDL3, link-only, scan guard); D6 (no tag, no artifact, version number is an interface identity, Open Question 2); D7 (neutral title, no icon, bundle or installer); physical-controller play-through is one `human_needed` non-blocking checklist; protected content none (authored guest, SRAM slot written by the guest at run time); D5 not involved; PII rule for lane logs.

## Sources

### Primary (HIGH confidence)
- Repository files read this session: `docs/abi/FREEZE-CANDIDATE.md`, `docs/spec/api.md`, `docs/spec/vi.md`, `docs/spec/audio-interface.md`, `docs/spec/si-pif-controller.md`, `docs/spec/replay.md`, `docs/spec/raw-capture.md`, `core/include/tendies64/t64_machine.h`, `core/t64_machine.c` (2713-2729), `scripts/check-abi.py` (680-1050), `scripts/check-ci-yml.sh`, `scripts/check-rdp-oracle-authority.py`, `scripts/build-pad-fixture.sh`, `scripts/build-composite-fixture.sh`, `fixtures/composite/composite.S`, `fixtures/rdp/fill.S`, `fixtures/manifest.json`, `runner/t64_real_fixture.[ch]`, `runner/t64_supervisor.c`, `tests/installed-consumer/*`, `tools/abi/CMakeLists.txt`, `.planning/*` (STATE, ROADMAP, REQUIREMENTS, AUTONOMY, PIN-IMPACT, 11-REVIEW).
- Runs this session (throwaway, not committed): prototype title assembled with the pinned Binutils; host probe against the installed prefix; SDL3 probes on macOS 3.4.10 and an Ubuntu 26.04 container (3.4.2); `apt-cache policy` in Ubuntu 24.04, 25.10 and 26.04 containers; `brew info sdl3`.
- LibFuturo committed objects at `4177a43e67312e2db88ab21cba13aa7350063105`: `include/libfuturo/libfuturo.h`, `docs/contract.md`, `README.md`, `LICENSE` (via `git show`, no path recorded).

### Secondary (MEDIUM confidence)
- https://github.com/actions/runner-images/blob/main/images/macos/macos-26-arm64-Readme.md (Homebrew, CMake, Ninja versions; no SDL).
- `.planning/research/v1.2-fanout/lens-host.md` (dated; re-verified where marked).

### Tertiary (LOW confidence)
- Hosted run multipliers and per-name seconds (estimates, A1); SDL timescale hints (A2).

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - every tool and prefix was used this session.
- Architecture (title, pump, SDL): MEDIUM-HIGH - prototype-verified, not yet in the repository's test harness.
- Final bind mechanics: MEDIUM - read from the checker and grep of pins; not exercised.
- Pitfalls: HIGH for 1 to 8 (code read or reproduced); MEDIUM for 9 to 11.

**Research date:** 2026-11-26
**Valid until:** 2026-12-10 for SDL package availability and LibFuturo HEAD; the in-repo facts hold until a plan edits the named files.
