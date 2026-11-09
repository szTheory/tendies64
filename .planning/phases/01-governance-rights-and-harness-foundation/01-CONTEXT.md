# Phase 1: Governance, Rights and Harness Foundation - Context

**Gathered:** 2026-11-09
**Status:** Ready for planning
**Method:** Four parallel decision-cluster researchers (A language/build, B runner/ADRs,
C rights/provenance, D lint/CI), then orchestrator synthesis with an adversarial pass,
per `.planning/AUTONOMY.md`. No owner prompts: every choice is reversible and records
its reversal evidence. Items marked **[owner-visible]** are surfaced to the owner but do
not block work.

<domain>
## Phase Boundary

Before any emulator code exists, the maintainer can run one gate command that enforces the
provenance, rights and determinism rules, plus a runner skeleton that classifies every
outcome correctly using a synthetic guest. Phase 1 delivers ADRs, policies, schemas,
tooling and CI. It delivers no CPU, bus, boot or ROM behavior (Phases 2+).

Requirements: GOV-01..07, RUN-02, RUN-04, PROF-07, VER-04.

</domain>

<decisions>
## Implementation Decisions

### Language, build and layout (D1) [owner-visible]
- **C17 core and runner**, `CMAKE_C_EXTENSIONS OFF`, `-Werror`, CMake 3.25 floor, Ninja, CTest.
  Reasoning: native C ABI, flat tree, copy-with-provenance from ISC/MIT/BSD-3 C references.
  The sibling-repo convention is a preference, not evidence (AGENTS.md).
- Gate and CI helper scripts are **POSIX sh + awk**; structured checks (manifest validation,
  inventory, outcome classification) are **small C tools** in `tools/`. No Python dependency.
- Layout: `core/` (with `core/include/tendies64/`), `runner/`, `tools/`, `tests/`, `fixtures/`
  (manifest only, no binaries unless cleared), `third_party/` (per-copy provenance),
  `docs/adr/`, `scripts/`. The `core/` directory is the determinism-lint scan boundary.
- ADRs: `docs/adr/NNNN-slug.md`, MADR 4.0.0 headings plus a mandatory **Reversal evidence**
  section and a Status line.
- **Time-boxed Rust spike** before Phase 1 exits: implement only the determinism lint
  equivalent and a ROM-admission size/header parser in Rust (no crates), then compare
  against the C seeded-violation results. Pre-registered switch criteria: switch D1 to Rust
  only if (a) the C lint cannot catch a seeded leak class that Rust rejects at compile time,
  or (b) fuzz/ASan finds a memory-safety bug in the C admission parser during the phase.
  Otherwise D1 stays C17. Spike code lives in `docs/spikes/` or a scratch branch, not in the
  shipped tree.
- Reversal window: nearly free until core code merges; soft-open through Phase 2 (test
  vectors, digests and manifests are language-neutral); effectively closed once the C API is
  frozen and consumed (v1.2).
- Matrix to prove PROF-05 later: ubuntu-24.04 gcc+clang and macos-26 Apple clang. Windows/MSVC
  is an advisory canary, deferred. Digest-portability rules fixed now: fixed-width types,
  explicit endian serialization, no hashing of struct padding, no host floating point.

### Runner, outcomes and negative controls
- **Result protocol:** one canonical JSON object per run (UTF-8, LF, sorted keys, integers
  only; u64 values as decimal strings), `schema="t64.result"` plus integer `schema_version`.
  Fields: profile_id, profile_hash, build_id, fixture_id, fixture_digest, evidence_tier,
  outcome, detail (closed enum), stop_reason, guest_tick, counts, log_digest, state_digest
  (null until Phase 2). No wall time, hostname or paths in the canonical object; those go in a
  non-canonical `run-meta.json`. Consumers reject unknown major versions and unknown fields.
  A suite is JSON Lines of results ending in a `suite` record (case count, inventory hash); a
  missing suite record is `harness-error`.
- A small **supervisor process** writes the result and derives the exit code, so a killed or
  crashed runner still yields a result. Raw ISViewer capture is a separate bounded artifact.
- **Six outcomes, precedence order (first match wins):** harness-error, missing-fixture,
  guest-fail, boot-fail, timeout, pass. Pass only on a positive match.
- **Wall-clock kill is `harness-error` with `detail=wall-backstop`** (overrides the research
  default of `timeout`). `timeout` means only "guest-time bound reached". Reason: negative
  controls assert `timeout` for spin-forever guests, so a hung core must not satisfy them.
- **Empty output is `boot-fail`** (RUN-03), whether the stop was the bound or a guest halt.
- Exit codes: pass 0, harness-error 1, guest-fail 10, boot-fail 11, timeout 12,
  missing-fixture 13. Any other code (including 134/139) is harness-error. The supervisor
  never trusts the runner's own exit code.
- **Negative controls:** a scripted synthetic guest (`t64-runner-synthetic`) behind the
  facade the real core will implement: `advance(bound) -> {stop_reason, tick}` and
  `drain_guest_log() -> (tick, bytes)`. Scripts are `(tick, bytes)` lists plus a terminal
  directive (`quiesce`, `spin`, `hang`, `abort`, `segv`, `exit0-no-result`). Synthetic
  results are always stamped `profile_id=synthetic-stub`, build id `+synthetic`,
  `evidence_tier=synthetic`, and the claims gate refuses them as support evidence. CI asserts
  the exact expected outcome per case (not merely non-zero): positive control, wrong counts,
  failure after Done, deleted fixture, tampered digest, truncated verdict, non-UTF8/over-long
  output, spin, empty output, abort/segv/exit0-no-result, hang, dropped inventory case,
  truncated suite. Every case runs twice and canonical bytes are compared; one run uses a
  different slice size. Phases 3 and 4 must replay the same control matrix against the real core.
- **CPU timing-tier ADR:** adopt T0 (ordering only), T1 (per-class cost table with a named
  version; first profile), T2 (pipeline/hazard accuracy; deferred, ACC-01). Tiers are never
  named after test groups; `timing`, `cycle` and `cop0hazard` results are separate evidence
  fields. Profile identity includes tier and cost-table version. Whether T1 can pass the
  `timing` group is unknown.
- **Emulator-extension opcode ADR:** hardware profiles implement no nemu64-test emux
  semantics (COP0 CO funct 0x20/0x25/0x2c). No emux profile in v1. Completion is the ISViewer
  `Done!` line plus the guest-time bound. The runner tolerates the resulting
  "unhandled exception" lines and counts them (`unhandled_exception_lines`). The reserved
  instruction behavior is a placeholder (inference, unverified against the VR4300 manual);
  Phase 3 validates it on day one.

### Rights, provenance and claims
- **Fixture manifest:** `fixtures/manifest.json` plus `fixtures/manifest.schema.json`. Entry
  fields: id, source (repo + 40-hex SHA), build (recipe + toolchain pins), sha256, size,
  profile, `source_rights` and `binary_rights` (each `authored-here | cleared | pending |
  forbidden`; `pending` is the default), `distribution` (list of `commit | artifact |
  build-only | local-only`), rights_evidence, notices, `expected` {kind, source, revision}
  with kind `self-checking | spec-derived | hardware-derived | differential |
  regression-pin`. `forbidden` entries record a reason and carry no digest, so the manifest
  never becomes a list of protected-file hashes. Bytes may exist in the tree or an artifact
  only when `binary_rights` is `authored-here` or `cleared` and `distribution` includes that
  channel.
- **GOV-07 check:** default-deny any non-text blob (NUL byte in first 8 KiB or invalid UTF-8)
  not listed by manifest `{path, sha256}` or a docs-asset allowlist; ROM magic bytes at
  offset 0 regardless of extension (`80371240`, `37804012`, `40123780`; format facts, not
  protected content); denied extensions (`.z64 .n64 .v64 .sav .eep .sra .fla .mpk .ndd
  .d64`); opaque containers rejected unless manifest-listed (never recurse into untrusted
  archives); advisory flags for large numeric byte-literal runs and the 4032-byte size.
  Scope: working tree, every blob added across the PR commit range, and the artifact staging
  directory before any `upload-artifact`. **No protected-file hashes are committed**; a
  maintainer may keep an optional local-only denylist outside the repo.
- **Copy policy (D4):** the allowlist stays exactly ISC, MIT, BSD-3, Unlicense, as written in
  GOV-02. **[owner-visible]** Candidate additions BSD-2, MIT-0/0BSD, zlib and Apache-2.0 are
  documented as "pre-vetted, not yet allowed", pending an owner decision. MPL-2.0 stays out.
  Per-copy record: SPDX header in the copied file (REUSE 3.3) plus `provenance/copies.json`
  (paths, upstream repo/path, 40-hex SHA, retrieval date, SPDX id, license text path, verbatim
  notice, verbatim|adapted, modifications, independent-verification test id,
  `maintenance_owner` as a GitHub login or role (never an email), upstream_status, exposure
  log). The check fails on a missing SHA or owner, a non-allowlisted SPDX id, or a
  copied-looking file with no entry. For an archived upstream the owner is Tendies64.
- **Reading rule (ratified):** a flat ban on reading source of GPL, MAME-licensed or unlicensed
  emulator code while implementing. Allowed: black-box runs, docs, issues, wiki, and reading
  permissive code (ares ISC, cen64 BSD-3). Accidental exposure goes in the `exposure` log and
  a reviewer may require re-derivation from tests. **[owner-visible]** This is stricter than
  the research's clean-room allowance; it can be relaxed later with counsel advice.
- **Claims and evidence tiers (GOV-04):** an unordered tier enum: `smoke`,
  `regression-pinned`, `unit-tested`, `self-checking-fixture`, `emulator-differential`
  (names emulator and revision), `hardware-derived` (named capture provenance), plus the
  reserved `synthetic`. The words "accurate", "compatible" and "supports" are reserved for
  `hardware-derived` with an explicit scope. Sentence template: "Tendies64 `{build-id}` on
  profile `{profile-id}`: `{tier phrase}` for `{fixture set @ manifest digest}` (`{pass}/{total}`;
  missing, skipped, timeout, harness-fail, guest-fail counts). Oracle: `{kind + revision}`.
  Not claimed: `{exclusions}`." A check validates ids, tier and reserved words.

### Determinism lint (PROF-07) and CI
- **Two layers**, POSIX sh + awk, no extra dependency. Layer 1 `tools/lint/core-lint.sh`:
  include allowlist (`stdint.h stddef.h stdbool.h stdalign.h limits.h string.h` and project
  headers), banned headers (stdio, stdlib, math, time, errno, assert, setjmp, signal,
  pthread, threads, stdatomic, unistd, fcntl, sys/*), banned tokens (`float`, `double`,
  `__DATE__`, `__TIME__`, `__TIMESTAMP__`, `asm`, `__thread`, `_Thread_local`), banned time,
  entropy, file, stdio and allocation names, heuristic non-const file-scope statics; each
  violation prints rule id, `file:line` and a hint. Layer 2 (authoritative): compile core
  objects with `-fno-common -fno-stack-protector -U_FORTIFY_SOURCE -fno-pic`; fail on any
  writable data section (`.data .bss COMMON .tdata .tbss`; Mach-O `__DATA,__data/__bss/
  __common/__thread_*`; const tables in `__DATA,__const` or `.data.rel.ro*` stay allowed);
  `nm -P -u` undefined symbols (minus core-defined) must be a subset of
  `tools/lint/allowlist.txt` (`name | why | ADR/date`; initial `memcpy memmove memset memcmp`;
  `malloc` is not allowed, memory comes from host callbacks).
- Lint self-test in `tools/lint/selftest/`: `good.c` plus `bad_*.c` (time, include-stdio,
  mutable static, TLS, malloc, libm, float, `__TIME__`), each starting with
  `/* EXPECT: RULE-ID */`; each must exit nonzero and print its rule id; a green-on-empty
  control fails if zero core files are scanned. The self-test runs inside the gate.
- **CI:** one workflow `ci.yml`, triggers `pull_request`, `push: [main]`, `workflow_dispatch`;
  two parallel jobs that each run only `./scripts/gate.sh`: `gate-linux` on `ubuntu-24.04` and
  `gate-macos` on `macos-26`. Top-level `permissions: {}`, per-job `contents: read`,
  `persist-credentials: false`, `timeout-minutes: 10`, `concurrency: ci-${{github.ref}}` with
  cancel-in-progress only for pull requests. Never `pull_request_target`. Explicit runner
  labels, never `*-latest`. Pin `actions/checkout` v7.0.1 to
  `3d3c42e5aac5ba805825da76410c181273ba90b1` (verified 2026-11-09); pin cache and
  upload-artifact only when a job first uses them. **No caching in Phase 1.** No
  `T64_CI_ACTIVATED` switch (repo is public). A gate self-check asserts `ci.yml` has no other
  `run:` lines. Tighten the repo's fork-PR approval to all outside contributors (protective,
  on the owner's own repo).
- **Gate:** `scripts/gate.sh`, POSIX sh with `set -eu`: print tool versions; `sh -n` on
  scripts; lint self-test; configure and build with warnings as errors; `ctest` (runner
  negative controls); source and object lint; manifest, provenance, GOV-07 and claims checks;
  print per-step timings. `CI=true` only adds `--require-fixtures`; nothing else may differ.
- **CI critical-path review (VER-04):** append-only `docs/ci-reviews/LOG.md` (one row per
  reviewed run plus a Findings table: id, finding, taken/declined, reason) and
  `scripts/ci-review.sh <run-id>`, which uses `gh api repos/szTheory/tendies64/actions/runs/
  <id>/jobs` (queue delay needs `created_at`, which `gh run view --json jobs` lacks). The first
  CI run gets a recorded review before the phase closes.

### Claude's Discretion
- Exact JSON parser approach for C tools (a minimal in-repo parser or a copied permissive
  helper such as jsmn under the allowlist, with a provenance record), ADR numbering, tool
  names, file names inside `tools/`, and the synthetic-script grammar details.
- Whether the Rust spike runs as a separate plan or as a task inside the lint plan.

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- None. The repository has no source code. Existing files: `AGENTS.md`, `.claude/CLAUDE.md`,
  `LICENSE`, `NOTICE`, `.planning/**`.

### Established Patterns
- Owner policy in `AGENTS.md` and `.planning/AUTONOMY.md`: evidence labels, no protected
  content, small dependency tree, one fast local gate aligned with CI.

### Integration Points
- Phase 2 consumes the runner facade, the result schema, the profile-identity fields and the
  lint boundary. Phase 3 replays the negative-control matrix against the real core.

</code_context>

<specifics>
## Specific Ideas

- The synthetic runner must be impossible to mistake for evidence: stamped identity, a refusal
  in the claims gate, and a distinct binary name.
- Keep Phase 1 minimal: no emulator behavior, no ROM bytes, no fixtures other than manifest
  entries marked `pending` or `forbidden`.

</specifics>

<deferred>
## Deferred Ideas

- Windows/MSVC canary job, compiler matrix, caches (libdragon toolchain, fixture ROM): Phase 3 or 4.
- Cross-OS digest comparison: Phase 3 (PROF-05).
- Expanding the copy allowlist (BSD-2, MIT-0/0BSD, zlib, Apache-2.0): owner decision.
- GitHub workflow lockfiles vs SHA pinning: re-check when it is documented as generally available.
- Optional local-only protected-hash denylist: maintainer's local choice, never committed.

</deferred>
