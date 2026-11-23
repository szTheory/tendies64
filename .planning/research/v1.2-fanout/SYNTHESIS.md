# v1.2 fan-out synthesis (2026-11-23)

Five read-only lenses (Phase 10 scope, C-ABI/freeze, host and homebrew, rolling roadmap,
adversarial process) reviewed the first v1.2 roadmap. Full reports sit beside this file
(`lens-*.md`). Planners should read them; this file records what changed and why.

## What the fan-out changed

1. **CORE-06 premise was stale.** Plan 09-10 already carries the RDP combiner block and coverage
   table in section 8 (docs/spec/state-container.md "Hidden RDP members"). Remaining work is the
   version marker, depth members, and a round trip that includes depth. VID-07 and CORE-06 are one
   identity/state bump.
2. **No in-place depth flip.** New registered profile ids keep the shipped ids' digests and Z
   refusal byte-identical (project precedent: docs/spec/rdp.md, cpu-compatibility.md).
3. **Phase 10 order:** GATE-02 (per-row oracle pins) and GATE-01 first because ci.yml,
   check-ci-yml.sh and gate.sh are bound source; then VER-06; then VID-07+CORE-06; then RUN-07.
   RUN-07 cannot report raw video, audio and save in one run (raw capture excludes the Phase 9
   producers), so it is two runs with equal final digest; no contract relaxation before a freeze.
4. **GATE-01 reworded:** the audio-records step produces records the emit step consumes, so only
   the redundant rsp-no-hle guard goes; success is a measured cold run, timeout-minutes 15 with the
   scanner rule changed, CTest COST properties, and before/after timings (349-487 s noise).
5. **Freeze split.** Phase 11 ends in a bound freeze candidate with a host-shaped loop in the
   installed consumer; the final bind is at the end of Phase 12 (prior art: SDL3 stable-ABI
   preview, Vulkan loader negotiation). Phase 11 research raised to DEEP (no install() rules, no
   reset/unload/poison/status classes, 248 global symbols, `__stack_chk_*` undefined).
6. **Phase 12:** route A authored title only; pure-C host pump unit-tested in the gate, SDL3 shell
   optional and non-gating (system install, never vendored: zlib is not on the D4 allowlist);
   physical controller is an owner-attested non-blocking checklist; LibFuturo is a written
   evaluation (v1 profile is synthetic, no admission or error classes) with the paper read in
   Phase 11.
7. **Rolling roadmap** now has horizons (v1.3 Real Microcode, v1.4 Commercial-Shaped Boot and
   Private Lane, v1.5+), guardrails G1-G7 and decision batches; see ROADMAP.md.
8. **Config for unattended runs:** auto_advance, code_review, ui_phase, ui_review,
   parallelization and git.create_tag set false (project-owned independent review is a mandated
   final wave per phase; create_tag would violate D6).

## Unverified items to check during planning

- Which retail titles need the Expansion Pak; commercial microcode families; per-file license of
  libdragon rspq/rdpq/mixer microcode (v1.3).
- SDL3 dummy-driver value strings and headless gamepad init on both hosted runners.
- XBUS/DPC status in docs/spec/rsp-profile.md and rdp.md before planning v1.3.
- Whether lens estimates (12-18 plans for Phase 10) hold once VG vectors expose real defects.
