# Lens: host and playable homebrew (Phase 12, API-05/06/07)

Date of checks: 2026-11-23. Repo read-only. Facts marked (V) were verified from a primary source in this run; (K) is background knowledge I did not re-verify; (I) is inference.

## Recommendations (first)

1. **API-06 route: choose A, authored in-tree.** Build a tiny assembly title with the existing pinned Binutils 2.45 + open IPL3 pipeline and register it as `authored-here / cleared / build-only` rows, exactly like the 30+ existing fixture rows. Defer route B (libdragon) to a documented non-goal for v1.2. It stays inside the owner rights stops, adds no toolchain, and is the only route whose every instruction is in qualified scope.
2. **Minimal game, inside qualified scope:** one screen, one 16x16 filled square, one beep, one save slot. Per frame the CPU writes a short RDP list (SetColorImage, SetScissor, SetOtherModes fill, SetFillColor, FillRectangle x2, SyncFull) in RDRAM and kicks DPC; reads the pad over SI/PIF joybus; D-pad/stick moves the square, A plays a fixed AI DMA square-wave burst, B resets; position/score persists via EEPROM or SRAM. Frame-lock on the VI interrupt (MI IP2) or on VI_CURRENT polling, never on instruction-count delay loops. Composes five idioms already authored and qualified (`fixtures/rdp/fill.S`, `pad/poll.S`, `audio/pcm.S`, `save/*.S`, `replay/mix.S`): copy-paste, no new dependency. Avoid Z (VID-07 is not landed until Phase 10 closes), avoid textures in the first slice (add a COPY-mode sprite only as a stretch), no RSP microcode, no TLB, no libultra.
3. **API-05: keep SDL3, but make it a thin shell over a non-SDL host-pump.** Split into (a) `host-pump` pure C, no SDL: drains the frozen API's video/audio rings, owns the pacing policy via a host-supplied monotonic clock, maps logical input, converts RGBA8 to a pixel buffer, accepts a scripted input source; unit-testable with a fake clock in the required gate. (b) ~300 lines of SDL3 glue, an optional CMake target, OFF by default, in a separate non-gating (or manual/nightly) CI lane so it cannot eat GATE-01 budget. A "no window" or raw-dump player does not satisfy Phase 12 success criterion 2 (window + physical controller), so it is not a substitute; (a) already is the raw-dump player.
4. **SDL3 sourcing:** link-only against a system/CI-installed SDL3 via `find_package(SDL3 CONFIG)`; do not vendor or FetchContent SDL source. Vendoring is a copy and zlib is not on the D4 allowlist (ISC/MIT/BSD-3/Unlicense), so it would hit the owner's D4 stop; link-only follows the ROADMAP D4 default. Record SDL version and license in the evidence record. No binary is redistributed before D6, so notice obligations are nil until a release.
5. **Presentation: window-surface blit, integer or nearest scale only** (SDL_GetWindowSurface path, no SDL_Renderer/GPU), which keeps PLAY-02 (GPU renderer) out and works with SDL's dummy video driver.
6. **Testing:** scripted-input equality is the CI evidence (see Q3); the physical-controller check is a single owner-attested manual checklist, and it is likely a physical-hardware owner stop under AUTONOMY section 4 ("testing with owner-held hardware"), so schedule it as a non-blocking human-needed item at phase end.
7. **API-07: written evaluation only**, pinned to a LibFuturo commit SHA, done after the Phase 11 freeze. Expected conclusion: not adoptable today (see Q4); no adapter code, no core API change.
8. **Insurance for the freeze (cross-phase, flag for the roadmap owner):** make the Phase 11 installed consumer run a host-shaped loop (advance to next frame boundary, drain video+audio rings, timed input) on the Phase 10 composite fixture. The host is otherwise the first real consumer and will discover ergonomics defects after the freeze, forcing an ABI revision.
9. **Define "plays" as correct, not real-time.** No throughput measurement exists (PERF-01 is v2), so the interpreter may not hold 60 fps with a software RDP. Success criterion 2 should not imply wall-clock speed; the host presents frames as produced and reports "slow" honestly. Take one throwaway measurement for the plan, make no claim from it.

## Q1. Homebrew route A vs B

Existing approach (V, repo): `scripts/build-pad-fixture.sh` assembles `.S` with the qualified `mips64-elf` as/ld/objcopy (Binutils 2.45, source SHA256 pinned, receipt `build/tools/qualified-host-tools.json`), prepends the pinned open IPL3 (Unlicense asset, SHA pinned, compat pin 98154daf...), packs with the in-tree `t64_rom_pack`. Manifest rows: `authored-here / cleared / build-only`; policy forbids committed ROMs (`docs/policy/rights-and-fixtures.md`; rights scan default-denies ROM magic and `.z64` extensions). Route A is therefore a new row set plus one build script copy; "copy-paste beats dependency" is literally the existing practice (each builder is documented as a copy of the previous one).

Route B facts (V via GitHub API, 2026-11-23):
- libdragon license: The Unlicense (`LICENSE.md`). Repo `DragonMinded/libdragon` shows `archived: true`, last push 2026-09-14. Maintenance owner is therefore unclear; the project's own rule wants "provenance and maintenance owner clear". Its `toolchain-continuous-prerelease` release dates from 2023 and is named V16.2.0, so prebuilt toolchain binaries are stale or mismatched and a source build is the realistic route.
- `tools/build-toolchain.sh` pins Binutils 2.45 (same as the repo's pin, a coincidence worth noting), GCC 16.2.0, Newlib 4.4.0.20231231, GMP 6.3.0, MPC 1.3.1, MPFR 4.2.1; applies an embedded GCC patch adding an N64 target and multilibs; macOS path needs Homebrew (gmp, mpfr, libmpc, gsed, isl, make, python3, texinfo, ninja). It downloads from ftp.gnu.org and sourceware.org. The script header only says "see root folder for license information"; it does not state GCC/newlib licenses.
- Third-party code inside the repo (V from tree listing): `src/fatfs` (FatFs license), `src/compress/lz4*`, `src/libcart`, tools with freetype, libsamplerate, shrinkler, rect_pack. Not all are Unlicense, so a "libdragon ROM" is not uniformly Unlicense.
- (K, not re-verified) libgcc is GPLv3 with the GCC Runtime Library Exception and is statically linked into every ROM; Newlib is a mix of BSD-style licenses. A built ROM would embed GPL-with-exception and BSD code, which collides with the project's "GPL never copied" and D4 allowlist reasoning and needs a binary-rights analysis the owner has not made. Treat as an owner/legal question, not a plan-time detail.
- Burden: the Binutils-only build is already about half of the macOS gate (docs/ci-reviews/LOG.md F-096, quoted in AUTONOMY section 2). A GCC 16 + newlib stage-1/stage-2 build is far larger; I did not measure it, but it cannot fit the GATE-01 budget logic and would need its own cache and receipt.
- Silent unqualified dependencies of libdragon output (I): display/rdpq init loads RSP microcode (rspq) while the RSP is only qualified against an authored smoke program; mixer/audio paths assume AI interrupt behavior and 44.1/48 kHz DAC rate pairs while the profile policy is a software-chosen rational (`fetched-rdram-stereo-s16be-rational-62500000-v1`); boot code does cache ops, CIC/PIF handshake assumptions (challenge model not modeled), TLB/COP0 timer use; CPU timing is the T1 software policy, not hardware. A passing run would be a boot-and-screenshot style claim the project explicitly distrusts.

Verdict: A is lower risk on rights, supply chain, CI cost, qualification honesty and flat dependencies. Cost of A: no C compiler, so the game is assembly; keep it to the cap below. B could be revisited post-v1.2 as a labelled "unqualified compatibility probe", local-only.

Scope cap for the title (prevents balloon): one screen; one sprite (filled rect); one audio burst; one save slot; no scrolling, no textures, no Z, no multi-port; <= ~600 lines assembly composed from existing idioms.

What qualified scope offers (V, docs/spec): composite profile family (descriptor 11 = VI v9 + AI; descriptor 12 = SI standard controller, port 0 only, EEPROM 4K/16K and SRAM 32K via SI cart channel / save-media); software RDP fill/copy/triangle/texture/combiner/blend; VI v9 filtered scanout RGBA8888; AI DMA two-descriptor queue; MI interrupt merged into Cause.IP2. Z-bearing triangle variants are refused until VID-07. Not modeled: ports 1-3, controller pak/rumble, PIF challenge, PIF window direct access, hardware timing, commercial microcode. Phase 12 must pick the one named composite profile Phase 10's RUN-07 fixture selects, so the title and the fixture share an identity.

Silent traps for the game author: do not time by instruction count (T1 costs are policy); do not read AI/VI timing as hardware-true; do not depend on PIF poll latency (a placeholder figure in si-pif-controller.md); do not use RDP features outside v6/v9 contracts (unqualified cycle types are refused before drawing, so a refused packet shows as a blank screen, which a test must distinguish).

## Q2. SDL3 as a link-only host dependency

- License: zlib. Upstream LICENSE.txt "Copyright (C) 1997-2026 Sam Lantinga" with the standard zlib terms (V). Wiki licensing FAQ: usable commercially as a static or shared library, shared recommended; it states no extra obligations (V). Latest release at check time: release-3.4.20, 2026-11-03 (V, GitHub API).
- D4: the ROADMAP D4 default says "link-only zlib host dependencies follow the dependency policy, with notices", so linking needs no owner decision; copying/vendoring would. Pre-vetted-but-not-allowed list in `docs/policy/copy-provenance.md` includes Zlib, which confirms vendoring would be a D4 decision. SDL stays out of `provenance/copies.json`; record it in a dependency record (version, license, how obtained) instead.
- Pinning: option 1, system install (Homebrew/apt): easy, but versions float and Linux distro availability of SDL3 is not verified here; a new package source triggers the AUTONOMY package/provenance audit. Option 2, pinned source tarball (sha256) built in the CI lane with ninja and optional subsystems disabled: reproducible, adds minutes, still link-only. Recommend option 1 for local dev and option 2 only if the lane becomes gating; record `SDL_GetVersion()` in the evidence.
- Headless CI: SDL source tree contains dummy video (`src/video/dummy`), dummy audio and disk audio (`src/audio/dummy`, `src/audio/disk`) drivers (V, tree listing); the hint reference lists `SDL_HINT_VIDEO_DRIVER`, `SDL_HINT_AUDIO_DRIVER` (comma-list, set before init, since 3.2.0) and `SDL_HINT_VIDEO_DUMMY_SAVE_FRAMES`, `SDL_HINT_AUDIO_DISK_OUTPUT_FILE` (V names only; semantics not read, verify at plan time). `SDL_AttachVirtualJoystick` (since 3.2.0) plus `SDL_SetJoystickVirtualButton/Axis` let a test feed the real mapping path without hardware (V). Whether gamepad init works with no video device on both runners: unverified.
- Alternatives considered: no-window raw player (fails criterion 2); single-file approach (no single-file window+audio+controller library fits the allowlist; stb-style copies lack window/audio; any copy of SDL or miniaudio is a D4 question); platform-native (Cocoa) host: not portable, more code. Recommend SDL3, optional, behind the pump seam.
- CI impact: keep SDL out of the required gate and off the macOS job-limit budget (GATE-01). `act` can run the Linux lane locally per AUTONOMY section 2; macOS only hosted.

## Q3. Testing a host without a human

Tiers (project vocabulary: manifest `expected.kind` = self-checking, spec-derived, hardware-derived, differential, regression-pin; claims tied to profile + build + evidence tier):
1. Headless runner result for the title: transcript, final framebuffer digest, audio packet bytes, save bytes under a scripted `t64.input` v1 file (existing `fixtures/replay/mix.input.json` shape; replay v1 exists). Expected values independently derived by an oracle script (the pattern of `poll_oracle.py`, `mix_oracle.py`), not by the core. A wrong-input negative control.
2. Same title through the installed consumer (VER-05 style) equals (1).
3. Host-pump with scripted source and fake clock produces byte-identical video/audio packets and final digest as (1): proves the host adds nothing to guest behavior; pacing must not change guest results (run at two different fake-clock speeds, assert equal).
4. SDL glue smoke under dummy video/audio with a virtual joystick: button/axis to logical-mask table test (stick scaling into -128..127, deadzone, D-pad, unplug), window creation succeeds, N frames presented, exit clean. Evidence tier: self-checking, host-glue only, explicitly not rendering-correctness.
5. Manual, owner-attested: real controller moves the square, audible beep, window title is neutral. Record as `docs/evidence/phase12-host-manual.md` with date, OS, SDL version, controller model, build id, pass/fail per line, labelled "maintainer observation, not reproducible, no latency or sync claim". Because it uses owner-held hardware it should be a listed human-needed item, not something autonomous execution waits on. Success criterion 2 is therefore split: automated half (scripted equality) closes autonomously, physical half closes on owner attestation.

## Q4. LibFuturo

What it is (V, `github.com/szTheory/libfuturo`, Apache-2.0): "experimental C interface for integrating console emulator cores with hosts"; only `synthetic-v1` profile is implemented; two commits on main (2026-11-01); branch `automation/phase3-browser-host` has work through 2026-11-10; no releases; no stable binary or plugin ABI; snapshots unsupported; installed/relocated consumer journey "remains Phase 4 work". Siblings (nesturbator NES, muhtari 7800, lynxplosion, cogmaster) are consumers or hosts; its NES seam doc is the pattern for an adapter evaluation.

Public header (V, `include/libfuturo/libfuturo.h`, 158 lines) vs Tendies64 needs:
| Axis | LibFuturo header today | Tendies64 | Fit |
|---|---|---|---|
| Content admission | `lf_create(profile_id, options)`; no ROM/cart load; only `lf_copy_media` of core-produced bytes | transactional ROM admission, profile selection | missing |
| Input | one `uint32 buttons` sampled once at advance start | timed per-tick stamped events, buttons + stick x/y, port 0 | packable (16+8+8 bits) but loses tick timing |
| Time | `work_units`, overshoot, `lf_time_unit` has one value | model ticks, frame events, instruction stops | needs new enum values |
| Video | `LF_VIDEO_GRAY8` only, fixed profile size | RGBA8 variable VI geometry | needs formats and per-call geometry |
| Audio | one fixed rate per profile, S16 | rational AI rate, packets | partial |
| Saves | in-memory media copy only; no durable save semantics | EEPROM/SRAM bytes, import/export | partial |
| Errors | 6 status codes | refused / guest-stop / defect-poison classes | coarser |
| Snapshots | unsupported | versioned state container | none |
Timing: the contract is moving (Phase 3 browser host in flight, Phase 4 installed consumer unbuilt), so any evaluation is a snapshot; pin by commit SHA. Do it after the Phase 11 freeze so it cannot shape the API. Minimal deliverable: one `docs/evidence`-style written evaluation (table above refreshed at the then-current SHA, decision "no adapter now" or "adapter optional, shape described", list of contract deltas as upstream feedback). Scope out: adapter code, build targets, CI, changes to the frozen header, Tendies64 acting as a LibFuturo conformance consumer, any claim that LibFuturo support exists.

## Q5. Adversarial pass: how Phase 12 balloons and bounds

- Audio sync: drift, underruns, resampling quality, latency. Bound: host drains the ring into an SDL audio stream at the profile's declared rate with one fixed-size slack; underrun policy is "play silence, count it"; no resampler quality work, no A/V sync claim, no latency number.
- Vsync/frame pacing: SDL vsync and refresh mismatch (NTSC ~59.94 vs 60/120/144 Hz). Bound: pacing is a pure function of an injected clock in host-pump; no vsync dependence; no frame-skip heuristics; correctness over smoothness. Core owns no clocks (API-04), so the host must supply one; enforce by the imported-symbol check in Phase 11.
- Input latency: sample at frame start, stamp `event_tick` at or after the completed tick (replay spec rule); no latency claim, no polling-thread tricks.
- Feature creep that must be refused up front: hotplug UX, remapping UI, config files, rumble/controller pak, ports 1-3, fullscreen/scaling filters, GPU path, save-state UI, rewind, host-side save persistence (headless already proves saves; if included at all, a single CLI-provided path with atomic write, otherwise omit).
- Real-time feasibility (I): never measured; the "playable" claim must not hide a slow interpreter. Bound as in Recommendation 9.
- Freeze risk: see Recommendation 8. Any host-discovered API defect must go through the compatibility policy with a recorded version bump, not an edit of the frozen header.
- Evidence-friendliness trap: a screenshot of the window proves nothing under this project's rules; the digest equality in Q3 is the claim.
- Raw capture limit (V, raw-capture.md): bundle default 64 MiB, max 256 MiB, and 320x240 RGBA frames are 307,200 bytes, so about 800 frames is the ceiling for a full capture; plan the headless title run to checkpoint digests instead of capturing long runs.
- D7: window title, binary names, bundle ids, icons and any `.app`/installer/store metadata carry no "Tendies64" product branding, logo or store-style listing. Use a neutral development title such as "t64 minimal host (development)" consistent with the existing `t64_` code prefix, no icon, no `.app` bundle, no installer (packaging is also D6). The repo name already exists; the constraint is public branding, so avoid putting the name in user-visible host strings and in the `--help` banner.
- D6: nothing here is a release; no tag, no packaged artifact, no binary redistribution (also keeps SDL notice duty at zero).
- D2 does not apply (no `nemu64-test` ROM involved); D5 not involved.

## Lens notes
- Game developer: assembly-only is the real cost of route A; mitigate by reuse and the scope cap.
- Emulator frontend dev: pump/shell split keeps nearly all host logic testable and the SDL part tiny.
- A/V pipeline: no sync, latency or fidelity claims; fixed policy, counted underruns.
- Legal: route A is clean; route B needs a binary-rights decision on libgcc/newlib/FatFs/lz4 contents of the ROM and a maintainer-less (archived) upstream.
- DevOps: SDL lane non-gating, optional target OFF by default, `act` for Linux, no new macOS minutes in the required gate.
- QA: independent oracle literals, wrong-input and refused-packet controls, equality between headless/installed/host-pump paths.

## Sources
- libdragon repo (archived, Unlicense): https://github.com/DragonMinded/libdragon , LICENSE: https://github.com/DragonMinded/libdragon/blob/trunk/LICENSE.md , toolchain script: https://github.com/DragonMinded/libdragon/blob/trunk/tools/build-toolchain.sh
- SDL repo and LICENSE.txt (zlib; release-3.4.20): https://github.com/libsdl-org/SDL , https://github.com/libsdl-org/SDL/blob/main/LICENSE.txt
- SDL licensing FAQ: https://wiki.libsdl.org/SDL3/FAQLicensing
- SDL virtual joystick: https://wiki.libsdl.org/SDL3/SDL_AttachVirtualJoystick
- SDL video driver hint: https://wiki.libsdl.org/SDL3/SDL_HINT_VIDEO_DRIVER ; audio hint: https://wiki.libsdl.org/SDL3/SDL_HINT_AUDIO_DRIVER ; hints list: https://wiki.libsdl.org/SDL3/CategoryHints
- LibFuturo: https://github.com/szTheory/libfuturo (README, include/libfuturo/libfuturo.h, docs/synthetic-profile.md, docs/nesturbator-seams.md)
- Repo files: AGENTS.md; .planning/AUTONOMY.md section 4; .planning/ROADMAP.md (D2-D7, Phase 12); .planning/REQUIREMENTS.md; docs/policy/rights-and-fixtures.md; docs/policy/copy-provenance.md; docs/spec/{rdp,vi,audio-interface,si-pif-controller,replay,raw-capture,cpu-timing}.md; scripts/build-pad-fixture.sh; fixtures/manifest.json; fixtures/{rdp,pad,audio,save,replay}/*.S; .planning/research/STACK.md line 40.
