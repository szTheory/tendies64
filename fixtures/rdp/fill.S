/* Authored CPU guest diagnostic. No SDK command encoder or implementation is
 * copied: the six original big-endian words below are literal test inputs. */
.set mips3
.set noreorder
.set noat

.section .text.start,"ax",@progbits
.globl _start
_start:
  /* Keep the diagnostic's MI pending/clear observation guest-owned without
   * diverting execution into IPL3's interrupt vector. */
  mfc0 $t2, $12
  addiu $t3, $zero, -2
  and $t2, $t2, $t3
  mtc0 $t2, $12
  nop

  /* Copy the authored command words into uncached RDRAM using CPU loads and
   * stores, then submit them through ordinary DPC START/END registers. */
  lui $t0, %hi(rdp_commands)
  addiu $t0, $t0, %lo(rdp_commands)
  lui $t1, 0xa000
  ori $t1, $t1, 0x3100
  addiu $t2, $zero, 12
copy_commands:
  lw $t3, 0($t0)
  sw $t3, 0($t1)
  addiu $t0, $t0, 4
  addiu $t1, $t1, 4
  addiu $t2, $t2, -1
  bne $t2, $zero, copy_commands
  nop

  /* Enable the documented modeled DP mask before submitting FullSync. */
  lui $t0, 0xa430
  addiu $t1, $zero, 0x0800
  sw $t1, 12($t0)

  lui $t0, 0xa410
  lui $t1, 0
  ori $t1, $t1, 0x3100
  sw $t1, 0($t0)
  ori $t1, $t1, 0x0030
  sw $t1, 4($t0)

  /* Program a 4x3 diagnostic source window and a short named VI interval.
   * H/V starts are half-open under this software profile policy. */
  lui $t0, 0xa440
  addiu $t1, $zero, 0x3000
  sw $t1, 4($t0)
  addiu $t1, $zero, 4
  sw $t1, 8($t0)
  addiu $t1, $zero, 2
  sw $t1, 24($t0)
  addiu $t1, $zero, 4
  sw $t1, 28($t0)
  sw $t1, 36($t0)
  addiu $t1, $zero, 3
  sw $t1, 40($t0)
  ori $t1, $zero, 0x0400
  sw $t1, 48($t0)
  sw $t1, 52($t0)
  addiu $t1, $zero, 2
  sw $t1, 0($t0)

  /* Wait in guest instructions so FullSync and multiple fields are observable
   * before the guest records and clears the DP interrupt state. */
  addiu $t2, $zero, 128
wait_for_fields:
  addiu $t2, $t2, -1
  bne $t2, $zero, wait_for_fields
  nop

  lui $t0, 0xa430
  lui $t1, 0xa000
  ori $t1, $t1, 0x3200
  lw $t2, 8($t0)
  sw $t2, 0($t1)
  lw $t2, 12($t0)
  sw $t2, 4($t1)
  addiu $t2, $zero, 0x0800
  sw $t2, 0($t0)
  lw $t2, 8($t0)
  sw $t2, 8($t1)

  /* Preserve the established exact authenticated guest transcript contract. */
  lui $t0, 0xb3ff
  lui $t1, 0x1234
  ori $t1, $t1, 0x5678
  lui $t2, 0xedcb
  ori $t2, $t2, 0xa987
  sw $t2, 0($t0)
  lw $t3, 0($t0)
  bne $t2, $t3, finish
  nop
  sw $t1, 0($t0)
  lw $t3, 0($t0)
  bne $t1, $t3, finish
  nop
  addiu $t2, $t0, 0x20
  lui $t3, %hi(hello_text)
  addiu $t3, $t3, %lo(hello_text)
  lui $t4, %hi(hello_text_end)
  addiu $t4, $t4, %lo(hello_text_end)
emit_text:
  beq $t3, $t4, emit_flush
  nop
  lbu $t5, 0($t3)
  sb $t5, 0($t2)
  addiu $t2, $t2, 1
  addiu $t3, $t3, 1
  b emit_text
  nop
emit_flush:
  addiu $t6, $zero, hello_text_end - hello_text
  sh $t6, 0x16($t0)
finish:
  break

.section .rodata,"a",@progbits
.align 3
rdp_commands:
  /* SetColorImage RGBA16, 4 pixels wide, origin 0x3000. */
  .word 0x3f100003, 0x00003000
  /* SetScissor from (0,0) through (3,3), in 10.2 coordinates. */
  .word 0x2d000000, 0x0000c00c
  /* Fill cycle, then 32-bit alternating 0xf801 / 0x07c1 halfwords. */
  .word 0x2f300000, 0x00000000
  .word 0x37000000, 0xf80107c1
  /* Inclusive lower-right fill endpoint (3,2), top-left (0,0). */
  .word 0x3600c008, 0x00000000
  .word 0x29000000, 0x00000000

hello_text:
  .ascii "Tendies64 hello via open IPL3\n"
hello_text_end:
