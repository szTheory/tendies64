# Phase 05 final native RSP qualification

The canonical native and instrumented core qualification at `3a3c8ca` passes
locally and on both hosted platforms. The host-only verifier correction at
`61542f9` passes its focused and isolated authored checks; its independent full
preflight and single current-core diagnostic run have completed. Native,
artifact and configured security evidence is now in place. The
source-anchored local partition and original C records validate, both latest
hosted jobs pass, and the scoped security verdict is SECURED. Phase-goal
verification remains separate. Earlier results retain their original identity.

The software profile is `ntsc-cart-4m-openipl3-rsp-functional-v1`, descriptor
version 3, machine model 6, state serialization 12 and SP model
`rsp-functional-issue-dma-divider-integer-v1`. The five historical profile
contracts remain regression owners. The
[instruction ledger](../spec/rsp-instructions.md) distinguishes supported
encodings, explicit refusals and selected software policies.

## Required final evidence

| Evidence | State |
|---|---|
| Configured deep code and ASVS1 security reviews | Initial and scoped findings preserved; accepted code corrections verified, including the actual full-entry setup failure. Final scoped security verdict: SECURED. |
| Fetched MTC0/DMA atomic-refusal regression | Implemented; accepted/refused fetched controls pass in normal and instrumented builds. Configured scoped review confirms the DMA correction. |
| No-HLE helper mutation guard | Complete RSP/SP function inventories, both host entry points and source/object mutation controls pass. |
| Evidence checker and mutation controls | Implemented; real artifact identities, passing native rows, original C metadata/output and quoted privacy controls pass authored tests. The actual saved-receipt check passes. |
| Stable-source canonical gate and exact 127-test normal inventory | Pass on core semantic baseline; unchanged production behavior is reused for the host-only correction. |
| All-target instrumented build, RSP/shared CPU protocol owners, admission fuzz and sanitizer negative control; exact 129-test inventory | Pass on core semantic baseline; unchanged production behavior is reused for the host-only correction. |
| Final artifact identity, composition, divider provenance, rights and privacy checks | Verified against actual saved artifacts; final receipt binds their hashes. |
| Authored-only setup without private diagnostic storage | Authored build, qualification and runner controls pass from an isolated source copy without private roots. |
| Required Linux/macOS hosted pair on exact source | Both jobs pass on the host-only correction, with exact successful proof artifacts collected. |

The local canonical gate and all-target instrumented selection are retained in
`build/gate/05-10-final-gate.log` and
`build/asan/05-10-final-{build,ctest,inventory}.log`. The tested executables,
libraries, runtime identities and log hashes are archived together under
`build/phase05-artifact-snapshots/3a3c8cae992a/`. The semantic-baseline
[hosted run](https://github.com/szTheory/tendies64/actions/runs/43663747138)
uses only cleared authored inputs; its measured timing and disposition are in
the [CI ledger](../ci-reviews/LOG.md). Its unchanged core behavior was retained
through the host-only qualification changes, and new exact artifacts were
qualified for the combined source rather than relabeling the earlier binaries.

The latest source has separate all-target provenance-refresh build logs and
exact artifacts at `build/phase05-artifact-snapshots/61542f9b2c3a/`. Its manifest
explicitly identifies `3a3c8ca` as the semantic test baseline. Production and
build-policy inputs, actual compiler-policy build IDs and profile identities
are unchanged; only the Git provenance annotation and host verification tools
changed. The earlier logs keep their original bytes and source identity.
Focused current host checks and clean authored controls cover the correction;
no new admission-fuzz result is claimed for the provenance refresh.

The [latest hosted pair](https://github.com/szTheory/tendies64/actions/runs/43665538190)
executes the complete normal gate on the corrected host source. Its actual
artifact identities and measured critical path are recorded in the final
receipt and CI ledger. The local source-derived partition is checked through
the existing C parser and original records, with outcomes retained privately;
classification completeness does not imply a passing diagnostic suite.

The [authored ROM evidence](phase05-authored-rsp.md) describes actual CPU→SP
DMA→fetched RSP→CPU checking and distinct negative controls. Fixture identities,
source/tool/boot composition and build-only rights are separately recorded.
No external diagnostic source, membership, identity, image or result is part of
the public report or required hosted inputs.

## Limits

The canonical scalar C implementation has per-opcode authored reference and
invariant checks. There is no separate optimized VU backend, so optimized
differential execution has not occurred. The bounded source/object no-HLE
guard does not prove arbitrary C behavior. Divider edge behavior, ambiguous
flag/clamp cases and issue/DMA timing retain their named software policies.
The selected divider helper has its own
[copy/provenance evidence](rsp-divider-provenance.md).

An individually classified private result is not a passing compatibility
member. Historical private execution retains its original source and artifact
identity when source changes. Native tests, deterministic output, agreement
with another implementation and a complete classification partition do not
establish silicon accuracy, general compatibility or physical qualification.
No binary release or private publication is implied.
