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

| Encoding family | Defined operations in scope | Disposition and evidence |
| --- | --- | --- |
| COP2 `rs=0`, `rd=0..31`, `e=0,2,...,14` | MFC2 | Sign-extends the selected vector halfword into the scalar GPR. The selected source byte pair and all legal element values are checked by `cop2_move_matrix_test`. Odd element values and nonzero undefined low bits refuse transactionally. |
| COP2 `rs=4`, `rd=0..31`, `e=0,2,...,14` | MTC2 | Writes the low 16 scalar bits to the selected vector halfword and preserves every other byte. All legal element values are checked by `cop2_move_matrix_test`; invalid fields are in `cop2_reserved_refusal_test`. |
| COP2 `rs=2` / `rs=6`, `vs=0,1,2` | CFC2 / CTC2 | Maps `vs=0` to VCO, `vs=1` to VCC, and `vs=2` to VCE. CFC2 sign-extends the 16-bit VU value into the scalar GPR; VCE has only eight populated bits. The element field is ignored as specified by the guide; all 16 element encodings are tested for reads and writes. The low 7 undefined bits and `vs` values outside 0..2 refuse. |
| COP2 VMOV function `0x33` | VMOV | Raw `de` (the `vs`-position field) selects destination lane 0..7. Raw `e` uses the vector element selector: `0` selects lane `de`; `2/3` select a quarter with `(e & 1) + (de & 6)`; `4..7` select a half with `(e & 3) + (de & 4)`; `8..15` broadcast lane `e & 7`. Raw `e=1` and `de>7` refuse. The move writes one destination lane and that lane's accumulator low 16 bits, preserving other lanes and accumulator bits. The element selector follows Table 3-3 (printed p. 58); the divide-family `vs` lane description is on printed p. 75; the raw `e, vt, de, vd` format and VMOV operation are in Appendix A (printed pp. 271–272). Raw words `0x4b031133` (`e=8`, `de=2`, source lane 0) and `0x4be32933` (`e=15`, `de=5`, source lane 7) anchor the selector/field interpretation. `vmov_matrix_test` checks all raw e/de combinations and source/destination aliasing. |
| LWC2 subopcode `0..3` | LBV, LSV, LLV, LDV | Normal byte/short/long/double transforms; offset scales are respectively `0,1,2,3`. Legal byte elements are every value for LBV, even 0..14 for LSV, multiples of 4 through 12 for LLV, and 0 or 8 for LDV. |
| LWC2 subopcode `4,5` | LQV, LRV | Quad/rest transforms with offset scale 4 and element 0. LQV accesses through the next 16-byte boundary; LRV accesses from the aligned boundary up to the effective address, and is empty at an aligned address. |
| LWC2 subopcode `6,7,8,9` | LPV, LUV, LHV, LFV | Packed transforms with offset scales `3,3,4,4`. LPV fills each lane's high byte; LUV uses bits 14..7; LHV selects every second byte; LFV selects every fourth byte into the low or high four lanes (`e=0` or `8`). |
| LWC2 subopcode `10` | No guide-defined load | Refused as reserved. The task brief's name “LWV” has no corresponding entry in the pinned guide's load table or appendix; no behavior is invented for this encoding. |
| LWC2 subopcode `11` | LTV | Transpose load with offset scale 4, an even element 0..14, and the guide's 8-register group `(VT & 0x18) | ((Slice + (Element >> 1)) & 7)`. Other element values refuse. |
| SWC2 subopcode `0..3` | SBV, SSV, SLV, SDV | Normal byte/short/long/double transforms with offset scales `0,1,2,3` and the same legal element positions as their load partners. |
| SWC2 subopcode `4,5` | SQV, SRV | Quad/rest stores with offset scale 4 and element 0. They write only the portion of the aligned 16-byte memory word selected by the effective address. |
| SWC2 subopcode `6,7,8,9` | SPV, SUV, SHV, SFV | Packed stores with offset scales `3,3,4,4`, inverse to the corresponding loads: SPV extracts high bytes, SUV extracts bits 14..7, SHV stores every second byte, and SFV stores every fourth byte from lane group 0 or 4. |
| SWC2 subopcode `10,11` | SWV, STV | Wrapped/transpose stores with offset scale 4 and an even element 0..14. They use the guide's eight-register group and preserve bytes outside the selected 16-byte transfer. |
| LWC2/SWC2 subopcode `12..31` | Reserved | Refused before effects. Every reserved subopcode has a fetched refusal case in `vector_memory_matrix_test`. |

The guide's signed 7-bit offset is scaled by each operation's shift and added
to the scalar base register. Only low 12 bits address DMEM. The transfer test
iterates all 16 effective-address alignments and all 16 element values for
each load/store subopcode, including invalid values; it compares the complete
vector-register bank and DMEM image, checks flags and accumulators remain
unchanged for memory transfers, and includes negative offsets and low-12-bit
wrap. Expected byte ordering is assembled explicitly in the test oracle.
`unit.core.rsp-transfers` owns these cases along with CP2 moves and transactional
reserved-field checks.

For VMOV, Appendix A writes the operands as `vd[de], vt[e]`, with raw fields
`e` at bits 24..21, `vt` at 20..16, `de` at 15..11, `vd` at 10..6, and the
function at 5..0. The guide describes the divide-family source as one scalar
element of `vt` and says destination `vs` encodes a lane 0..7 (printed p. 75);
the Appendix names that destination field `de` (printed p. 271). The raw `e`
field uses Table 3-3's vector/quarter/half/whole selector (printed p. 58),
rather than the byte-offset convention used by MFC2/MTC2 and LWC2/SWC2. Thus
raw `e=8..15` selects one of lanes 0..7 for the single destination selected by
`de`. The transfer test executes fixed raw words `0x4b031133` and
`0x4be32933` and checks every e/de combination against this mapping.

## Evidence boundary

The authored native cases fetch instructions from IMEM and inspect resulting
machine state; no helper directly executes an opcode. Values are derived from
the guide's operation descriptions and explicit byte/lane arithmetic. These
tests establish the listed deterministic model behavior. They do not establish
silicon behavior for undocumented arbitration, corruption, or timing cases.

## Reviewed instruction-sequence and bit-pattern controls

J/JAL target bits within IMEM come entirely from the shifted target field;
source PC quadrant bits are discarded, following the guide's printed pp.173–174.
Fetched jumps from every source quadrant exercise low and high target regions.
Unsigned pack/half/fourth loads place a byte in bits14..7 with bit15 and
bits6..0 zero (Figure3-4, printed p.54, and Appendix pp.182,186,195).
In the appendix concatenation, `0_1` and `0_7` mean one and seven zero bits;
they are not binary digits. Fixed input/output anchors are checked independently
of the general transfer oracle, which originally repeated the same notation error.

VMOV is a VU instruction and cannot share an issue slot with VADD, even when
registers are disjoint (guide printed pp.39–40). Loads/moves executed by the SU
may pair only without vector and VCO dependencies. Fetched VMOV/LBV/CTC2 then
VADD sequences check both issue count and the resulting value after readiness.

Table3-1 (printed pp.49–50) defines LHV/SHV memory alignment as quad+0/1,
LFV/SFV as quad+0..3, and transpose/wrapped forms as quad-aligned.
Other memory alignments refuse before effects in this profile. The guide
explicitly leaves illegal alignment results unspecified; the refusal is a
software boundary, not a claim about the silicon result. All16 alignment
cases in the transfer matrix include these transactional refusals.
