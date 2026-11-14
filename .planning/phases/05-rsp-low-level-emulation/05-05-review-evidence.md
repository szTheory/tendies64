# Plan05 review and regression evidence

The executor implemented fetched scalar/control/CP0 and COP2/vector memory
operations, with complete encoding dispositions and normal/sanitizer matrices.
Source review found errors that the original expected-value helpers repeated.
This record preserves the corrections rather than treating the initial passing
matrix as independent proof of instruction correctness.

## Reproduced defects and corrections

- J/JAL retained source-PC quadrant bits in the low12 target. Fetched jumps
  from four source locations to two target regions reproduce the defect.
  Target low bits now come only from the shifted instruction target, following
  the guide's printed pp.173–174. Both J and JAL preserve delay retirement.
- LUV/LHV/LFV misread the primary guide's bit-concatenation notation. The
  subscript in `0_1` means one zero bit; Figure3-4 confirms bytes occupy
  bits14..7. Literal anchors for bytes00/01/80/ff reproduce the erroneous
  forced bit and shift. Both production mapping and general oracle are fixed;
  the new literal anchors stay separate from that oracle.
- New transfers could pair with VADD using stale vector or VCO state.
  Fetched VMOV, LBV and CTC2 sequences reproduce both premature dual retirement
  and wrong results. The first attempted VMOV anchor selected a different
  destination lane; that setup was corrected before accepting the final RED
  record. The valid raw-word attempt remains separately saved.
- An independent VMOV followed by VADD also issued twice in one slot despite
  both occupying the VU. A separate disjoint-register case reproduces it.
  VMOV now occupies one VU slot; SU loads/moves retain pairing only without
  vector or VCO dependencies (guide printed pp.39–40).

The new failures are saved in build/gate/05-05-review-red.log,
05-05-review-pair-red-valid.log and05-05-review-vu-red.log. Final all-target
normal and sanitizer builds precede the seven owning checks; both selections
pass in build/{gate,asan}/05-05-review-final-ctest.log. Strict GCC13 checks
of changed C translation units pass in build/gate/05-05-review-gcc.log.
The final canonical gate and hosted pair are recorded in the summary/CI ledger.

- CFC2 zero-extended VCO instead of applying the VU move section's16-bit
  sign extension. The existing negative VCO pattern now expects its full32-bit
  signed value and reproduces the defect before the fix. VCE still has only
  eight populated bits and remains nonnegative. Saved RED:
  build/gate/05-05-review-cfc2-red.log.

The final source selections (including CFC2) are saved in
build/{gate,asan}/05-05-review-complete-ctest.log, seven checks passing each.
The final GCC log is build/gate/05-05-review-complete-gcc.log. Earlier final
logs predate that last fix and are not substituted for this evidence.
The initial combined gate stopped on the newly added CFC2 assertion; because
its tree was rebuilt during that run, it is superseded as combined evidence.
The reviewed final gate runs only after source and binaries are stable.

## Evidence boundaries and efficiency

The initial VMOV byte-index interpretation was challenged during execution.
The executor reconciled raw selector fields against guide tables and added
fixed instruction words. Primary documentation distinguishes legal general
COP2 move byte elements from computational element selectors; odd MFC2/MTC2
byte elements remain explicitly refused under the documented profile.
No external guest or Cargo build occurred during Plan05. Existing private
qualification artifacts retain their tested identities; changed core bytes
require deliberate future-use requalification before the next actual guest.

The executor's scalar RED is committed separately. Task2 has genuine saved
pre-implementation opcode failures, but its test/implementation share the final
Task2 commit; this sequencing limitation is retained. Native source review,
fixed bit-pattern anchors and instruction sequences complement the generated
matrices. These establish bounded model behavior, not hardware qualification.
Full configured code/security review remains Plan10 work.

Review corrected the affected behavior and reran its owners. Unchanged source,
metadata annotations and closeout prose do not justify repeated broad tests or
private diagnostic builds. The owner AGENTS/config edits were preserved.

## Alignment review tail

Table3-1 limits memory alignments for half/fourth packed and transpose forms.
The initial matrix accepted those extra cases using an invented linear/aligned
mapping. Adding the documented alignment bounds reproduces those acceptances
in build/gate/05-05-review-align-red.log. Production now refuses unsupported
alignments transactionally; all16 alignment/element combinations stay in the
matrix. The earlier combined gate passed its pre-alignment source and remains
historical evidence. Final stable-source logs use the05-05-publish prefix.
No same-tree rebuild overlaps the final combined gate.
