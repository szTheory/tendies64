# Phase 1: Governance, Rights and Harness Foundation - Research

**Researched:** 2026-11-09
**Domain:** Portable (macOS + Ubuntu) POSIX sh/awk + C17 governance tooling, CMake/CTest harness, a fork/exec supervisor, a rights/provenance scanner, and CI. No emulator code.
**Confidence:** HIGH for portability, CMake, supervisor and git-enumeration facts (run here, on macOS and Linux). MEDIUM for the Rust-spike forecast (a pilot, not the pre-registered run).

**Label key.** `[MEASURED: where]` = I ran it this session (macOS arm64 Apple clang 21.0.0, or Ubuntu 24.04 containers: arm64 gcc 13.3/clang 18.1.3/CMake 3.28.3/dash/gawk/mawk, plus x86-64 gcc 13.3 under emulation, plus Debian bookworm CMake 3.25.1). `[CITED: url]` = primary documentation. `[VERIFIED: path:lines]` = an in-repo value I read this session. `[INFERENCE]`, `[ASSUMED]`, `[UNKNOWN]` as named. "Community" sources were not needed.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
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

### Deferred Ideas (OUT OF SCOPE)
- Windows/MSVC canary job, compiler matrix, caches (libdragon toolchain, fixture ROM): Phase 3 or 4.
- Cross-OS digest comparison: Phase 3 (PROF-05).
- Expanding the copy allowlist (BSD-2, MIT-0/0BSD, zlib, Apache-2.0): owner decision.
- GitHub workflow lockfiles vs SHA pinning: re-check when it is documented as generally available.
- Optional local-only protected-hash denylist: maintainer's local choice, never committed.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description (REQUIREMENTS.md) | Research Support |
|----|-------------|------------------|
| GOV-01 | ADR: language/build/ABI with research default and reversal evidence | MADR 4.0.0 headings + mandatory `Reversal evidence`; ADR-0002 must be `accepted` before any `core/` file merges; Rust spike pre-registration (section "Rust spike") |
| GOV-02 | Copy-provenance policy, per-copy record | REUSE 3.3 minimum header, `LICENSES/<id>.txt`, `provenance/copies.json` checker rules; JSON decision (hand-written, no copy) means zero copies in Phase 1, so the checker is proven by seeded fixtures only |
| GOV-03 | Fixture manifest with rights/provenance fields | Schema-subset validator in C + cross-field rules; seeded-invalid manifests |
| GOV-04 | Claims/evidence-tier template | Claim blocks + checker scope to avoid prose false positives |
| GOV-05 | One gate command, same as CI | `scripts/gate.sh` portability rules (LC_ALL=C, no pipefail, no SECONDS, per-step timing via `date +%s`) |
| GOV-06 | ADRs: timing tiers, emux opcodes, wall-clock classification, lint mechanism | ADR numbering plan; lint mechanism facts feed ADR-0006 |
| GOV-07 | Automated check: no protected material in tree, CI inputs, artifacts | `t64-blobscan` C tool fed by `git cat-file --batch`; `git rev-list --objects`; seeded temp repos built at test time |
| RUN-02 | Six outcomes distinguishable | Supervisor + result protocol + exit-code table (CONTEXT, verbatim above) |
| RUN-04 | Negative controls in the suite | Control matrix, twice-run byte comparison, inventory pin, ctest `noTestsAction: error` |
| PROF-07 | Mechanical lint fails build on host time/files/threads/entropy/mutable globals | Two-layer lint, tested section/symbol rules, seeded corpus |
| VER-04 | Explicit runner labels, pinned SHAs, measured critical-path review | Verified runner labels, checkout pin, `gh api .../jobs` queue-delay recipe |

## Summary

Every locked decision is implementable with the toolchain on both OSes, but several details in CONTEXT will fail on first run unless corrected. The three that matter most: (1) on Apple clang `-U_FORTIFY_SOURCE` alone does not stop `___memcpy_chk` appearing as an undefined symbol; `-D_FORTIFY_SOURCE=0` is also required, and Apple clang rewrites `memset(p,0,n)` to `bzero`, so `bzero` must be a darwin-only allowlist entry [MEASURED]. (2) Apple clang's default (no flag) emits common (`C`) symbols that `size -m` does not list at all, and GNU object files always list empty `.data`/`.bss`, so the writable-section check must pair `-fno-common`, a size>0 filter and an `nm` type check [MEASURED]. (3) `git diff --diff-filter=A base..HEAD` misses a protected blob that was added and deleted inside the PR range; `git rev-list --objects` catches it, and a first-8-KiB NUL test misses NULs after byte 8192 [MEASURED].

The harness pieces were prototyped and run on all three toolchains: a CMake 3.25-floor layout (presets version 6 with a workflow preset) builds a core static library, supervisor, synthetic runner and a C tool under `-Werror`; a fork/exec supervisor with a polled wall-clock backstop classified exit, signal, hang, spin, grandchild and exec-failure cases and always wrote its result file; and the awk lint tokenizer produced byte-identical output on BWK awk 20200816, gawk 5.2.1 and mawk 1.3.4. The recommended JSON approach is a hand-written strict parser (jsmn is a tokenizer that validates neither numbers nor escapes nor duplicates, so a strict post-pass is needed anyway).

The Rust spike's pilot shows the comparison is narrower than assumed: pure `#![no_std]` + `#![forbid(unsafe_code)]` rejects std time/IO/TLS, `static mut`, inline asm, extern calls and libm methods, but accepts `extern crate std;`, floats, atomic statics, `extern crate alloc` and `env!`. The C two-layer lint is expected to catch the first set, so switch criterion (a) is forecast not to trigger [INFERENCE]; the pre-registered run must still decide it.

**Primary recommendation:** Build Wave 0 (CMake skeleton, `LC_ALL=C` gate shell, tiny C test header, tools/common JSON+SHA-256+UTF-8 library) first; then fan out lint, runner/supervisor, rights/provenance/manifest/claims checks and docs; make every check prove itself with a seeded violation that is generated at test time, observable to the compiler, and asserted by rule id.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Determinism boundary (no host time/files/threads/entropy/globals) | `core/` (freestanding-style C) | `tools/lint` | Core is the lint scan boundary; lint is a build-time gate over it |
| Result protocol, exit-code derivation, wall backstop | `runner/` supervisor (host process) | `tools/common` JSON writer | Supervisor is outside core, may use host clock and POSIX; it must never trust the runner |
| Synthetic guest and negative controls | `runner/` synthetic binary behind facade header in `core/include` | `tests/controls` matrix | Facade is the seam Phase 3 replays against the real core |
| Manifest/provenance/claims validation | `tools/` C tools | `scripts/` sh+awk glue | Structured JSON needs a real parser; sh/awk only orchestrates |
| Rights scan (GOV-07) | `tools/t64-blobscan` + `scripts/rights-scan.sh` | CI (`gate-*` jobs) | Byte classification must be identical on both OSes, so C not shell pipelines |
| Gate orchestration | `scripts/gate.sh` | `CMakePresets.json` | One entry point; presets only configure/build/test |
| CI | `.github/workflows/ci.yml` (runs only the gate) | `scripts/ci-review.sh` (local) | CI has no logic of its own |
| Decisions/policy | `docs/adr`, `docs/policy`, `provenance/` | gate checks | Docs are checked mechanically where a rule exists |

## Standard Stack

### Core
| Tool | Version (verified) | Purpose | Why |
|------|---------|---------|-----|
| C17 compilers | Apple clang 21.0.0 (local and `macos-26` image), gcc 13.3.0 and clang 18.1.3 (Ubuntu 24.04) | core, runner, tools | Locked D1 [MEASURED; CITED: runner-images readmes] |
| CMake | floor 3.25; tested 3.25.1 (Debian 12), 3.28.3 (Ubuntu), 4.4.3 (macOS local); runner images carry 3.31.6 (Ubuntu) and 4.4.5 (macOS) | configure/build/ctest | Presets v6 + workflow preset need exactly 3.25 [CITED: cmake.org/cmake/help/v3.25/manual/cmake-presets.7.html] |
| Ninja | 1.13.2 (local and both runner images) | generator | Locked |
| POSIX sh + awk | dash 0.5.12 (Ubuntu), bash 3.2.57 as `/bin/sh` (macOS); awk: BWK 20200816 (macOS), gawk 5.2.1 / mawk 1.3.4 (Ubuntu) | gate glue, source lint | Locked; code must run on all of them [MEASURED] |
| binutils `nm -P`, `size` | GNU binutils 2.42 (Ubuntu); llvm-nm/Apple `size -m` (macOS) | Layer-2 lint | Locked; dispatch on `uname -s` [MEASURED] |
| git | 2.41 local, 2.56.0 runner | blob enumeration | `rev-list --objects`, `cat-file --batch` [MEASURED] |
| `gh` | preinstalled on runners and locally | `ci-review.sh` | embedded jq: `--jq` supports `fromdate` [MEASURED] |

### Supporting
| Item | Purpose | When |
|------|---------|------|
| `tools/common` static lib (`t64_json`, `t64_sha256`, `t64_utf8`) | shared by every C tool and the runner | Wave 0; hand-written, FIPS 180-4 / RFC 3629 / RFC 8259 test vectors |
| MADR 4.0.0 headings | ADR format | `MIT OR CC0-1.0` [CITED: github.com/adr/madr LICENSE at 4.0.0] |
| REUSE 3.3 header convention | copied-file notices | `SPDX-FileCopyrightText` + `SPDX-License-Identifier`; license text in `LICENSES/<SPDX-id>.txt` [CITED: reuse.software/spec-3.3] |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Hand-written JSON parser | jsmn (MIT, 471-line `jsmn.h`, last push 2024-06-09) | Tokenizer only: no number/escape/duplicate/UTF-8 validation, no canonical writer; a strict layer is needed anyway, so the copy buys little and adds a provenance record [MEASURED: gh api; CITED: github.com/zserge/jsmn README] |
| C tool for UTF-8/NUL/magic | `iconv`, `tr`, `head -c` pipelines | `iconv -f UTF-8 -t UTF-8` returned 0 for U+110000 on both OSes; macOS `tr` errors on invalid UTF-8 unless `LC_ALL=C` [MEASURED] |
| `shasum -a 256` / `sha256sum` | in-tree C SHA-256 | OS tools differ (`/sbin/sha256sum` exists on macOS 26 here but not guaranteed older); runner needs a hash in-process anyway |

**Installation:** nothing to install on runners (all tools preinstalled). Local macOS needs Xcode CLT, CMake, Ninja (present). No `npm`/`pip`/`cargo` packages.

## Package Legitimacy Audit

Phase 1 installs **no external packages** (no npm/PyPI/crates; build tools are OS- or runner-provided) and vendors **no third-party source** under the recommended JSON and SHA-256 approaches. `slopcheck`/`package-legitimacy` is therefore not applicable. **Packages removed: none. Flagged: none.** If the planner nevertheless chooses to copy jsmn or a SHA-256 helper, it enters `provenance/copies.json` and needs a `checkpoint:human-verify` for the license text (jsmn is MIT per its README and GitHub; `amosnier/sha-2` reports `NOASSERTION`, so do not copy it without reading its LICENSE.md [UNKNOWN]).

## Architecture Patterns

### System Architecture Diagram

```
                    ./scripts/gate.sh  (export LC_ALL=C; set -eu; print versions + per-step seconds)
                                     |
   +-----------+-----------+---------+---------+------------+--------------+
   |           |           |         |         |            |              |
 sh -n    lint selftest   cmake    ctest   source+object  manifest/     rights scan (GOV-07)
 scripts  (bad_*.c must   --preset --preset  lint of core/ provenance/    tree + git objects
          fail w/ rule id) gate     gate     (L1 awk, L2   claims check   + artifact staging dir
                           build   (noTests  compile+nm+   (C tools)      (t64-blobscan <- git cat-file --batch)
                           -Werror  Error)   size)
                                     |
                 tests/controls/matrix.tsv  (case, script, expected outcome, exit code)
                                     |
   case --> t64-supervisor --wall-ms N --result r.json -- t64-runner-synthetic script   (x2: slice sizes differ)
                  |  fork/setpgid/exec, poll waitpid(WNOHANG)+2 ms nap, SIGKILL pgrp, rename(tmp,r.json)
                  +--> runner writes report file --> supervisor validates (t64-result-check) --> canonical result JSON + exit code
   twice-run canonical bytes compared with cmp; suite record (count, inventory hash) checked against pinned INVENTORY

 CI: ci.yml -> gate-linux (ubuntu-24.04, x64) | gate-macos (macos-26, arm64) -> each: actions/checkout@<sha> (fetch-depth 0) + ./scripts/gate.sh
```

### Recommended Project Structure
```
core/include/tendies64/   t64_facade.h (advance/drain interface), identity constants
core/                     one real stub .c so the lint never scans zero files
runner/                   t64-supervisor.c, t64-runner-synthetic.c, script grammar
tools/common/             t64_json.c t64_sha256.c t64_utf8.c (+ headers) -> static lib t64_common
tools/                    t64-manifest-check.c t64-provenance-check.c t64-claims-check.c t64-blobscan.c t64-result-check.c
tools/lint/               core-lint.sh lint-object.sh allowlist.txt rules.txt selftest/{good.c,bad_*.c}
scripts/                  gate.sh rights-scan.sh ci-review.sh check-ci-yml.sh
tests/                    t64_test.h, unit/, controls/{matrix.tsv,INVENTORY,scripts/}, seeded/
docs/{adr,policy,claims,ci-reviews,spikes}/  provenance/  fixtures/  LICENSES/ (only license ids actually used)
```

### Pattern 1: One-source-of-truth lint rules
`tools/lint/rules.txt` lists every rule id; the selftest asserts each id appears in at least one `/* EXPECT: RULE-ID */` file, so a rule with no seeded violation fails the gate. [INFERENCE: closes the "rule exists, never proven" gap].

### Pattern 2: Seeded violations are generated, never committed, when they look protected
GOV-07 would flag its own fixtures. Build seeded repos/dirs under `mktemp -d "${TMPDIR:-/tmp}/t64.XXXXXX"` with `printf` (ROM-magic bytes, NUL late in a file, Latin-1, UTF-16, gitlink, add-then-delete) and run the scanner against them. Source-lint seeds (`bad_*.c`) are plain text and are committed under `tools/lint/selftest/` (outside `core/`, so not scanned as core).

### Pattern 3: Supervisor owns truth
Runner writes a *report* to a path the supervisor created; the supervisor validates it, cross-checks it against the child's wait status (a "pass" report with a signaled child becomes `harness-error`), then writes the canonical result atomically (`*.tmp` then `rename`). The exit code is derived from the validated outcome only (table in the verbatim decisions).

### Anti-Patterns to Avoid
- `timeout(1)`, `sed -i`, `readlink -f`, `stat`, `date -d`, `echo -e/-n`, `grep -P`, `xargs -r`, `head -c`, `$SECONDS`, `set -o pipefail`: each is GNU-only, BSD-different or absent in dash. Use C tools, `printf`, `date +%s`, `wc -c < f`. [MEASURED: dash has no `$SECONDS` and masks pipeline failure: `false | cat` returns 0]
- awk extensions: avoid `length(array)`, `nextfile`, `gensub`, `IGNORECASE`, `\<`/`\b`, `{n,m}` intervals, backslashes in `-v`. [INFERENCE from portability; intervals did work on mawk 20240123 and BWK 20200816, but older mawks lack them]
- Piping a gate step through `tee`/`grep` (masks the exit code in dash).
- `ctest` with no tests (exits 0).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Wall-clock timeout | shell `sleep &; kill` loops or GNU `timeout` | C supervisor (below) | `timeout` is absent on macOS; shell cannot capture signals/exec failure reliably |
| Killing the whole process tree | `kill pid` | `setpgid` in child and parent, `kill(-pid, SIGKILL)`, blocking `waitpid` | Grandchild case measured: all killed on both OSes |
| C source linting by regex alone | a full C parser | Layer 1 token scan + Layer 2 object/link facts | Compiler/linker is authoritative; tokenizer strips comments/strings only |
| Invalid-UTF-8 detection | `iconv`/`grep` | `t64_utf8` (RFC 3629: reject overlong, surrogates, > U+10FFFF) | iconv accepted U+110000 [MEASURED] |
| General JSON Schema engine | draft 2020-12 implementation | Declared keyword subset; unknown keyword in schema = tool failure | Prevents the schema from carrying unenforced constraints |
| YAML parsing of `ci.yml` | a YAML parser | line-pattern awk (`run:`, `uses:`, `permissions`, runner labels) | Scope is five fixed assertions; fail on any unrecognised `run:` shape |
| Queue-delay measurement | scraping the UI | `gh api repos/szTheory/tendies64/actions/runs/<id>/jobs` | `gh run view --json jobs` returns only `startedAt/completedAt`, no `created_at` [MEASURED] |

**Key insight:** the failure mode in this phase is a check that cannot fail. Every check needs (a) a seeded violation asserted by rule id and (b) a green-on-empty control (zero inputs scanned = failure).

## Common Pitfalls

### Pitfall 1: Apple clang keeps fortify and bzero
**What goes wrong:** `nm -P -u` on a clean core object shows `___memcpy_chk` and `_bzero` under `-U_FORTIFY_SOURCE`. **Fix:** use `-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0`; add `bzero` to `allowlist.txt` with `why = Apple clang rewrites memset(p,0,n); darwin only`. [MEASURED: `-U_FORTIFY_SOURCE` -> `___chkstk_darwin ___memcpy_chk _bzero`; with `-D_FORTIFY_SOURCE=0` -> `___chkstk_darwin _bzero _memcpy`]
**Warning sign:** macOS gate red on a file Linux passes.

### Pitfall 2: Mach-O symbol names carry a leading underscore
`nm -P -u` prints `_memcpy U 0 0` on macOS and `memcpy U` (two fields plus blanks) on Linux. Strip one leading `_` when `uname -s` is Darwin before comparing to the allowlist. Mach-O TLS yields `__tlv_bootstrap`; x86-64 ELF TLS yields `_GLOBAL_OFFSET_TABLE_` even with `-fno-pic` [MEASURED].

### Pitfall 3: Compiler runtime symbols surface from innocent code
`__builtin_popcountll` -> `__popcountdi2` on x86-64 gcc; `unsigned __int128` division -> `__udivti3`/`__divti3`; a >4 KiB stack frame -> `___chkstk_darwin` on macOS [MEASURED]. None belong on the allowlist by default. Policy for the planner: core avoids these constructs (add `-Wframe-larger-than=2048` and forbid `__builtin_popcount*` and `__int128` division); a genuine need is an ADR-backed allowlist row, per arch.

### Pitfall 4: The section check misses commons and flags empty sections
Apple clang with no flag emitted `_zinit_static C` and **no** `__DATA` section in `size -m`; with `-fno-common` it appears as `__DATA,__common: 16 (zerofill)`. GNU gcc objects always list `.data 0` and `.bss 0`. [MEASURED] Rule: compile with `-fno-common`, ignore size 0, and additionally fail any `nm -P` type `C`/`c`.
Constructors land in `__DATA,__mod_init_func` (macOS) and `.init_array` (ELF) [MEASURED]: both are denied. Const pointer tables land in `__DATA,__const` (macOS, allowed) and `.data.rel.ro*` only under `-fpic` (not with `-fno-pic`) [MEASURED].

### Pitfall 5: Seeded violations the optimizer deletes
`void *p = malloc(4); free(p);` vanished at `-O2` (no `U malloc`), and an empty `static` constructor vanished too [MEASURED]. Seeds must have an observable effect (return the pointer, write a `volatile`, make the function non-static).

### Pitfall 6: `ctest` with zero tests is green
Plain `ctest` returned 0 with "No tests were found!!!"; the preset with `"execution": {"noTestsAction": "error"}` returned 8 on 3.28.3 and 4.4.3 [MEASURED]. Use the preset in the gate.

### Pitfall 7: `-std=c17` (extensions off) hides POSIX declarations
`fork`, `sigaction`, `clock_gettime` need `#define _POSIX_C_SOURCE 200809L` before includes (or `target_compile_definitions(... PRIVATE _POSIX_C_SOURCE=200809L)`) on `runner/` and `tools/` targets only, never on `core/`. The prototype supervisor compiled warning-free with that define on all three compilers [MEASURED].

### Pitfall 8: macOS byte tools depend on locale
With the default `C.UTF-8`-style locale, `tr -d '\000'` printed `tr: Illegal byte sequence` and returned wrong counts on invalid-UTF-8 input; `LC_ALL=C` fixed it [MEASURED]. `export LC_ALL=C` at the top of `gate.sh` (also fixes `sort`/`comm` ordering and gawk's UTF-8 `length`).

### Pitfall 9: GOV-07 evasion paths
(a) Add-then-delete inside the range: `git diff --diff-filter=A base..HEAD` listed `late.txt latin1.txt link ren_dst.txt sub utf16.txt` and **not** the deleted ROM-magic `data.bin`; `git rev-list --objects base..HEAD` listed it [MEASURED]. (b) NUL after byte 8192 passes a head-8KiB test: a 9002-byte file with NULs only at the end classified `text` [MEASURED]. (c) Renames share one blob id (dedupe by oid, keep all names). (d) Gitlinks (`160000`) appear in `diff-tree` with a non-object oid; `rev-list --objects` omits them. (e) Shallow clones: `actions/checkout` defaults to depth 1, so the range is empty; require `fetch-depth: 0` and fail the gate when `git rev-parse --is-shallow-repository` is `true`. (f) Git LFS pointer files are valid text that hides bytes: reject `version https://git-lfs.github.com/spec/v1` and any `filter=lfs` in `.gitattributes`. (g) Text-encoded payloads (hex/base64/C arrays) pass default-deny: keep as advisory with a path+sha allowlist, never a hard fail without a threshold.

### Pitfall 10: Crash controls leave host artifacts
Child `abort`/`segv` on macOS wrote `~/Library/Logs/DiagnosticReports/kid-*.ips` (I deleted mine). The supervisor child should `setrlimit(RLIMIT_CORE, 0)` (done in the prototype) and the controls doc should note the `.ips` side effect. Do not assert "no stray processes" immediately after a grandchild kill: `ps` showed one dying process for a moment, then none [MEASURED].

### Pitfall 11: `-Werror` against drifting runner compilers
Runner images update weekly; a new `-Wall` warning can turn main red with no code change. Use an explicit curated warning list that exists in gcc 13, clang 18 and Apple clang 21 (the one in the example compiled clean on all three) and expose `T64_WERROR` as a cache option in the preset so a drift fix is one reviewed line, not a gate bypass. [INFERENCE]

### Pitfall 12: Architecture differs per runner
`ubuntu-24.04` is x64 and `macos-26` is arm64 (`macos-26-intel` is the x64 mac label) [CITED: github.com/actions/runner-images README]. Print `uname -m` in the gate banner; Layer-2 results differ per arch (item 3, `__popcountdi2`).

## Code Examples

### CMake presets (version 6, workflow preset) — [MEASURED: configure+build+ctest green on CMake 3.25.1 gcc 12.2, 3.28.3 gcc 13/clang 18, 4.4.3 Apple clang]
```json
{ "version": 6, "cmakeMinimumRequired": {"major":3,"minor":25,"patch":0},
  "configurePresets": [{ "name":"gate","generator":"Ninja","binaryDir":"${sourceDir}/build/gate",
    "cacheVariables":{"CMAKE_BUILD_TYPE":"Release","T64_WERROR":"ON"},
    "warnings":{"dev":true,"uninitialized":true}, "errors":{"dev":true} }],
  "buildPresets":[{"name":"gate","configurePreset":"gate"}],
  "testPresets":[{"name":"gate","configurePreset":"gate",
    "output":{"outputOnFailure":true}, "execution":{"noTestsAction":"error"} }],
  "workflowPresets":[{"name":"gate","steps":[{"type":"configure","name":"gate"},{"type":"build","name":"gate"},{"type":"test","name":"gate"}]}] }
```
Version 6 is the first preset version allowing workflow presets; the first step must be `configure` [CITED: cmake presets v3.25]. `gate.sh` calls `cmake --preset gate`, `cmake --build --preset gate`, `ctest --preset gate` as three timed steps; `cmake --workflow --preset gate` is a developer convenience.

```cmake
cmake_minimum_required(VERSION 3.25)
project(tendies64 VERSION 0.0.0 LANGUAGES C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
option(T64_WERROR "Treat warnings as errors" ON)
include(CTest)
set(T64_WARN -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes -Wmissing-prototypes -Wundef -Wvla -Wcast-qual -Wformat=2 -Wwrite-strings)
function(t64_target_warnings tgt)
  target_compile_options(${tgt} PRIVATE ${T64_WARN})
  if(T64_WERROR)
    target_compile_options(${tgt} PRIVATE -Werror)
  endif()
endfunction()
add_library(t64_core STATIC core/t64_core_stub.c)
target_include_directories(t64_core PUBLIC core/include)
add_executable(t64-supervisor runner/t64-supervisor.c)
target_compile_definitions(t64-supervisor PRIVATE _POSIX_C_SOURCE=200809L)
```
The compile line observed on macOS: `cc -I.../core/include -O3 -DNDEBUG -std=c17 -arch arm64 -Wall ... -Werror` (extensions off gives `-std=c17`, not `gnu17`) [MEASURED].

### Layer-2 object check (verified on dash+gawk, dash+mawk, bash3.2+BWK awk; gcc, clang, Apple clang; x86-64 and arm64)
```sh
# lint flags: -std=c17 -O2 -fno-common -fno-stack-protector -fno-pic -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -c
if [ "$os" = Darwin ]; then size -m "$o"; else size -A "$o"; fi | awk -v obj="$o" -v os="$os" '
  os=="Darwin" && /^[ \t]*Section \(/ { l=$0; sub(/^[ \t]*Section \(/,"",l); split(l,a,"): "); s=a[1]; gsub(/, /,",",s); z=a[2]+0
    if (z>0 && s ~ /^__DATA,(__data|__bss|__common|__thread_|__mod_init_func|__mod_term_func)/) bad(s) }
  os!="Darwin" && NF==3 && $2 ~ /^[0-9]+$/ && $2+0>0 &&
    $1 ~ /^\.(data|bss|tdata|tbss|sdata|sbss|ldata|lbss|init_array|fini_array|preinit_array|ctors|dtors)(\.|$)/ &&
    $1 !~ /^\.data\.rel\.ro/ { bad($1) }
  function bad(s){ printf "RULE W-SECTION %s: writable section %s\n", obj, s; f=1 }  END{ exit f }'
nm -P "$o" | awk -v os="$os" '{ n=$1; t=$2; if (os=="Darwin") sub(/^_/,"",n)
  if (t=="U"||t=="w"||t=="v") print n >> und; else if (t=="C"||t=="c") { print "RULE W-COMMON " n; bad=1 } else if (t ~ /^[A-Z]$/) print n >> def }'
# then: comm -23 und.sorted def.sorted | comm -23 - allow.sorted   (all sorted with LC_ALL=C)
```
Observed output on a seeded file: `.data/.bss/.tdata` (ELF) or `__DATA,__data/__thread_data/__thread_vars/__bss/__common` (Mach-O) plus `sin` undefined; the clean file exited 0 on every toolchain [MEASURED]. Compile core objects in the lint script itself with the lint flags (independent of CMake's `-O3`/`-arch` flags) so paths and flags are deterministic; one object per `core/*.c`.

### Layer-1 tokenizer core (identical output on BWK awk 20200816, gawk 5.2.1, mawk 1.3.4)
```awk
# per line: strip /* */ (state carried across lines), //, "..." and '...' with a char state machine, then
while (match(rest, /[A-Za-z_][A-Za-z0-9_]*/)) { tok=substr(rest,RSTART,RLENGTH); rest=substr(rest,RSTART+RLENGTH)
  if (tok in isbanned) report("TOKEN", FNR, "banned identifier '" tok "'") }
# #include lines examined on the raw line: /^[ \t]*#[ \t]*include[ \t]*[<"]/, header = text up to > or "
```
Tested on `#  include <stdio.h>`, `float` in comments/strings (ignored), `floating_x` (not a token match), `'"'` char literal, and `__TIME__` use [MEASURED]. Add: a floating-literal rule (tokenise pp-numbers; non-hex containing `.` or `e/E`), brace-depth tracking for the "non-const file-scope static" heuristic, and `-Wdouble-promotion`. Layer 2 is authoritative; Layer 1 only explains.

### Supervisor core (fork/exec, poll backstop; passed 9 cases on macOS, Ubuntu gcc, Ubuntu clang)
```c
int ep[2]; pipe(ep); fcntl(ep[0],F_SETFD,FD_CLOEXEC); fcntl(ep[1],F_SETFD,FD_CLOEXEC);  /* exec-failure channel; pipe2 is Linux-only */
pid_t pid = fork();
if (pid == 0) { setpgid(0,0); struct rlimit rl={0,0}; setrlimit(RLIMIT_CORE,&rl);
                execvp(argv[i], &argv[i]); int e=errno; (void)!write(ep[1],&e,sizeof e); _exit(127); }
setpgid(pid,pid); close(ep[1]);                               /* both sides: avoids the race */
for (;;) { pid_t r = waitpid(pid,&status,WNOHANG);
  if (r == pid) { reaped=1; break; }
  if (g_term) { detail="supervisor-signalled"; break; }       /* SIGTERM/SIGINT handlers without SA_RESTART */
  if (now_ms() >= end) { detail="wall-backstop"; break; }     /* CLOCK_MONOTONIC */
  struct timespec nap={0,2*1000*1000}; nanosleep(&nap,NULL); }
if (!reaped) { kill(-pid,SIGKILL); kill(pid,SIGKILL); while (waitpid(pid,&status,0)<0 && errno==EINTR) {} }
/* classify: read(ep[0]) got errno -> exec-failed; WIFSIGNALED -> signal-N; WEXITSTATUS -> exit-N; exit 0 without valid report -> no-result */
```
Measured outcomes (all `harness-error`): `exit0`->`no-result`, `exit7`->`exit-7`, `spin`/`hang`/`ignterm`/grandchild->`wall-backstop` (a 700 ms bound returned in about 0-1 s), `abort`->`signal-6`, `segv`->`signal-11`, missing binary->`exec-failed` [MEASURED]. `sigtimedwait` is not usable: it compiles on Ubuntu gcc but is an undeclared function on macOS (Apple clang error `call to undeclared function 'sigtimedwait'`) [MEASURED]; polling with a 2 ms nap is portable. Child stdout/stderr go to files (never pipes) so a chatty guest cannot deadlock the supervisor; bound them with `RLIMIT_FSIZE` [INFERENCE, not run]. A real supervisor additionally validates the report, writes `*.tmp` then `rename`, and ignores `SIGPIPE`.

### GOV-07 enumeration (git 2.41 and Ubuntu git; identical classification on both OSes)
```sh
git rev-list --objects "$base"..HEAD | awk '{print $1}' \
 | git cat-file --batch-check='%(objectname) %(objecttype)' | awk '$2=="blob"{print $1}' > oids
git cat-file --batch < oids | t64-blobscan          # reads "<oid> <type> <size>\n<bytes>\n" records
```
`t64-blobscan` classes: `rom-magic` (`80371240`, `37804012`, `40123780` at offset 0), `nul` (anywhere in the blob; record separately whether it was within the first 8192 bytes to keep the locked rule id), `invalid-utf8` (whole blob), `container` (zip `PK\3\4`, gzip `1f8b`, xz `fd377a585a00`, bzip2 `BZh`, zstd `28b52ffd`, tar `ustar` at 257), `lfs-pointer`, advisory `size-4032`. Working tree: `git ls-files -z --cached --others --exclude-standard`; artifact staging: `find "$dir" -type f -print0` plus fail on any `find "$dir" -type l`. Default base: `origin/main`'s merge-base when present, else every reachable blob (`git rev-list --objects HEAD`). The seeded run classified: UTF-16 -> `nul`, ROM-magic -> `rom-magic`, Latin-1 -> `invalid-utf8`, symlink and rename -> `text` [MEASURED].

### CI critical-path recipe (verified against a public run)
```sh
gh api "repos/szTheory/tendies64/actions/runs/$RUN/jobs" --jq \
 '.jobs[] | [.name, ((.started_at|fromdate)-(.created_at|fromdate)), ((.completed_at|fromdate)-(.started_at|fromdate))] | @tsv'
# columns: job, queue seconds, run seconds. Step timing: .steps[] has started_at/completed_at/name/number.
```
The repo `szTheory/tendies64` is already public [MEASURED: `gh api`]. Current fork-PR approval is `first_time_contributors` and `sha_pinning_required` is `false` [MEASURED: read-only GET]; the locked "tighten fork-PR approval" is `PUT .../actions/permissions/fork-pr-contributor-approval` with `all_external_contributors` [ASSUMED: PUT body not exercised]. `sha_pinning_required: true` is an optional repo-level backstop for the SHA-pin rule, not in CONTEXT [owner-visible, not locked].

## JSON approach (Claude's Discretion; recommendation)

Write `tools/common/t64_json.[ch]` (~350 lines, authored here, no provenance record): a strict RFC 8259 recursive-descent parser into an arena of nodes with hard limits (input 1 MiB, depth 16, string 4096 bytes, 4096 nodes), rejecting duplicate keys, trailing commas, comments, BOM, control characters, invalid UTF-8, floats and exponents, integers outside +-(2^53-1) (larger u64 values must be decimal strings matching `0|[1-9][0-9]*` and <= 18446744073709551615), and unknown fields (callers declare allowed keys). Reasons: the locked protocol is stricter than any permissive tokenizer; jsmn does not validate numbers or escapes and cannot reject duplicates [CITED: jsmn README; MEASURED: 471-line header, `JSMN_STRICT` exists only in source]. RFC 8785 explains the u64-as-string rule: JSON numbers must be expressible as IEEE 754 doubles [CITED: rfc-editor.org/rfc/rfc8785 section 3.1].

Canonical emission (recommendation mirroring RFC 8785 where it applies): no whitespace between tokens; keys sorted by unsigned byte value (restrict keys to ASCII `[a-z0-9_]`, which makes byte order equal to the UTF-16 code-unit order RFC 8785 specifies); strings escape `"` `\\`, use `\b \t \n \f \r` for those controls and lowercase `\u00xx` for other controls below 0x20, emit everything else as raw UTF-8; one trailing LF. Writers emit fields in a compile-time sorted table; a `t64-json-canon` tool (parse then emit) is the oracle: every committed or produced canonical file must satisfy `canon(f) == f`, and `canon(canon(x)) == canon(x)`. Test inputs are independently sourced: the RFC 8259 grammar plus ~40 hand-authored negative cases (categories: structure, numbers, escapes, UTF-8, duplicates, limits).
Provenance if the planner copies jsmn instead: add `SPDX-License-Identifier: MIT` and the upstream copyright line, put `LICENSES/MIT.txt`, record upstream repo, 40-hex SHA (`git ls-remote`), retrieval date, `verbatim`, owner (a GitHub login or role), and `upstream_status` (last push 2024-06-09), plus a strictness post-pass and an independent test id.

## Manifest schema validation without dependencies

Keep `fixtures/manifest.schema.json` as the single structural source and have `t64-manifest-check` read it, implementing only this keyword subset: `type, required, properties, additionalProperties (false only), enum, const, items, minItems, maxItems, minLength, maxLength, pattern (POSIX ERE via <regex.h>; `regcomp` with `REG_EXTENDED` compiled and ran on macOS, Linux header compiled [MEASURED]), minimum, maximum`. Any other keyword in the schema file is a hard error, so unenforced constraints cannot hide. Cross-field rules go in C and in `rules.txt`: `forbidden` entry has a reason and no `sha256`/`size`; `pending` default; bytes allowed only when `binary_rights` is `authored-here`/`cleared` and `distribution` contains that channel; a `hardware-derived` `expected.kind` needs capture provenance. Seeded invalid manifests (one per required field and per cross rule, 15-20 files) assert the rule id. Phase 1 manifest contains only `pending`/`forbidden` entries. `jq`, `python3` and `ajv` are not used (jq exists locally and on the macOS runner, but "no extra dependency" is locked).

## ADR and policy documents
- Template `docs/adr/0000-template.md` uses MADR 4.0.0 headings in this order [CITED: raw template at tag 4.0.0, `MIT OR CC0-1.0`]: front matter keys `status`, `date`, `decision-makers`, `consulted`, `informed`; `# Title`; `## Context and Problem Statement`; `## Decision Drivers`; `## Considered Options`; `## Decision Outcome` (`### Consequences`, `### Confirmation`); `## Pros and Cons of the Options`; `## More Information`. Add `## Reversal evidence` after Decision Outcome (locked). A check requires the front-matter `status`, the `Status` line and the `Reversal evidence` heading.
- MADR is dual-licensed `MIT OR CC0-1.0`; MIT is on the allowlist, so use headings (not the template's comment text) and record the structural adoption in `provenance/copies.json` as MIT, `adapted`. Do not copy the template prose.
- Proposed numbering (planner discretion): 0001 use-madr; 0002 language-build-abi (GOV-01); 0003 cpu-timing-tiers; 0004 emulator-extension-opcodes; 0005 wall-clock-timeout-classification; 0006 determinism-lint-mechanism (the allowlist `ADR/date` column cites it). **Ordering constraint:** ROADMAP says D1 is "accepted before any core code merges" [VERIFIED: .planning/ROADMAP.md:84: `D1 is recorded here as an ADR (default C17, CMake 3.25+ and Ninja) and accepted before any core code merges`], so ADR-0002 must be `accepted` before the first `core/*.c` lands (gate check: any `core/*.c` present implies ADR-0002 status accepted).
- REUSE 3.3 minimum for a copied file: one `SPDX-FileCopyrightText:` line naming the holder (year recommended) and one `SPDX-License-Identifier:` line in a comment header; license text at `LICENSES/<SPDX-id>.txt`; that directory "MUST NOT include any other files" so it holds only licenses in use; uncommentable files use a `.license` sidecar or `REUSE.toml` (do not mix with deprecated `.reuse/dep5`) [CITED: reuse.software/spec-3.3]. Project files need no header in Phase 1; the root `LICENSE` is Apache-2.0 and `LICENSES/Apache-2.0.txt` is only required if the project opts into full REUSE compliance [UNKNOWN: not decided].
- Claims check scope: reserved words ("accurate", "compatible", "supports") appear in ordinary prose, so scan only delimited claim blocks and lines starting with `Tendies64 ` outside them; refuse `tier=synthetic` or `profile=synthetic-stub` in any claim.

## Rust spike (time-boxed; pre-registration)

**Smallest meaningful comparison:** one frozen corpus, two implementations, one table. Corpus = the C `bad_*.c`/`good.c` selftest list (time, include-stdio, mutable static, TLS, malloc, libm, float, `__TIME__`) plus added classes: file I/O, thread, entropy call, interior-mutable static, constructor, inline asm, extern host call, build-time-nondeterminism macro. Rust twin of each in `docs/spikes/rust-vs-c/`, compiled with `rustc --edition 2021 --crate-type=lib --emit=metadata` (no crates, no cargo) under `#![no_std] #![forbid(unsafe_code)]`; run in the pinned image `rust:1.91` (present locally; rustc 1.91.1 [MEASURED]); no host Rust is installed (`rustc`/`cargo` absent, `rustup` 1.29.3 present without a toolchain). The second artifact is a bounded `admission(buf,len)` function in both languages, spike-only (see Open Question 1).

**Pilot (not the registered run) [MEASURED, rustc 1.91.1]:** rejected: `std::time`/`println!`/`thread_local!`/`std::fs` when `std` is not linked (E0433), `static mut` use (E0133), inline `asm!` (E0133), calling an `extern "C"` function (E0133), `f64::sin` (E0599). Accepted: `extern crate std;` followed by `SystemTime::now()` (one-line escape hatch), floats, `static AtomicU32`, `extern crate alloc` + `Box`, `env!("HOME")`, `core::time::Duration`. So Rust does not make floats, atomic globals, allocation or build-time macros impossible; and `#![no_std]` itself needs a source rule against `extern crate std`.

**Pre-register before writing any spike code** (commit `docs/spikes/rust-vs-c/PREREG.md` first; its commit must precede the results commit):
1. Frozen leak-class list and corpus file hashes; the C result table (rule id fired, exit code) and Rust result table (compile error code or accepted), both blank.
2. Rule (a) restated as computable: a class is a switch trigger only if the **full C gate** (Layer 1 + Layer 2, both OSes) exits 0 on it while the Rust twin fails to compile under the stated flags and the class is in the frozen list. Classes added after registration go in a "post-registration" table that cannot flip the decision.
3. Rule (b): trigger only on an ASan/UBSan report in the C admission parser attributable to the parser (not the harness), found by the registered fuzz: fixed seed, 2,000,000 deterministic mutations (byte flips, truncation, extension, boundary sizes 0, 1, 3, 4, 4031, 4032, 4033, `limit-1`, `limit`, `limit+1`) built with `-fsanitize=address,undefined -fno-sanitize-recover=all`; record input digests of any failure.
4. Time box: stop at N=1 working session of executor effort or the first trigger, whichever is first; non-triggers recorded, D1 stays C17, ADR-0002 `Confirmation` updated. Style, compile speed, ecosystem and preference do not count.
5. Forecast, for calibration only: (a) will not trigger because Layer 2 catches atomics/ctors/commons and Layer 1 catches banned tokens; (b) unknown [INFERENCE].
Spike code stays in `docs/spikes/` (text only; the GOV-07 scan covers it).

## Common first-run failures beyond the pitfalls above
- `gate.sh` must resolve its root without `readlink -f`: `cd "$(dirname "$0")/.." && pwd -P`.
- `mktemp -d "${TMPDIR:-/tmp}/t64.XXXXXX"` with an explicit template works on both OSes [MEASURED]; trap EXIT/HUP/INT/TERM for cleanup.
- Print tool versions with `|| true` per tool (`size --version` errors on macOS; `awk --version` varies by awk).
- Provenance "copied-looking" detection needs a definition: file under `third_party/`, or an `SPDX-License-Identifier` other than `Apache-2.0`, or a copyright holder other than the project. Without it the check is untestable.
- `CI=true` adds only `--require-fixtures`; define it as "a manifest entry whose bytes are permitted but absent fails instead of being reported as skipped" and add a seeded control for both modes, reporting skipped counts.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A2 | `RLIMIT_FSIZE` bounds captured child output without side effects (SIGXFSZ) | Supervisor | Plan adds a test; low |
| A3 | POSIX `<regex.h>` ERE matches identically on both OSes (compiled on both; semantics only run on macOS) | Manifest validator | Fall back to hand-coded hex/length checks |
| A4 | `PUT .../fork-pr-contributor-approval` body field is `approval_policy` | CI | Owner-repo setting; wrong body returns an error, no harm |
| A5 | Rust spike (a) will not trigger | Rust spike | Wrong forecast only; the registered run decides |
| A6 | Hand-written SHA-256 (identity digests only, FIPS 180-4 vectors) is acceptable under ASVS "don't hand-roll crypto" | Stack | If not acceptable, copy a vetted permissive SHA-256 with provenance (needs license read) |
| A7 | Whole-blob NUL rule (stricter than the locked "first 8 KiB") is wanted | GOV-07 | Keep locked rule id and add the stricter one as a second rule id; no conflict |
| A8 | Layer-2 results on arm64 Linux + emulated x86-64 gcc represent the x64 runner; GitHub's CMake 3.31.6/4.4.5 not run here | Lint, CMake | First CI run is the check; item logged in `docs/ci-reviews/LOG.md` |
| A9 | `macos-26` runner `awk`/`sh` match local (BWK 20200816, bash 3.2.57) | Portability | Runner readme lists Bash 3.2.57 [CITED]; awk build not listed [UNKNOWN] |

## Open Questions

1. **Where does the "C admission parser" for the spike live?**
   - Known: CONTEXT criterion (b) refers to fuzzing "the C admission parser during the phase" while Phase 1 delivers no ROM behavior (BUS-04 is Phase 2).
   - Unclear: whether it is a shipped `core/` file. Recommendation: spike-only under `docs/spikes/`, not in `core/`; BUS-04 later reuses its tests.
2. **`fetch-depth: 0` in `ci.yml`.** It is a `with:` key, not a `run:` line, so the "no other `run:` lines" self-check still holds. Required by GOV-07 range scans (Pitfall 9e). Recommendation: add it and assert it in `check-ci-yml.sh`.
3. **Where SHA-256 lives.** Recommendation: `tools/common` in Phase 1 (core stays minimal so ADR-0002 can precede it), migrating to `core/` in Phase 2 if the state digest needs it. A one-line core stub file keeps the lint scanning a real file.
4. **Full REUSE compliance?** Not required by CONTEXT; leave undecided and document that only copied files carry headers in Phase 1.
5. **macOS `.ips` crash reports from controls.** Cosmetic; document in `tests/controls/README`.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Apple clang / cc | build | yes | 21.0.0 | none needed |
| CMake | build | yes | 4.4.3 local; 3.25.1/3.28.3 verified in containers | none |
| Ninja | generator | yes | 1.13.2 | none |
| git, gh | rights scan, ci-review | yes | 2.41; gh present | none |
| awk | gate, lint | yes | BWK 20200816 (also gawk/mawk in containers) | none |
| nm / size | Layer 2 | yes | llvm-nm; Apple `size -m` | none |
| Docker | Linux experiments only | yes | 29.5.2 (ubuntu:24.04, debian:bookworm-slim, rust:1.91 local) | Not a gate dependency |
| rustc/cargo | Rust spike | no (host) | `rust:1.91` image (rustc 1.91.1) | run spike in the image |
| reuse tool, shellcheck, jq-in-gate | optional | reuse no; jq present | - | not used |
| gsd-tools | research cache seam | no | - | skipped; no cache writes this run |

Note: a direct CMake 3.25.3 binary download was blocked by the sandbox permission classifier; Debian 12's apt CMake 3.25.1 was used instead and verifies the floor [MEASURED].

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | CTest (CMake 3.25 floor) + in-repo C asserts (`tests/t64_test.h`) + sh drivers; no third-party framework |
| Config file | `CMakePresets.json` (`gate` presets), `tests/controls/matrix.tsv`, `tools/lint/rules.txt` |
| Quick run | `ctest --preset gate -R <name>`; lint only: `sh tools/lint/core-lint.sh --selftest`; rights only: `sh scripts/rights-scan.sh --selftest` |
| Full suite | `./scripts/gate.sh` (the only command CI runs) |

### Phase Success Criteria -> Automated Verification
| SC | Behavior | Test type | Automated command | Exists |
|----|----------|-----------|-------------------|--------|
| 1 | ADRs/policy/claims template exist with required headings, statuses, reversal evidence | doc check | `sh scripts/check-docs.sh` (required headings per ADR, ADR-0002 accepted when `core/*.c` exists) | Wave 0 gap |
| 2 | Gate runs locally and CI runs it verbatim; labels explicit; pins are SHAs; first run reviewed | static + manual-record | `sh scripts/check-ci-yml.sh` (no other `run:`; `uses:` ends `@<40 hex>`; no `*-latest`; no `pull_request_target`; top-level `permissions: {}`; `fetch-depth: 0`); `docs/ci-reviews/LOG.md` row for first run id | Wave 0 gap |
| 3 | Six outcomes classify exactly; wrong value fails; missing/malformed/timeout/crash never pass; inventory change detected | CTest control matrix | `ctest --preset gate -L controls` (one test per matrix row, run twice, `cmp` canonical bytes; row count equals `tests/controls/INVENTORY`) | Wave 0 gap |
| 4a | Core reading time/files/threads/entropy/mutable global fails both layers | seeded | lint selftest: each `bad_*.c` must exit nonzero and print its `EXPECT:` rule id; Layer-2 selftest compiles each `bad_*.c` with lint flags | Wave 0 gap |
| 4b | Protected/unmanifested binary in tree, in range, or in staging fails | seeded temp repos | `rights-scan.sh --selftest` builds ROM-magic, NUL-late, Latin-1, UTF-16, add-then-delete, LFS pointer, gitlink, symlink and archive cases at run time and asserts class per case | Wave 0 gap |
| 4c | Manifest entry missing any required field fails | seeded | `t64-manifest-check` over `tests/seeded/manifest/bad_*.json` (one per field, one per cross rule); asserts rule id | Wave 0 gap |

### Negative controls and green-on-empty (all required)
- Lint: zero files scanned -> failure (the one stub file in `core/` prevents a vacuous pass). Rights: zero blobs/files scanned -> failure. Manifest: zero entries -> failure. Controls: zero tests (`noTestsAction: error`, measured rc 8).
- Each rule id in `rules.txt` has at least one seeded violation (Pattern 1); a rule with none fails the selftest.
- Positive controls: `good.c` exits 0 on gcc, clang, Apple clang; a clean temp repo with only text passes the rights scan; a valid manifest passes.
- Control matrix (CONTEXT list): positive, wrong counts, failure after Done, deleted fixture, tampered digest, truncated verdict, non-UTF8/over-long output, spin, empty output, abort, segv, exit0-no-result, hang, dropped inventory case, truncated suite; each asserts exact outcome and exit code, each runs twice (different slice size) and canonical bytes must match.
- Supervisor timing cases use a small `--wall-ms` (700 ms measured fine) so the whole matrix stays well under a minute.

### Sampling Rate
- **Per task commit:** the relevant quick command above (< 30 s).
- **Per wave merge:** `./scripts/gate.sh` locally.
- **Phase gate:** full gate green on both `gate-linux` and `gate-macos`, the first run recorded in `docs/ci-reviews/LOG.md`, and the Rust spike result recorded, before `/gsd-verify-work`.

### Wave 0 Gaps
- [ ] `CMakeLists.txt`, `CMakePresets.json`, `tests/t64_test.h`, `tools/common/*` (JSON, SHA-256 with FIPS 180-4 vectors, UTF-8 with RFC 3629 negatives)
- [ ] `scripts/gate.sh` skeleton (LC_ALL=C, versions banner, timed steps) and `scripts/check-docs.sh`, `scripts/check-ci-yml.sh`
- [ ] Seeded directories `tests/seeded/{manifest,provenance,claims}` and `tools/lint/selftest/`
- [ ] Planning note: ADR-0002 must be accepted before the first `core/*.c`

## Security Domain

`security_enforcement` is on, ASVS level 1, block on high [VERIFIED: .planning/config.json:20-22: `"security_enforcement": true`, `"security_asvs_level": 1`, `"security_block_on": "high"`].

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2/V3 Authentication, Session | no | no network service or accounts |
| V4 Access Control | yes (CI) | top-level `permissions: {}`, per-job `contents: read`, `persist-credentials: false`, never `pull_request_target`, fork-PR approval for all outside contributors |
| V5 Input Validation | yes | bounded strict JSON parser (size, depth, duplicate keys, UTF-8), manifest paths reject `..`, absolute paths and symlinks; manifest/result files are untrusted input to the tools |
| V6 Cryptography | limited | SHA-256 for identity digests only (A6); no keys, no signing |
| V12 Files and Resources | yes | never recurse into untrusted archives; artifact staging scanned before any `upload-artifact`; reject symlinks in staging |
| V14 Configuration / supply chain | yes | SHA-pinned actions (`actions/checkout` v7.0.1 -> `3d3c42e5aac5ba805825da76410c181273ba90b1` [VERIFIED: `gh api repos/actions/checkout/git/ref/tags/v7.0.1` returned that sha, release published 2026-07-20]), explicit runner labels, no third-party actions in Phase 1, no caching |

### Known Threat Patterns
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Protected bytes smuggled through history, a rename, text encoding or an archive | Tampering / Info disclosure | History-wide `rev-list --objects` scan, default-deny non-text, container rejection, advisory encoded-payload flags |
| Malformed or oversized JSON crashing a tool | DoS | Hard limits, no recursion beyond depth 16, fuzz-style negative cases, ASan build of the tools in the spike |
| Hostile PR altering the gate to pass | Tampering | CI runs only `./scripts/gate.sh`; the `ci.yml` self-check; gate and workflow changes need review (CODEOWNERS is optional, not locked) |
| Path traversal via manifest paths | Tampering | Reject `..`, absolute and symlinked paths before any file open |
| Crash controls leaking host info in results | Info disclosure | Canonical result excludes paths, hostname and wall time (locked); `run-meta.json` is non-canonical and not uploaded |

## State of the Art

| Old Approach | Current Approach | Impact |
|--------------|------------------|--------|
| `.reuse/dep5` | `REUSE.toml` (spec 3.3; dep5 deprecated, mutually exclusive) | Use `REUSE.toml` or `.license` sidecars if any annotation is needed |
| Plain `ctest` | Test preset `noTestsAction: error` (presets v6, CMake 3.25) | No green-on-empty |
| `-U_FORTIFY_SOURCE` alone | plus `-D_FORTIFY_SOURCE=0` on Apple SDKs | See Pitfall 1 |

## Sources

### Primary (HIGH)
- https://cmake.org/cmake/help/v3.25/manual/cmake-presets.7.html (version table, workflow presets, noTestsAction)
- https://reuse.software/spec-3.3/ (minimum header, LICENSES/, REUSE.toml)
- `adr/madr` tag 4.0.0 via `gh api` (template, `MIT OR CC0-1.0` in LICENSE and README)
- https://www.rfc-editor.org/rfc/rfc8785 (sorting, whitespace, escapes, IEEE 754 numbers)
- `actions/runner-images` READMEs via `gh api` (macos-26 arm64: Apple clang 21.0.0, CMake 4.4.5, Ninja 1.13.2; Ubuntu 24.04: CMake 3.31.6, gcc 13.3.0, clang 18.1.3, dash 0.5.12, Ninja 1.13.2, Git 2.56.0; labels `macos-26` = arm64, `ubuntu-24.04` = x64)
- `gh api` for actions/checkout tag, jobs timing fields, `szTheory/tendies64` visibility and Actions permissions
### Secondary (MEDIUM)
- https://github.com/zserge/jsmn README and `gh api` metadata (license MIT; no validation claims)
### Experiments (MEASURED, session scratch, not retained in the repo)
nm/size/section behavior (3 compilers, 2 arches); lint prototype on 3 awks; supervisor 9-case run; CMake layout on 3.25.1/3.28.3/4.4.3; blob enumeration and classifier; iconv/tr behavior; Rust pilot (rustc 1.91.1).

## Metadata

**Confidence breakdown:**
- Portability and lint mechanics: HIGH (run on both OSes; x64 only via emulation)
- CMake/presets: HIGH (3.25.1 exercised)
- Supervisor: HIGH for classification; MEDIUM for output bounding (not run)
- GOV-07: HIGH for enumeration/classification; advisory heuristics are design, not measured
- Rust spike: MEDIUM (pilot only)
- JSON/manifest design: MEDIUM (design recommendation; not built beyond the pilots)

**Research date:** 2026-11-09. **Valid until:** about 2026-12-09 for runner-image-dependent facts (images update weekly); toolchain behavior facts are stable.
