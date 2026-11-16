---
phase: 01-governance-rights-and-harness-foundation
audited: 2026-11-10
asvs_level: 1
block_on: high
threats_total: 62
threats_closed: 58
threats_accepted: 4
threats_open: 0
status: secured
gate: "./scripts/gate.sh PASS (61/61 ctest, core-lint seeds 27/27) re-run during this audit"
---

# Phase 1 security audit

Method: each `mitigate` row was checked against code and a test or check that guards it (file:line or test name
below). `accept` rows are logged here with rationale (this table is the accepted-risk log). Summaries were not
taken as evidence. Live checks: `./scripts/gate.sh` (exit 0), `scripts/pii-scan.sh` (tree 327 files, 105 commits,
0 findings), `scripts/rights-scan.sh` (tree 327, history 502 blobs, 0 findings), and
`gh api .../fork-pr-contributor-approval` returned `all_external_contributors`.

All "tests" named below are CTest names in the gate (`ctest --preset gate`) unless noted.

| Threat | ASVS | Disposition | Status | Evidence |
|--------|------|-------------|--------|----------|
| T-01-01 | V6 | accept (low) | ACCEPTED | SHA-256 is an identity digest only (fixture, log, profile digests in the canonical result). FIPS 180-4 vectors pin correctness: `unit.common`, tools/common/tests/test_common.c:53-75. No key, MAC or signature use found. Reversal condition recorded in plan 01-01 |
| T-01-02 | V5 | mitigate | CLOSED | Writer errors E_ORDER, E_RANGE (+-2^53-1), E_UTF8, E_OVERFLOW each negative-tested: test_common.c:198-251 (`unit.common`) |
| T-01-03 | V14 | mitigate | CLOSED | Curated `-W*` list plus `option(T64_WERROR ... ON)` in CMakeLists.txt:10-20; `CMakePresets.json:11` sets ON; build is green under it |
| T-01-04 | V12 | mitigate | CLOSED | runner/t64_supervisor.c:672 `setpgid`, :675-678 RLIMIT_CORE=0 and RLIMIT_FSIZE, :657-660 saturating CLOCK_MONOTONIC deadline, :722-725 `kill(-pid,SIGKILL)` then blocking `waitpid` on every exit path (WR-04), output to files (:1, :2 dup2 of file fds). Tests: `controls.hang`, `controls.log-flood-backstop`, `controls.hostile.orphan`, `controls.hostile.wall-ms-max` |
| T-01-05 | V12 | mitigate | CLOSED | :238 `O_CREAT\|O_EXCL\|O_NOFOLLOW` 0600, :94 `.tmp` then rename, :607 `mkdtemp`, :456-467 `lstat`+S_ISREG+`FIXTURE_MAX_BYTES` (64 MiB) then `O_NOFOLLOW`+`fstat`. Tests: `controls.deleted-fixture`, `controls.tampered-digest`, `unit.classify` (`process-bad-fixture-is-usage`), `controls.hostile.result-tmp-fifo`, `controls.hostile.seal-tmp-symlink` |
| T-01-06 | V5 | mitigate | CLOSED | runner/t64_num.h:20-42 overflow-checked decimals (no sign, no leading zero); `T64_REPORT_MAX_BYTES 512` (t64_classify.h:141); exact-line parser; runner exit code does not decide outcome. Tests: `unit.classify` (u64 overflow, CRLF, oversize, report-invalid cases at test_classify.c:125-196), `controls.hostile.fifo-report`, `fifo-capture`, `symlink-report`, `short-capture` |
| T-01-07 | V8 | mitigate | CLOSED | Canonical object has a fixed key set with no free text: build_result writes only enums, digests, counts, `fixture_id`; `fixture_id` restricted to `[A-Za-z0-9._-]{1,64}` (supervisor `valid_fixture_id`, result_check.c:448). Host OS and `wall_ms` go only to non-canonical run-meta (supervisor `build_meta`). Guarded by `unit.result-goldens`, `unit.result-seeds` (exact key sets), and `controls.*` golden byte compares. Note: the plan's "goldens checked for path characters" is satisfied structurally (charset and key-set enforcement), not by a grep |
| T-01-08 | V14 | mitigate | CLOSED | Layer 2 allow-lists read-only sections (tools/lint/lint-objects.sh:164 L2-W-SECTION, :180 L2-W-COMMON, :195 L2-UNDEF-SYMBOL). Seed `tools/lint/selftest/bad_extern_host.c`. `core-lint.sh --selftest` is a gate step (seeds 27/27) |
| T-01-09 | V14 | mitigate | CLOSED | Coverage gate core-lint.sh:441-470 (every rule id has an EXPECT seed or a dedicated control; SELFTEST-RULE-UNPROVEN/UNKNOWN); L1-ZERO-FILES control :475 |
| T-01-10 | V10 | accept (low) | ACCEPTED | Lint compiles objects only (never linked or run) in a `mktemp` dir; CI job has read-only token and no secrets (T-01-12). core/ holds only two files |
| T-01-11 | V4 | mitigate | CLOSED | .github/workflows/ci.yml triggers are pull_request, push main, workflow_dispatch; sole run line `./scripts/gate.sh`. scripts/check-ci-yml.sh rules CI-PR-TARGET, CI-TRIGGER, CI-EXPR-IN-RUN, CI-RUN-LINE, each with a seeded case (:219-294). Tests `repo.ci-yml`, `repo.ci-yml-selftest` |
| T-01-12 | V4 | mitigate | CLOSED | ci.yml: `permissions: {}`, per-job `contents: read`, `persist-credentials: false`, no secrets. Rules CI-PERMISSIONS-TOP/JOB, CI-PERSIST-CREDS, CI-SECRETS (all spellings), CI-UPLOAD-ARTIFACT seeded. Same tests |
| T-01-13 | V14 | mitigate | CLOSED | Only action is `actions/checkout@3d3c42e5...` (40-hex pin, ci.yml); CI-USES-PIN seeds (:226-228). No third-party actions |
| T-01-14 | V14 | accept (medium) | ACCEPTED | CI runs only gate.sh; control is review of scripts/, tools/, tests/, CMake diffs plus fork-PR approval (T-01-46, live-verified). CODEOWNERS covers the three waiver files. Limit: no branch protection, so review is not enforced by GitHub (residual R-5) |
| T-01-15 | V1 | mitigate | CLOSED | scripts/check-docs.sh rules DOC-ADR-STRUCT, DOC-STATUS-MISMATCH, DOC-ADR-MISSING, DOC-ADR-NOT-ACCEPTED, DOC-CORE-BEFORE-ADR (exact-path allowlist, content never consulted; selftest cases (a)-(f) at :370-408). Tests `repo.docs`, `repo.docs-selftest`. ADR-0002 is `accepted` |
| T-01-16 | V8 | mitigate | CLOSED | `gov.pii.repo` (tree and history) and `gov.pii.selftest` run in the gate; scan clean at audit time |
| T-01-20 | V12 | mitigate | CLOSED | rights-scan.sh scans all reachable history (HEAD, branches, tags, remotes) with oid dedup. Seeds `rom-add-then-delete`, `rom-deleted-tree-only`, `history-side-branch`, `history-tag-only`, `rename` (selftest :455-731). Tests `gov.rights.selftest`, `gov.rights.repo` |
| T-01-21 | V12 | mitigate | CLOSED | R-NUL-HEAD/R-NUL-LATE/R-INVALID-UTF8 in tools/rights/t64_blobscan.c; seeds `utf16-bom`, `nul-late(-history)`, `nul-head-edge`, `latin1`, `overlong`, `straddle*` (:511-562) |
| T-01-22 | V12 | mitigate | CLOSED | R-CONTAINER (zip, gzip, xz, bzip2, zstd, 7z, tar), R-LFS-POINTER, R-LFS-ATTR (incl. `-diff` attribute, CR-12), R-GITLINK, R-SYMLINK; seeds :669-742 |
| T-01-23 | V14 | mitigate | CLOSED | R-SHALLOW seed `shallow` (:485), R-ZERO-SCANNED (blobscan.c:783; seeds :489-490, :754), P-ZERO-SCANNED/P-SHALLOW (pii-scan.sh:183,259,320; seeds :598,605). CI `fetch-depth: 0` enforced by CI-FETCH-DEPTH |
| T-01-24 | V7 | mitigate | CLOSED | blobscan prints only rule, id, path, oid (blobscan.c:105-107); pii-scan prints first 3 chars only (pii-scan.sh header). Seeds are generated under `mktemp -d` with an EXIT/INT/TERM trap (rights-scan.sh:417-420); no protected hashes committed (rights scan clean). Residual: path names are printed (acceptable) |
| T-01-25 | V5 | mitigate | CLOSED | 64 KiB constant streaming buffer (blobscan.c:44, 625, 679); short batch read is a harness error (:647 `die`); ASan preset exists (CMakePresets.json `asan`) and was run per REVIEW-DISPOSITION |
| T-01-26 | V8 | mitigate | CLOSED | pii-scan.sh PATH_RE covers /Users, /home, /root, /Volumes, /private/tmp, /var/folders, drive paths; scans tree, diffs, messages, author/committer. Runs in every gate (`gov.pii.repo`). Waiver is 1 row, exact count, tree and messages excluded (tools/rights/pii-history-waivers.tsv, residual R-3) |
| T-01-27 | V5 | mitigate | CLOSED | runner/t64_result_check.c exact key sets, schema_version, canonical-bytes check, R-SYNTHETIC-STAMP (:533-541). Seeds `bad_unknown_field`, `bad_missing_field`, `bad_not_canonical`, `bad_version`, `bad_synthetic_stamp`, `bad_dup_key` (`unit.result-seeds`) |
| T-01-28 | V11 | mitigate | CLOSED | S-TRUNCATED (:853-886), S-EMPTY (:900), S-DUPLICATE (:920), S-ORDER, S-INVENTORY (:936). Seeds `truncated`, `dropped`, `duplicate`, `bad_empty`, `bad_order`, `bad_set`, `bad_hash` (`unit.suite-seeds`); live `controls.suite-dropped`, `controls.suite-truncated`, `controls.inventory-drift` |
| T-01-29 | V1 | mitigate | CLOSED | PREREG added in 9390f0b before RESULTS in 0063abd (git log). `run-spike.sh --verify-frozen` (:296-340: committed, clean, frozen hashes, image digest prefix). Decision made three-valued by Amendment 1; `repo.spike-decision-selftest` in gate. A-3 owner escalation recorded in ADR-0002:75. Decision evidence limits disclosed (residual R-1) |
| T-01-30 | V14 | accept (low) | ACCEPTED | Spike evidence only, never in gate or CI; `--network none`, `:ro` mount (run-spike.sh:95-97); image digest prefix recorded in PREREG |
| T-01-31 | V14 | mitigate | CLOSED | Spike code only under docs/spikes; no CMakeLists references it except `repo.spike-decision-selftest`, which runs the script's synthetic selftest and builds nothing (tools/repocheck/CMakeLists.txt:20). Note: the plan's "verify guard" was a plan-time check, not a standing gate check; the standing control is code review (CODEOWNERS does not cover it). Medium severity, left CLOSED on structure; see residual R-9 |
| T-01-32 | V12 | mitigate | CLOSED | t64_manifest_check.c lexical `..`/absolute check (:176) then per-component `lstat` (:191-210). Seeds `bad_path_traversal`, symlink cases in `gov.manifest.seeds`, `gov.manifest.require-fixtures` |
| T-01-33 | V8 | mitigate | CLOSED | M-FORBIDDEN-DIGEST (t64_manifest_check.c:365) and M-FORBIDDEN-REASON; seeds `bad_forbidden_digest`, `bad_forbidden_reason`, `bad_null_digest_cleared`; committed manifest has no digests (`gov.manifest.repo`) |
| T-01-34 | V5 | mitigate | CLOSED | tools/manifest/t64_schema.c:265 "keyword is outside the supported subset", :184 unanchored pattern, :229 additionalProperties; `gov.manifest.require-fixtures` proves refusal on a temp schema; `gov.manifest.schema-seeds`; WR-07 re-validation guarded by `unit.schema-sanitize` |
| T-01-35 | V12 | mitigate | CLOSED | Allowlist rows only for authored-here/cleared + commit + path + sha (manifest_check.c:373-445); emits nothing on findings (`require-fixtures` "emit-allowlist-empty-on-findings", :113); rights seeds `manifest-digest-mismatch` (exit 2), `manifest-invalid` (exit 2), `manifest-unlisted` fails, `manifest-allowlisted` passes |
| T-01-36 | V10 | mitigate | CLOSED | scripts/check-provenance.sh + tools/provenance: P-UNRECORDED-COPY, P-SPDX-NOT-ALLOWED, P-SHA, P-OWNER-MISSING/EMAIL, P-HEADER, P-LICENSE-TEXT; all 16 rule ids seeded in tests/seeded/provenance-selftest.sh (`gov.provenance.selftest`, `gov.provenance.repo`) |
| T-01-37 | V12 | mitigate | CLOSED | P-PATH (9 seeded cases), P-PATH-MISSING, P-DUP-PATH in the same selftest |
| T-01-38 | V12 | mitigate | CLOSED | RLIMIT_FSIZE backstop supervisor:676-678; `controls.log-flood-backstop` (runner ignores limit, killed); `controls.log-over-limit`; files not pipes; fstat before read (:250-254) |
| T-01-39 | V8 | mitigate | CLOSED | RLIMIT_CORE=0 (supervisor:674-675); tests/controls/README.md:101-108 documents the macOS `.ips` side effect. The executor's one-time deletion of `.ips` files is not repo-verifiable and 12 `t64*` reports exist locally from gate runs; low severity, local only, never uploaded |
| T-01-40 | V11 | mitigate | CLOSED | matrix.tsv pins outcome, detail and exit per case; byte compare against goldens (run-case.sh:106-108); INVENTORY pin with live `controls.suite-dropped`, `suite-truncated`, `inventory-drift`, `inventory` |
| T-01-41 | V11 | mitigate | CLOSED | run-case.sh runs slices 1 and 7 and `cmp`s both (:106 "slice-independence"); harness-error results null capture-dependent fields (goldens e.g. abort.json) |
| T-01-42 | V11 | mitigate | CLOSED | scripts/check-claims.sh C-SYNTHETIC, C-EVIDENCE-SYNTHETIC, C-EVIDENCE-MISMATCH via `t64-result-check identity`; seeds `bad_synthetic_tier/profile/build`, `bad_evidence_synthetic` (`gov.claims.seeds`, `gov.claims.selftest`, `gov.claims.repo`) |
| T-01-43 | V11 | mitigate | CLOSED | C-RESERVED-WORD (6 uses), C-STATEMENT; seeds `bad_reserved_prose`, `bad_reserved_in_block`, `bad_statement` |
| T-01-44 | V12 | mitigate | CLOSED | check-claims.sh:109 dot-segment rejection, :314 per-component symlink check, :321 `-f` check; `C-EVIDENCE-INVALID` selftest cases (:579) |
| T-01-45 | V8 | mitigate | CLOSED | Pre-push sequence (gate, CI-mode gate, rights scan, PII tree+history, PII over origin/main..HEAD) executed for each push per 01-13-SUMMARY; independently re-run at audit: both scans 0 findings over all 105 commits and every ref. Gate also runs both scans (`gov.pii.repo`, `gov.rights.repo`) in CI with full history. Limit: procedural, no git pre-push hook installed (residual R-8) |
| T-01-46 | V4 | mitigate | CLOSED | Live read-back at audit: `approval_policy = all_external_contributors`. Workflow has `permissions: {}`, contents:read, no secrets (T-01-12) |
| T-01-47 | V4 | mitigate | CLOSED | scripts/ci-review.sh uses `gh api` reads only (no `-X`/`--method`); the single PUT is documented in 01-13-SUMMARY; no token appears in tracked files (pii-scan, ci-yml CI-SECRETS) |
| T-01-48 | V7 | mitigate | CLOSED | Canonical results carry no paths or hostnames (T-01-07); scanners redact or print only rule/path/oid (T-01-24); CI logs read in 01-13 contain tool versions and the banner only |
| T-01-49 | V14 | mitigate | CLOSED | Every check has a selftest in the gate; ci-review LOG records zero red rounds and no fix-up commits; CI-RUN-LINE forbids extra CI commands. Process control, no mechanical anti-weakening check beyond selftests and review |
| T-01-SC-01 to T-01-SC-13 (13 rows) | V10/V14 | mitigate | CLOSED | No package manifests or lockfiles, vendored trees or submodules in `git ls-files` (326 files); no `npm/pip/cargo/FetchContent/ExternalProject` use in scripts, tools, tests, runner, CMake, workflows or spikes (grep clean); only `actions/checkout` (SHA-pinned) is fetched in CI, enforced by CI-USES-PIN and CI-RUN-LINE; copied code is zero (provenance/copies.json checked by `gov.provenance.repo`); jsmn not vendored (hand-written tools/common/t64_json.c). Repo-level `sha_pinning_required` is false (owner setting, F-015) |
| T-01-17 | V5 | mitigate | CLOSED | Limits 1 MiB, depth 16, string 4096, nodes 4096, key 127 (t64_json.h:30-36) each with a negative test: test_json.c:259-330 (`unit.json`) |
| T-01-18 | V5 | mitigate | CLOSED | `unit.json-fuzz` runs 200000 fixed-seed mutants (test_json_fuzz.c:21) in the gate; `asan` preset with `-fno-sanitize-recover=all` (CMakePresets.json:23); CR-01 and WR-07 memory-safety findings fixed with `unit.schema-sanitize` |
| T-01-19 | V5 | mitigate | CLOSED | test_json.c:202-212 rejects floats (E_FLOAT), exponents (E_EXPONENT), 2^53 and -2^53 (E_INT_RANGE); u64 only as decimal strings (t64_num.h) |

## Unregistered threat flags

None. Every `## Threat Flags` section in the 13 SUMMARY files is "None" or maps to a register id.

## Residual risks (known, disclosed, not OPEN)

| Id | Risk | Why it is not OPEN | Tracking |
|----|------|--------------------|----------|
| R-1 | ADR-0002 "Evidence limits": the language decision (C17) rests on a spike whose tests missed real memory-safety bugs found in review (CR-01, WR-07) | Disclosed in the accepted ADR; reversible default; no security claim depends on it | docs/adr/0002 :120; Phase 2 success criterion 6 (admission parser fuzz) |
| R-2 | WR-06: the supervisor hashes the fixture and the runner reopens it, so `fixture_digest` can attest different bytes (TOCTOU) | Synthetic fixtures only in Phase 1; no real ROM runs exist | docs/spec/result-protocol.md:263; ROADMAP Phase 3 criterion 6 |
| R-3 | PII history waiver for commit 097cc7e: 5 hits of a fictitious example name in an already-pushed immutable commit | Exact-count waiver, never applies to tree, messages or metadata; value is a placeholder, not private data | tools/rights/pii-history-waivers.tsv; REVIEW-DISPOSITION WR-12 |
| R-4 | No CI artifact staging scan: the `--staging` scanner exists and is selftested but no CI step calls it | CI uploads nothing; CI-UPLOAD-ARTIFACT refuses any upload step, so the capability stays closed until a scan step exists | scripts/check-ci-yml.sh:96; rights-scan.sh header |
| R-5 | `main` has no branch protection, so CODEOWNERS review and required gate checks are not enforced by GitHub (also weakens the T-01-14 acceptance) | Owner repository setting, not a code defect | LOG F-007 |
| R-6 | Deferred review infos: IN-03 (schema anchored-pattern check superficial), IN-04 (runner inherits duplicate stdout/stderr fds), IN-14 (PII scan does not read author names, tags, non-HEAD refs for names), IN-24, IN-25 | Info severity, recorded with reasons | 01-REVIEW-DISPOSITION.md |
| R-7 | `setsid` escapees: a runner descendant that starts a new session survives the process-group kill (WR-04); lint tool hashes are not recorded (WR-22) | Stated limits in the spec and docs; synthetic runner only | docs/spec/result-protocol.md; REVIEW-DISPOSITION |
| R-8 | Pre-push scan sequence is procedural (no git hook); a human or agent could push without it | CI re-runs both scans over full history, so a leak is detected on the first run, but public exposure would already have happened; owner stop rule in AUTONOMY.md | Suggest a `pre-push` hook or `scripts/pre-push.sh` in a later phase |
| R-9 | T-01-31 and T-01-49 rely on review rather than a standing mechanical gate check (no check that CMake never references docs/spikes; no check that selftests are not weakened) | Medium severity; structure currently correct | Consider a DOC-/repocheck rule in Phase 2 |
| R-10 | GitHub repo setting `sha_pinning_required` is false and workflow actions policy is `all` | Workflow currently pins its single action and CI-USES-PIN enforces it in-repo | LOG F-015 (owner decision) |
| R-11 | macOS `.ips` crash reports from the abort/segv controls land in the user's DiagnosticReports directory | Local only, never uploaded or read; documented | tests/controls/README.md:101 |

## Verdict

SECURED: 0 open threats at any severity (62 rows: 58 closed, 4 accepted, all accepted rows low or medium).
