/* Authored MIPS III guest payload for the Phase 9 pad-poll diagnostic. No SDK
 * or third-party code is copied. Register setup, the interrupt mask and the
 * ISViewer print idiom follow fixtures/audio/pcm.S and fixtures/hello/hello.S.
 *
 * The program builds one literal 64-byte joybus frame in uncached RDRAM,
 * writes it to the modeled PIF RAM with one write DMA, then polls four times
 * with a read DMA of the same parsed frame. Per poll it reads CP0 Count before
 * the DMA, spins until the DMA completes, acknowledges the interrupt, and
 * prints one line through the ISViewer window:
 *
 *   P<n> <8 lowercase hex digits> <2 lowercase hex digits> <w>\n
 *
 * The 8 digits are PIF RAM bytes 3 to 6 of the read-back image (the
 * controller state response of channel 0), the 2 digits are byte 8 (the
 * response count byte of channel 1, which has no device), and w is '-' for
 * poll 1, otherwise '+' when the unsigned Count difference from the previous
 * poll is at least 10,000 and 'x' otherwise. Count advances once per two
 * guest ticks in the model, so w is a model-time witness at two-tick
 * resolution. Each line is 17 bytes; four lines are 68 bytes.
 *
 * One variant exists, selected by an assembler defsym:
 *   T64_PAD_WRONG  when defined, byte 8 is printed with its 0x80 bit cleared
 * Editing this file re-pins every pad-poll row, so it is authored once with
 * its only variant. Every figure here is a software-profile policy value, not
 * a hardware measurement.
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
.equ T64_SI_BASE_HI, 0xa480
.equ T64_SI_RD64B, 0x04
.equ T64_SI_WR64B, 0x10
.equ T64_SI_STATUS, 0x18
.equ T64_PIF_ADDRESS_HI, 0x1fc0
.equ T64_PIF_ADDRESS_LO, 0x07c0
.equ T64_FRAME_RDRAM, 0x3800
.equ T64_RECEIVE_RDRAM, 0x3900
.equ T64_FRAME_WORDS, 16
.equ T64_POLL_COUNT, 4
.equ T64_COUNT_STEP_MINIMUM, 10000
.equ pad_line_length, 17
.equ frame_literal_length, 64

.equ CH_P, 0x50
.equ CH_ZERO, 0x30
.equ CH_SPACE, 0x20
.equ CH_MINUS, 0x2d
.equ CH_PLUS, 0x2b
.equ CH_X, 0x78
.equ CH_NEWLINE, 0x0a

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
  lui $s1, T64_SI_BASE_HI
  lui $s2, 0xa000
  ori $s2, $s2, T64_FRAME_RDRAM
  lui $s3, 0xa000
  ori $s3, $s3, T64_RECEIVE_RDRAM

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

  /* Copy the literal frame (channel 0 state command expecting four bytes,
   * channel 1 the same, end escape, parse request in byte 63) into uncached
   * RDRAM with CPU loads and stores. */
  lui $t0, %hi(frame_literal)
  addiu $t0, $t0, %lo(frame_literal)
  or $t1, $s2, $zero
  addiu $t2, $zero, T64_FRAME_WORDS
copy_frame:
  lw $t3, 0($t0)
  sw $t3, 0($t1)
  addiu $t0, $t0, 4
  addiu $t1, $t1, 4
  addiu $t2, $t2, -1
  bne $t2, $zero, copy_frame
  nop

  /* Write DMA: RDRAM frame to the modeled PIF RAM, parsed at the start. */
  addiu $t0, $zero, T64_FRAME_RDRAM
  sw $t0, 0($s1)
  lui $t1, T64_PIF_ADDRESS_HI
  ori $t1, $t1, T64_PIF_ADDRESS_LO
  sw $t1, T64_SI_WR64B($s1)
wait_write:
  lw $t2, T64_SI_STATUS($s1)
  andi $t2, $t2, 1
  bne $t2, $zero, wait_write
  nop
  sw $zero, T64_SI_STATUS($s1)

  addiu $s5, $zero, 1
  addiu $s6, $zero, 0
poll_loop:
  /* Count before the poll is the model-time witness read of this poll. */
  mfc0 $s7, $9
  addiu $t0, $zero, T64_RECEIVE_RDRAM
  sw $t0, 0($s1)
  lui $t1, T64_PIF_ADDRESS_HI
  ori $t1, $t1, T64_PIF_ADDRESS_LO
  sw $t1, T64_SI_RD64B($s1)
wait_read:
  lw $t2, T64_SI_STATUS($s1)
  andi $t2, $t2, 1
  bne $t2, $zero, wait_read
  nop
  sw $zero, T64_SI_STATUS($s1)

  /* Compose one 17-byte line in the ISViewer buffer. */
  addiu $t2, $s0, T64_ISVIEWER_BUFFER
  addiu $t6, $zero, CH_P
  sb $t6, 0($t2)
  addiu $t6, $s5, CH_ZERO
  sb $t6, 1($t2)
  addiu $t6, $zero, CH_SPACE
  sb $t6, 2($t2)
  addiu $t2, $t2, 3
  lbu $a0, 3($s3)
  jal emit_hex_byte
  nop
  lbu $a0, 4($s3)
  jal emit_hex_byte
  nop
  lbu $a0, 5($s3)
  jal emit_hex_byte
  nop
  lbu $a0, 6($s3)
  jal emit_hex_byte
  nop
  addiu $t6, $zero, CH_SPACE
  sb $t6, 0($t2)
  addiu $t2, $t2, 1
  lbu $a0, 8($s3)
.ifdef T64_PAD_WRONG
  andi $a0, $a0, 0x7f
.endif
  jal emit_hex_byte
  nop
  addiu $t6, $zero, CH_SPACE
  sb $t6, 0($t2)
  addiu $t2, $t2, 1

  /* w: '-' for the first poll, otherwise the Count step witness. */
  addiu $t6, $zero, CH_MINUS
  addiu $t7, $zero, 1
  beq $s5, $t7, have_w
  nop
  subu $t4, $s7, $s6
  sltiu $t5, $t4, T64_COUNT_STEP_MINIMUM
  addiu $t6, $zero, CH_PLUS
  beq $t5, $zero, have_w
  nop
  addiu $t6, $zero, CH_X
have_w:
  sb $t6, 0($t2)
  addiu $t6, $zero, CH_NEWLINE
  sb $t6, 1($t2)

  /* One flush per line. */
  addiu $t6, $zero, pad_line_length
  sh $t6, T64_ISVIEWER_FLUSH_HALFWORD($s0)

  or $s6, $s7, $zero
  addiu $s5, $s5, 1
  sltiu $t0, $s5, T64_POLL_COUNT + 1
  bne $t0, $zero, poll_loop
  nop
.globl t64_pad_completion
t64_pad_completion:
  break
empty_break:
  break

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
frame_literal:
  /* Channel 0: TX 1 byte (state command 0x01), RX 4 bytes. */
  .byte 0x01, 0x04, 0x01, 0xff, 0xff, 0xff, 0xff
  /* Channel 1: the same handshake; no device answers on this channel. */
  .byte 0x01, 0x04, 0x01, 0xff, 0xff, 0xff, 0xff
  /* End escape, then zero fill to byte 62, then the parse request. */
  .byte 0xfe
  .fill 48, 1, 0
  .byte 0x01
frame_literal_end:
.if (frame_literal_end - frame_literal) != frame_literal_length
.error "pad-poll frame literal length changed"
.endif
hex_digits:
  .ascii "0123456789abcdef"
