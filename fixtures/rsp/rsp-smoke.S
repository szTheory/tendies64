/* SPDX-License-Identifier: Apache-2.0
 * Original Tendies64 CPU/RSP smoke guest. RSP words are raw encodings so the
 * expected instruction stream does not depend on assembler pseudoinstructions.
 */
.set mips3
.set noreorder
.set noat

.equ PAYLOAD_BASE, 0x80000400
.equ CART_PAYLOAD_BASE, 0xb0001000
.equ ISVIEWER_BASE, 0xb3ff0000
.equ ISVIEWER_BUFFER, 0x20
.equ ISVIEWER_FLUSH, 0x16
.equ ISVIEWER_MAGIC, 0x12345678

.section .text.start,"ax",@progbits
.globl _start
_start:
  /* Copy ROM-resident RSP IMEM words to RDRAM through the cartridge bus. */
  lui $s0, %hi(rsp_program - PAYLOAD_BASE + CART_PAYLOAD_BASE)
  addiu $s0, $s0, %lo(rsp_program - PAYLOAD_BASE + CART_PAYLOAD_BASE)
  lui $s1, 0xa000
  addiu $s1, $s1, 0x1000
  addiu $s2, $zero, (rsp_program_end - rsp_program) / 4
copy_imem:
  lw $t0, 0($s0)
  sw $t0, 0($s1)
  addiu $s0, $s0, 4
  addiu $s1, $s1, 4
  addiu $s2, $s2, -1
  bne $s2, $zero, copy_imem
  nop

  /* Copy one authored 32-byte input block, then DMA it into SP DMEM. */
  lui $s0, %hi(rsp_input - PAYLOAD_BASE + CART_PAYLOAD_BASE)
  addiu $s0, $s0, %lo(rsp_input - PAYLOAD_BASE + CART_PAYLOAD_BASE)
  lui $s1, 0xa000
  addiu $s1, $s1, 0x2000
  addiu $s2, $zero, (rsp_input_end - rsp_input) / 4
copy_dmem:
  lw $t0, 0($s0)
  sw $t0, 0($s1)
  addiu $s0, $s0, 4
  addiu $s1, $s1, 4
  addiu $s2, $s2, -1
  bne $s2, $zero, copy_dmem
  nop

  lui $s4, 0xa404
  addiu $t0, $zero, 0x1000
  sw $t0, 0($s4)              /* SP_MEM_ADDR = IMEM */
  addiu $t0, $zero, 0x1000
  sw $t0, 4($s4)              /* SP_DRAM_ADDR = RDRAM 0x1000 */
  addiu $t0, $zero, (rsp_program_end - rsp_program) - 1
  sw $t0, 8($s4)              /* SP_RD_LEN */
wait_imem_dma:
  lw $t1, 0x10($s4)
  andi $t1, $t1, 4
  bne $t1, $zero, wait_imem_dma
  nop

  sw $zero, 0($s4)            /* SP_MEM_ADDR = DMEM */
  addiu $t0, $zero, 0x2000
  sw $t0, 4($s4)              /* SP_DRAM_ADDR = RDRAM 0x2000 */
  addiu $t0, $zero, (rsp_input_end - rsp_input) - 1
  sw $t0, 8($s4)              /* SP_RD_LEN */
wait_dmem_dma:
  lw $t1, 0x10($s4)
  andi $t1, $t1, 4
  bne $t1, $zero, wait_dmem_dma
  nop

  addiu $t0, $zero, 1
  sw $t0, 0x10($s4)           /* Clear HALT; start fetched RSP execution. */
wait_rsp_halt:
  lw $t1, 0x10($s4)
  andi $t1, $t1, 1
  beq $t1, $zero, wait_rsp_halt
  nop

  /* Check vector output, CFC2(VCO), DMA input, SP status and exact SP_PC. */
  lui $t0, 0xa400
  lhu $t1, 0($t0)
  addiu $t2, $zero, 7
  bne $t1, $t2, guest_fail
  nop
  lw $t1, 0x20($t0)
  bne $t1, $zero, guest_fail
  nop
  lw $t1, 0x100($t0)
  lui $t2, 0x1122
  ori $t2, $t2, 0x3344
  bne $t1, $t2, guest_fail
  nop
  lw $t1, 0x10($s4)
  andi $t1, $t1, 3
  addiu $t2, $zero, 3
  bne $t1, $t2, guest_fail
  nop
  lui $t0, 0xa408
  lw $t1, 0($t0)
  addiu $t2, $zero, (rsp_break - rsp_program)
  bne $t1, $t2, guest_fail
  nop
  lw $t1, 0x1c($s4)           /* First semaphore read returns zero. */
  bne $t1, $zero, guest_fail
  nop
  lw $t1, 0x1c($s4)           /* Read-to-set: second read returns one. */
  addiu $t2, $zero, 1
  bne $t1, $t2, guest_fail
  nop
  b guest_pass
  nop

guest_fail:
  lui $t0, ISVIEWER_BASE >> 16
  lui $t1, ISVIEWER_MAGIC >> 16
  ori $t1, $t1, ISVIEWER_MAGIC & 0xffff
  sw $t1, 0($t0)
  addiu $t2, $zero, 'F'
  sb $t2, ISVIEWER_BUFFER($t0)
  addiu $t2, $zero, 'A'
  sb $t2, ISVIEWER_BUFFER + 1($t0)
  addiu $t2, $zero, 'I'
  sb $t2, ISVIEWER_BUFFER + 2($t0)
  addiu $t2, $zero, 'L'
  sb $t2, ISVIEWER_BUFFER + 3($t0)
  addiu $t2, $zero, '\n'
  sb $t2, ISVIEWER_BUFFER + 4($t0)
  addiu $t2, $zero, 5
  sh $t2, ISVIEWER_FLUSH($t0)
  b terminal
  nop

guest_pass:
  lui $t0, ISVIEWER_BASE >> 16
  lui $t1, ISVIEWER_MAGIC >> 16
  ori $t1, $t1, ISVIEWER_MAGIC & 0xffff
  sw $t1, 0($t0)
  addiu $t2, $zero, 'T'
  sb $t2, ISVIEWER_BUFFER($t0)
  addiu $t2, $zero, '6'
  sb $t2, ISVIEWER_BUFFER + 1($t0)
  addiu $t2, $zero, '4'
  sb $t2, ISVIEWER_BUFFER + 2($t0)
  addiu $t2, $zero, '-'
  sb $t2, ISVIEWER_BUFFER + 3($t0)
  addiu $t2, $zero, 'R'
  sb $t2, ISVIEWER_BUFFER + 4($t0)
  addiu $t2, $zero, 'S'
  sb $t2, ISVIEWER_BUFFER + 5($t0)
  addiu $t2, $zero, 'P'
  sb $t2, ISVIEWER_BUFFER + 6($t0)
  addiu $t2, $zero, '-'
  sb $t2, ISVIEWER_BUFFER + 7($t0)
  addiu $t2, $zero, 'S'
  sb $t2, ISVIEWER_BUFFER + 8($t0)
  addiu $t2, $zero, 'M'
  sb $t2, ISVIEWER_BUFFER + 9($t0)
  addiu $t2, $zero, 'O'
  sb $t2, ISVIEWER_BUFFER + 10($t0)
  addiu $t2, $zero, 'K'
  sb $t2, ISVIEWER_BUFFER + 11($t0)
  addiu $t2, $zero, 'E'
  sb $t2, ISVIEWER_BUFFER + 12($t0)
  addiu $t2, $zero, ':'
  sb $t2, ISVIEWER_BUFFER + 13($t0)
  addiu $t2, $zero, 'P'
  sb $t2, ISVIEWER_BUFFER + 14($t0)
  addiu $t2, $zero, 'A'
  sb $t2, ISVIEWER_BUFFER + 15($t0)
  addiu $t2, $zero, 'S'
  sb $t2, ISVIEWER_BUFFER + 16($t0)
  addiu $t2, $zero, 'S'
  sb $t2, ISVIEWER_BUFFER + 17($t0)
  addiu $t2, $zero, '\n'
  sb $t2, ISVIEWER_BUFFER + 18($t0)
  addiu $t2, $zero, 19
  sh $t2, ISVIEWER_FLUSH($t0)
terminal:
.if VARIANT == 4
  b terminal
  addiu $s7, $zero, 1
.else
  b terminal
  nop
.endif

/* CPU uses these exact raw fetched words after SP DMA; all values are literal. */
.section .rodata.rsp,"a",@progbits
.balign 8
.globl rsp_program
rsp_program:
  .word 0x24010003             /* ADDIU r1, r0, 3 */
  .word 0x48810800             /* MTC2 v1[0], r1 */
  .word 0x24020004             /* ADDIU r2, r0, 4 */
  .word 0x48821000             /* MTC2 v2[0], r2 */
  .word 0x10000001             /* BEQ r0, r0, +1 */
  .word 0x24030009             /* Delay slot: ADDIU r3, r0, 9 */
  .word 0x24040007             /* Independent SU paired with VADD */
  .if VARIANT == 2
    .word 0x4a0208f8           /* Reserved function 0x38: intentional refusal */
  .else
    .word 0x4a0208d0           /* VADD v3, v1, v2; lane 0 is 3 + 4 = 7 */
  .endif
  .word 0xe8032000             /* SQV v3, 0(r0) */
  .word 0x48450000             /* CFC2 r5, VCO */
  .word 0xac050020             /* SW r5, 0x20(r0) */
.if VARIANT == 3
  .word 0x00000000             /* Missing BREAK negative variant */
.else
rsp_break:
  .word 0x0000000d             /* BREAK */
.endif
  .balign 8
rsp_program_end:

.section .rodata.rsp_input,"a",@progbits
.balign 8
rsp_input:
  .word 0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff00
  .word 0x01234567, 0x89abcdef, 0x76543210, 0xfedcba98
rsp_input_end:

.globl rsp_break
.globl terminal
