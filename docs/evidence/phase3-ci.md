# Phase 3 hosted CI evidence

This record tracks the exact-source Linux/GCC and macOS/Clang guest evidence
required before Phase 3 can claim cross-host agreement. A local run or a
mocked collection test is not hosted evidence.

## Publication contract

The existing gate-linux and gate-macos jobs remain the only hosted jobs.
Each runs ./scripts/gate.sh, then the real supervisor executes the qualified
hello-openipl3 fixture and emits one canonical t64.boot-evidence v1 record
whose nested result is a strictly checked t64.result v2 pass. The result
source revision must equal GITHUB_SHA; its capture must byte-match the
authored transcript Tendies64 hello via open IPL3 (newline terminated).

Before publication, the workflow scans build/evidence with the rights
scanner and scans a temporary visible copy of that same JSON with the PII
scanner. Each job uploads only its exact build/evidence/gate-<job>.json file.
No ROM, capture, raw log, manual, or tool binary is in the upload path.

The wrapper contains exactly schema, schema_version, commit_sha, run_id,
run_attempt, job_name, and result. The artifact name binds the same commit,
run, attempt, and job. Missing output fails the job.

## Upload action verification

The official actions/upload-artifact tag v4.6.2 resolves directly to
ea165f8d65b6e75b540449e92b4886f43607fa02. The GitHub API reports the
commit's signature verification as valid. The official action metadata at that
commit identifies it as “Upload a Build Artifact” and defines the name,
path, and if-no-files-found inputs used here. The [official release entry](https://github.com/actions/upload-artifact/releases/tag/v4.6.2),
[tag ref](https://api.github.com/repos/actions/upload-artifact/git/ref/tags/v4.6.2),
[pinned action metadata](https://github.com/actions/upload-artifact/blob/ea165f8d65b6e75b540449e92b4886f43607fa02/action.yml),
[pinned action README](https://github.com/actions/upload-artifact/blob/ea165f8d65b6e75b540449e92b4886f43607fa02/README.md),
and [workflow syntax documentation](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax)
are the sources checked.

## Exact-run collection and comparison

`sh scripts/collect-boot-evidence.sh --collect` checks the run and requested
attempt through the GitHub REST API, including repository, workflow path,
push event, exact source SHA and attempt number. It requires exactly one
completed successful `gate-linux` and `gate-macos` job with matching run,
attempt and SHA. It selects exactly the two artifact names for that run's
requested attempt. Properly identified artifacts from lower attempts may remain
listed and are not downloaded; their names, attempt ordering, source SHA, run
metadata, digest shape, size bound and unique IDs are still checked. The two
selected artifacts must be unexpired and bounded.
It downloads each exact artifact name into a fresh private directory with
`gh run download`, refuses links, special filesystem entries, nested
directories, extra files and JSON larger than 64 KiB, and validates each
record through the strict C checker. The pair is renamed into a new output
directory only after both records pass. The collector reports each artifact
ID and the API-reported archive digest plus each sanitized record's SHA-256.
It checks the API digest's `sha256:<hex>` shape but does not recompute the
downloaded archive digest: `gh run download` extracts the artifact directly.
Separately, `shasum` computes `record_sha256` over each extracted JSON file's
bytes. These are different layers and are never compared; the archive digest
is retained as GitHub metadata, while the record digest is locally verified.

`sh scripts/compare-boot-evidence.sh` reruns the C selector checks for both
job names, requires all nested guest result fields except `build_id` to match,
and independently binds the log digest and byte count to the newline-terminated
authored transcript. Since the closed nested result contains the fixture,
full IPL3, IPL3 payload, profile, input, guest-stop bound, progress, log and
state identities, this comparison checks each of those identities. A
different valid compiler build ID alone is permitted.

The collector selftest passed a local-only positive pair and 24 independent
rejection cases, including a valid attempt-2 pair while retaining a validated
attempt-1 artifact and rejection of a current artifact mislabeled as attempt 1.
The comparator selftest passed a local-only pair with
different valid build IDs and rejected eight selector, missing/duplicate-input,
unequal-input, unequal-bound and unequal-state cases. These local selftests exercise behavior only and do
not satisfy the hosted proof. API shapes were checked read-only against prior
run 43020272408: the run and attempt objects expose the selected run id in
`id`, and the attempt jobs expose run id, attempt, head SHA, name, status and
conclusion. That historical run has no artifacts and is not evidence for this
change. The [GitHub artifact API reference](https://docs.github.com/en/rest/actions/artifacts)
defines artifact digest and workflow-run metadata; the [pinned upload-artifact
README](https://github.com/actions/upload-artifact/blob/ea165f8d65b6e75b540449e92b4886f43607fa02/README.md)
defines `artifact-digest` as the artifact SHA-256; and the [GitHub CLI download
manual](https://cli.github.com/manual/gh_run_download) documents extraction of
one selected artifact into the requested directory.

## Local validation before hosted publication

- `./scripts/gate.sh`: passed on macOS arm64 in 57 s: exact gate inventory
  91/91, CTest 91/91, evidence selftests (collector positive plus 23 rejection
  cases; comparator positive plus 8 mismatch cases), and core lint with zero
  violations.
- Full all-target ASan/UBSan build and exact 93-name inventory passed. With
  `detect_leaks=0`, active address/undefined checks, the bounded admission fuzz
  and intentional sanitizer control, the parallel full suite passed 93/93 in
  121.53 s; `sanitizer.negative-control` confirmed the injected fault is
  detected. The earlier full run was 92/93 because the test's 1000 ms
  after-output wall limit killed before the guest transcript appeared (0 bytes
  captured), although the test expected output followed by the 3 s delayed
  child's kill. Raising only that test limit to 2500 ms preserved the 3 s
  delay and production watchdog policy; the focused case passed alone and in
  the full suite (23.15 s under parallel load).
- After the later test-only setup-result checks, the all-target ASan/UBSan
  build passed and the affected `runner.fixture-descriptor` and
  `integration.open-ipl3-hello` tests passed 2/2 in 19.57 s. The full 93-test
  sanitizer suite was not repeated after those setup-only changes.
- A cached Ubuntu 24.04 image ran GCC 13.3.0 on Linux x86_64 with the CI
  warning flags in `-fsyntax-only` mode for `runner/t64_real.c`,
  `runner/tests/test_fixture_descriptor.c`, and
  `runner/tests/test_real_runner.c`; all passed. The image had no CMake/Ninja,
  so this is a compiler warning preflight, not a complete Linux build.
- scripts/check-ci-yml.sh --selftest: passed 43 mutation cases.
- ctest --preset gate -R '^repo\.ci-yml(-selftest)?$': passed both existing CI-checker registrations.
- The collector refused prior successful run 43020272408 after validating its run, attempt and two successful job records because it had no proof artifacts; it published no output. This negative API-path check is not hosted evidence.
- The emitter produced a canonical wrapper from a real local supervisor pass; both staged rights and PII scans passed. This local smoke confirms the emitter path only and does not satisfy the hosted proof.
- The current common gate includes both collector and comparator selftests;
  their real local supervisor records remain explicitly non-hosted inputs.
- After making artifact selection attempt-specific, `./scripts/gate.sh` passed
  on macOS arm64 in 160 s: exact gate inventory 91/91, CTest 91/91, the
  collector's retained-attempt case plus 24 rejections, comparator positive
  plus 8 mismatches, rights and PII checks, and core lint with zero violations.
  The initial collector selftest before this run found a stale local build
  embedding source revision `d6adbec...` while HEAD was `d3ad1c8...`; after
  configuring and rebuilding the gate preset, the local selftest passed.

## Hosted run records

The first complete candidate was pushed as SHA
`73e63e04ca2bc21ac5c7e80adbc6b6041cc55285` and ran as
[workflow 43087470400, attempt 1](https://github.com/szTheory/tendies64/actions/runs/43087470400).
It failed on Linux during compilation: GCC 13.3 with `-Werror` rejected four
ignored `ftruncate` results in test-only report mutation cases. The macOS job
passed and uploaded its sanitized record, but no Linux record exists, so there
was no collector/comparator run and no cross-host proof. The single macOS
artifact below is retained as failed-attempt metadata only and must not be
paired with a later run. Run wall time was 266 s; Linux queue/run was 2/108 s,
macOS queue/run was 2/262 s, with the macOS Gate step taking 249 s. Review
F-030 records that the test-only syscall results were checked without changing
production watchdog behavior or the two-job CI shape.

| Job | Conclusion | Artifact ID | Record SHA-256 |
| --- | --- | --- | --- |
| gate-linux (job 130755528714) | failure; no artifact | none | none |
| gate-macos (job 130755529262) | success; not accepted as a pair | 13888016671, archive digest `sha256:3d6040c64993002bbcc432304c3c12c3032ee9266018bdf54cdfb739f036eafb` | not retrieved; artifact is incomplete proof |

The corrected candidate SHA
`d6adbec2c1a28c00b31786b55ca3b832658b6185` ran as
[workflow 43091181251, attempt 1](https://github.com/szTheory/tendies64/actions/runs/43091181251).
Linux reached compilation but GCC 13.3 rejected ignored results from test-only
`symlink`, `ftruncate` and `write` calls in the fixture-descriptor and source
replacement tests. The macOS job passed and uploaded one sanitized record, but
Linux uploaded none, so this second attempt also produced no pair. Its
macOS-only artifact is not eligible for a later run's proof.

| Job | Conclusion | Artifact ID | Archive digest |
| --- | --- | --- | --- |
| gate-linux (job 130768347502) | failure; no artifact | none | none |
| gate-macos (job 130768347199) | success; not accepted as a pair | 13888448324 | API-reported `sha256:da8933bba84cf1515a58267a7c3bf353d33692551bf10c9de48f604869663a36`; record not retrieved |

This run took 274 s wall time; Linux queue/run was 2/95 s, macOS was 2/270 s,
and the macOS Gate step took 253 s. Review F-031 records the compile finding
and test-only setup checks. A later successful exact-SHA run and comparator
result are still required to establish hosted proof. Its run, attempt, both
job IDs, artifact IDs and API digests, extracted-record hashes, measured
timings, and comparison will be appended here.

The test-only setup-check candidate SHA
`d3ad1c8093fba2aa9b637d3cdaf74e244875346a` ran as
[workflow 43094128662, attempt 1](https://github.com/szTheory/tendies64/actions/runs/43094128662).
The Linux job 130778472362 completed successfully and uploaded artifact
13889518730. Its API metadata reports archive size 962 bytes and digest
`sha256:dde81e2da0d015769fdde87da814dba9865a23d7d057e17db315edf512d74879`.
The separately extracted `gate-linux.json` is 1587 bytes with record SHA-256
`394604abb080e8457af0e99515950e36862e5b0b2295e44e9197f16ec311b1e5`. The API
archive size and digest describe the uploaded container; the JSON size and
record hash describe its extracted payload. They are not compared.

The macOS job 130778471704 passed Gate, evidence emission, rights scan and PII
scan. The job started at 20:15:18Z; its upload step began at 20:20:11Z and was
marked `cancelled` at 20:25:18Z, exactly at the unchanged 10-minute job timeout.
No macOS artifact was created, so the Linux record is preliminary only and
this run cannot establish the hosted pair. Run, attempt, check-suite and job reads continued to report
`in_progress` with `updated_at` frozen at 20:15:18Z. Both normal and force
cancel endpoints returned HTTP 409 stating the run was not in progress. The
connected log endpoint timed out and the direct job-log REST request returned
HTTP 502, so the upload's underlying cause is unavailable; it is not attributed
to an Actions outage or to the workflow. The live [GitHub Status API](https://www.githubstatus.com/api/v2/summary.json)
reported `All Systems Operational` and Actions `operational` at
2026-11-11T20:29:11.880Z. Review LOG.md records a deferral because run timing
and step logs could not be read.

This failure exposed a collector recovery gap: a later attempt on the same run
can retain an artifact from an earlier attempt. The collector now selects only
the requested exact-attempt pair and validates any lower-attempt artifacts as
historical metadata without downloading or pairing them. The 10-minute timeout,
two hosted jobs and workflow remained unchanged. A new normal push was required
for clean exact-run collection; prior artifacts remain untouched.

## Final exact-SHA hosted pair

The normal push of source
`7c0ea9b5f3076122d506074aa7d3a2283cf9e2a6` completed as [run
43103669084, attempt 1](https://github.com/szTheory/tendies64/actions/runs/43103669084).
The run and both jobs completed successfully: `gate-linux` job `130801302890`
and `gate-macos` job `130801302572`. Each passed the common gate, emitted from
its real supervisor result, passed the staged rights/PII scans, and uploaded
only its canonical JSON record.

| Job | Artifact ID | API archive size | API archive digest | Extracted JSON size | Extracted record SHA-256 | Build ID |
| --- | --- | ---: | --- | ---: | --- | --- |
| gate-linux | 13892259569 | 962 bytes | `sha256:6317ebb793436ba93abc737c909266665d86763a6fd36f4a209bb612d802e2df` | 1587 bytes | `cd2a21c8471de0e1f2040aeacaeaf03b57f04e8b5cf157782e4e0b1787a0c013` | `t64-build-v1-4209255e99dfd12fa973fa21f1093ec79ac0af6e2f7e6d3439a4d09603363e2b` |
| gate-macos | 13892669317 | 962 bytes | `sha256:e0aceddaa6f661d8590c9899f18a5db23bb94122423ce0806a204cd7857a05db` | 1587 bytes | `edc14bf88b6bebe74c4a427a2e4f5034e685873e517046585a9309f3ce950e61` | `t64-build-v1-065bf0cc1fc8ed2cfdbe674bf3db3cd9db1f421e4875b3d401799a8b31562ead` |

Artifact IDs and archive sizes/digests are GitHub API metadata. Record sizes
and hashes were measured over the extracted JSON files. The collector checks
the API digest shape but does not recompute the archive digest from
`gh run download` output; archive and record hashes are distinct. The actual
collector published the pair atomically to `build/evidence-run-43103669084`,
and `scripts/compare-boot-evidence.sh` passed against the exact SHA/run/attempt
selectors. The nested results match source SHA, fixture/profile/asset/payload/
input identities, guest bounds, log and state. Their different truthful build
IDs are allowed.

Both records report `outcome=pass`, `detail=positive-match`,
`evidence_kind=guest-execution`, and `evidence_tier=smoke` for profile
`ntsc-cart-4m-openipl3-warm-v1`. They match at guest tick 530438, 530437
retired instructions, authored completion bound 530500, final PC
`ffffffff80000478`, 30-byte transcript digest
`86594505bea53c9843f9eaf07e4cf0121cbe0055670324674fd8385c88ffd18e`, and
state digest `2e336d7b28c9696a47fa8b794a0583a581b8660d39e6e70664174549540f2f7c`.
This establishes agreement for this authored fixture and warm profile; it
does not establish cold boot, hardware accuracy, or broad ROM compatibility.

`scripts/ci-review.sh` measured 287 s wall time and a 286 s critical path
through macOS. Linux queue/run was 2/123 s; macOS was 2/284 s, with
`gate-macos/Gate` the slowest step at 272 s. The earlier run 43094128662 is
now terminal/cancelled: Linux queue/run was 2/108 s and macOS was 240/603 s
before cancellation. Its upload log shows three retries that parsed HTML
where JSON was expected, then backoff; the underlying cause is unknown.
`docs/ci-reviews/LOG.md` preserves the earlier deferral row and records this
follow-up as F-034. Its single Linux artifact was not mixed into this pair.
Queued zero-job run 43098235914 was superseded and was not used.

The full 93-test ASan/UBSan suite passed before the final test-setup and
shell-guard changes. After the later test-setup checks, the all-target
sanitizer build and focused affected tests passed; the final source's two
hosted common gates passed. The full local sanitizer suite was not repeated
solely for the later shell-only guard change. ASVS level 1 is closed at 27/27
in `03-SECURITY.md`; the zlib host prerequisite audit is recorded separately
and preserves the historical pre-use audit gap without claiming human
approval.
