# Plan06 review evidence

The executor implemented fetched integer vector arithmetic/logical operations,
all16 multiply/round forms plus VSAR, and comparison/clip/merge/VNOP. The three
new registrations have separate fixed raw-word RED evidence before production
support and complete-state native matrices afterward. Source-defined reciprocal
functions remain explicitly refused until Plan07. The full64-slot ledger keeps
those pending functions distinct from reserved encodings.

## Reproduced review findings

VNOP checked only the primary opcode and low function bits, accepting malformed
scalar COP2 words. Fetched raw non-vector words for every rs0..15 reproduce the
incorrect retirement. The decoder now also requires the vector-format bit.

VNOP could pair with a following VADD because it was not classified as a VU
operation at the pairing guard. A raw VNOP/VADD sequence reproduces double
retirement and vector/accumulator changes during the VNOP slot. The guard now
reserves that VU slot; the next instruction still executes with its expected
result on the following opportunity.

VABS wrote its signed-saturated destination into ACC low, losing the raw
negative-minimum magnitude. Independent literal destination/ACC arrays reproduce
the mismatch. The profile keeps its selected signed-saturated destination and
retains the raw wrapped16-bit magnitude in ACC low, consistent with the
Appendix's raw result assignment and the existing add/sub accumulator contract.
This distinction is now explicit in the instruction ledger.

All three failures are preserved in build/gate/05-06-review-red.log. Final
all-target normal and sanitizer builds precede ten owning checks, both passing
in build/{gate,asan}/05-06-review-ctest.log. Strict GCC checks use the actual
core flags separately from test flags; logs are build/gate/05-06-review-gcc-core.log
and05-06-review-gcc-tests.log. The final combined gate and hosted pair are
recorded in the summary and CI ledger. No source/build change overlaps that
gate; closeout metadata reuses unchanged semantic evidence.

## Documentation and qualification limits

The pinned guide contains incompatible descriptions and underspecified slices.
The source ledger records VADDC/VNE flag choices, VCH's VCE-width resolution,
partial-product slice updates and VMULQ/VMACQ integer policies. These are
explicit deterministic software choices with native expected-value tests;
they are not successful hardware checks or proof of external diagnostic
agreement. Qualification must revisit them when independent evidence conflicts.
VSAR selectors/read-write behavior follows the recorded Appendix equation;
no undocumented hardware behavior is inferred from the native matrix.

The initial literal carry mask and some clip-chain expected values required
arithmetic corrections; the attempts remain saved. The first GCC probe
incorrectly applied a core-only stack-frame warning to test fixtures; it was
rejected as a command-scope error and repeated with owning target flags.
Task2 began with a real fetched VMULF RED, then expanded the other per-opcode
matrices after that GREEN; there is no invented separate RED for every row.
Exact RED commits: ce8f489(vector), cfa05e4(multiply),77eb49e(compare).

No private diagnostic guest, Cargo rebuild, external oracle or physical test
ran in Plan06. Original private qualification and execution receipts retain
exact tested identities; future actual use needs deliberate requalification
of changed native bytes. Owner AGENTS/config edits remain untouched. Full
configured phase code/security review remains Plan10 work.

## Hosted cleanup-control follow-up

Linux's full run failed the older source-replacement control because the helper
could close its readiness pipe before publishing the descendant PID. Cleanup
then legitimately killed the descendant before its PID write, and the test
mistook that missing write for cleanup failure. This is a test-handshake race,
not evidence of vector arithmetic failure or a leaked production guest.

A local-only derived helper delayed that exact gap and reproduced missing-PID
failure in build/gate/05-06-cleanup-race/before2.log. The first probe delayed
before closing readiness and passed; it did not reproduce the failure and is
not counted as RED. The corrected helper publishes its PID then signals the
existing readiness barrier itself; no timing sleep, new retry, relaxed assertion
or wider bound enters production tests. The same controlled gap passes in
after.log. Original attempts remain separate.

All-target normal and sanitizer builds precede the two owning controls, both
passing in build/{gate,asan}/05-06-cleanup-ctest.log. Actual optimized GCC13
compilation with test-target flags passes in build/gate/05-06-cleanup-gcc.log.
Verification-scope confirms production inputs unchanged; previous core semantic
evidence is reused. A fresh hosted pair must still execute the formerly
unreached Linux lint/evidence/publication tail before closeout.
