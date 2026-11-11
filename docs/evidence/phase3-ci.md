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
attempt and SHA, then exactly two current-attempt proof artifacts with matching
run and source SHA, unexpired bounded sizes and valid SHA-256 artifact digests.
It downloads each exact artifact name into a fresh private directory with
`gh run download`, refuses links, special filesystem entries, nested
directories, extra files and JSON larger than 64 KiB, and validates each
record through the strict C checker. The pair is renamed into a new output
directory only after both records pass. The collector reports each artifact
ID and archive digest plus each sanitized record's SHA-256.

`sh scripts/compare-boot-evidence.sh` reruns the C selector checks for both
job names, requires all nested guest result fields except `build_id` to match,
and independently binds the log digest and byte count to the newline-terminated
authored transcript. Since the closed nested result contains the fixture,
full IPL3, IPL3 payload, profile, input, guest-stop bound, progress, log and
state identities, this comparison checks each of those identities. A
different valid compiler build ID alone is permitted.

The collector selftest passed one local-only positive pair and 23 independent
rejection cases. The comparator selftest passed a local-only pair with
different valid build IDs and rejected eight selector, missing/duplicate-input,
unequal-input, unequal-bound and unequal-state cases. These local selftests exercise behavior only and do
not satisfy the hosted proof. API shapes were checked read-only against prior
run 43020272408: the run and attempt objects expose the selected run id in
`id`, and the attempt jobs expose run id, attempt, head SHA, name, status and
conclusion. That historical run has no artifacts and is not evidence for this
change. The [GitHub artifact API reference](https://docs.github.com/en/rest/actions/artifacts)
defines artifact digest and workflow-run metadata; the [GitHub CLI download
manual](https://cli.github.com/manual/gh_run_download) documents extraction of
one selected artifact into the requested directory.

## Local validation before hosted publication

- ./scripts/gate.sh: passed with 91 registered tests and zero failures.
- scripts/check-ci-yml.sh --selftest: passed 43 mutation cases.
- ctest --preset gate -R '^repo\.ci-yml(-selftest)?$': passed both existing CI-checker registrations.
- The collector refused prior successful run 43020272408 after validating its run, attempt and two successful job records because it had no proof artifacts; it published no output. This negative API-path check is not hosted evidence.
- The emitter produced a canonical wrapper from a real local supervisor pass; both staged rights and PII scans passed. This local smoke confirms the emitter path only and does not satisfy the hosted proof.
- The current common gate includes both collector and comparator selftests; their real local supervisor records remain explicitly non-hosted inputs.

## Hosted run records

No hosted run has yet been selected. After the complete emitter, collector, and
comparator path is committed, this section will record the exact pushed SHA,
run attempt, job conclusions, artifact IDs, artifact digests, measured CI
timings, and comparator result. Only records retrieved from that exact
successful GitHub run attempt count.

| Job | Conclusion | Artifact ID | Record SHA-256 |
| --- | --- | --- | --- |
| gate-linux | pending | pending | pending |
| gate-macos | pending | pending | pending |
