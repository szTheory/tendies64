# VR4300 Integer and Control Instruction Coverage

This table is the reviewed contract for the integer/control slice in
`test_cpu_integer.c`. The tests calculate expected values independently from
the implementation and execute through the normal machine scheduler and
exception path. The inventory checker compares the table with the required
mnemonic set, verifies each encoding token appears in the test source, verifies
each named test case is registered in one of the two CTest modes, and rejects
blank encoding or source/vector fields.

Hardware source: NEC, *VR43XX User's Manual*, document
`U10504EJ7V0UMJ1`, seventh edition, August 2000. The verified reference copy
has SHA-256
`b7c1900f1707a4b238ebd82af1669e9628e4df4875ceabfa012a9d95b4716b75`.
Integer operation tables are §3.2.2, Tables 3-4 through 3-11, pp. 68–76;
control flow is §3.2.3, Tables 3-15 and 3-16, pp. 78–80; SYNC is §3.2.4,
Table 3-17, p. 81. The detailed operation entries are in Chapter 16.

## Defined edge policies

- Word results are sign-extended in the 64-bit profile. Shift amounts are
  masked to the word or doubleword width, and fixed 32-offset shifts cover
  offsets 32–63.
- `DIV` by zero uses the VR4300-specific 32-bit values in Appendix B.1.6,
  p. 631: LO is `0x7fffffff` for a nonnegative dividend and `0x80000001` for
  a negative dividend; HI receives the dividend. Those values are manual
  facts, not a general MIPS ISA guarantee.
- `DIVU` by zero, `DDIV`/`DDIVU` by zero, and signed quotient overflow are
  undefined in the manual or ISA description used here. This profile stops
  with model-unsupported before changing HI/LO or retiring the instruction.
  The refusal is a deterministic model policy, not a claimed hardware result.
- The 64-bit product is formed from 32-bit partial products; signed products
  convert unsigned magnitudes and then apply two-limb two's complement. Signed
  division likewise uses unsigned magnitudes and reapplies quotient and
  remainder signs, avoiding signed-overflow and zero-divisor host operations.
- `SYNC` with stype zero retires as an ordering point. This machine processes
  guest memory operations synchronously and in instruction order, so no host
  fence or external timing behavior is added.
- The manual says BLTZAL/BGEZAL and BLTZALL/BGEZALL write PC+8 to r31
  unconditionally; not-taken likely forms annul only the delay slot. This is
  exercised for both branch outcomes.
- Branch-in-delay-slot behavior is not defined by the selected manual contract.
  The model stops unsupported before the nested branch changes GPRs, PC state,
  or retirement count.

## Opcode matrix

<!-- CPU-COVERAGE-START -->
| Opcode | Encoding | Executed case | Expected source / vector |
| --- | --- | --- | --- |
| ADD | SPECIAL funct 0x20 | test_alu_word_doubleword_and_immediate | §16 ADD; high source bits discarded, 0x7ffffffe + 1 = 0x7fffffff, plus signed overflow fault |
| ADDU | SPECIAL funct 0x21 | test_alu_word_doubleword_and_immediate | §16 ADDU; word result sign-extension and upper source-bit discard |
| SUB | SPECIAL funct 0x22 | test_alu_word_doubleword_and_immediate | §16 SUB; word subtraction and signed overflow vector |
| SUBU | SPECIAL funct 0x23 | test_alu_word_doubleword_and_immediate | §16 SUBU; word subtraction without overflow trap |
| ADDI | opcode 0x08 | test_alu_word_doubleword_and_immediate | §16 ADDI; sign-extended immediate and signed overflow fault vector |
| ADDIU | opcode 0x09 | test_alu_word_doubleword_and_immediate | §16 ADDIU; sign-extended immediate and word result |
| DADD | SPECIAL funct 0x2c | test_alu_word_doubleword_and_immediate | §16 DADD; doubleword carry and signed overflow fault vector |
| DADDU | SPECIAL funct 0x2d | test_alu_word_doubleword_and_immediate | §16 DADDU; independently derived doubleword sum |
| DSUB | SPECIAL funct 0x2e | test_alu_word_doubleword_and_immediate | §16 DSUB; doubleword subtraction and signed overflow fault vector |
| DSUBU | SPECIAL funct 0x2f | test_alu_word_doubleword_and_immediate | §16 DSUBU; independently derived doubleword difference |
| DADDI | opcode 0x18 | test_alu_word_doubleword_and_immediate | §16 DADDI; sign-extended immediate and signed overflow fault vector |
| DADDIU | opcode 0x19 | test_alu_word_doubleword_and_immediate | §16 DADDIU; sign-extended immediate without overflow trap |
| AND | SPECIAL funct 0x24 | test_alu_word_doubleword_and_immediate | §16 AND; bitwise intersection of explicit 64-bit operands |
| OR | SPECIAL funct 0x25 | test_alu_word_doubleword_and_immediate | §16 OR; bitwise union including high bits |
| XOR | SPECIAL funct 0x26 | test_alu_word_doubleword_and_immediate | §16 XOR; independently derived bitwise result |
| NOR | SPECIAL funct 0x27 | test_alu_word_doubleword_and_immediate | §16 NOR; complement of the full-width OR result |
| ANDI | opcode 0x0c | test_alu_word_doubleword_and_immediate | §16 ANDI; zero-extended immediate |
| ORI | opcode 0x0d | test_alu_word_doubleword_and_immediate | §16 ORI; zero-extended immediate |
| XORI | opcode 0x0e | test_alu_word_doubleword_and_immediate | §16 XORI; zero-extended immediate |
| LUI | opcode 0x0f | test_alu_word_doubleword_and_immediate | §16 LUI; sign-extended word result and reserved rs check |
| SLT | SPECIAL funct 0x2a | test_alu_word_doubleword_and_immediate | §16 SLT; signed minimum compared with positive one |
| SLTU | SPECIAL funct 0x2b | test_alu_word_doubleword_and_immediate | §16 SLTU; unsigned ordering across high-bit boundary |
| SLTI | opcode 0x0a | test_alu_word_doubleword_and_immediate | §16 SLTI; signed comparison with sign-extended immediate |
| SLTIU | opcode 0x0b | test_alu_word_doubleword_and_immediate | §16 SLTIU; unsigned compare against sign-extended immediate |
| SLL | SPECIAL funct 0x00 | test_shift_matrix | §16 SLL; shift by 31, word-width discard, sign-extended result |
| SRL | SPECIAL funct 0x02 | test_shift_matrix | §16 SRL; logical shift by 31 |
| SRA | SPECIAL funct 0x03 | test_shift_matrix | §16 SRA; arithmetic shift by 31 |
| SLLV | SPECIAL funct 0x04 | test_shift_matrix | §16 SLLV; low five shift-count bits |
| SRLV | SPECIAL funct 0x06 | test_shift_matrix | §16 SRLV; low five shift-count bits |
| SRAV | SPECIAL funct 0x07 | test_shift_matrix | §16 SRAV; negative word sign fill |
| DSLLV | SPECIAL funct 0x14 | test_shift_matrix | §16 DSLLV; low six shift-count bits, shift by 63 |
| DSRLV | SPECIAL funct 0x16 | test_shift_matrix | §16 DSRLV; zero and 63 shift amounts |
| DSRAV | SPECIAL funct 0x17 | test_shift_matrix | §16 DSRAV; arithmetic doubleword shift by 63 |
| DSLL | SPECIAL funct 0x38 | test_shift_matrix | §16 DSLL; fixed shift by zero and 31 |
| DSRL | SPECIAL funct 0x3a | test_shift_matrix | §16 DSRL; fixed logical shift by 31 |
| DSRA | SPECIAL funct 0x3b | test_shift_matrix | §16 DSRA; fixed arithmetic shift by 31 |
| DSLL32 | SPECIAL funct 0x3c | test_shift_matrix | §16 DSLL32; encoded offsets zero and 31 give shifts 32 and 63 |
| DSRL32 | SPECIAL funct 0x3e | test_shift_matrix | §16 DSRL32; encoded offsets zero and 31 give shifts 32 and 63 |
| DSRA32 | SPECIAL funct 0x3f | test_shift_matrix | §16 DSRA32; encoded offsets zero and 31 give shifts 32 and 63 |
| MULT | SPECIAL funct 0x18 | test_hilo_multiply_and_move_matrix | §16 MULT; signed low/high words sign-extended in 64-bit mode |
| MULTU | SPECIAL funct 0x19 | test_hilo_multiply_and_move_matrix | §16 MULTU; 0xffffffff × 2 gives HI=1 and sign-extended LO=0xfffffffffffffffe |
| DIV | SPECIAL funct 0x1a | test_hilo_division_matrix | §16 DIV; -100 / 7 gives quotient -14 and remainder -2 |
| DIVU | SPECIAL funct 0x1b | test_hilo_division_matrix | §16 DIVU; 0xffffffff / 3 gives quotient 0x55555555 and remainder 0 |
| DMULT | SPECIAL funct 0x1c | test_hilo_multiply_and_move_matrix | §16 DMULT; -2 × 3 gives 128-bit -6 |
| DMULTU | SPECIAL funct 0x1d | test_hilo_multiply_and_move_matrix | §16 DMULTU; 2^32×(2×2^32+3) gives HI 2, LO 0x0000000300000000; (2^64-1)^2 gives HI 0xfffffffffffffffe, LO 1 |
| DDIV | SPECIAL funct 0x1e | test_hilo_division_matrix | §16 DDIV; -100 / 7 gives quotient -14 and remainder -2 |
| DDIVU | SPECIAL funct 0x1f | test_hilo_division_matrix | §16 DDIVU; UINT64_MAX / 3 gives quotient 0x5555555555555555 and remainder 0 |
| MFHI | SPECIAL funct 0x10 | test_hilo_multiply_and_move_matrix | §16 MFHI; reads seeded HI and multiply high halves |
| MFLO | SPECIAL funct 0x12 | test_hilo_multiply_and_move_matrix | §16 MFLO; reads seeded LO and multiply low halves |
| MTHI | SPECIAL funct 0x11 | test_hilo_multiply_and_move_matrix | §16 MTHI; copies GPR source to HI |
| MTLO | SPECIAL funct 0x13 | test_hilo_multiply_and_move_matrix | §16 MTLO; copies GPR source to LO |
| SYNC | SPECIAL funct 0x0f | test_hilo_multiply_and_move_matrix | §3.2.4 and §16 SYNC; stype-zero ordering point retires |
| TEQ | SPECIAL funct 0x34 | test_trap_register_and_immediate_matrix | §16 TEQ; equality traps, unequal operands retire |
| TNE | SPECIAL funct 0x36 | test_trap_register_and_immediate_matrix | §16 TNE; unequal operands trap, equality retires |
| TGE | SPECIAL funct 0x30 | test_trap_register_and_immediate_matrix | §16 TGE; signed greater/equal and false-control cases |
| TGEU | SPECIAL funct 0x31 | test_trap_register_and_immediate_matrix | §16 TGEU; unsigned greater/equal and false-control cases |
| TLT | SPECIAL funct 0x32 | test_trap_register_and_immediate_matrix | §16 TLT; signed less-than and false-control cases |
| TLTU | SPECIAL funct 0x33 | test_trap_register_and_immediate_matrix | §16 TLTU; unsigned less-than and false-control cases |
| TGEI | REGIMM rt 0x08 | test_trap_register_and_immediate_matrix | §16 TGEI; sign-extended immediate, true and false controls |
| TGEIU | REGIMM rt 0x09 | test_trap_register_and_immediate_matrix | §16 TGEIU; unsigned comparison with sign-extended immediate |
| TLTI | REGIMM rt 0x0a | test_trap_register_and_immediate_matrix | §16 TLTI; signed comparison with sign-extended immediate |
| TLTIU | REGIMM rt 0x0b | test_trap_register_and_immediate_matrix | §16 TLTIU; unsigned comparison with sign-extended immediate |
| TEQI | REGIMM rt 0x0c | test_trap_register_and_immediate_matrix | §16 TEQI; equal immediate traps, unequal control retires |
| TNEI | REGIMM rt 0x0e | test_trap_register_and_immediate_matrix | §16 TNEI; unequal immediate traps, equal control retires |
| J | opcode 0x02 | test_jump_targets_and_link_alias | §3.2.3 and §16 J; PC+4 region bits, full target and delay slot |
| JAL | opcode 0x03 | test_jump_targets_and_link_alias | §3.2.3 and §16 JAL; full target and PC+8 link |
| JR | SPECIAL funct 0x08 | test_jump_targets_and_link_alias | §3.2.3 and §16 JR; full-width GPR target and delay slot |
| JALR | SPECIAL funct 0x09 | test_jump_targets_and_link_alias | §3.2.3 and §16 JALR; full target, PC+8 link, source/link alias |
| BEQ | opcode 0x04 | test_conditional_branch_outcomes | §3.2.3 and §16 BEQ; taken/not-taken delay-slot and target effects |
| BNE | opcode 0x05 | test_conditional_branch_outcomes | §3.2.3 and §16 BNE; taken/not-taken delay-slot and target effects |
| BLEZ | opcode 0x06 | test_conditional_branch_outcomes | §3.2.3 and §16 BLEZ; signed zero boundary and reserved rt |
| BGTZ | opcode 0x07 | test_conditional_branch_outcomes | §3.2.3 and §16 BGTZ; signed zero boundary and reserved rt |
| BEQL | opcode 0x14 | test_conditional_branch_outcomes | §3.2.3 and §16 BEQL; not-taken likely slot annulment |
| BNEL | opcode 0x15 | test_conditional_branch_outcomes | §3.2.3 and §16 BNEL; not-taken likely slot annulment |
| BLEZL | opcode 0x16 | test_conditional_branch_outcomes | §3.2.3 and §16 BLEZL; likely annulment and reserved rt |
| BGTZL | opcode 0x17 | test_conditional_branch_outcomes | §3.2.3 and §16 BGTZL; likely annulment and reserved rt |
| BLTZ | REGIMM rt 0x00 | test_conditional_branch_outcomes | §3.2.3 and §16 BLTZ; signed negative and zero outcomes |
| BGEZ | REGIMM rt 0x01 | test_conditional_branch_outcomes | §3.2.3 and §16 BGEZ; signed zero and negative outcomes |
| BLTZL | REGIMM rt 0x02 | test_conditional_branch_outcomes | §3.2.3 and §16 BLTZL; not-taken slot annulment |
| BGEZL | REGIMM rt 0x03 | test_conditional_branch_outcomes | §3.2.3 and §16 BGEZL; not-taken slot annulment |
| BLTZAL | REGIMM rt 0x10 | test_conditional_branch_outcomes | §3.2.3 and §16 BLTZAL; unconditional PC+8 link for either outcome |
| BGEZAL | REGIMM rt 0x11 | test_conditional_branch_outcomes | §3.2.3 and §16 BGEZAL; unconditional PC+8 link for either outcome |
| BLTZALL | REGIMM rt 0x12 | test_conditional_branch_outcomes | §3.2.3 and §16 BLTZALL; unconditional link, not-taken slot annulment |
| BGEZALL | REGIMM rt 0x13 | test_conditional_branch_outcomes | §3.2.3 and §16 BGEZALL; unconditional link, not-taken slot annulment |
<!-- CPU-COVERAGE-END -->

## Invalid encodings and undefined cases

`test_reserved_integer_encodings_raise_ri` checks reserved SLL source bits,
JR destination bits, multiply destination bits, SYNC stype, LUI source bits,
BLEZ target-register bits, and an unused REGIMM suboperation. Each case checks
the shared precise RI exception and verifies HI, LO, and a destination GPR
remain unchanged. Undefined divide cases verify model-unsupported and unchanged
HI/LO; the branch-in-slot vector verifies no partial branch-link mutation.

These native tests establish deterministic behavior for this build and profile.
They do not imply silicon execution, broad game compatibility, diagnostic-ROM
qualification, or human validation.
