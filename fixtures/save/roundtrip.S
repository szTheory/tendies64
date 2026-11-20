/* Authored MIPS III guest payload for the Phase 9 save-roundtrip diagnostic. No
 * SDK or third-party code is copied. Register setup, the interrupt mask, the
 * PI DMA register idiom and the ISViewer print idiom follow
 * fixtures/save/sram.S.
 *
 * The program reads and rewrites the SRAM window [0x08000000, 0x08008000) of
 * an explicit SRAM profile through the PI DMA registers (the write-length
 * register starts SRAM to RDRAM, the read-length register starts RDRAM to
 * SRAM, length register value 15 for 16 bytes) and prints four lines of 36
 * bytes each, 144 bytes in total:
 *
 *   R0 <32 hex>   SRAM offsets 0x0000 to 0x000F, read before any write
 *   R1 <32 hex>   SRAM offsets 0x7FF0 to 0x7FFF, read before any write
 *   W0 <32 hex>   SRAM offset 0x0100 after writing the W block there
 *   W1 <32 hex>   SRAM offset 0x7FF0 after writing the V block there
 *
 * The host supplies the SRAM content before the guest starts (the save import
 * path); the guest never creates it. The scratch area at RDRAM 0x3800 is
 * zeroed before every read-back so a failed DMA is visible as zero digits.
 *
 * W block: c3 5a 96 e1 0f 78 b2 4d 21 8e f6 34 69 d0 17 ab
 * V block: 18 e5 7c b9 03 d6 4a 91 ff 2e a7 60 c8 35 8b 5e
 *
 * Editing this file re-pins the roundtrip row, so it is authored once with no
 * variant. Every figure here is a software-profile policy value, not a
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
.equ T64_SRAM_CART_HI, 0x0800
.equ T64_SRAM_OFFSET_FIRST, 0x0000
.equ T64_SRAM_OFFSET_W, 0x0100
.equ T64_SRAM_OFFSET_V, 0x7ff0
.equ T64_SCRATCH_RDRAM, 0x3800
.equ T64_W_RDRAM, 0x3900
.equ T64_V_RDRAM, 0x3910
.equ T64_DMA_LENGTH_RAW, 15
.equ T64_BLOCK_BYTES, 16

.equ CH_SPACE, 0x20
.equ CH_NEWLINE, 0x0a
.equ CH_R, 0x52
.equ CH_W, 0x57
.equ CH_ZERO, 0x30
.equ CH_ONE, 0x31

/* Copy 16 literal bytes to the uncached RDRAM address held in \dst. */
.macro COPY16 literal, dst
  lui $t0, %hi(\literal)
  addiu $t0, $t0, %lo(\literal)
  or $t1, \dst, $zero
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

/* Zero the 16-byte scratch area. */
.macro ZERO_SCRATCH
  or $t1, $s3, $zero
  addiu $t2, $zero, 4
1:
  sw $zero, 0($t1)
  addiu $t1, $t1, 4
  addiu $t2, $t2, -1
  bne $t2, $zero, 1b
  nop
.endm

/* One 16-byte PI DMA between RDRAM \dram and the SRAM offset \cart_lo. */
.macro PI_DMA dram, cart_lo, length_reg
  addiu $t0, $zero, \dram
  sw $t0, T64_PI_DRAM_ADDR($s4)
  lui $t1, T64_SRAM_CART_HI
  ori $t1, $t1, \cart_lo
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

/* Print "<tag1><tag2> <32 hex of the scratch area>\n" and flush once. */
.macro PRINT_LINE tag1, tag2
  addiu $a0, $zero, \tag1
  addiu $a1, $zero, \tag2
  jal print_line
  nop
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
  lui $s3, 0xa000
  ori $s3, $s3, T64_SCRATCH_RDRAM
  lui $s4, T64_PI_BASE_HI
  lui $s5, 0xa000
  ori $s5, $s5, T64_W_RDRAM
  lui $s7, 0xa000
  ori $s7, $s7, T64_V_RDRAM

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

  /* R0 and R1: the imported content before any guest write. */
  ZERO_SCRATCH
  PI_DMA T64_SCRATCH_RDRAM, T64_SRAM_OFFSET_FIRST, T64_PI_WR_LEN
  PRINT_LINE CH_R, CH_ZERO
  ZERO_SCRATCH
  PI_DMA T64_SCRATCH_RDRAM, T64_SRAM_OFFSET_V, T64_PI_WR_LEN
  PRINT_LINE CH_R, CH_ONE

  /* Rewrite two blocks: W at offset 0x0100, V at offset 0x7ff0. */
  COPY16 w_literal, $s5
  PI_DMA T64_W_RDRAM, T64_SRAM_OFFSET_W, T64_PI_RD_LEN
  COPY16 v_literal, $s7
  PI_DMA T64_V_RDRAM, T64_SRAM_OFFSET_V, T64_PI_RD_LEN

  /* W0 and W1: read the rewritten blocks back. */
  ZERO_SCRATCH
  PI_DMA T64_SCRATCH_RDRAM, T64_SRAM_OFFSET_W, T64_PI_WR_LEN
  PRINT_LINE CH_W, CH_ZERO
  ZERO_SCRATCH
  PI_DMA T64_SCRATCH_RDRAM, T64_SRAM_OFFSET_V, T64_PI_WR_LEN
  PRINT_LINE CH_W, CH_ONE
.globl t64_save_completion
t64_save_completion:
  break
empty_break:
  break

/* $a0 and $a1 = the two tag characters; prints "<tag> <hex of the 16 bytes at
 * the scratch area>\n" and flushes once. */
print_line:
  or $s6, $ra, $zero
  addiu $t2, $s0, T64_ISVIEWER_BUFFER
  sb $a0, 0($t2)
  sb $a1, 1($t2)
  addiu $t6, $zero, CH_SPACE
  sb $t6, 2($t2)
  addiu $t2, $t2, 3
  or $t5, $s3, $zero
  addiu $a2, $zero, T64_BLOCK_BYTES
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
w_literal:
  .byte 0xc3, 0x5a, 0x96, 0xe1, 0x0f, 0x78, 0xb2, 0x4d
  .byte 0x21, 0x8e, 0xf6, 0x34, 0x69, 0xd0, 0x17, 0xab
w_literal_end:
.if (w_literal_end - w_literal) != T64_BLOCK_BYTES
.error "save-roundtrip W block length changed"
.endif
v_literal:
  .byte 0x18, 0xe5, 0x7c, 0xb9, 0x03, 0xd6, 0x4a, 0x91
  .byte 0xff, 0x2e, 0xa7, 0x60, 0xc8, 0x35, 0x8b, 0x5e
v_literal_end:
.if (v_literal_end - v_literal) != T64_BLOCK_BYTES
.error "save-roundtrip V block length changed"
.endif
hex_digits:
  .ascii "0123456789abcdef"
