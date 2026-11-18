# Plan11 hosted emitter setup repair

The first hosted run, [44225542279](https://github.com/szTheory/tendies64/actions/runs/44225542279),
passed both complete normal gates at checkpoint385fd2a. Both jobs then failed
before raw production: the actual `--emit-hosted` entry passed an already-created
temporary directory to the bundle constructor, which requires a fresh directory.
No successful hosted raw export or comparison is claimed for this run.

The emitter now uses an uncreated `original-run` child of its private temporary
root. A new regression launches the real CLI parser and dispatch in a child
process with only public output storage isolated. It uses the actual source,
fixture, binaries, producer and original-record verifier. Its workflow identity
values are authored local controls, not proof of a hosted run. The public files
are checked for exact membership and source/run/attempt/job identity. No output
is left in the production upload namespace.

With complete local-control workflow environment, that regression reproduced
the original `File exists` refusal against the old call, then passed with the
child-directory correction. The full two-test helper module passed in1.715s.
The cheap `--selftest` preflight now invokes this exact emitter entry control
before a long native qualification. The first local control omitted job/SHA
environment and refused earlier; that distinct setup failure remains recorded
and is not counted as the reproduced hosted failure or a pixel semantic RED.

Private original logs/outcomes are in `build/gate/07-11-hosted-emitter-repair/`;
the reached old-entry regression is `entry-regression-before/`. Both hosted
job outcomes and the Linux original job log are retained under `build/gate/`.
The measured failed run is recorded in `docs/ci-reviews/LOG.md`.

A cheap real transport probe downloaded the previously published, rights-cleared
boot artifact14379632904: exit0, exactly962bytes, matching its API SHA-256.
That establishes the actual raw ZIP API command, not new RDP/VI evidence. Its
original archive/API records are also retained in the private repair directory.

The correction changes only Python harness/test code. The original full
ASan/UBSan and GCC C-source results remain bound to their actual artifacts;
no C/build/fixture/spec input changed. Scoped helper tests and a required normal
gate will qualify the harness delta before a new hosted source run. No private
guest replay, oracle use, widened limit, dependency or new CI job is involved.
