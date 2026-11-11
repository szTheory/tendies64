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
and [workflow syntax documentation](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax)
are the sources checked.

## Local validation before hosted publication

- ./scripts/gate.sh: passed with 91 registered tests and zero failures.
- scripts/check-ci-yml.sh --selftest: passed 43 mutation cases.
- ctest --preset gate -R '^repo\.ci-yml(-selftest)?$': passed both existing CI-checker registrations.
- The emitter produced a canonical wrapper from a real local supervisor pass; both staged rights and PII scans passed. This local smoke confirms the emitter path only and does not satisfy the hosted proof.

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
