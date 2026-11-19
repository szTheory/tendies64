/* Authored MIPS III guest payload for the Phase 9 admit family. No SDK or
 * third-party code is copied. The ISViewer print idiom follows
 * fixtures/hello/hello.S.
 *
 * One source builds every variant, selected by optional assembler defsyms:
 *   T64_ADMIT_ID    0 to 3, the digit N of the transcript (default 0)
 *   T64_ADMIT_WRONG when defined, the digit is replaced by the letter x
 *   T64_ADMIT_SPIN  decrement-loop iteration count run before the print
 *                   (default 0 skips the loop)
 * The transcript is "t64 phase09 admit N" plus a newline (20 bytes). Editing
 * this file re-pins every admit row, so it is authored once with all variants.
 */
.set mips3
.set noreorder
.set noat

/* Guest uses the uncached KSEG1 alias of physical ISViewer 0x13ff0000. */
.equ T64_ISVIEWER_BASE, 0xb3ff0000
.equ T64_ISVIEWER_BUFFER, 0x20
.equ T64_ISVIEWER_FLUSH_HALFWORD, 0x16
.equ T64_ISVIEWER_MAGIC_HI, 0x1234
.equ T64_ISVIEWER_MAGIC_LO, 0x5678
.equ admit_text_length, 20

.ifndef T64_ADMIT_ID
.set T64_ADMIT_ID, 0
.endif
.ifndef T64_ADMIT_SPIN
.set T64_ADMIT_SPIN, 0
.endif
.if (T64_ADMIT_ID < 0) || (T64_ADMIT_ID > 3)
.error "T64_ADMIT_ID must be 0 to 3"
.endif
.if (T64_ADMIT_SPIN < 0) || (T64_ADMIT_SPIN > 0x7fffffff)
.error "T64_ADMIT_SPIN must fit a positive 31-bit count"
.endif

.section .text.start,"ax",@progbits
.globl _start
_start:
.if T64_ADMIT_SPIN != 0
  /* Pure CPU delay before the print, used by the long-cap control. */
  lui $t7, (T64_ADMIT_SPIN >> 16) & 0xffff
  ori $t7, $t7, T64_ADMIT_SPIN & 0xffff
admit_spin:
  addiu $t7, $t7, -1
  bne $t7, $zero, admit_spin
  nop
.endif
  lui $t0, T64_ISVIEWER_BASE >> 16
  lui $t1, T64_ISVIEWER_MAGIC_HI
  ori $t1, $t1, T64_ISVIEWER_MAGIC_LO
  lui $t2, 0xedcb
  ori $t2, $t2, 0xa987
  sw $t2, 0($t0)
  lw $t3, 0($t0)
  bne $t2, $t3, empty_break
  nop
  sw $t1, 0($t0)
  lw $t3, 0($t0)
  bne $t1, $t3, empty_break
  nop
  addiu $t2, $t0, T64_ISVIEWER_BUFFER
  lui $t3, %hi(admit_text)
  addiu $t3, $t3, %lo(admit_text)
  lui $t4, %hi(admit_text_end)
  addiu $t4, $t4, %lo(admit_text_end)
emit_next:
  beq $t3, $t4, emit_flush
  nop
  lbu $t5, 0($t3)
  sb $t5, 0($t2)
  addiu $t2, $t2, 1
  addiu $t3, $t3, 1
  b emit_next
  nop
emit_flush:
  addiu $t6, $zero, admit_text_length
  sh $t6, T64_ISVIEWER_FLUSH_HALFWORD($t0)
.globl t64_admit_completion
t64_admit_completion:
  break
empty_break:
  break

.section .rodata,"a",@progbits
admit_text:
  .ascii "t64 phase09 admit "
.ifdef T64_ADMIT_WRONG
  .ascii "x"
.else
  .byte '0' + T64_ADMIT_ID
.endif
  .ascii "\n"
admit_text_end:
.if (admit_text_end - admit_text) != admit_text_length
.error "admit transcript length changed"
.endif
