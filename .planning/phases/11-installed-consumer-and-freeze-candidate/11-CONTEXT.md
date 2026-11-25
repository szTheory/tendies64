# Phase 11: Installed Consumer and Freeze Candidate - Context

**Gathered:** 2026-11-25
**Status:** Ready for planning

<domain>
## Phase Boundary

A clean, relocated installed package of the core is exercised by an independent consumer through every API-01 operation, and the API reaches a bound freeze CANDIDATE (0.x ABI). Requirements: VER-05, API-01, API-02, API-03, API-04. Order inside the phase: API shape changes land first and once; then installability; then the independent consumer, the call-sequence generator and the object checks; then the freeze-candidate record. The final ABI bind is Phase 12 (API-03 wording). No release, tag or packaging channel (D6); "mismatched version" means an ABI constant. No oracle material (D5). No host, SDL or title work (Phase 12).

</domain>

<decisions>
## Implementation Decisions

### Locked by REQUIREMENTS.md, ROADMAP.md and the v1.2 fan-out synthesis (not re-asked)
- Freeze is candidate-then-final: Phase 11 binds a 0.x candidate record (header hash, public symbol list, struct-layout table, status-class table); the final bind is the last plan of Phase 12; between the two only defect-driven amendments with evidence and a re-run of the installed-consumer suite — **Reversibility:** costly — the bound record is the contract every later check compares against
- Every in/out public struct carries a leading caller-filled `uint32_t struct_size`; the callee reads/writes at most `min(caller, known)` and refuses an input smaller than the v1 minimum with state unchanged; no `sType`/`pNext` chains — **Reversibility:** one-way — struct layouts cannot change additively after a bind
- `t64_machine_create` takes a create-info with `struct_size` and `abi_version`; mismatch returns a distinct status (ABI mismatch); a `t64_abi_version()` query and the build identity let a consumer assert header/library agreement (SQLite pattern) — **Reversibility:** one-way — first-call handshake is what makes later growth safe
- Three error classes with one status-class table listing every `T64_STATUS_*` and `T64_STOP_REASON_*` exactly once: refused (state unchanged, observations bit-identical), guest-stop (not an error, resumable), defect (instance poisoned: only destroy and a poison query succeed, outputs untouched, destroy always releases host allocator storage); poison is never set by a caller fault; a check fails if a status or stop reason is unclassified — **Reversibility:** costly
- Public `reset` and `unload` are new surface with accepted and refused cases; `bool` and `size_t` leave public structs (fixed-width types; LP64-only is not the answer); `extern "C"` guards added; copy-out ring semantics kept (core never hands out a pointer into core memory); `advance` never drops output and stops resumably at backpressure; host wall time is never an input — **Reversibility:** one-way
- Installability: fold the sha256 sources into `t64_core` (no private link to `t64_sha256`, which makes `install(EXPORT)` fail), relocatable `tendies64Config.cmake` plus version file with `SameMinorVersion` for 0.x, in-tree `tendies64::core` alias, package version is the ABI candidate number (not a release version, no tag, `project(VERSION)` untouched), config records the single built configuration and `T64_BUILD_ID`; internal symbols prefixed (`t64_` internal cross-TU helpers and the unprefixed Berkeley SoftFloat names via a small provenance-documented rename header); public API exported through one `T64_API` macro; a golden public symbol list is checked with `nm`
- Independent consumer is a separate top-level CMake project under `tests/installed-consumer/` that includes no runner or core-internal file, runs the Phase 10 composite fixture, and agrees three ways (installed consumer, in-tree runner, independent literals); the runner is NOT re-pointed at the installed package; a grep guard fails if a public-API consumer includes anything outside `include/tendies64/`
- Negative controls (all must fail loudly): missing package, version mismatch, visible source tree (compile database and link line), doctored header (wrong ABI constant or `struct_size`), symbol collision (consumer defines its own `f32_add` and `t64_sha256_init`; link succeeds and results unchanged), relocated prefix including a path with spaces, seeded `clock_gettime`/`fopen`/`getentropy`/`pthread_create` object
- Host-shaped loop in the installed consumer, no SDL: frame-paced bounded advances, drain video and audio rings in host order, forced backpressure stops, two instances interleaved in one process; the digest never depends on how or when the host drains
- Refusal-is-a-no-op harness: a deterministic call-sequence generator snapshots observations before each call and asserts bit-equality after any refused-class status; also proves a guest-stop resumes and a defect poisons; runs under ASan and UBSan against the shipped library
- API-04 enforced on the shipped objects: extract archive members, compute the undefined set at archive level (undefined minus defined), reuse `tools/lint/lint-objects.sh --objects`, scan installed headers with the L1-INCLUDE allowlist, scan disassembly for hardware clock/entropy instructions, and prove failure with seeded objects
- Order of work: shape changes once and early with a before/after proof that every existing digest and expected byte is identical; cheap-first checks (self-contained header compile as C99 pedantic and C++17, `_Static_assert` layout file against source then installed headers, `cmake --install` from the existing build tree, `nm` golden, object lint, absolute-path scan) before the long suite; relabel the 20 stale "Phase 10 freeze" mentions and the 19-20 deferred "until Phase 10" production comments in core/, runner/ and tools/ in the same wave as the shape change because they move the build id (done when `git grep -n -i -E 'until (the )?Phase 10' -- core runner tools` prints nothing)
- Phase 10 carry-ins are each judged by the freeze-candidate review and none blocks the install work: W2-01, W2-02, DEF-W1-01, W1-03, D-10-17-01 and the Phase 9 items (see ROADMAP.md); triage each as done, deferred or dropped with a recorded reason
- The paper LibFuturo contract read happens in this phase before the freeze candidate (input to API-07; the written evaluation is Phase 12); any adapter is optional and must not shape the core API
- Plans are capped at 4 tasks and about 45 minutes; no decimal plans; a defect fix is a new numbered plan; independent review per wave in scratch clones; parallelization stays false; `git.create_tag` stays false

### Decided autonomously
- [Installed headers] Q: "Which headers are public and installed?" → Selected: "only t64_machine.h (plus any new public header the shape change adds); t64_facade.h is the synthetic-guest runner header, stays in-tree and is relabeled, not installed, not in the freeze record" — the installed set is re-bindable until the Phase 12 final bind
- [Stack protector] Q: "How is the `__stack_chk_fail`/`__stack_chk_guard` import handled by the installed-object check?" → Selected: "keep the shipped build hardened and allowlist exactly these two symbols for the installed-archive check via an ADR-0006 amendment (compiler runtime, no host service, abort only); the lint's own compile stays -fno-stack-protector" — one allowlist entry plus an ADR note, reversible by building with -fno-stack-protector
- [Stop reasons] Q: "Classification of CPU_UNSUPPORTED, RESOURCE_LIMIT, QUIESCENT?" → Selected: "RESOURCE_LIMIT is a resumable guest-stop (backpressure); QUIESCENT is a guest-stop; CPU_UNSUPPORTED is a terminal guest-stop that neither poisons nor claims the guest chose to stop, and is documented as an emulator limitation; the status-class table is the single source and the planner may refine it against code observations" — table rows are re-bindable until the final bind
- [ABI status names] Q: "Names and numeric values of the new statuses (ABI mismatch, poisoned, new refusal codes)?" → Selected: "append after 33 without renumbering any shipped value; planner chooses names following the T64_STATUS_* convention" — numeric additions are cheap before the candidate is bound
- [Poison scope] Q: "Where does the poison flag live?" → Selected: "one flag inside the instance, excluded from the digest and the state container, set only on internal invariant violation; an in-call flag makes reentrant calls (allocator callback) a refusal" — internal data shape, not serialized
- [Output status] Q: "Add a ring status query?" → Selected: "yes, one `t64_machine_output_status` style call reporting per ring queued count, free capacity and next packet size, so a host paces without trial and error" — additive and re-bindable before final bind
- [Thread statement] Q: "Threading guarantee wording?" → Selected: "distinct instances are independent; one instance needs external serialization; checked by a two-thread, one-instance-each TSan consumer run in a non-gating or local lane" — documentation plus an optional test
- [Descriptor struct] Q: "How does `t64_profile_descriptor` freeze?" → Selected: "give it struct_size/version or move to getters; the planner picks the cheaper once it counts consumers" — pre-bind shape work
- [Package smoke] Q: "Second non-CMake consumer?" → Selected: "optional plain `cc` line plus a short `.pc` file, only if it costs under one task" — optional extra evidence
- [Hosted footprint] Q: "How much of the installed-consumer suite runs on hosted CI?" → Selected: "cheap-first checks plus the install-from-existing-build-tree path on hosted macOS and Linux (`act` locally first per AUTONOMY section 2); the fuller fresh-archive install variant runs in a manual or nightly lane; no new hosted timeout change" — ci.yml edits are reversible and the 15-minute macOS job limit is the constraint
- [Evidence tier] Q: "How are results recorded?" → Selected: "authored-scope, build-identity-bound evidence in a Phase 11 qualification record like Phases 8-10; the freeze candidate is a software-interface claim only, no compatibility or accuracy claim" — documentation
- [Review] Q: "Review cadence?" → Selected: "per wave in scratch clones plus one final independent audit before qualification, as in Phase 10" — process choice

### Claude's Discretion
- Exact new status and function names, the `T64_API` macro spelling and the internal symbol prefix, chosen against existing conventions
- Plan decomposition inside the stated order (estimate 12-16 plans; the shape-change wave and its re-qualification are the ballooning risk)
- Whether poison tests use an injected invariant-violation hook or a test-only trigger, and the exact call-sequence generator grammar

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `tools/lint/` (rules, allowlist, `lint-objects.sh --objects`, `tools/lint/selftest` seeded-violation corpus) is the base of the shipped-object check
- `runner/t64_real.c` is already a public-header-only client and the reference for the Phase 10 composite fixture; `scripts/qualify-phase10.py`, `scripts/gate.sh` and the Phase 9/10 qualifier pattern are the template for a thin Phase 11 qualifier
- Existing refusal snapshot concept in `docs/spec/machine-contract.md`, documented check orders for save import/export, and `core/tests/fuzz_state_import.c`

### Established Patterns
- Status is `uint32_t` constants (0 to 33 today), not an enum; size-then-copy-out with `BUFFER_TOO_SMALL`; opaque `t64_machine*` with host allocator callbacks; stop reasons are in the result struct (1 to 8), separate from status
- Build identity (`T64_BUILD_ID`) hashes listed inputs, so any production-source edit moves identities and pins keyed to them

### Integration Points
- Verified at this commit (167a084): no `install()` rules anywhere; `core/include/tendies64/t64_machine.h` has one `struct_size` mention, 26 `size_t` uses and no `extern "C"`; `t64_facade.h` has no `struct_size` and states "PROVISIONAL ... freeze is Phase 10"; the built archive `build/gate/core/libt64_core.a` has 293 global symbols (248 at the v1.2 fan-out) and still imports `___stack_chk_fail`, `___stack_chk_guard`, `_bzero` and the sha256 functions; the Phase 10 pin counts for stale "until Phase 10" comments need re-measurement by the stated `git grep`
- Three white-box probes (`t64_cold_boot.c`, `t64_rsp_diagnostic.c`, `t64_rsp_smoke.c`) and `runner/tests/test_cpu_diagnostic.c` include internal headers and stay in-tree
- ADR-0002 (language-reversal window closes once the API is frozen and consumed in v1.2) and ADR-0006 (lint mechanism) need amendments recorded with this phase

</code_context>

<specifics>
## Specific Ideas

Full findings, prior art (SDL3 stable-ABI preview, libretro API version rule, Vulkan loader negotiation, SQLite version assert, CMake packaging guidance) and open points are in `.planning/research/v1.2-fanout/lens-api.md` and `SYNTHESIS.md`; the researcher and planner must read both and re-verify mutable facts against the tree. Unverified in the lens: mupen64plus version-mask detail (second-hand), OpenXR negotiation (from memory), libretro serialization text (partial fetch). Check at plan time: whether `cmake --install` from the existing gate build tree works for the Development component without a rebuild, whether the Linux `act` job can run the cheap-first checks, and the real count of public `size_t`/`bool` uses after Phase 10.

</specifics>

<deferred>
## Deferred Ideas

- Final ABI bind to major 1, host pump, SDL3 shell, authored playable title, the written LibFuturo evaluation: Phase 12
- VID-04 differential oracle evidence (D5); any release, tag or channel (D6)
- abi-compliance-checker/libabigail and any new dependency: rejected for a ~30-function C API (three goldens give equal coverage)
- A libretro-style core wrapper or LibFuturo adapter: optional, must not shape the core API

</deferred>
