/* Authored MIPS III guest payload for the Phase 10 composite scenario (RUN-07).
 * No SDK or third-party code is copied. Register setup, the interrupt mask,
 * the RDP command copy, the VI window and the ISViewer print idiom follow
 * fixtures/rdp/fill.S; the audio setup follows fixtures/audio/pcm.S; the SI
 * poll idiom follows fixtures/replay/mix.S; the PI DMA idiom follows
 * fixtures/save/sram.S. The command words of the two Z triangles, the samples
 * and the SRAM record are literals authored here.
 *
 * The program runs four phases on the profile with RDP depth, the filtered VI,
 * the AI, the SI controller and an SRAM 32K save device, and prints seven
 * lines through the ISViewer window (188 bytes in all):
 *
 *   VC <24 hex pairs>\n   the twelve RGBA16 color words of the 4x3 image at
 *                         RDRAM 0x3000, read back after FullSync (52 bytes)
 *   VZ <24 hex pairs>\n   the twelve 16-bit Z words at RDRAM 0x3400 (52 bytes)
 *   I1 <8 hex digits>\n   controller poll 1, PIF RAM bytes 3 to 6 (12 bytes)
 *   I2 <8 hex digits>\n   controller poll 2 (12 bytes)
 *   I3 <8 hex digits>\n   controller poll 3 (12 bytes)
 *   SR <32 hex digits>\n  the 16-byte SRAM record read back (36 bytes)
 *   AI <8 hex digits>\n   MI_INTERRUPT masked with the AI bit 0x04 after both
 *                         DMAs ended (12 bytes)
 *
 * Phase order: audio is queued first and plays in the background; the RDP
 * draws two primitive-color Z triangles over the whole 4x3 window (the second,
 * farther one is rejected by the Z test); the VI scans the window out for a few
 * fields and is then disabled so the raw capture stays small; three SI polls
 * read the controller the host input script drives; a 16-byte SRAM record (the
 * magic "T64C", the last polled word, their sum and their exclusive or) is
 * written and read back by PI DMA; the guest waits for the AI and completes.
 *
 * One variant exists, selected by an assembler defsym:
 *   T64_COMPOSITE_WRONG  the low bit of the last record word is flipped
 * Editing this file re-pins every composite row, so it is authored once with
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
.equ T64_PI_BASE_HI, 0xa460
.equ T64_PI_DRAM_ADDR, 0x00
.equ T64_PI_CART_ADDR, 0x04
.equ T64_PI_RD_LEN, 0x08
.equ T64_PI_WR_LEN, 0x0c
.equ T64_PI_STATUS, 0x10
.equ T64_SRAM_CART_HI, 0x0800
.equ T64_SRAM_CART_LO, 0x0100
.equ T64_DMA_LENGTH_RAW, 15

/* RDRAM layout (physical offsets, reached through the KSEG1 base in $s2). */
.equ T64_COLOR_RDRAM, 0x3000
.equ T64_COMMAND_RDRAM, 0x3100
.equ T64_COMMAND_BYTES, 176
.equ T64_COMMAND_WORDS, 44
.equ T64_DEPTH_RDRAM, 0x3400
.equ T64_DEPTH_WORDS, 6
.equ T64_IMAGE_BYTES, 24
.equ T64_PCM_RDRAM, 0x3500
.equ T64_PCM_WORDS, 8
.equ T64_FRAME_RDRAM, 0x3600
.equ T64_RECEIVE_RDRAM, 0x3700
.equ T64_RECORD_RDRAM, 0x3800
.equ T64_READBACK_RDRAM, 0x3900
.equ T64_MISC_RDRAM, 0x3a00
.equ T64_FRAME_WORDS, 16
.equ T64_RECORD_BYTES, 16

.equ T64_DACRATE, 1103
.equ T64_BITRATE, 15
.equ T64_VI_WAIT, 400
.equ T64_POLL_COUNT, 3
.ifndef T64_COMPOSITE_SPIN
.equ T64_COMPOSITE_SPIN, 1000
.endif
.equ frame_literal_length, 64

.equ CH_SPACE, 0x20
.equ CH_NEWLINE, 0x0a
.equ CH_ZERO, 0x30
.equ CH_V, 0x56
.equ CH_C, 0x43
.equ CH_Z, 0x5a
.equ CH_I, 0x49
.equ CH_S, 0x53
.equ CH_R, 0x52
.equ CH_A, 0x41

/* Zero the 16-byte readback area. */
.macro ZERO_READBACK
  ori $t1, $s2, T64_READBACK_RDRAM
  addiu $t2, $zero, 4
1:
  sw $zero, 0($t1)
  addiu $t1, $t1, 4
  addiu $t2, $t2, -1
  bne $t2, $zero, 1b
  nop
.endm

/* One PI DMA of 16 bytes against the SRAM offset 0x0100: the RDRAM address and
 * the length register offset are macro arguments (the read length register
 * writes RDRAM to SRAM, the write length register reads SRAM to RDRAM). */
.macro PI_DMA dram, length_reg
  addiu $t0, $zero, \dram
  sw $t0, T64_PI_DRAM_ADDR($s3)
  lui $t1, T64_SRAM_CART_HI
  ori $t1, $t1, T64_SRAM_CART_LO
  sw $t1, T64_PI_CART_ADDR($s3)
  addiu $t0, $zero, T64_DMA_LENGTH_RAW
  sw $t0, \length_reg($s3)
2:
  lw $t2, T64_PI_STATUS($s3)
  andi $t2, $t2, 1
  bne $t2, $zero, 2b
  nop
  addiu $t0, $zero, 2
  sw $t0, T64_PI_STATUS($s3)
.endm

/* Print "<c1><c2> <hex of count bytes at the address>\n" and flush once. */
.macro PRINT c1, c2, count, address
  addiu $a0, $zero, \c1
  addiu $a1, $zero, \c2
  addiu $a2, $zero, \count
  ori $a3, $s2, \address
  jal print_line
  nop
.endm

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

  lui $s0, T64_ISVIEWER_BASE_HI
  lui $s1, T64_SI_BASE_HI
  lui $s2, 0xa000
  lui $s3, T64_PI_BASE_HI
  lui $s4, 0xa450

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

  /* Fill the 4x3 Z image with the far word 0xfffc (six words of two). */
  lui $t0, 0xfffc
  ori $t0, $t0, 0xfffc
  ori $t1, $s2, T64_DEPTH_RDRAM
  addiu $t2, $zero, T64_DEPTH_WORDS
fill_depth:
  sw $t0, 0($t1)
  addiu $t1, $t1, 4
  addiu $t2, $t2, -1
  bne $t2, $zero, fill_depth
  nop

  /* Copy the authored command words into uncached RDRAM with CPU loads and
   * stores, then submit them through ordinary DPC START/END registers. */
  lui $t0, %hi(rdp_commands)
  addiu $t0, $t0, %lo(rdp_commands)
  ori $t1, $s2, T64_COMMAND_RDRAM
  addiu $t2, $zero, T64_COMMAND_WORDS
copy_commands:
  lw $t3, 0($t0)
  sw $t3, 0($t1)
  addiu $t0, $t0, 4
  addiu $t1, $t1, 4
  addiu $t2, $t2, -1
  bne $t2, $zero, copy_commands
  nop

  /* Copy eight authored signed stereo S16BE frames into current RDRAM. */
  lui $t0, %hi(pcm_samples)
  addiu $t0, $t0, %lo(pcm_samples)
  ori $t1, $s2, T64_PCM_RDRAM
  addiu $t2, $zero, T64_PCM_WORDS
copy_pcm:
  lw $t3, 0($t0)
  sw $t3, 0($t1)
  addiu $t0, $t0, 4
  addiu $t1, $t1, 4
  addiu $t2, $t2, -1
  bne $t2, $zero, copy_pcm
  nop

  /* Enable AI, set DACRATE 1103 / BITRATE 15 and queue two 16-byte DMAs; the
   * AI plays in the background while the later phases run. */
  addiu $t1, $zero, 1
  sw $t1, 8($s4)
  addiu $t1, $zero, T64_DACRATE
  sw $t1, 16($s4)
  addiu $t1, $zero, T64_BITRATE
  sw $t1, 20($s4)
  addiu $t1, $zero, T64_PCM_RDRAM
  sw $t1, 0($s4)
  addiu $t1, $zero, 16
  sw $t1, 4($s4)
  addiu $t1, $zero, T64_PCM_RDRAM + 16
  sw $t1, 0($s4)
  addiu $t1, $zero, 16
  sw $t1, 4($s4)

  /* Enable the documented modeled DP mask before submitting FullSync. */
  lui $t0, 0xa430
  addiu $t1, $zero, 0x0800
  sw $t1, 12($t0)

  lui $t0, 0xa410
  ori $t1, $zero, T64_COMMAND_RDRAM
  sw $t1, 0($t0)
  ori $t1, $zero, T64_COMMAND_RDRAM + T64_COMMAND_BYTES
  sw $t1, 4($t0)

  /* Wait for the FullSync interrupt (MI pending bit 0x20), then clear it. */
  lui $t0, 0xa430
wait_fullsync:
  lw $t2, 8($t0)
  andi $t2, $t2, 0x20
  beq $t2, $zero, wait_fullsync
  nop
  addiu $t2, $zero, 0x0800
  sw $t2, 0($t0)

  /* Program the 4x3 source window and a short named VI interval, wait a
   * bounded number of guest instructions so a few fields are emitted, then
   * disable scanout so later phases emit no further video packets. H/V starts
   * are half-open and the V span counts half-lines under this software
   * profile policy, so 6 half-lines are three rows. */
  lui $t0, 0xa440
  addiu $t1, $zero, T64_COLOR_RDRAM
  sw $t1, 4($t0)
  addiu $t1, $zero, 4
  sw $t1, 8($t0)
  addiu $t1, $zero, 128
  sw $t1, 24($t0)
  addiu $t1, $zero, 4
  sw $t1, 28($t0)
  sw $t1, 36($t0)
  addiu $t1, $zero, 6
  sw $t1, 40($t0)
  ori $t1, $zero, 0x0400
  sw $t1, 48($t0)
  sw $t1, 52($t0)
  addiu $t1, $zero, 2
  sw $t1, 0($t0)
  addiu $t2, $zero, T64_VI_WAIT
wait_fields:
  addiu $t2, $t2, -1
  bne $t2, $zero, wait_fields
  nop
  sw $zero, 0($t0)

  /* VC and VZ: the color and Z words after both triangles. */
  PRINT CH_V, CH_C, T64_IMAGE_BYTES, T64_COLOR_RDRAM
  PRINT CH_V, CH_Z, T64_IMAGE_BYTES, T64_DEPTH_RDRAM

  /* Copy the literal joybus frame (channel 0 state command expecting four
   * bytes, the end escape, the parse request in byte 63) into uncached RDRAM,
   * then write it to the modeled PIF RAM with one write DMA. */
  lui $t0, %hi(frame_literal)
  addiu $t0, $t0, %lo(frame_literal)
  ori $t1, $s2, T64_FRAME_RDRAM
  addiu $t2, $zero, T64_FRAME_WORDS
copy_frame:
  lw $t3, 0($t0)
  sw $t3, 0($t1)
  addiu $t0, $t0, 4
  addiu $t1, $t1, 4
  addiu $t2, $t2, -1
  bne $t2, $zero, copy_frame
  nop
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

  /* Three polls with a delay between them; the host input script stamps one
   * controller state in each gap, so the transcript depends on the ordering
   * of the stamps and the polls only. */
  addiu $s5, $zero, 1
poll_loop:
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
  addiu $a0, $zero, CH_I
  addiu $a1, $s5, CH_ZERO
  addiu $a2, $zero, 4
  ori $a3, $s2, T64_RECEIVE_RDRAM + 3
  jal print_line
  nop
  lui $t0, T64_COMPOSITE_SPIN >> 16
  ori $t0, $t0, T64_COMPOSITE_SPIN & 0xffff
poll_delay:
  addiu $t0, $t0, -1
  bne $t0, $zero, poll_delay
  nop
  addiu $s5, $s5, 1
  sltiu $t0, $s5, T64_POLL_COUNT + 1
  bne $t0, $zero, poll_loop
  nop

  /* The last polled word: PIF RAM bytes 3 to 6 of the last read-back image
   * (buttons high, buttons low, x, y), composed with byte loads. */
  lbu $s7, T64_RECEIVE_RDRAM + 3($s2)
  sll $s7, $s7, 24
  lbu $t0, T64_RECEIVE_RDRAM + 4($s2)
  sll $t0, $t0, 16
  or $s7, $s7, $t0
  lbu $t0, T64_RECEIVE_RDRAM + 5($s2)
  sll $t0, $t0, 8
  or $s7, $s7, $t0
  lbu $t0, T64_RECEIVE_RDRAM + 6($s2)
  or $s7, $s7, $t0

  /* The 16-byte SRAM record: the magic "T64C", the last polled word, their
   * sum modulo 2^32 and their exclusive or. */
  lui $t0, 0x5436
  ori $t0, $t0, 0x3443
  sw $t0, T64_RECORD_RDRAM($s2)
  sw $s7, T64_RECORD_RDRAM + 4($s2)
  addu $t1, $t0, $s7
  sw $t1, T64_RECORD_RDRAM + 8($s2)
  xor $t3, $t0, $s7
.ifdef T64_COMPOSITE_WRONG
  xori $t3, $t3, 1
.endif
  sw $t3, T64_RECORD_RDRAM + 12($s2)

  /* SR: the record to SRAM, the readback area zeroed, the record read back. */
  PI_DMA T64_RECORD_RDRAM, T64_PI_RD_LEN
  ZERO_READBACK
  PI_DMA T64_READBACK_RDRAM, T64_PI_WR_LEN
  PRINT CH_S, CH_R, T64_RECORD_BYTES, T64_READBACK_RDRAM

  /* Wait for both AI descriptors; the runner drains packets between yields. */
wait_for_audio:
  lw $t2, 12($s4)
  bne $t2, $zero, wait_for_audio
  nop

  /* AI: the guest-visible AI interrupt bit of MI_INTERRUPT. */
  lui $t0, 0xa430
  lw $t2, 8($t0)
  andi $t2, $t2, 0x04
  sw $t2, T64_MISC_RDRAM($s2)
  PRINT CH_A, CH_I, 4, T64_MISC_RDRAM
.globl t64_save_completion
t64_save_completion:
  break
empty_break:
  break

/* $a0 and $a1 = the two tag characters, $a2 = byte count, $a3 = byte address;
 * prints "<tag> <hex of the bytes>\n" and flushes once. */
print_line:
  or $s6, $ra, $zero
  addiu $t2, $s0, T64_ISVIEWER_BUFFER
  sb $a0, 0($t2)
  sb $a1, 1($t2)
  addiu $t6, $zero, CH_SPACE
  sb $t6, 2($t2)
  addiu $t2, $t2, 3
  or $t5, $a3, $zero
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
rdp_commands:
  /* SetColorImage RGBA16, 4 pixels wide, origin 0x3000. */
  .4byte 0x3f100003, 0x00003000
  /* SetScissor from (0,0) to (4,3) end-exclusive, in 10.2 coordinates. */
  .4byte 0x2d000000, 0x0001000c
  /* SetOtherModes: one cycle, primitive Z source, Z compare and Z update. */
  .4byte 0x2f000000, 0x00000034
  /* SetZImage at 0x3400 (the far words written above). */
  .4byte 0x3e000000, 0x00003400
  /* SetCombine: the literal combiner that selects the primitive color. */
  .4byte 0xfc000000, 0x00018600
  /* Near primitive: depth word 0x1357, delta 9, then green (R0 G255 B0 A255). */
  .4byte 0x2e000000, 0x13570009
  .4byte 0x3a000000, 0x00ff00ff
  /* Z triangle 0x09 over the whole window: YL=3.0, YM=3.0, YH=0, major edge
   * x=0.0 with slope 0 (left, lft bit 0), minor edge from x=0.0 with slope
   * +40.0 pixels per row, low edge x=120.0 slope 0, Z plane words zero. */
  .4byte 0x0900000c, 0x000c0000
  .4byte 0x00780000, 0x00000000
  .4byte 0x00000000, 0x00000000
  .4byte 0x00000000, 0x00280000
  .4byte 0x00000000, 0x00000000
  .4byte 0x00000000, 0x00000000
  /* Far primitive: depth word 0x2000, delta 9, then red (R255 G0 B0 A255). */
  .4byte 0x2e000000, 0x20000009
  .4byte 0x3a000000, 0xff0000ff
  /* The same triangle again; the Z test rejects every pixel. */
  .4byte 0x0900000c, 0x000c0000
  .4byte 0x00780000, 0x00000000
  .4byte 0x00000000, 0x00000000
  .4byte 0x00000000, 0x00280000
  .4byte 0x00000000, 0x00000000
  .4byte 0x00000000, 0x00000000
  /* FullSync. */
  .4byte 0x29000000, 0x00000000
rdp_commands_end:
.if (rdp_commands_end - rdp_commands) != T64_COMMAND_BYTES
.error "composite RDP command list length changed"
.endif

.align 2
pcm_samples:
  /* Packet 1: frames (L,R) = (0x0000,0x4000) (0x2000,0xe000) (0x7fff,0x8001)
   * (0x0123,0xfedd). Packet 2: (0x1111,0xeeef) (0x8000,0x0001)
   * (0x5a5a,0xa5a6) (0xffff,0x0000). */
  .4byte 0x00004000, 0x2000e000, 0x7fff8001, 0x0123fedd
  .4byte 0x1111eeef, 0x80000001, 0x5a5aa5a6, 0xffff0000

.align 2
frame_literal:
  /* Channel 0: TX 1 byte (state command 0x01), RX 4 bytes. */
  .byte 0x01, 0x04, 0x01, 0xff, 0xff, 0xff, 0xff
  /* End escape, then zero fill to byte 62, then the parse request. */
  .byte 0xfe
  .fill 55, 1, 0
  .byte 0x01
frame_literal_end:
.if (frame_literal_end - frame_literal) != frame_literal_length
.error "composite frame literal length changed"
.endif

hex_digits:
  .ascii "0123456789abcdef"
