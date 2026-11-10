---
phase: 01-governance-rights-and-harness-foundation
plan: "10"
subsystem: governance
tags: [c17, provenance, copy-policy, madr, spdx, posix-sh, ctest, gov-02]
status: complete

requires:
  - phase: 01-governance-rights-and-harness-foundation
    provides: "copy and provenance policy text and the ADR template (01-04), strict JSON parser (01-05), gate.sh and the tools glob (01-01), seeded-case conventions (01-06, 01-09)"
provides:
  - "tools/provenance/t64_provenance_check.c: t64-provenance-check --root DIR --copies FILE --files-from LIST, sixteen rule ids, copied-looking detection over each file's first 20 lines"
  - "provenance/copies.json: first real record, madr-4-0-0-headings (MIT, adapted), pinned to the peeled 4.0.0 commit"
  - "LICENSES/MIT.txt (the only file in LICENSES/) and the template's header tags in docs/adr/0000-template.md"
  - "scripts/check-provenance.sh: builds the NUL-separated file list from git and runs the checker in every gate run"
  - "tests/seeded/provenance-selftest.sh --bin PATH: 76 temp-tree cases asserting the exact rule-id set, plus a check that all 16 ids were exercised"
  - "CTest gov.provenance.repo and gov.provenance.selftest (label gov)"
affects: [01-11, 01-12, 01-13]

actuals:
  tokens: 12600
  tasks: 2
  commits: 2
plan_head_before: f0cacf0622c6be70f98b42df15b7be7e54d19635
plan_head_after: e660b7fb030aff65088f414b7109db53539bcea4

tech-stack:
  added: []
  patterns:
    - "Tag text is assembled from fragments in every checker and test source, so the repository scan never finds a header-form tag in a non-copied file"
    - "A selftest compares the exact set of rule ids a seeded tree prints with the set the case expects, so a cascading rule shows up as a failure instead of hiding behind a substring match"
    - "A record that does not parse stops the run after P-JSON; a record that parses but is malformed still scans the tree, so unrecorded copies are reported next to the field errors"
    - "Record and listed paths are checked lexically, then every component is lstat-ed, then the file is opened with O_NOFOLLOW"

key-files:
  created:
    - tools/provenance/CMakeLists.txt
    - tools/provenance/t64_provenance_check.c
    - scripts/check-provenance.sh
    - provenance/copies.json
    - LICENSES/MIT.txt
    - tests/seeded/provenance-selftest.sh
  modified:
    - docs/adr/0000-template.md

key-decisions:
  - "The MIT text comes from LICENSE.MIT at tag 4.0.0, not from LICENSE: at that tag LICENSE holds only the expression MIT OR CC0-1.0, and LICENSE.MIT holds the text MADR presents (copyright 2017-2022, two named authors, no email)"
  - "The record pins the peeled commit 2475fe1973f66a12aaf58a91d8fa7b42c0f5ea3d, not the annotated tag object 61cc5891d9e160c0db8b18d5fd45f9123a7a1c21; gh api repos/adr/madr/commits/4.0.0 returns the same commit id"
  - "upstream.revision must be 40 lowercase hex characters; a missing or short value is P-SHA, a missing maintenance_owner is P-OWNER-MISSING (not P-FIELD), so each seeded defect maps to the rule id the plan names"
  - "The holder test strips a leading Copyright word, (c), a copyright sign, year ranges and commas, then compares the rest to Tendies64 contributors exactly, so the holder Tendies64 contributors and others is copied-looking"
  - "Header tags count only at line start after optional whitespace and one comment leader from <!--, the C block opener, //, --, *, # and ;, in the first 20 lines (line 20 counts, line 21 does not; both are seeded)"
  - "A listed path that is deleted from the working tree is skipped without a finding, because git ls-files --cached still names it"

patterns-established:
  - "scripts/check-provenance.sh passes root and records as relative paths, so the CTest log carries no absolute path"
  - "The driver writes the list to a temp file so git's exit status is checked, and returns the checker's status without exec so the EXIT trap removes the temp directory"

requirements-completed: [GOV-02]

coverage:
  - id: D1
    description: "provenance/copies.json records the MADR 4.0.0 heading adoption with source, 40-hex SHA, license, verbatim notice, modifications, verification test, owner and upstream status, and the record is checked in every gate run"
    requirement: "GOV-02"
    verification:
      - kind: integration
        ref: "ctest --preset gate -R gov.provenance.repo; sh scripts/check-provenance.sh prints files=232 copied_looking=1 entries=1 errors=0"
        status: pass
    human_judgment: false
  - id: D2
    description: "Each of the sixteen rule ids fails the gate on a seeded tree, including the pre-vetted-but-not-allowed licenses, a stray LICENSES file, an archived upstream not owned by Tendies64 and symlink and traversal paths"
    requirement: "GOV-02"
    verification:
      - kind: integration
        ref: "sh tests/seeded/provenance-selftest.sh --bin build/gate/tools/provenance/t64-provenance-check (cases=76 failures=0 rules=16); ctest --preset gate -R gov.provenance.selftest"
        status: pass
    human_judgment: false
  - id: D3
    description: "The detector reads only each file's first 20 lines for line-start tags, so checker sources, policy prose and plan documents never self-trigger"
    requirement: "GOV-02"
    verification:
      - kind: integration
        ref: "sh scripts/check-provenance.sh prints copied_looking=1 over 232 files; selftest cases late-spdx, line20-spdx, line21-spdx and midline-spdx"
        status: pass
    human_judgment: false

duration: 25min
completed: 2026-11-09
---

# Phase 1 Plan 10: Per-copy provenance records Summary

**`t64-provenance-check` enforces `provenance/copies.json` over every file git knows about, with sixteen rule ids proven on 76 temp-tree cases, and the one copied structure (the MADR 4.0.0 heading adoption in the ADR template, MIT, pinned to a 40-hex commit) is recorded and checked in every gate run.**

## Performance

- **Duration:** about 25 min
- **Completed:** 2026-11-09 (local)
- **Tasks:** 2 (a tracer and an auto task)
- **Files:** 7 (6 created, 1 modified)

## Accomplishments

- Tracer slice first (Task 1): MADR facts fetched from primary sources, the record, `LICENSES/MIT.txt`, the template header, the checker, the driver and `gov.provenance.repo` were verified end to end (`cmake --preset gate && cmake --build --preset gate && ctest --preset gate -R gov.provenance.repo && sh scripts/check-docs.sh`) before the selftest was written. Auto mode applied: the tracer verify was re-run, passed, and expansion continued.
- `t64_provenance_check.c`: every rule id of the interfaces block. `P-JSON` (unreadable or not strict JSON, stops the run), `P-FIELD` (unknown or missing key, wrong type, bad date or mode or status, bad exposure item, duplicate id), `P-SHA`, `P-OWNER-MISSING`, `P-OWNER-EMAIL`, `P-SPDX-NOT-ALLOWED` (exact allowlist ISC, MIT, BSD-3-Clause, Unlicense), `P-UNRECORDED-COPY`, `P-PATH-MISSING`, `P-PATH` (absolute, `..`, `.`, empty segment, backslash, control byte, symlink at any component), `P-DUP-PATH`, `P-LICENSE-TEXT` (exact `LICENSES/<id>.txt`, present, regular, non-empty), `P-LICENSES-EXTRA`, `P-ARCHIVED-OWNER`, `P-HEADER`, `P-MODS-MISSING`, `P-ZERO-SCANNED`. Only the first 16 KiB and 20 lines of each file are read.
- `provenance/copies.json`: entry `madr-4-0-0-headings` for `docs/adr/0000-template.md`: upstream `https://github.com/adr/madr` path `template/adr-template.md` revision `2475fe1973f66a12aaf58a91d8fa7b42c0f5ea3d`, retrieved 2026-11-09, MIT, `LICENSES/MIT.txt`, verbatim notice, adapted, owner `szTheory`, upstream not archived (`gh api repos/adr/madr --jq .archived` printed `false`), exposure `[]`.
- `docs/adr/0000-template.md`: an HTML comment block right after the front matter carries the MADR copyright text line and the MIT license-identifier line (lines 8 to 11); `scripts/check-docs.sh` still passes.
- `tests/seeded/provenance-selftest.sh`: 76 cases, each in its own mktemp tree, tags assembled from fragments. Includes the three plan-named non-allowlisted ids (`gpl`, `apache-copy`, `bsd2`) plus MIT-0, 0BSD, Zlib and MPL-2.0, positive cases for the three other allowed ids, the 20-line boundary on both sides, every comment leader, symlink file and symlink directory records, a listed traversal path, a missing records file, and two zero-scan cases.

## Measured results

| Item | Value |
|------|-------|
| `git ls-remote https://github.com/adr/madr 'refs/tags/4.0.0^{}'` | `2475fe1973f66a12aaf58a91d8fa7b42c0f5ea3d`; the plain tag ref is the annotated tag object `61cc5891d9e160c0db8b18d5fd45f9123a7a1c21` |
| `gh api repos/adr/madr/commits/4.0.0 --jq .sha` | `2475fe1973f66a12aaf58a91d8fa7b42c0f5ea3d`, equal to the peeled SHA |
| `gh api 'repos/adr/madr/contents/LICENSE?ref=4.0.0'` | the single line `MIT OR CC0-1.0`; the text is in `LICENSE.MIT` and `LICENSE.CC0-1.0` at the same tag |
| `sh scripts/check-provenance.sh` | `provenance: files=232 copied_looking=1 entries=1 errors=0`, then `ok provenance/copies.json` |
| `grep -cE '"revision": *"[0-9a-f]{40}"' provenance/copies.json`; `grep -c '@' provenance/copies.json`; `ls LICENSES/` | 1; 0; `MIT.txt` |
| Selftest (`sh`, `dash`, `bash`) | `cases=76 failures=0 rules=16`, 76 `ok ` lines, all 16 ids named |
| `./scripts/gate.sh` (Darwin arm64) | `[gate] total 16s`, `[gate] PASS`; sh-n 0s, lint-selftest 2s, configure 1s, build 0s, ctest 13s (24 of 24 passed; label gov 10 tests), core-lint 0s |
| `ctest --preset asan -R gov.provenance` | both passed, no sanitizer output |
| `sh scripts/pii-scan.sh`; `sh scripts/rights-scan.sh` | `findings=0` (tree_files=232 commits=53); `findings=0` (tree=232 history=300) |

Mutation sanity checks on scratch copies (not committed). "caught" means the selftest failed:

| Mutation | Result |
|----------|--------|
| 20-line window widened to 21 and narrowed to 19; Apache-2.0 treated as copied-looking; holder compare turned into a prefix compare; `;` leader removed; `third_party/` rule removed; allowlist widened; header match, unrecorded rule, SHA length, empty license text, license name, mode-adapted modifications, duplicate id, duplicate path, email rule, archived-owner rule, LICENSES-extra rule, zero-scan rule, `..` segment rule, exposure date rule removed | caught |
| `lstat` symlink branch removed | equivalent mutant: a symlink is neither a regular file nor a directory under `lstat`, so the later type tests still refuse it |
| absolute-path branch removed | equivalent mutant: the empty first segment is still reported as `P-PATH` |
| exposure date rule removed | survived until `exposure-bad-date` was added, then caught |

## Task Commits

1. **Task 1: end-to-end provenance (tracer)** - `f760fd7` (feat)
2. **Task 2: temp-tree selftest for every rule id** - `e660b7f` (test)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] License text source file at the MADR tag**
- **Found during:** Task 1, step 1
- **Issue:** The plan says to decode `LICENSE` at tag 4.0.0. That file holds only the expression `MIT OR CC0-1.0`, not license text.
- **Fix:** Decoded `LICENSE.MIT` from the same tag with the same `gh api` call and used it as `LICENSES/MIT.txt`. This is the MIT text as MADR presents it, as the plan intends. No guessed text.
- **Files modified:** `LICENSES/MIT.txt`
- **Commit:** `f760fd7`

**2. [Rule 1 - Bug] `verification_test` names a command line the script does not accept**
- **Found during:** Task 1, step 4
- **Issue:** The plan's value `scripts/check-docs.sh DOC-TEMPLATE` implies a positional rule argument; `check-docs.sh` accepts only `--root` and `--selftest`.
- **Fix:** Recorded `repo.docs (scripts/check-docs.sh rule DOC-TEMPLATE)`: the CTest id that runs the check, plus the rule id. No test or script was changed.
- **Files modified:** `provenance/copies.json`
- **Commit:** `f760fd7`

**3. [Rule 3 - Blocking] Selftest expectations for malformed-but-parsable records**
- **Found during:** Task 2
- **Issue:** The `top-not-object` and `schema-wrong` cases first expected only `P-FIELD`, but a record set that parses scans the tree, so the unrecorded copy and the now-unreferenced `LICENSES/MIT.txt` are reported too. That is the intended behavior; only the expectation was wrong.
- **Fix:** Both cases expect `P-FIELD P-LICENSES-EXTRA P-UNRECORDED-COPY`.
- **Files modified:** `tests/seeded/provenance-selftest.sh`
- **Commit:** `e660b7f`

**4. [Rule 1 - Bug] `check-provenance.sh` leaked an absolute path into the log**
- **Found during:** Task 1 verification
- **Issue:** The first driver passed absolute root and records paths, so the `ok` line carried the checkout path into the CTest log.
- **Fix:** The driver runs from the repository root with `--root .` and `--copies provenance/copies.json`, and returns the checker's status without `exec` so its EXIT trap removes the temp directory.
- **Files modified:** `scripts/check-provenance.sh`
- **Commit:** `f760fd7`

**Total deviations:** 4 auto-fixed (1 plan-instruction mismatch measured at the tag, 1 field wording, 2 own-code corrections). **Impact:** none on scope or design; every acceptance criterion holds.

## Issues Encountered

- The rule id `P-ZERO-SCANNED` also exists in `scripts/pii-scan.sh` (plan 01-06). The plan fixes the name here; the two tools print different summary lines and are separate CTest tests, so there is no clash in the gate.
- The phase cannot close offline: this plan needed `git ls-remote` and `gh api` (RESEARCH I-6). Both succeeded.

## Authentication Gates

None.

## Known Stubs

None.

## Threat Flags

None. The checker adds no network endpoint, auth path or schema at a trust boundary. T-01-36 is mitigated by `P-UNRECORDED-COPY`, `P-SPDX-NOT-ALLOWED`, `P-SHA` and `P-OWNER-*`, each with a seeded case; T-01-37 by the lexical and `lstat` path checks (`path-traversal`, `path-absolute`, `path-dot-segment`, `path-symlink`, `path-symlink-dir`, `list-path-traversal`); T-01-SC-10 by installing nothing and copying no code.

## Next Phase Readiness

Ready for 01-11. Any later copied file needs, in the same change, a license-identifier line and a copyright-text line in its first 20 lines, its `LICENSES/<id>.txt`, and a `copies.json` entry, or `gov.provenance.repo` fails. `LICENSES/` may hold only texts that an entry names.

## Self-Check: PASSED

- Files found: `tools/provenance/CMakeLists.txt`, `tools/provenance/t64_provenance_check.c`, `scripts/check-provenance.sh`, `provenance/copies.json`, `LICENSES/MIT.txt`, `docs/adr/0000-template.md`, `tests/seeded/provenance-selftest.sh`.
- Commits found on HEAD: `f760fd7`, `e660b7f`. Measured commit count `f0cacf0..HEAD` is 2.
- Acceptance criteria re-run: `copied_looking=1 entries=1 errors=0`; revision grep 1; `@` grep 0; `LICENSES/` lists exactly `MIT.txt`; selftest 76 `ok ` lines, no `FAIL`, all 16 ids named; `./scripts/gate.sh` PASS.
