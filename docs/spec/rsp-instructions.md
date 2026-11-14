# RSP instruction coverage ledger

This ledger describes the authored scalar/control slice in the current RSP
execution profile. It is an implementation and test-coverage statement, not a
claim of cycle accuracy or complete RSP compatibility. The opcode facts follow
the pinned SGI/Nintendo RSP Programmer's Guide recorded in
`.planning/phases/05-rsp-low-level-emulation/05-RESEARCH.md` (guide revision
1.0; acquired-file SHA-256 `6ec646ba9bd836af9aa30ae90099af55dbaf291fbd8b39ff8be5664ada3bd668`).

## Scalar and control ledger

| Encoding family | Defined operations in scope | Disposition and evidence |
| --- | --- | --- |
| SPECIAL `000000`, function `00,02,03,04,06,07` | SLL, SRL, SRA, SLLV, SRLV, SRAV | Implemented from fetched IMEM; fixed and variable shift edge cases are in `unit.core.rsp-scalar` (`scalar_alu_matrix_test`). Unused/reserved field combinations refuse before retirement. |
| SPECIAL function `08,09` | JR, JALR | Implemented with delay-slot/target behavior. JR target alignment and JALR destination/source alias restrictions are explicit refusal boundaries. Fetched cases are in `scalar_jump_matrix_test`; delay-slot branch refusal is separate. |
| SPECIAL function `0d` | BREAK | Implemented through the existing RSP break/status path; tested by the existing fetched RSP tracer. The instruction code field does not change the break action. |
| SPECIAL function `20,21,22,23,24,25,26,27,2a,2b` | ADD, ADDU, SUB, SUBU, AND, OR, XOR, NOR, SLT, SLTU | Implemented with 32-bit wrap and no CPU overflow exception. Fetched independent boundary values are in `scalar_alu_matrix_test`. |
| REGIMM `rt=00,01,10,11` | BLTZ, BGEZ, BLTZAL, BGEZAL | Implemented. Link instructions write `PC+8` unconditionally as specified by the guide, including an untaken branch. Taken, untaken, delay-slot, and link behavior is in `scalar_branch_matrix_test` and `branch_in_delay_slot_refusal_test`. |
| Primary opcode `02,03,04,05,06,07` | J, JAL, BEQ, BNE, BLEZ, BGTZ | Implemented with a single delay instruction and the 4 KiB IMEM target domain. Fetched taken/untaken and delay instruction cases are in `scalar_jump_matrix_test` and `scalar_branch_matrix_test`. |
| Primary opcode `08,09,0a,0b,0c,0d,0e,0f` | ADDI, ADDIU, SLTI, SLTIU, ANDI, ORI, XORI, LUI | Implemented with sign-extended arithmetic/comparison immediates and zero-extended logical immediates. Fetched independent values are in `scalar_alu_matrix_test`; the ADDI-specific RED/GREEN record is `build/gate/05-05-task1-red.log` and `build/gate/05-05-task1-addi-green.log`. |
| Primary opcode `20,21,23,24,25` | LB, LH, LW, LBU, LHU | Implemented as explicit big-endian byte assembly; each byte address wraps independently in low-12-bit DMEM. Fetched wrap/sign cases are in `scalar_load_matrix_test`. |
| Primary opcode `28,29,2b` | SB, SH, SW | Implemented as explicit big-endian byte writes; each byte address wraps independently in low-12-bit DMEM. Fetched wrap/untouched-byte cases are in `scalar_store_matrix_test`. |
| COP0 `rs=0`, `rd=0..7` | MFC0 from SP registers | Implemented through SP register peek/read paths, including semaphore read behavior. Register-by-register fetched cases are in `cp0_matrix_test`. |
| COP0 `rs=4`, `rd=0..7` | MTC0 to SP registers | Implemented for writable SP registers after pre-validating status commands. DMA/control/semaphore mappings are checked in `cp0_matrix_test`. Writes to c5/c6 refuse as read-only; nonzero c7 writes refuse as invalid. |
| COP0 `rd=8..15` | DP register requests, read or write | Refused with `T64_RSP_REFUSAL_DP_UNAVAILABLE` before RSP/SP/DMEM effects because RDP is outside this profile. The test snapshots those states for each register and direction in `cp0_matrix_test`. |

SPECIAL functions not listed above (including SYSCALL, SYNC, HI/LO operations,
multiply/divide, trap instructions, and 64-bit shifts/arithmetic) refuse with
`T64_RSP_REFUSAL_UNSUPPORTED_OPCODE`. Unassigned SPECIAL function values are
reserved and refuse the same way. REGIMM branch-likely, branch-likely-link,
trap-immediate and unassigned `rt` values are not part of the implemented
profile and refuse before effects. Primary opcodes for branch-likely,
unaligned/64-bit memory, cache, COP1, COP3, and SPECIAL2 operations are likewise
outside this profile. Their RSP scalar fetched refusal coverage includes
MULT, COP1, BEQL, and an ADD with reserved encoding bits in
`unsupported_scalar_refusal_test`; other members share the same default
pre-effect refusal path.

COP0 encodings with nonzero reserved low bits are rejected. MTC0 c5/c6 writes
are refused as read-only; an invalid status command or nonzero semaphore write
uses `T64_RSP_REFUSAL_CP0_INVALID_VALUE`. These checks occur before committing
the instruction. A branch in a delay slot refuses before changing its target,
link register, PC, or retirement count; this is an explicit profile boundary
for an encoding whose nested-delay behavior is not modeled.

## COP2 scalar transfers and vector memory ledger

The guide defines MFC2, MTC2, CFC2, CTC2, VMOV and the LWC2/SWC2 vector memory
forms LBV, LSV, LLV, LDV, LQV, LRV, LPV, LUV, LHV, LFV, LWV, LTV, SBV, SSV,
SLV, SDV, SQV, SRV, SPV, SUV, SHV, SFV, SWV and STV. This plan's second task
owns these encodings and their independent byte/lane/element tests in
`unit.core.rsp-transfers`; this section will receive the per-form status and
test mapping with that implementation. Guide-reserved COP2 suboperations and
reserved LWC2/SWC2 subopcode values must refuse rather than alias a defined
transform.

## Evidence boundary

The authored native cases fetch instructions from IMEM and inspect resulting
machine state; no helper directly executes an opcode. Values are derived from
the guide's operation descriptions and explicit byte/lane arithmetic. These
tests establish the listed deterministic model behavior. They do not establish
silicon behavior for undocumented arbitration, corruption, or timing cases.
