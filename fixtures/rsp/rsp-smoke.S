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
  /* Copy the authored RSP program and input block through the cartridge bus. */
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

.if VARIANT == 5
  addiu $t0, $zero, 0x0ff8    /* Negative: input DMA wraps within DMEM bank. */
.else
  addiu $t0, $zero, 0x0100
.endif
  sw $t0, 0($s4)              /* SP_MEM_ADDR = DMEM input destination */
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

  /* Compare all bytes touched by aligned and partial vector transfers. */
  lui $t0, 0xa400
  addiu $t1, $t0, 0x80
  lui $t2, %hi(expected_rsp_bytes - PAYLOAD_BASE + CART_PAYLOAD_BASE)
  addiu $t2, $t2, %lo(expected_rsp_bytes - PAYLOAD_BASE + CART_PAYLOAD_BASE)
  addiu $t3, $zero, expected_rsp_bytes_end - expected_rsp_bytes
check_rsp_bytes:
  lbu $t4, 0($t1)
  lbu $t5, 0($t2)
  bne $t4, $t5, guest_fail
  nop
  addiu $t1, $t1, 1
  addiu $t2, $t2, 1
  addiu $t3, $t3, -1
  bne $t3, $zero, check_rsp_bytes
  nop

  /* VCO after VADDC, the SU load-use result, and original DMA input. */
  lw $t1, 0x20($t0)
  addiu $t2, $zero, 2
  bne $t1, $t2, guest_fail
  nop
  lw $t1, 0x24($t0)
  lui $t2, 0x1122
  ori $t2, $t2, 0x3344
  bne $t1, $t2, guest_fail
  nop
  lw $t1, 0x100($t0)
  bne $t1, $t2, guest_fail
  nop

  /* VMULF product slices: 32767*1 -> ACC=0x000000017ffe. */
  lhu $t1, 0xa0($t0)
  bne $t1, $zero, guest_fail
  nop
  lhu $t1, 0xb0($t0)
  addiu $t2, $zero, 1
  bne $t1, $t2, guest_fail
  nop
  lhu $t1, 0xc0($t0)
  addiu $t2, $zero, 0x7ffe
  bne $t1, $t2, guest_fail
  nop

  /* VRCPH/VRCPL high-low sequence uses the guide's zero-input policy. */
  lhu $t1, 0xd0($t0)
  bne $t1, $zero, guest_fail
  nop
  lhu $t1, 0xe0($t0)
  ori $t2, $zero, 0xffff
  bne $t1, $t2, guest_fail
  nop

  /* SP status, BREAK PC, and read-to-set semaphore are bus-visible. */
  lw $t1, 0x10($s4)
  andi $t1, $t1, 3
  addiu $t2, $zero, 3
  bne $t1, $t2, guest_fail
  nop
  lui $t0, 0xa408
  lw $t1, 0($t0)
  addiu $t2, $zero, rsp_break - rsp_program
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
  addiu $s7, $zero, 1       /* Negative: changed terminal delay-slot word. */
.else
  b terminal
  nop
.endif

/* Raw fetched RSP program; every expected value is derived independently. */
.section .rodata.rsp,"a",@progbits
.balign 8
.globl rsp_program
rsp_program:
  .word 0x24017fff             /* ADDIU r1, r0, 0x7fff */
  .word 0x48810800             /* MTC2 v1[0], r1 */
  .word 0x24020001             /* ADDIU r2, r0, 1 */
  .word 0x48821000             /* MTC2 v2[0], r2 */
  .word 0x10000001             /* BEQ r0, r0, +1 */
  .word 0x24030009             /* Delay slot: ADDIU r3, r0, 9 */
  .word 0x24040007             /* Independent SU paired with VADD */
.if VARIANT == 2
  .word 0x4a0208f8             /* Reserved function 0x38: refusal control. */
.elseif VARIANT == 1
  .word 0x4a020950             /* Wrong-result control: writes v5, not v3. */
.else
  .word 0x4a0208d0             /* Saturating VADD v3, v1, v2. */
.endif
  .word 0x2401ffff             /* ADDIU r1, r0, -1 */
  .word 0x48810900             /* MTC2 v1[1], r1 */
  .word 0x24020001             /* ADDIU r2, r0, 1 */
  .word 0x48821100             /* MTC2 v2[1], r2 */
  .word 0x4a0208d0             /* VADD v3, v1, v2; signed saturation */
  .word 0x4a020954             /* VADDC v5, v1, v2; lane 1 carry */
  .word 0x48450000             /* CFC2 r5, VCO */
  .word 0xac050020             /* SW r5, 0x20(r0) */
  .word 0x8c0a0100             /* LW r10, 0x100(r0) */
  .word 0x01405821             /* ADDU r11, r10, r0: immediate load use */
  .word 0xac0b0024             /* SW r11, 0x24(r0) */
  .word 0x24060107             /* ADDIU r6, r0, 0x107 */
  .word 0xc8c42000             /* LQV v4, 0(r6): nine-byte partial load */
  .word 0x4b031133             /* VMOV v4[2], v3[0]; other lanes preserved */
  .word 0x24060080             /* ADDIU r6, r0, 0x80 */
  .word 0xe8c42000             /* SQV v4, 0(r6): aligned output block */
  .word 0x24060099             /* ADDIU r6, r0, 0x99 */
  .word 0xe8c42000             /* SQV v4, 0(r6): seven-byte partial store */
  .word 0x4a020980             /* VMULF v6, v1, v2; 32767 * 1 */
  .word 0x4a02095d             /* VSAR e=0: ACC[47:32] */
  .word 0xe8052000             /* SQV v5, 0xa0(r0) */
  .word 0x4a22099d             /* VSAR e=1: ACC[31:16] to v6 */
  .word 0xe8062000             /* SQV v6, 0xb0(r0) */
  .word 0x4a4209dd             /* VSAR e=2: ACC[15:0] to v7 */
  .word 0xe8072000             /* SQV v7, 0xc0(r0) */
  .word 0x24090000             /* ADDIU r9, r0, 0 */
  .word 0x48894800             /* MTC2 v9[0], r9 */
  .word 0x4a090232             /* VRCPH v8[0], v9[0]: previous high */
  .word 0xe8082000             /* SQV v8, 0xd0(r0) */
  .word 0x4a090231             /* VRCPL v8[0], v9[0]: zero special result */
  .word 0xe8082000             /* SQV v8, 0xe0(r0) */
.if VARIANT == 3
rsp_break:
  .word 0x00000000             /* Missing-BREAK control. */
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

.section .rodata.expected,"a",@progbits
.balign 4
expected_rsp_bytes:
  .byte 0x88,0x99,0xaa,0xbb,0x7f,0xff,0xee,0xff,0x00
  .byte 0,0,0,0,0,0,0
  .byte 0,0,0,0,0,0,0,0,0
  .byte 0x88,0x99,0xaa,0xbb,0x7f,0xff,0xee
expected_rsp_bytes_end:

.globl rsp_break
.globl terminal
