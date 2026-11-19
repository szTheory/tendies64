/* Authored MIPS III guest payload for the Phase 9 sram-rw diagnostic. No SDK
 * or third-party code is copied. Register setup, the interrupt mask and the
 * ISViewer print idiom follow fixtures/pad/poll.S.
 *
 * The program exercises the SRAM window [0x08000000, 0x08008000) of an
 * explicit SRAM profile through the PI DMA registers and aligned word access,
 * printing five lines:
 *
 *   SRAM 32k              the header (9 bytes with the newline)
 *   Z <8 hex>             the word at 0xA8007FFC, read before any DMA (blank)
 *   D <32 hex>            16 bytes written RDRAM to SRAM at 0x08000100 by the
 *                         PI read-length DMA, the RDRAM destination zeroed,
 *                         then read back by the PI write-length DMA
 *   C <8 hex>             the CPU word 0x12345678 stored at 0xA8000104 and
 *                         loaded back
 *   M <32 hex>            a second read-back DMA after zeroing the destination
 *                         again: the CPU word shows inside the pattern
 *
 * One variant exists, selected by an assembler defsym:
 *   T64_SAVE_WRONG  the last pattern byte is 0xfe, not 0xff
 * Editing this file re-pins every sram-rw row, so it is authored once with its
 * only variant. Every figure here is a software-profile policy value, not a
 * hardware measurement.
 */
.set mips3
.set noreorder
.set noat

/* Guest uses the uncached KSEG1 aliases of the physical device addresses. */
.equ T64_ISVIEWER_BASE_HI, 0xb3ff
.equ T64_ISVIEWER_BUFFER, 0x20
.equ T64_ISVIEWER_FLUSH_HALFWORD, 0x16
.equ T64_ISVIEWER_MAGIC_HI, 0x1234
.equ T64_ISVIEWER_MAGIC_LO, 0x5678
.equ T64_PI_BASE_HI, 0xa460
.equ T64_PI_DRAM_ADDR, 0x00
.equ T64_PI_CART_ADDR, 0x04
.equ T64_PI_RD_LEN, 0x08
.equ T64_PI_WR_LEN, 0x0c
.equ T64_PI_STATUS, 0x10
.equ T64_SRAM_WINDOW_HI, 0xa800
.equ T64_SRAM_LAST_WORD, 0x7ffc
.equ T64_SRAM_DMA_WORD, 0x0104
.equ T64_SRAM_DMA_CART_HI, 0x0800
.equ T64_SRAM_DMA_CART_LO, 0x0100
.equ T64_PATTERN_RDRAM, 0x3800
.equ T64_RETURN_RDRAM, 0x3a00
.equ T64_DMA_LENGTH_RAW, 15
.equ T64_PATTERN_BYTES, 16
.equ header_length, 9

.equ CH_SPACE, 0x20
.equ CH_NEWLINE, 0x0a

/* Copy 16 pattern bytes from the literal to the uncached RDRAM pattern area. */
.macro COPY_PATTERN
  lui $t0, %hi(pattern_literal)
  addiu $t0, $t0, %lo(pattern_literal)
  or $t1, $s2, $zero
  addiu $t2, $zero, 4
1:
  lw $t3, 0($t0)
  sw $t3, 0($t1)
  addiu $t0, $t0, 4
  addiu $t1, $t1, 4
  addiu $t2, $t2, -1
  bne $t2, $zero, 1b
  nop
.endm

/* Zero the 16-byte return area. */
.macro ZERO_RETURN
  or $t1, $s3, $zero
  addiu $t2, $zero, 4
1:
  sw $zero, 0($t1)
  addiu $t1, $t1, 4
  addiu $t2, $t2, -1
  bne $t2, $zero, 1b
  nop
.endm

/* One PI DMA: the RDRAM address and the length register offset (the read
 * length register writes RDRAM to SRAM, the write length register reads SRAM
 * to RDRAM) are macro arguments; the cart address is always 0x08000100. */
.macro PI_DMA dram, length_reg
  addiu $t0, $zero, \dram
  sw $t0, T64_PI_DRAM_ADDR($s4)
  lui $t1, T64_SRAM_DMA_CART_HI
  ori $t1, $t1, T64_SRAM_DMA_CART_LO
  sw $t1, T64_PI_CART_ADDR($s4)
  addiu $t0, $zero, T64_DMA_LENGTH_RAW
  sw $t0, \length_reg($s4)
2:
  lw $t2, T64_PI_STATUS($s4)
  andi $t2, $t2, 1
  bne $t2, $zero, 2b
  nop
  addiu $t0, $zero, 2
  sw $t0, T64_PI_STATUS($s4)
.endm

.section .text.start,"ax",@progbits
.globl _start
_start:
  /* Keep the diagnostic's interrupt observation guest-owned without
   * diverting execution into IPL3's interrupt vector. */
  mfc0 $t2, $12
  addiu $t3, $zero, -2
  and $t2, $t2, $t3
  mtc0 $t2, $12
  nop

  lui $s0, T64_ISVIEWER_BASE_HI
  lui $s2, 0xa000
  ori $s2, $s2, T64_PATTERN_RDRAM
  lui $s3, 0xa000
  ori $s3, $s3, T64_RETURN_RDRAM
  lui $s4, T64_PI_BASE_HI
  lui $s5, T64_SRAM_WINDOW_HI

  /* ISViewer magic handshake; no output when it fails. */
  lui $t1, T64_ISVIEWER_MAGIC_HI
  ori $t1, $t1, T64_ISVIEWER_MAGIC_LO
  lui $t2, 0xedcb
  ori $t2, $t2, 0xa987
  sw $t2, 0($s0)
  lw $t3, 0($s0)
  bne $t2, $t3, empty_break
  nop
  sw $t1, 0($s0)
  lw $t3, 0($s0)
  bne $t1, $t3, empty_break
  nop

  /* Header line. */
  lui $t0, %hi(header_text)
  addiu $t0, $t0, %lo(header_text)
  addiu $t2, $s0, T64_ISVIEWER_BUFFER
  addiu $t4, $zero, header_length
header_copy:
  lbu $t5, 0($t0)
  sb $t5, 0($t2)
  addiu $t0, $t0, 1
  addiu $t2, $t2, 1
  addiu $t4, $t4, -1
  bne $t4, $zero, header_copy
  nop
  addiu $t6, $zero, header_length
  sh $t6, T64_ISVIEWER_FLUSH_HALFWORD($s0)

  /* Z: the last word of the window, read before any DMA, copied to the
   * return area so the shared print routine reads it from RDRAM. */
  lw $t0, T64_SRAM_LAST_WORD($s5)
  sw $t0, 0($s3)
  addiu $a0, $zero, 0x5a
  addiu $a2, $zero, 4
  jal print_line
  nop

  /* D: pattern to SRAM, zero the return area, read it back. */
  COPY_PATTERN
  PI_DMA T64_PATTERN_RDRAM, T64_PI_RD_LEN
  ZERO_RETURN
  PI_DMA T64_RETURN_RDRAM, T64_PI_WR_LEN
  addiu $a0, $zero, 0x44
  addiu $a2, $zero, T64_PATTERN_BYTES
  jal print_line
  nop

  /* C: a CPU word store inside the DMA'd region, loaded back. */
  lui $t0, 0x1234
  ori $t0, $t0, 0x5678
  sw $t0, T64_SRAM_DMA_WORD($s5)
  lw $t1, T64_SRAM_DMA_WORD($s5)
  sw $t1, 0($s3)
  addiu $a0, $zero, 0x43
  addiu $a2, $zero, 4
  jal print_line
  nop

  /* M: zero the return area again and read the region back. */
  ZERO_RETURN
  PI_DMA T64_RETURN_RDRAM, T64_PI_WR_LEN
  addiu $a0, $zero, 0x4d
  addiu $a2, $zero, T64_PATTERN_BYTES
  jal print_line
  nop
.globl t64_save_completion
t64_save_completion:
  break
empty_break:
  break

/* $a0 = tag character, $a2 = byte count; prints "<tag> <hex of the bytes at
 * the return area>\n" and flushes once. */
print_line:
  or $s6, $ra, $zero
  addiu $t2, $s0, T64_ISVIEWER_BUFFER
  sb $a0, 0($t2)
  addiu $t6, $zero, CH_SPACE
  sb $t6, 1($t2)
  addiu $t2, $t2, 2
  or $t5, $s3, $zero
print_data:
  lbu $a0, 0($t5)
  jal emit_hex_byte
  nop
  addiu $t5, $t5, 1
  addiu $a2, $a2, -1
  bne $a2, $zero, print_data
  nop
  addiu $t6, $zero, CH_NEWLINE
  sb $t6, 0($t2)
  addiu $t2, $t2, 1
  addiu $t3, $s0, T64_ISVIEWER_BUFFER
  subu $t6, $t2, $t3
  sh $t6, T64_ISVIEWER_FLUSH_HALFWORD($s0)
  jr $s6
  nop

/* $a0 = byte, $t2 = output pointer; writes two lowercase hex digits. */
emit_hex_byte:
  lui $t8, %hi(hex_digits)
  addiu $t8, $t8, %lo(hex_digits)
  srl $t9, $a0, 4
  andi $t9, $t9, 15
  addu $t9, $t8, $t9
  lbu $t9, 0($t9)
  sb $t9, 0($t2)
  andi $t9, $a0, 15
  addu $t9, $t8, $t9
  lbu $t9, 0($t9)
  sb $t9, 1($t2)
  jr $ra
  addiu $t2, $t2, 2

.section .rodata,"a",@progbits
.align 2
pattern_literal:
  .byte 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77
  .byte 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee
.ifdef T64_SAVE_WRONG
  .byte 0xfe
.else
  .byte 0xff
.endif
pattern_literal_end:
.if (pattern_literal_end - pattern_literal) != T64_PATTERN_BYTES
.error "sram-rw pattern length changed"
.endif
header_text:
  .ascii "SRAM 32k\n"
header_text_end:
.if (header_text_end - header_text) != header_length
.error "sram-rw header text length changed"
.endif
hex_digits:
  .ascii "0123456789abcdef"
