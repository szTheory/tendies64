# Phase 11: Installed Consumer and Freeze Candidate - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-11-25
**Phase:** 11-Installed Consumer and Freeze Candidate
**Areas discussed:** none asked (hard-to-reverse choices already locked upstream; remaining choices decided under AUTONOMY.md)

---

## Locked upstream

Per AUTONOMY.md prompt dispositions, hard-to-reverse choices locked in REQUIREMENTS.md, ROADMAP.md or the v1.2 fan-out are used as-is. Alternatives considered upstream (lens-api.md sections 1-5):

| Question | Options considered | Chosen |
|----------|--------------------|--------|
| Freeze timing | freeze in Phase 11; candidate in 11, final bind end of 12 | candidate then final bind |
| Struct evolution | struct_size; sType/pNext chains; frozen fixed structs | leading struct_size |
| Package version policy | SameMajorVersion; SameMinorVersion for 0.x | SameMinorVersion, ABI candidate number |
| ABI drift check | abi-compliance-checker/libabigail; three goldens (symbols, layout asserts, header hash) | three goldens |
| Runner relationship | re-point runner at installed package; separate independent consumer | separate consumer, three-way agreement |
| Visibility | -fvisibility=hidden only; prefix internal symbols plus T64_API | prefix plus T64_API |

## Decided autonomously

[auto] [Installed headers] — Q: "Which headers are public and installed?" → Selected: "t64_machine.h only; t64_facade.h stays in-tree" (hard-to-reverse by the letter but re-bindable until the Phase 12 final bind — decided under AUTONOMY self-unblock rule)
[auto] [Stack protector] — Q: "How is the __stack_chk import handled?" → Selected: "allowlist two symbols for the installed check via an ADR-0006 amendment" (reversible: one allowlist entry; alternative -fno-stack-protector rejected as a hardening loss for input parsers)
[auto] [Stop reasons] — Q: "Classification of CPU_UNSUPPORTED, RESOURCE_LIMIT, QUIESCENT?" → Selected: "RESOURCE_LIMIT resumable, QUIESCENT guest-stop, CPU_UNSUPPORTED terminal guest-stop without poison" (table rows re-bindable before the final bind)
[auto] [ABI status names] — Q: "Numbering of new statuses?" → Selected: "append after 33, never renumber" (cheap before the candidate is bound)
[auto] [Poison scope] — Q: "Where does the poison flag live?" → Selected: "in-instance flag outside digest and container" (internal data shape)
[auto] [Output status] — Q: "Add a ring status query?" → Selected: "yes, additive" (re-bindable)
[auto] [Thread statement] — Q: "Threading wording?" → Selected: "distinct instances independent, one instance externally serialized" (documentation)
[auto] [Descriptor struct] — Q: "How does t64_profile_descriptor freeze?" → Selected: "struct_size/version or getters, planner picks" (pre-bind)
[auto] [Package smoke] — Q: "Second non-CMake consumer?" → Selected: "optional, one task max" (extra evidence)
[auto] [Hosted footprint] — Q: "Hosted CI share?" → Selected: "cheap-first plus install-from-existing-tree; fuller variant manual/nightly" (ci.yml edits reversible)
[auto] [Evidence tier] — Q: "How are results recorded?" → Selected: "authored-scope qualification record, interface claim only" (documentation)
[auto] [Review] — Q: "Review cadence?" → Selected: "per wave in scratch clones plus final audit" (process choice)

## Claude's Discretion

Exact new status/function names and macro spelling, plan decomposition, poison trigger mechanism and call-sequence grammar.

## Deferred Ideas

Final bind, host, SDL3, title and LibFuturo evaluation (Phase 12); VID-04 (D5); release channel (D6); libabigail; adapters.
