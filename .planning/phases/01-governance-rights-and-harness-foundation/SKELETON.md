# Walking Skeleton — Tendies64

**Phase:** 1
**Generated:** 2026-11-09
**Adapted:** this project has no database, UI or deployment target. The skeleton is the thinnest
end-to-end governance loop, not a web stack. The slots in the standard template map as follows:
"database read/write" becomes "canonical result written and read back", "UI interaction" becomes
"the maintainer runs one command", "deployment" becomes "CI runs the same command on hosted runners".

## Capability Proven End-to-End

The maintainer runs `./scripts/gate.sh`. It builds a stub core, the supervisor and the synthetic runner,
runs one positive and one negative control through the supervisor (each producing the canonical
`t64.result` JSON, compared byte-for-byte to an independently derived golden), runs the determinism lint
on the stub core and checks the ADR rules. CI runs that same command, and nothing else, on `ubuntu-24.04`
and `macos-26`.

Delivered in layers across waves 1-2, to respect the CONTEXT A-6 sizing rule (about 15 files per plan,
fewer than 10 per task):
- 01-01 (wave 1): gate, CMake/presets, core stub, `tools/common`.
- 01-02 (wave 2): supervisor, synthetic runner, the two golden controls.
- 01-03 (wave 2): two-layer lint wired into the gate.
- 01-04 (wave 2): ADR checks and the CI workflow.
The skeleton is complete when wave 2 finishes. Because `workflow.use_worktrees` is false, the plans of each wave run sequentially in plan-number order on one working tree (they share `build/gate` and `scripts/gate.sh`), never concurrently. The first hosted run happens in plan 01-13. Every later Phase 1
plan adds one vertical slice to this loop without changing the decisions below.

## Architectural Decisions

| Decision | Choice | Rationale | Source |
|---|---|---|---|
| Language (core, runner, tools) | C17, `CMAKE_C_EXTENSIONS OFF`, curated warnings, `-Werror` behind the `T64_WERROR` cache option | Native C ABI, flat tree, copy-with-provenance from ISC/MIT/BSD-3 C references. ADR-0002 records it as `proposed` until the pre-registered Rust spike (plan 01-08) decides it. A "switch" outcome stops and escalates to the owner (CONTEXT A-3) | CONTEXT D1 [owner-visible] |
| Build | CMake 3.25 floor, presets v6 (`gate` configure/build/test/workflow), Ninja, CTest with `noTestsAction: error` | Presets v6 is the first version with workflow presets. An empty CTest run is an error, not a pass | CONTEXT D1; RESEARCH (measured on 3.25.1, 3.28.3, 4.4.3) |
| Gate glue | POSIX sh + awk, `export LC_ALL=C`, `set -eu`, no pipefail, no `$SECONDS`, no GNU-only flags | Must behave the same on dash/mawk/gawk (Ubuntu) and bash 3.2/BWK awk (macOS) | CONTEXT; RESEARCH portability section |
| Structured checks | Small C tools in `tools/` sharing `tools/common` (SHA-256, UTF-8, canonical JSON writer, strict JSON parser) | No Python, jq or schema-engine dependency. Byte classification must be identical on both OSes | CONTEXT; RESEARCH "Don't Hand-Roll" |
| Determinism boundary | Only `core/` is scanned by the lint. `core/` never gets `_POSIX_C_SOURCE`. The host side (`runner/`, `tools/`) may use POSIX. Until ADR-0002 is accepted, `core/` holds exactly `core/t64_core_stub.c` and `core/include/tendies64/t64_facade.h`; the `t64_core` target is declared in the root `CMakeLists.txt`, and `DOC-CORE-BEFORE-ADR` rejects any other path under `core/` | AGENTS.md; CONTEXT D1 layout; CONTEXT A-1 |
| Core/runner seam | `core/include/tendies64/t64_facade.h`: an ops table with `advance(inst, bound_tick) -> {reason, tick}` and `drain_guest_log(inst, &tick, buf, cap) -> bytes`. It is provisional, not an ABI promise; the API freeze is in Phase 10. The synthetic guest implements it now. The real core implements it in Phase 3 | Phases 3 and 4 replay the same negative-control matrix against the real core through this seam | CONTEXT "Runner, outcomes and negative controls"; CONTEXT A-1 |
| Truth owner | `t64-supervisor` forks the runner in its own process group, applies a wall-clock backstop, never trusts the runner's exit code, classifies the outcome and writes the canonical result atomically | A killed or crashed runner still yields a classified result | CONTEXT; RESEARCH Pattern 3 |
| Result protocol | `t64.result` v1: one canonical JSON object per run (sorted keys, no whitespace, integers only, u64 as decimal strings, trailing LF). No wall time, hostname or paths. Six outcomes with precedence harness-error > missing-fixture > guest-fail > boot-fail > timeout > pass. Exit codes 0/1/10/11/12/13 | Byte-comparable across runs, slice sizes and operating systems | CONTEXT (locked) |
| CI | One workflow `.github/workflows/ci.yml`, two jobs that each run only `./scripts/gate.sh`, SHA-pinned `actions/checkout`, `permissions: {}`, `persist-credentials: false`, `fetch-depth: 0`, explicit runner labels | One canonical local command that matches the PR gate. Untrusted PR content gets no token and no secrets | CONTEXT; AUTONOMY section 2 |
| Decisions | `docs/adr/NNNN-slug.md` with MADR 4.0.0 headings, a mandatory `## Reversal evidence` section, and a front-matter `status` that matches the body Status line | Every decision can be reversed when its stated evidence appears | CONTEXT D1 |
| Directory layout | `core/` (`core/include/tendies64/`), `runner/`, `tools/` (`common/`, `lint/`, and one dir per checker, auto-discovered by the root `CMakeLists.txt` glob: `rights/`, `repocheck/`, `manifest/`, `provenance/`, `claims/`), `tests/` (`controls/`, `seeded/`), `fixtures/` (manifest only), `provenance/`, `third_party/` (empty until a copy is recorded), `docs/` (`adr/`, `policy/`, `spec/`, `claims/`, `ci-reviews/`, `spikes/`), `scripts/` | Flat tree. Each checker registers its CTest tests (labels `unit`, `controls`, `repo`, `gov`) in its own directory, so later plans add checks without editing `scripts/gate.sh` | CONTEXT D1 layout |

## Stack Touched in Phase 1 (adapted)

- [ ] Project scaffold: CMake + presets, curated warnings with `-Werror`, CTest, `.gitignore` for `build/` (01-01)
- [ ] One real write: the supervisor writes a canonical `t64.result` file (01-02)
- [ ] One real read: the run-case driver byte-compares it with a golden (01-02), and `t64-result-check` parses and validates it (01-07)
- [ ] One interaction: the maintainer runs `./scripts/gate.sh` and gets one pass/fail with per-step timings (01-01; lint steps 01-03)
- [ ] "Deployment": CI runs the same command on `ubuntu-24.04` and `macos-26`. Every completed CI run of the phase gets a recorded critical-path review, with the first run's review as the VER-04 minimum (workflow 01-04; runs and reviews 01-13)

## Out of Scope (Deferred to Later Slices)

- Any CPU, bus, boot, ROM-admission or device behavior in `core/` (Phases 2+). Phase 1's `core/` holds only the two CONTEXT A-1 paths. `scripts/check-docs.sh` refuses any other file under `core/` until ADR-0002 is `accepted`.
- A real profile or real build identity. Phase 1 results are always stamped `synthetic-stub` / `+synthetic` / `synthetic`, and the claims gate refuses them.
- Windows/MSVC canary, compiler matrix, caches, cross-OS digest comparison (CONTEXT deferred; Phase 3/4).
- Expanding the copy allowlist beyond ISC, MIT, BSD-3-Clause and Unlicense (owner decision D4).
- Any committed fixture bytes. The manifest holds only `pending` and `forbidden` entries.
- Workflow lockfiles in place of SHA pinning (re-check when generally available).

## Subsequent Slice Plan

Each later phase adds one vertical slice on top of this skeleton without changing its decisions:

- Phase 2: a real machine instance behind the facade (guest-time scheduler, named profile, memory map, DMA) with state digests equal between isolated and interleaved runs. `state_digest` stops being null.
- Phase 3: a hello ROM through HLE-PIF and a pinned open IPL3 under the real runner. The Phase 1 control matrix is replayed against the real core.
- Phase 4: the complete VR4300, with per-group `nemu64-test` status reported in `t64.result` suites under pinned inventories.
