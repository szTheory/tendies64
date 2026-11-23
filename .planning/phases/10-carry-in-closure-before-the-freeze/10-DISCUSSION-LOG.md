# Phase 10: Carry-in Closure Before the Freeze - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-11-23
**Phase:** 10-Carry-in Closure Before the Freeze
**Areas discussed:** none asked (all hard-to-reverse choices already locked)

---

## Locked upstream

Per AUTONOMY.md prompt dispositions, hard-to-reverse choices already locked in ROADMAP.md, STATE.md or the v1.2 fan-out synthesis are used as-is. Alternatives considered upstream (lens-phase10.md R3, R5, R6, R7):

| Question | Options considered | Chosen |
|----------|--------------------|--------|
| Depth activation | flip descriptors 11/12 in place; new registered ids | new ids (state v26, container v2) |
| RUN-07 shape | relax raw-capture exclusion; two runner runs with equal digest; C-ABI composite in Phase 11 | two runner runs |
| Oracle re-pin | per-milestone rebind; per-row pins | per-row pins plus standing identity-only rebind until landed |
| GATE-01 route | delete audio-records step; COST properties + rsp-no-hle guard removal + 15 min limit | COST + guard removal + 15 min with scanner rule |

## Decided autonomously

[auto] [Test layout] — Q: "Where do VG-01..05 and the NTSC clock vector tests live?" → Selected: "core unit tests, no manifest row" (reversible technical — decided autonomously: test files are cheap to move)
[auto] [Preflight] — Q: "How is the Phase 9 lesson replay packaged?" → Selected: "one scripted local preflight under about 90 s" (reversible technical — decided autonomously: repo-local script)
[auto] [Qualifier] — Q: "New qualifier or delta on Phase 9?" → Selected: "thin delta binding changed ids" (reversible technical — decided autonomously: tooling only)
[auto] [Fuzz] — Q: "Which importer bounds coverage for new members?" → Selected: "extend fuzz_state_import.c and field-use audit rows" (reversible technical — decided autonomously: test code only)
[auto] [Review] — Q: "Review cadence?" → Selected: "per wave in scratch clones plus final audit" (reversible technical — decided autonomously: process choice)
[auto] [Docs] — Q: "How are K1 and depth-silent docs updated?" → Selected: "same wave as the identity bump" (reversible technical — decided autonomously: documentation edits)

## Claude's Discretion

New profile id values, plan decomposition, optional make -j2 change.

## Deferred Ideas

VID-04 (D5), Phase 11/12 scope.
