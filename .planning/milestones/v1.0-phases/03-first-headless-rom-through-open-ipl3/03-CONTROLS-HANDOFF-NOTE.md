# Real controls and determinism handoff

Parent preparation after 03-11 terminal handoff.

- Current gate/ASan inventories are 84/86. Keep both the descriptor-helper
  replacement proof and actual CPU-admission replacement proof. Six new real
  controls plus one determinism test yield 91/93, not the old planned 90/92.
  Preserve every exact name; fix the hardcoded selftest summary to report its
  measured per-name omission controls accurately.
- 03-11 deliberately implemented a positive-fixture smoke path. Both producer
  and supervisor currently hardcode only hello's size/digest, and the
  supervisor also hardcodes the empty 54-byte schedule and bound 530500.
  Exercise the already-built six authored variants through actual production
  admission, rather than accepting their early setup refusal as a guest
  failure. Extending those host tools and the strict checker for this named,
  rights-cleared set is necessary and authorized. Keep unknown fixtures refused
  and pass limited to the original positive fixture. A small shared fixture
  descriptor table is permitted if it prevents production identity drift;
  tests must retain independent manifest/output expectations.
- Preserve two input FDs and source/payload/whole-ROM identity distinctions.
  A valid nonempty logical schedule may need bounded raw size/SHA arguments in
  real supervisor mode so the shared snapshot helper can verify it. Defaults
  for the existing positive example can remain compatible. Do not require a
  third source-asset FD or forward original paths to the child. Input events
  stay trace-only; no SI/controller-consumption claim.
- Task 1 adds missing variant behavior and needs meaningful RED at that
  behavioral boundary. Task 2 is verification-only over existing machine
  behavior and no longer has a TDD marker. It may start green. If determinism
  exposes a real defect, retain that real failure and fix it; never add a
  hardcoded false placeholder or loosen an expectation merely to pass.
- One-shot advance has a bounded event budget. Continue monotonic resource
  yields until the selected bound/stop; do not compare a yielded prefix to a
  completed partitioned run. Traced runs drain at most every 16 ticks, including
  inside larger 31-tick logical partitions, without changing guest identity.
  Rewind owned descriptors between child launches and clear per-case outputs.
- Empty completion/empty bound, wrong complete text, proper prefix at bound,
  unsupported execution and real overflow need their precise guest outcomes.
  Unsupported PC/opcode evidence must show the rejected instruction did not
  retire. Correct text cannot override process/report/capture errors.
- Existing 03-11 actual-core fault modes already cover missing/truncated/
  oversize/malformed/unknown reports, closed capture, nonzero exit, crash and
  watchdog after output. Reuse those bounded helpers where useful and preserve
  their evidence; do not duplicate an entire second process harness.
- Identity tests must state which comparison rejects the mutation. A closed
  report checker validates claimed state-digest shape, not an arbitrary final
  state against unknowable guest state. Use independently known state vectors
  or actual-run comparisons for semantic state mismatches; do not claim that
  rejecting a nonhex string proves rejection of every forged valid digest.
  Hosted comparison remains 03-13 evidence, not a local claim.
- The producer's defensive `core-content-mismatch` detail is not in the closed
  result enum. Map that unreachable-on-valid-input branch to an admitted host
  error detail or deliberately version/document/test a new one; do not emit
  invalid v2 on a defensive error path.
- Preserve all existing human-dirty instructions/config, all judgment flags,
  cleared fixture rights and historical qualification vectors. Use captured
  terminal test results, focused checks before one final broad gate, and prompt
  metadata closeout without repeated checks solely for documentation.
