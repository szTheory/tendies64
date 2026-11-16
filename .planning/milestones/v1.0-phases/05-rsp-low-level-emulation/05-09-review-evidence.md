# Plan09 parent review

The authored ROM reaches CPU checks through production boot, bus, SP DMA,
fetched RSP execution and BREAK. The inherited warm SP overlay and IsViewer
predicates omitted the functional profile: the two scoped route fixes preserve
all five historical contracts. The guest checks the documented arithmetic,
partial memory, load-use, flag, accumulator, divider and peripheral values.
These are software-profile results, not hardware qualification.

## Parent findings and disposition

- The new consumer originally accepted any caller-supplied matching ROM digest.
  It now refuses every identity except the six checked authored fixtures before
  file access, verifies owned-buffer digest and the exact embedded boot payload,
  and publishes the actual core build identity. Unknown/modified/nonregular input
  controls pass. FIFO opens are nonblocking so type refusal cannot hang.
- Binutils version commands preceded the saved executable-hash gate. The actual
  authored changed-assembler marker control failed before correction in
  build/gate/05-09-review-tool-red.log: the unqualified executable ran. Hashes
  now precede all version commands. The reused packer also has audited-source
  checks and a saved compiled-binary receipt checked before standalone selftest;
  a changed-packer marker control rejects it before execution.
- The child wall deadline was sampled only before advance. It is now checked
  after advance and before final classification, with a hard default SIGALRM
  guard as well. Failed/reversed clocks cannot renew time. Captured PASS text
  never overrides a failed process exit or outer deadline.
- Result fields and classifications were substring assertions, and expected
  classification came from the generated receipt. Tests now use independent
  per-variant expected classifications and exact parsed field values. A failed
  positive baseline stops dependent negative runs.
- Authored fixture rows mixed build-only with maintainer-local-only restrictions
  despite being required in hosted CI, and variant1 provenance still described
  an ineffective earlier VADD change. The six cleared Apache/Unlicense fixtures
  now use the existing build-only channel, with neither commit nor artifact
  distribution. Their binary identities stay unchanged. Variant1 records the
  actual persistent VMOV mismatch. External local-only restrictions are intact.

## Evidence and process limits

Fresh all-target normal/ASan builds precede the owning integration and three
route regressions in build/{gate,asan}/05-09-review-{configure,build,ctest}.log.
Final integration follow-ups are recorded in the plan summary. Optimized GCC13
objects pass owning flags for both changed core files and the new host consumer
in build/gate/05-09-review-gcc13.log. No compiler installation occurred.

The final stable-source combined gate passes127 tests, exact inventory controls,
lint86/0 and authored evidence tail in65s: build/gate/05-09-final-gate.log.
The selected source/core/host policy has passed the local rights and PII scans.
Hosted results are recorded in SUMMARY after completion.

Preserved setup/coverage corrections: truncated manual launch digest; DMA input
address; raw move/memory fields and offsets; stale completion pins; shell-global
candidate-install clobber; and an overwritten wrong-result mutation that did not
actually fail the guest. None is counted as a core RED or passing negative control.
Named field packers, independent linked-word anchors, candidate-to-install identity,
receipt-derived launch arguments and an actual persistent CPU comparison now cover
those mistakes. The aggregate12s CTest timeout was an implementation scope error:
policy assigns outer12s to each case. The serial batch is bounded120s; each guest
retains10M ticks, maxstep16, child10s, outer12s and4096 capture. Safe boot batching
matches the saved single-step baseline, and the DMA/RSP stage remains single-tick
observed. No cache, dependency, concurrency increase or budget renewal was added.

Task2's original formal RED used the correct64-character identity and observed
CPU SP_STATUS refusal before the route fix. The earlier63-character launches
remain separate setup failures. Task1 did not save an original formal
pre-implementation TDD report despite its marker. This is a disclosed process
deviation: fixture construction/verification used existing qualified primitives;
current mutation/guard regressions are current evidence, never invented original
TDD history. The parent tool-order RED is specifically a later review regression.

A broad ignored-build search during execution also touched unrelated private
receipts. They were neither used as authored evidence nor executed/modified or
published. Subsequent searches were scoped to owned source and authored logs.
No actual external diagnostic, Cargo rebuild, physical test, binary publication
or third-party source adaptation occurred. Full configured phase code/security
review remains Plan10 work.

## Reviewed local artifact identities

- gate: core `t64-build-v1-dcbce492d84ff6f6381706bb00059ed93ad78b3c9f654fe0cde0fb1f0c3f658b`; consumer SHA-256 `7745552f16f6f2365e575d374328775e7c5fa87ff79157507d3653d7c2ba2520`.
- asan: core `t64-build-v1-b2c13d93aa5aafb96bfbd563b898cf945acb7bc9b29a9618c00850fffcaf9d51`; consumer SHA-256 `bb564a4b012a315188c0341da3456d70f5b1f514d451d415a59e4cd63c452eeb`.

Build identities and source-revision annotations are separate. The local core
annotation is c6a7aef; review edits were present and content-identified during
builds. Final hosted source identity is recorded separately, without relabeling
these local binaries.
