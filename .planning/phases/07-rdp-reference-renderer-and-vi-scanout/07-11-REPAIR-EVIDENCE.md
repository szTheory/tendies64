# Phase 07 Plan 11 Segment A repair evidence

This note records the bounded repairs for the four findings in
`07-11-SEGMENT-A-REVIEW-INITIAL.md`. The source base was
`a059e64d65ffcf10f88d75b865bfd237f4ac30f3`. Source and collector control
artifacts remain under ignored `build/gate/07-qualification/` and
`build/gate/07-11-review/` paths.

## CR-01: public bytes now have an independent authored contract

`verify_evidence(..., public=True)` decodes the public serialization, validates
its existing manifest and provenance bindings, then compares decoded RDRAM and
VI packet bytes to the independently authored fill output. The producer still
exports actual bytes from the real RDP/VI consumer; the constants are used only
as the verifier's comparator.

The `raw-resealed` archive mutation flips a RDRAM byte and recomputes the byte
length and SHA, private-original length and SHA, and public serialization length
and SHA. A local smoke control created and extracted such an archive, then the
actual `--verify-upload` CLI refused it with
`public raw bytes differ from the independently authored output contract`.
The combined collector selftest also mutated both host archives and resealed
their API digest and size metadata; the final collector CLI refused the pair.
The review's original RED record remains at
`build/gate/07-11-review/public-pair-forgery.json`: both host records shared a
modified RDRAM byte and the old pair verifier accepted them.
The fresh final-helper run passed in 7.99 seconds with return code 0 and 32
rejection controls. Its saved command binds helper SHA-256
`d3408320e72d6a56984a7528b4cad3427c6552ce6a083f5656641857298520d8` and
collector SHA-256
`5c3fe6b2dd3c80bcb9f61b797d69e3af0e0069d5737923887ad62629c6c61e58` in
`build/gate/07-11-review/collector-repair-final/`; the retained layout control
is `build/gate/07-qualification/upload-layout-control.7v1xg1/control.json`.
That selftest used mocked transport and is not hosted proof.

## CR-02: process deadlines clean up the process group

`_run_bounded` starts commands in a fresh POSIX session. On timeout it signals
the process group, escalates to `SIGKILL` if needed, waits for the direct child,
and checks that the group is gone. A group that remains causes refusal. Hosts
without POSIX process-group support fail closed.

The actual deadline control started a parent and a grandchild writer. Its
500 ms deadline returned in 504 ms with return code `-15`; the saved record
reports the grandchild PID was no longer alive, the process group was empty,
and the write-marker size remained 18 bytes after an additional 120 ms. The
original command, process result, stdout, stderr, and descendant check are
retained in `build/gate/07-qualification/deadline-control-jyqzpddo/`.
The review's one-second RED control at
`build/gate/07-11-review/deadline-process-tree.json` records that the old
wrapper returned while its grandchild was still alive; the repro itself then
killed that child during cleanup.

## WR-01: collection validates downloaded ZIP bytes before extraction

The production collector now downloads by selected artifact ID through the
GitHub Actions artifact API, verifies the exact archive size and API SHA-256,
then calls the existing strict `--extract-upload` command before the exact
post-extraction tree check. The mocked downloader follows the same API-ID ZIP
path and supplies the raw archive bytes. The endpoint behavior is documented
in the [GitHub Actions artifacts REST reference](https://docs.github.com/en/rest/actions/artifacts#download-an-artifact).

The final collector selftest passed in 7.74 seconds using actual local boot and
raw producers plus mocked transport. It passed 32 rejection cases, including
digest mismatch, both-host fully resealed raw forgery, duplicate/traversal
members, expanded-size overflow, special member type, and extra members.
The retained control is
`build/gate/07-qualification/upload-layout-control.2FruZ1/control.json`; it
records `hosted_proof=false`. Both actual five-file upload trees passed the PII
and rights/provenance scans.

An earlier collector attempt is retained in
`build/gate/07-11-review/collector-repair-attempt01/`: it exited 1 after
104 ms because the strict boot checker saw old compiled runner binaries whose
embedded source revision did not match selected HEAD `a059e64`. No archive was
downloaded in that attempt. The owning normal targets were rebuilt in 779 ms;
the retry then passed. The setup refusal remains recorded and was not counted
as a semantic negative control.

## WR-02: native qualification pins all relevant source inputs

Before native qualification, the helper now requires the selected `HEAD` and a
clean source input set covering CMake definitions/presets, core, runner,
scripts, tools, fixtures, tests, and `docs/spec` (including the RDP/VI case
denominator). It hashes each regular file's contents with SHA-256 and records
its Git blob ID and mode. It refuses tracked changes, new untracked or ignored
inputs, symlinks, mode changes, and content changes. It compares the input
manifest again after the selftest and around each native stage, so a change
during the run also refuses. The owner files `AGENTS.md`,
`.planning/config.json`, and `.planning/AUTONOMY.md` are outside that source
scope and may remain dirty.

The cheap source-drift control used the actual `core/t64_vi.c` bytes in a
temporary Git repository. It confirmed refusal for tracked changes at startup
and during a run, untracked and ignored new source, and a committed symlink;
simulated owner-file edits were allowed and left the source manifest unchanged.
Its retained record is under
`build/gate/07-qualification/native-source-control-0c8ec25e/control.json`.

The prepared GCC route compares the selected commit with Phase 07 baseline
`18dcaa0c4849f4c2841d1fb4c14959600e42b257` and includes all 30 changed C
translation units, including the owning `runner/tests/test_rdp_video.c`
consumer. It uses the existing Phase 07 core, core-test, and runner warning
flags, plus the existing `gcc:13` route's `--pull=never`, `linux/amd64`,
network-none, and read-only settings. The locally inspected image identity is
`sha256:ad13696c6b2464e813639c0479ec2f66877516ec18a183c85282378720376786`.
The generated command passed `sh -n`; GCC was not run in this repair segment.
The Phase06 saved GCC log contained only a Docker platform warning, so no
compiler identity or source-closure pass is inferred from that log.

## Bounded verification and limits

`python3 -B scripts/qualify-rdp-vi.py --deadline-selftest` passed the actual
parent/grandchild deadline control. The final
`harness.rdp-vi-qualification` CTest passed 1/1 in 1.61 seconds with its
pre-run and post-run `LastTest.log`, unique JUnit, stdout, stderr, and exit code
retained under `build/gate/07-qualification/repair-helper-ctest-final/`.
Python AST parsing, the generated GCC shell syntax check, `sh -n` for the
collector, and `git diff --check` passed.
Final precommit scans also passed: PII scan covered 1045 repository files with
zero findings, rights/provenance scan covered the same 1045 files with zero
findings, the exact two upload trees passed PII scanning, and each combined
upload tree passed the rights scan with five whitelisted members and zero
findings.

This repair segment did not run the full native gate, ASan/UBSan suite, GCC
compiler route, or live hosted artifact collection. The collector integration
result is mocked-only; it does not establish a hosted run or broader support
claim.
