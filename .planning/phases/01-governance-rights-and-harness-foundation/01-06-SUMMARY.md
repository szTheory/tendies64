---
phase: 01-governance-rights-and-harness-foundation
plan: "06"
subsystem: governance
tags: [c17, posix-sh, git, rights-scan, pii-scan, ctest, gov-07]
status: complete

requires:
  - phase: 01-governance-rights-and-harness-foundation
    provides: "plan 01-01 gate, CMake presets, tools glob, t64_common SHA-256"
provides:
  - "t64-blobscan: streaming byte classifier for git cat-file --batch streams and NUL-separated file lists"
  - "scripts/rights-scan.sh: GOV-07 driver over tree, all reachable history (or a range) and a staging directory, with a 49-case seeded --selftest"
  - "scripts/pii-scan.sh: private-path and email scan over tree, history diffs, commit messages and commit metadata, with redacted output and a 21-case --selftest"
  - "provenance/docs-assets.tsv: empty docs-asset allowlist (header comments only)"
  - "CTest gov.rights.selftest, gov.rights.repo, gov.pii.selftest, gov.pii.repo (label gov)"
affects: [01-07, 01-09, 01-10, 01-11, 01-12, 01-13]

actuals:
  tokens: 14600
  tasks: 3
  commits: 3
plan_head_before: b6adbb707af17998c35bad4edc7f123943998449
plan_head_after: 93616ad19900c4d9a549b3947fc57167f0eb7313

tech-stack:
  added: []
  patterns:
    - "Seeded protected-looking inputs are generated with printf into a mktemp root at selftest time and removed on exit; nothing protected-looking is ever committed"
    - "Every driver pipeline stage goes through a temp file whose status is checked (dash masks pipeline failures)"
    - "Scanner output carries rule id, path and oid only (blobscan) or a 3-character redaction (pii-scan), never content"

key-files:
  created:
    - tools/rights/CMakeLists.txt
    - tools/rights/t64_blobscan.c
    - scripts/rights-scan.sh
    - scripts/pii-scan.sh
    - provenance/docs-assets.tsv
  modified: []

key-decisions:
  - "Allowlisted digests waive content classes only; R-DENIED-EXT is policy on the name and is never waived (seeded case allowlist-keeps-denied-ext)"
  - "R-ZERO-SCANNED and P-ZERO-SCANNED exit 2 (harness error); every other rule exits 1"
  - "Blob names for history come from rev-list --objects plus every (blob, path) pair in git log -m --raw, because rev-list names a shared blob only once (measured: two identical files, one name listed)"
  - "R-GITLINK in history is read from mode-160000 entries of the raw log instead of running ls-tree over every commit; same set, one pass"
  - "The history stream goes through a temp file rather than a fifo, so a git failure is a checked status; fine at Phase 1 size"
  - "Paths git quotes (leading and trailing double quote) are judged for the denied extension without the closing quote"

patterns-established:
  - "Selftest cases print 'ok NAME (RULE)'; a missing rule id or a stray RULE line fails the case"
  - "Seeded private paths and addresses are assembled from printf fragments so the script source never contains them and the scan cannot trip on its own controls"

requirements-completed: [GOV-07]

coverage:
  - id: D1
    description: "A ROM-magic blob in any byte order (named .txt), in the tree, in add-then-delete history, or under a rename (both names) fails the rights scan by rule id; shallow clones and zero-scanned inputs fail"
    requirement: "GOV-07"
    verification:
      - kind: integration
        ref: "sh scripts/rights-scan.sh --selftest (cases rom-z64-tree, rom-v64-tree, rom-n64-tree, rom-add-then-delete, rename, shallow, zero-scanned, zero-scanned-batch); ctest gov.rights.selftest"
        status: pass
    human_judgment: false
  - id: D2
    description: "NUL head and late, invalid UTF-8 (including across the read-buffer edge), denied extensions, seven container formats, LFS pointers and attributes, gitlinks and symlinks each fail by rule id; advisories do not fail"
    requirement: "GOV-07"
    verification:
      - kind: integration
        ref: "sh scripts/rights-scan.sh --selftest (49 cases, failures=0)"
        status: pass
    human_judgment: false
  - id: D3
    description: "A ROM-magic file or a NUL-bearing file in an artifact staging directory fails rights-scan.sh --staging DIR (R-ROM-MAGIC, R-NUL-HEAD); the CI upload-staging step is deferred until a job first uploads"
    requirement: "GOV-07"
    verification:
      - kind: integration
        ref: "selftest cases staging-rom-magic, staging-nul, staging-symlink, staging-fifo, staging-empty, staging-clean"
        status: pass
    human_judgment: false
  - id: D4
    description: "Private home paths, macOS/OS temp paths and non-noreply emails in tracked files, history diffs, messages and commit metadata fail the PII scan with redacted matches; the repository itself is clean"
    requirement: "GOV-07"
    verification:
      - kind: integration
        ref: "sh scripts/pii-scan.sh --selftest (21 cases); sh scripts/pii-scan.sh (tree_files=127 commits=34 findings=0)"
        status: pass
    human_judgment: false
  - id: D5
    description: "Both scans run in every gate run as CTest tests labeled gov, and seeded inputs never touch the repository"
    requirement: "GOV-07"
    verification:
      - kind: integration
        ref: "./scripts/gate.sh ([gate] PASS); ctest --preset gate -L gov -N lists four tests; selftest-root is removed and no t64.* path exists in the repository"
        status: pass
    human_judgment: false

duration: 8min
completed: 2026-11-10
---

# Phase 1 Plan 06: GOV-07 rights scan and PII scan Summary

**A streaming C byte classifier plus a POSIX sh driver fail the gate, by rule id, on ROM magic in any byte order, NUL or invalid UTF-8 anywhere in a blob, denied extensions, opaque containers, LFS pointers, gitlinks and symlinks, in the working tree, in all reachable history (add-then-delete and renames included) and in a staging directory; a separate scan fails on private paths and non-noreply emails with redacted output.**

## Accomplishments

- Tracer slice first (Task 1): ROM magic end to end through `t64-blobscan`, `rights-scan.sh` (tree + history), the CTest `gov` tests and seeded temp-repo controls, re-verified before expansion.
- `t64-blobscan` (703 lines, C17, links `t64_common`): constant 64 KiB buffer, validated batch headers and sizes, short reads and malformed streams are harness errors (exit 2). Every name of a shared blob is reported. Streaming RFC 3629 validator carries partial sequences across buffer boundaries; byte-literal-run and 4032-byte advisories never fail the scan.
- `rights-scan.sh`: tree, history, `--range`, `--staging`, `--allow`, `--selftest`; driver rules `R-SHALLOW`, `R-NOT-GIT`, `R-LFS-ATTR`, `R-GITLINK`, `R-SYMLINK` (staging). `provenance/docs-assets.tsv` is added to the allowlist automatically when present.
- `pii-scan.sh`: `P-PRIVATE-PATH`, `P-EMAIL`, `P-COMMIT-EMAIL`, `P-ZERO-SCANNED` (plus `P-SHALLOW`, see deviations). Tree scan includes untracked non-ignored files. A diff line that looks like a `+++` header is still scanned.
- Both selftests print `selftest-root:` first, generate every seed in a mktemp root outside the repository, and remove it on exit.

## Measured results

| Item | Value |
|------|-------|
| `sh scripts/rights-scan.sh --selftest` | cases=49 failures=0 (plan floor: 30 ok lines); about 5-8 s |
| `sh scripts/pii-scan.sh --selftest` | cases=21 failures=0; about 2.5 s |
| `sh scripts/rights-scan.sh` on this repository | `tree=126 history=171 findings=0` |
| `sh scripts/pii-scan.sh` on this repository | `tree_files=127 commits=34 findings=0` |
| `./scripts/gate.sh` (Darwin arm64, Apple clang 21, CMake 4.4.3, git 2.41.0, BWK awk 20200816) | `[gate] total 16s`, `[gate] PASS`; steps: sh-n 0s, lint-selftest 4s, configure 0s, build 0s, ctest 12s (15 tests), core-lint 0s |
| `ctest --preset gate -L gov -N` | 4 tests: gov.rights.selftest, gov.rights.repo, gov.pii.selftest, gov.pii.repo |

Mutation sanity checks (scratch copies, not committed): not carrying UTF-8 state across chunks failed `straddle`; moving the NUL head bound failed `utf16-bom`, `nul-head-edge`, `staging-nul`; reporting only the first name of a blob failed `rename`; narrowing the bzip2 digit range failed `bzip2`; disabling the extension rule failed three cases. For the PII scan: dropping the `/private/tmp` pattern failed `tree-private-tmp`, treating `+++`-prefixed lines as headers failed `history-header-lookalike`, skipping the allowlist check failed three email cases, and not redacting failed both redaction cases.

## Task Commits

1. **Task 1 (tracer): ROM-magic GOV-07 end to end** - `7402e67`
2. **Task 2: remaining classes, staging, LFS, gitlink, symlink, advisories** - `b353612`
3. **Task 3: PII and private-path scan, gov.pii tests** - `93616ad`

## Staging scan (ROADMAP SC4, artifact leg)

Phase 1 uploads no CI artifacts, so no CI step scans a staging directory yet. `rights-scan.sh --staging DIR` is proven by the seeded cases `staging-rom-magic` (R-ROM-MAGIC), `staging-nul` (R-NUL-HEAD), `staging-symlink`, `staging-fifo`, `staging-empty` and `staging-clean`, which run inside `gov.rights.selftest` in every local and CI gate run. The CI step becomes real when a job first uses upload-artifact (recorded in docs/policy/rights-and-fixtures.md by plan 01-04). The phase verifier and phase summary should carry this statement.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Names for shared blobs need the raw log, not rev-list alone**
- **Found during:** Task 1 design, confirmed by measurement
- **Issue:** `git rev-list --objects` lists a blob once even when two paths share it (two identical files: one name printed). The plan's `rename` case ("R-ROM-MAGIC for both names") cannot pass from rev-list names alone.
- **Fix:** the names file is the sorted union of rev-list output and every `(new blob oid, path)` pair from `git log -m --raw --no-abbrev --no-renames`. The oid set to scan still comes from rev-list.
- **Files modified:** scripts/rights-scan.sh
- **Commit:** 7402e67

**2. [Rule 2 - Missing critical] `P-SHALLOW` added to the PII scan**
- **Found during:** Task 3
- **Issue:** the PII history scan over a depth-1 clone would silently cover one commit, the vacuous-scan failure mode T-01-23 names for the rights scan.
- **Fix:** `pii-scan.sh` reports `RULE P-SHALLOW history` (exit 2) when history is requested on a shallow repository. This id is not in the plan's closed rule list; it is seeded (`shallow` case).
- **Files modified:** scripts/pii-scan.sh
- **Commit:** 93616ad

### Interpretation notes (no conflict with a measured fact)

- `R-GITLINK` in history reads mode-160000 entries from the raw log used for names, instead of `git ls-tree -r` per commit. The set is the same (a gitlink enters history only as such an entry) and it costs one pass; the plan's "affordable at Phase 1 size" caveat no longer applies.
- The history stream is staged in a temp file (not a fifo), as the plan states; disk use is roughly the size of all text blobs, acceptable now, a candidate for a fifo if history grows large.
- Selftest cases beyond the plan list (49 vs 30 for rights, 21 vs 14 for PII): byte-order and edge cases, `rom-deleted-tree-only`, `nul-head-edge`, `straddle-truncated`, `straddle-broken`, bzip2/zstd/7z, `lfs-attr-history`, `gitlink-history`, `tree-symlink`, `staging-fifo`, `byte-literal-short`, allowlist variants (`allowlist-wrong-sha`, `allowlisted-history`, `allowlist-keeps-denied-ext`), `tree-windows-path`, `prose-allowed`, `history-header-lookalike`, `zero-scanned`, `shallow`.
- `P-ZERO-SCANNED` and `R-ZERO-SCANNED` exit 2; the plan's exit table lists `R-ZERO-SCANNED` under harness errors and the PII scan follows it.
- The Task 1 acceptance command `ctest --preset gate -R gov.rights.repo` passes on this repository: every tracked file is text, no tracked file contains a NUL, and no tracked file has a denied extension.

**Total deviations:** 2 auto-fixed (1 blocking, 1 missing-critical) plus the interpretation notes above. **Impact:** none on scope or on any acceptance criterion.

## Known Stubs

None.

## Threat Flags

None. The new surface is local scripts and one local C tool; no network endpoint, auth path or schema was added.

## Issues Encountered

- `printf` octal escapes in the selftest are the only source of seeded protected-looking bytes; confirmed no ROM-magic or NUL-bearing file and no denied-extension file is tracked (`git ls-files` checked), and `.planning/state.json` stays untracked as instructed.
- An empty `t64.*` directory older than this plan's start exists under the user temp directory; it predates this run and was left alone. Both selftests removed their own roots (verified by the plan's positive check).

## Next Phase Readiness

- Plan 01-13: run `sh scripts/pii-scan.sh` and `sh scripts/rights-scan.sh` before pushing (both also run in the gate as `gov.*`). CI already uses `fetch-depth: 0` (plan 01-04); the first CI run confirms both scripts on ubuntu-24.04 (gawk/mawk, dash, git 2.43) and `macos-26`. `git clone --depth 1 file://...` and `GIT_CONFIG_GLOBAL=/dev/null` are used by the selftests and need git 2.32 or newer.
- Plans that add committed seeds must keep them text: a seed that looks protected (ROM magic, NUL, container magic, denied extension) must be generated at test time, never committed (RESEARCH Pattern 2).
- A docs asset that must be binary is added as one row `path<TAB>sha256<TAB>reason` in `provenance/docs-assets.tsv` (history blobs match on the digest alone; tree and staging files on path and digest).
- Known limit: paths that git quotes (control characters, quotes) are shown in the log in git's quoted form; the denied-extension check strips the closing quote. A repository whose history has no blobs reports `R-ZERO-SCANNED`.

## Self-Check: PASSED

- FOUND: tools/rights/CMakeLists.txt, tools/rights/t64_blobscan.c, scripts/rights-scan.sh, scripts/pii-scan.sh, provenance/docs-assets.tsv
- FOUND commits: 7402e67, b353612, 93616ad (all ancestors of HEAD)
- Acceptance re-run: `sh scripts/rights-scan.sh` exit 0 with `history=171`; `ctest --preset gate -L gov -N` lists the four tests; `ok staging-rom-magic`, `ok staging-nul`, `ok tree-var-folders`, `ok tree-private-tmp`, `ok redaction` printed; the self-match grep on scripts/pii-scan.sh counts 0; `./scripts/gate.sh` prints `[gate] PASS`.
