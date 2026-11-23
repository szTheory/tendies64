# Lens report: rolling roadmap for Tendies64 (v1.2 scope sanity check)

Date: 2026-11-23. Read-only on the repo. Nothing here edits planning files.

## 0. Evidence labels

- Repo observations: read from AGENTS.md, AUTONOMY.md, PROJECT.md, ROADMAP.md, REQUIREMENTS.md, v1.1 requirements, v1.1 audit, RETROSPECTIVE.md, docs/spec/rdp.md, docs/spec/boot-limits.md, docs/profile-limits.md.
- Primary-source checks done in this session (fetched today): n64brew RDRAM page (Expansion Pak, osMemSize at 0x80000318), n64brew PIF-NUS page (CIC/IPL2/IPL3 sequence), n64-systemtest README (MIT, deprecated in favor of nemu64-test, covers CPU/TLB/RSP, no RDP), parallel-rdp README (MIT, aims bit-exact with Angrylion-Plus, ~150 fuzz tests, vi-conformance, uses a modified Angrylion fork as reference), libdragon README (Unlicense, GCC+newlib toolchain, recommends ares), ares site (v148, N64 and 64DD listed as supported).
- NOT verified here, from background knowledge, marked "(to verify)": which commercial titles need the Expansion Pak; commercial microcode families (F3DEX/F3DEX2/aspMain/custom); that retail OS schedulers block on SP/DP interrupts; libdragon rspq/rdpq/mixer license and which libdragon init paths need RSP; ares/simple64 being LLE-RSP with ParaLLEl-RDP. Promote any of these only after a primary-source check.

## 1. Key findings that shape the roadmap

1. **Real-microcode RSP is the true gate to "a commercial title renders a frame".** A retail title can run CPU-side OS init without the RSP, but its first frame and its audio both depend on RSP tasks (graphics ucode feeding the RDP, audio ucode feeding AI), and the OS scheduler waits on SP/DP completion (to verify). Today the RSP is "functional, ROM-level tests; microcode compatibility unestablished" and Phase 8 explicitly avoided an RSP-produced-audio promise. So "boots" without RSP microcode is observable only as CPU state, not as a picture. Everything in the compatibility story sits behind: real-ucode RSP -> RSP-to-RDP command path (XBUS/DPC) -> RDP feature breadth -> VI.
2. **A rights-cleared real-microcode corpus exists:** libdragon (Unlicense repo) builds rspq/rdpq/mixer microcode from source; homebrew built with it exercises the same RSP-to-RDP and RSP-to-AI paths that retail uses, without any Nintendo bytes. License of the individual microcode files is not confirmed by the README (to verify per file before copying/building in CI). Nintendo's F3DEX2 etc. are protected: commercial-ucode evidence needs an owner rights decision (new D8 below).
3. **Boot of commercial-shaped ROMs does not need Nintendo bytes if done HLE.** PIF-NUS page: IPL3 initializes RDRAM, DMAs the first MiB of ROM to the header PC, checksums it, resets RSP/interrupts, jumps to entry. HLE can reproduce that post-state without running Nintendo IPL3. Known trap from the same page: "most emulators get the seeds wrong". Profile identity must carry the CIC variant and seeds (6101/6102/7101/7102 seed 0x3F; 6103/7103 0x78; 6105/7105 0x91; 6106/7106 0x85) and the game must answer PIF command 0x08 within about 5 s. Current repo state: open libdragon IPL3 only, 1 MiB ceiling on admitted image in the warm profile, 4 MiB RDRAM, no CIC challenge model (EXP-03). The 1 MiB number matches IPL3's first-MiB DMA, but retail ROMs run to 64 MiB, so PI-driven paging beyond the first MiB must work.
4. **Expansion Pak is cheap on the machine side but not a gate for most of the library.** RDRAM page: 4 MiB base +4 MiB; IPL3 probes/records size at 0x80000318 (0x800003f0 for CIC 6105); osGetMemSize probes at 1 MiB steps from 0xA0300000; RDRAM has a 9th bit used only by RDP/VI. The repo already models the register protocol for RDRAM A/B boot, so 8 MiB is mostly: map size, probe behavior, 0x318 value, a new profile id/state version. It needs a state-format change, so it must not land after the freeze without an additive-profile mechanism (see guardrail G1). It is also a hard gate for the subset of titles that require it (to verify list), so it belongs next to commercial boot, not before the API freeze.
5. **"Playable" needs a speed number, and nobody has measured one.** v1.0 retrospective: one private sweep took about 63 minutes; the scheduler quantum is one model tick in early profiles and "in the profile identity" later; the RDP is a software reference renderer. Phase 12 criterion "plays in the minimal host" has no real-time criterion. If the core runs at a fraction of real time, "plays" is hollow. Cheap fix: PERF-01 in minimal form (guest ticks per host second and a real-time factor on the Phase 12 title, reported not gated). PERF-02/03 stay later.
6. **The RDP oracle is the biggest accuracy multiplier but sits behind D5 and a rights audit.** parallel-rdp is MIT and designed to match Angrylion bit-for-bit via conformance fuzz suites and dump replay; it ships a modified Angrylion fork as its reference (that part is the audit risk). Authored independent expectations (the project's current style) cannot scale to the RDP's mode space; without an oracle or hardware-derived vectors, RDP claims stay at "authored scope" forever. Note the ecosystem baseline: libdragon's own docs say ares is the only emulator that accurately emulates the hardware, and ares wants a Vulkan GPU. A deterministic software-RDP core is a real differentiator but also means ParaLLEl-class speed is out of reach without PLAY-02.

## 2. Proposed rolling roadmap

Numbering note: AUTONOMY section 3 says milestone numbers and software versions are separate. Keep planning ids sequential (v1.3, v1.4, ...) and drop "v2.0" as a name for an unscheduled grab bag; reserve the label for "first release-candidate milestone" only if the owner wants semver alignment (D6).

### NEXT: v1.2 Embeddable and Playable (Phases 10-12), as defined, with three tightenings

Goal unchanged. Suggested adjustments (planner decides; none change requirement ids):
- A1. Move the paper-only LibFuturo contract read (API-07 research half) into Phase 11 research, before the freeze. A fit evaluation after the freeze can only discover a mismatch that costs an ABI break; reading a contract does not let it shape the core API. The adapter itself stays optional and later.
- A2. Add to API-01..03 an explicit extensibility rule so the freeze survives v1.3/v1.4: versioned struct sizes or size fields, extensible enums with a reserved "unknown" policy, profile and device selection by name/ID with additive registration, additive error subcodes inside the three classes, ABI/header version query. Freeze = "stable ABI v1 with additive extension rules", not "never changes". Without this, EXP-01, CIC profiles, FlashRAM and 64DD each force a break.
- A3. Phase 12: classify the playable title's subsystem needs in research. If it only uses CPU-drawn framebuffer, VI, controller and CPU-written AI PCM (authored route), it fits. If the libdragon route pulls in rspq/rdpq/mixer, it silently needs v1.3's real-microcode work (to verify per libdragon init path); then the libdragon route is v1.3, and v1.2 keeps the authored route. Also add "report real-time factor on the title headless and in host (no threshold)" to success criterion 2.

Promotion evidence to close: GATE-01 inside limit, installed-consumer negative controls, freeze record, playable title in host with scripted-input equality to headless.

### NEAR: v1.3 Real Microcode (proposed 3 phases)

Goal: RSP microcode compatibility proven on rights-cleared real microcode, driving the RDP and audio through the actual RSP-to-device paths, with a first performance baseline and the Expansion Pak profile.
- Phase A: RSP-to-RDP command path (XBUS/DPC start/end/current/status semantics) and RSP-produced audio into AI, exercised by libdragon rspq/rdpq/mixer built from a pinned toolchain (per-file license check first; build in a cached/scheduled lane, not the required gate). Evidence: authored expected pixels/samples independent of the producer, plus RSP vector ISA coverage from PeterLemon tests (Unlicense; verify per case, no wholesale clone).
- Phase B: PERF-01 (named workloads, output held constant, real-time factor, throughput vs latency separate from CI time) and EXP-01 Expansion Pak 8 MiB profile with libdragon `get_memory_size`-style probe tests and the 0x318 value.
- Phase C (conditional on D5): VID-04 oracle conformance, out-of-tree, labeled. Prerequisites: D5 yes, parallel-rdp per-file audit, exported corpus authored by us (command streams we write, not game-derived), replay adapter (Plan 07-13). If D5 is denied or the audit fails, Phase C becomes ACC-02: hardware-derived public vectors for RDP/VI where a primary source exists. Never empty-run it.
Promotes from: v1.2 frozen API (profile registration is additive), Phase 12 title working, audit evidence per microcode file.
Why before commercial boot: without real-ucode RSP there is no frame to judge, and VID-04 is most valuable once the RDP is fed by real command streams.

### MID: v1.4 Commercial-Shaped Boot and Private Compatibility Lane (proposed 3 phases)

Goal: boot retail-shaped ROMs (up to 64 MiB, all common CIC variants) via HLE-IPL3/PIF seeds, with compatibility measured in a private local lane and reported per named profile and evidence tier.
- EXP-03 CIC variants as separate named profiles (6101/6102/6103/6105/6106 families; seeds, 0x08 command and watchdog; 6105 challenge model last, it is the hard one), ROM byte-order handling and large-ROM PI behavior. Self-check: cross-check HLE post-boot state against the documented per-CIC post-boot register tables (public hardware-derived, ACC-02-flavored), not against another emulator.
- Save breadth that retail needs: FlashRAM (EXP-04), Controller Pak (EXP-05a). Rumble/Transfer Pak and RTC later (EXP-05b/c, EXP-06).
- Private compatibility lane (needs D8): owner-supplied commercial dumps run locally; results recorded as hash-keyed, locally reproducible records with tier labels (T0 reaches entry; T1 first stable VI frame; T2 attract/title stable for N frames; T3 scripted input reaches a named state). Public statements limited to "tested locally by the owner on profile X; not reproducible by third parties", never a compatibility list.
- PERF-02 (decode cache, RSP decode cache) promoted here when PERF-01 plus the private lane show iteration time or real-time factor as the bottleneck.
Promotes from: v1.3 real-ucode evidence, D8 granted, D6 decided (so results can feed a release).

### LATER (v1.5-v1.6, order by evidence)

- Accuracy: ACC-01 cycle-accurate CPU timing and CP0 hazards, PI/SI/RSP arbitration timing; n64-systemtest-style timing/cop0hazard feature sets (note nemu64-test is the maintained successor; D2 governs public use). Promote when a primary timing source exists and the private lane shows titles failing on timing. Hardware-derived vectors only from public sources unless the owner approves a hardware lane (see D9).
- EXP-02 PAL/MPAL (needs DACRATE/VI timing evidence), EXP-05b/c, EXP-06 RTC.
- PERF-03 block JIT last, only with lockstep digest equality against the interpreter and only if the cached interpreter cannot reach real-time on the reference workloads. A JIT is a large determinism risk for a project whose claim is determinism.
- PLAY-01 save-state UI/rewind/fast-forward (state export/import already exists, so this is a cheap host-side win once the minimal host exists; could be pulled into v1.3 as an optional slice), PLAY-03 GDB stub over the trace seam (cheap, high developer-value, matches the "observable and scriptable N64" positioning; reasonable v1.3/v1.4 filler if capacity allows).

### LONG-TERM (v3 and beyond)

- EXP-07 64DD as a separate system profile, only with a rights-cleared disk route (blocks on rights, not engineering).
- PLAY-02 optional GPU renderer as host presentation (never feeds guest state). Ecosystem note: ParaLLEl-RDP exists; do not duplicate it unless the host-side use case is clear.
- LibFuturo adapter (if the fit evaluation says yes), public CI with external diagnostics (D2), broader multi-title compatibility documentation.
- Netlify-style distribution channels, store listings, branding: only after D7.

### Dependency sketch

v1.2 frozen API (additive rules) -> v1.3 real microcode + PERF-01 + EXP-01 (+ VID-04 if D5) -> v1.4 CIC/HLE boot + private lane (D8) + saves -> PERF-02 -> ACC-01 / PAL / RTC -> PERF-03 -> 64DD / GPU / adapters.

## 3. Questions asked

(a) Next 3-4 milestones: v1.2 (as is, tightened), v1.3 Real Microcode, v1.4 Commercial-Shaped Boot and Private Lane, v1.5 Accuracy and Timing (with PERF-02/03 and PAL/RTC as evidence dictates). Promotion evidence is listed per milestone above. One-line goals:
- v1.2: frozen, extensible C-ABI proven by an installed consumer, plus a cleared playable title.
- v1.3: real cleared microcode drives RDP and AI; first performance baseline; Expansion Pak; oracle conformance if D5.
- v1.4: retail-shaped boot via HLE per CIC profile; FlashRAM and Controller Pak; private commercial lane with tier labels.
- v1.5: timing accuracy and speed (ACC-01, PERF-02/03, PAL), each promoted by a named measured gap.

(b) Pull earlier / push later versus the current v2.0 outline:
- Pull earlier: PERF-01 (measurement only, v1.2 criterion or v1.3 phase B; it is cheap and sets expectations for everything downstream); VID-04 right after real-ucode work (v1.3, if D5); EXP-01 to v1.3 (cheap, cleared test via libdragon memory probes); PLAY-01 (state export exists) and PLAY-03 (trace seam) if capacity remains; LibFuturo paper read into Phase 11.
- Push later: ACC-01 (large, needs primary timing source, only a minority of titles need it, to verify), PERF-02 until measurement and the private lane justify it, PERF-03 last, PAL/MPAL, RTC, Transfer Pak, 64DD, PLAY-02.
- Answers to your three explicit questions: yes, RSP microcode compatibility gates anything beyond CPU-side init for commercial titles; Expansion Pak is a cheap machine-side win but should ride the v1.3 state-version change, not precede the freeze; performance matters for "playable" claims (Phase 12) and for iteration time on commercial-sized sweeps, but only measurement (PERF-01) is needed early, optimization follows measured bottlenecks.

(c) Owner decision batches (autonomous runs can proceed on Phases 10-11 while these are open; none of D2/D4/D5/D6/D7 blocks Phase 10 work):
- **Batch 1, ask now (v1.2 start or before Phase 11 closes):** D6 release channel and versioning (this is the one that stalls every milestone close, because AGENTS.md requires targeting a release at close); D4 allowlist extension; D5 local-oracle authority; D7 name posture.
  - D6 recommended default to offer: GitHub Releases from the exact tested CI artifact, pre-1.0 SemVer (0.MINOR per milestone, planning ids unchanged), source tarball plus CLI/runner binaries, no package-manager channels yet.
  - D4 recommended: add BSD-2, zlib, MIT-0 and Apache-2.0 sibling code to the copy allowlist with notices; keep GPL/LGPL/MPL/Angrylion-Plus legacy license out. Unblocks SDL3 notices, libdragon-adjacent code and future small helpers in one answer.
  - D5 recommended: approve local, out-of-tree oracle runs of parallel-rdp (after the per-file audit) with derived data staying local and never in public CI; Angrylion-Plus as a local reference only, never copied. This is the project's existing default plus a go/no-go for running it, so it is a one-line yes/no.
  - D7 recommended: ask for a posture, not clearance: either "ship 0.x under the working name with a stated knockout-search disclosure" or "rename to a neutral codename before the first release asset". Do a public trademark knockout search first (agent task, not legal advice). Everything but first public branding proceeds meanwhile.
- **Batch 2, ask at v1.2 close, before v1.3 planning:** D8 (new) private commercial-ROM lane: may the agent run owner-held legally dumped commercial ROMs locally, with hash-keyed local-only results, nothing committed or uploaded? Without D8 the v1.4 compatibility story cannot start. D2 (nemu64-test in public CI): recommend deferring unless the owner wants to contact the author; the authored hello already serves as the permanent public smoke lane, and D2 only adds value once public result reporting exists.
- **Batch 3, ask at v1.3 close:** D9 (new, optional) whether the owner will ever run public test ROMs on their own console/flash cart to supply hardware truth, which is the single biggest evidence-tier jump available and today is a physical-measurement stop. A "no" is a fine answer; it fixes the evidence ceiling for ACC claims at "documented source".
- Rule: each batch is one message with a default and a consequence per item. Never ask the same item twice; record the answer in PROJECT.md Key Decisions.

(d) Where the roadmap can mislead, and guardrails to carry:
- Overclaiming: (1) "frozen API" with no external consumer yet; (2) "playable" from homebrew while real-time factor is unknown; (3) "reference renderer" read as Angrylion parity; (4) commercial "boots" from private results; (5) HLE boot success read as IPL2/PIF fidelity; (6) oracle agreement read as hardware truth (Angrylion itself is a model); (7) per-CIC claims inheriting from a single CIC test.
- Evidence debt: Nyquist validation not run for phases 7-9; stale verification digests; docs drift items; open VG gaps. Carry-ins are already 13-requirement scale. Guardrail G4.
- CI cost: gate-macos is 349-673 s against a 600 s limit before any new work; libdragon needs a GCC+newlib toolchain build (heavy), SDL3 adds a host build on both OS, oracle lanes need Vulkan-class or large builds. Public repo means Actions minutes are free for standard runners, so the cost is wall time and maintenance, not dollars, unless a larger runner or private visibility changes that (paid-service stop applies).
- Capacity: v1.1 Phase 9 had 13 plans of 40-95 min each; every plan exceeded the token estimate; the 63-minute private sweep and 600 s wrapper-limit failure show host-time pressure. The real scarce resource is owner decision bandwidth and reviewable evidence volume, not agent wall-clock.
- Staleness: ecosystem facts (ares v148, libdragon GCC 16, parallel-rdp license text) change; recheck at each promotion.

Guardrails the rolling roadmap should carry (as a short block under the horizons):
- G1. Additive-extension rule: nothing after the freeze may require an ABI break to add a profile, device, expansion, error subcode or output type.
- G2. Every item in NEXT/NEAR names its promotion evidence and its blocking owner decision; items in MID/LONG name only the evidence class, not dates or phase counts.
- G3. No horizon item claims compatibility; compatibility language exists only as per-profile evidence tiers, and private-lane results never appear as public support claims.
- G4. Debt budget: at most 5 carried warning-level gaps entering a milestone; any carry-in older than two milestones is closed or moved to Out of Scope with a reason; run the Nyquist/validate pass once per milestone, not per phase.
- G5. CI budget: the required gate wall time may not grow across a milestone without removing equal time; heavy toolchains (libdragon GCC, oracle, SDL3 host) live in cached/scheduled/optional lanes with pinned hashes; record the measured gate time in the milestone close.
- G6. Capacity budget: at most 3 phases per milestone, at most about 10 plans per phase, one owner-decision batch per milestone, and conditional phases (like VID-04) explicitly marked conditional with a named fallback.
- G7. Independent expected values: no new evidence derived from the producer under test; oracle output never stands in for authored expectations or hardware.
- G8. Rights first: new corpora (libdragon microcode, PeterLemon cases, SDL3) get per-file rights rows before first use; commercial content only under D8.

(e) New-milestone ritual (about 15 lines):
1. Close audit first: milestone audit, tech-debt list, Nyquist/validate pass done once; main green; PRs triaged.
2. Record measured costs: required-gate wall time (cold and warm), longest test, Hosted failures; compare with last milestone and with the G5 budget.
3. Carry-in review: list every carried gap with its age; close, schedule or move to Out of Scope with a reason (G4).
4. Extract lessons into AUTONOMY.md and RETROSPECTIVE.md; delete lessons that duplicate existing ones.
5. Re-verify mutable facts the next milestone depends on (tool versions, upstream licenses, specs) from primary sources; mark the date.
6. Refresh the rolling roadmap: NEXT detailed (goal, phases, requirement ids, success criteria), NEAR (goal, promotion evidence, blocking decision), MID/LONG (evidence class only); re-rank by value per risk; state what moved and why.
7. Check the horizons against the ABI/state/profile compatibility rule (G1) and the rights table (G8).
8. Build the owner-decision batch for this milestone: only decisions that block a NEXT or NEAR item, each with default, recommendation and consequence; ask once; record answers in Key Decisions; keep a "decided, do not reopen" list.
9. Confirm the release path (D6/D7 status) so the close can ship from the exact tested artifact, or record the blocker, owner and next action.
10. Set capacity limits for the milestone (phases, plans per phase, conditional phases and their fallback).
11. Name the cheap consistency checks and the first real-consumer smoke test for each phase before long suites.
12. Update PROJECT.md (core value check, out-of-scope audit), REQUIREMENTS.md (traceability), ROADMAP.md (horizons and decision gates), STATE.md (position).
13. Run the privacy/rights scan on all planning edits before commit.
14. Commit the refresh as one planning commit before phase planning begins.

## 4. Verification of claims against the repo (selected)

- RDP contract doc names only descriptors up to composite v6 and says full RDP support and hardware accuracy remain unqualified; unimplemented triangle packet forms and unsupported cycle types are refused, Z variants refused in some profiles; Z inactive on descriptors 11/12 (K1).
- Boot limits doc: warm profile admits only the pinned open IPL3 route, up to 1 MiB, 4 MiB RDRAM, no cold PIF, no cold boot or broad compatibility claim.
- Phase 8 state note: "Authored PCM proves transport without a new RSP microcode promise" (so no RSP-produced audio evidence exists yet).

## 5. Open risks in this report itself

- Titles requiring the Expansion Pak, microcode families, and libdragon init paths are from background knowledge; verify before they drive scope.
- The proposed v1.3 phase content assumes the XBUS/DPC path is not already complete; check docs/spec/rsp-profile.md and rdp.md before planning.
- Release-channel, D5 and D7 defaults are recommendations, not legal advice; the owner decides.
