/* Authored MIPS III guest payload of the Phase 12 title (plan 12-03, API-06).
 * Behavior authority: fixtures/title/title-spec.md (TS-1 to TS-16); the section
 * of each block is named in its comment. No SDK or third-party code is copied.
 * Idiom sources, all authored fixtures of this repository:
 *   interrupt mask, base registers, ISViewer handshake   fixtures/composite/composite.S
 *   RDP command copy, DPC START/END, DP mask and wait    fixtures/rdp/fill.S via composite.S
 *   VI register block                                    fixtures/composite/composite.S
 *   AI enable, DACRATE/BITRATE and descriptor queueing   fixtures/audio/pcm.S via composite.S
 *   joybus frame literal, WR64B and RD64B poll           fixtures/replay/mix.S via composite.S
 *   16-byte SRAM PI DMA (PI_DMA macro)                   fixtures/save/sram.S via composite.S
 *   ISViewer line and lower-case hex digits              composite.S print_line, emit_hex_byte
 * The RDP words, the colors, the tone table, the slot layout and the frame-lock
 * loop are literals authored here.
 *
 * The guest never takes an interrupt (Status.IE is cleared and no handler
 * exists), never counts instructions to wait and uses no Z, texture, RSP
 * microcode, TLB or library: it is locked to the video field by polling the MI
 * VI pending bit (TS-5). Every figure is a software-profile policy value, not a
 * hardware measurement.
 *
 * Registers kept across the loop: $s0 ISViewer, $s1 SI, $s2 RDRAM (KSEG1),
 * $s3 PI, $s4 AI, $s5 x, $s6 y, $s7 press count, $fp previous A (0 or 0x80),
 * $gp polls made.
 */
.set mips3
.set noreorder
.set noat

/* Device bases (uncached KSEG1 aliases of the physical addresses). */
.equ T64_ISVIEWER_BASE_HI, 0xb3ff
.equ T64_ISVIEWER_BUFFER, 0x20
.equ T64_ISVIEWER_FLUSH_HALFWORD, 0x16
.equ T64_ISVIEWER_MAGIC_HI, 0x1234
.equ T64_ISVIEWER_MAGIC_LO, 0x5678
.equ T64_DPC_BASE_HI, 0xa410
.equ T64_MI_BASE_HI, 0xa430
.equ T64_VI_BASE_HI, 0xa440
.equ T64_AI_BASE_HI, 0xa450
.equ T64_PI_BASE_HI, 0xa460
.equ T64_SI_BASE_HI, 0xa480
.equ T64_MI_MODE, 0x00
.equ T64_MI_INTERRUPT, 0x08
.equ T64_MI_MASK, 0x0c
.equ T64_MI_VI_PENDING, 0x08
.equ T64_MI_DP_PENDING, 0x20
.equ T64_MI_DP_BIT, 0x0800

/* VI registers (offsets of docs/spec/vi.md) and the values of TS-3. */
.equ T64_VI_STATUS, 0
.equ T64_VI_ORIGIN, 4
.equ T64_VI_WIDTH, 8
.equ T64_VI_INTR, 12
.equ T64_VI_CURRENT, 16
.equ T64_VI_V_SYNC, 24
.equ T64_VI_H_SYNC, 28
.equ T64_VI_H_START, 36
.equ T64_VI_V_START, 40
.equ T64_VI_X_SCALE, 48
.equ T64_VI_Y_SCALE, 52
.equ T64_FRAMEBUFFER_HI, 0x0010
.equ T64_SCREEN_WIDTH, 128

/* AI registers (docs/spec/audio-interface.md) and the TS-8 burst. */
.equ T64_AI_DRAM_ADDR, 0x00
.equ T64_AI_LEN, 0x04
.equ T64_AI_CONTROL, 0x08
.equ T64_AI_STATUS, 0x0c
.equ T64_AI_DACRATE, 0x10
.equ T64_AI_BITRATE, 0x14
.equ T64_AI_BUSY_FULL_HI, 0xc000
.equ T64_DACRATE, 1103
.equ T64_BITRATE, 15
.equ T64_TONE_BYTES, 2048

/* SI and PI registers (composite.S) and the SRAM slot at offset 0. */
.equ T64_SI_RD64B, 0x04
.equ T64_SI_WR64B, 0x10
.equ T64_SI_STATUS, 0x18
.equ T64_PIF_ADDRESS_HI, 0x1fc0
.equ T64_PIF_ADDRESS_LO, 0x07c0
.equ T64_PI_DRAM_ADDR, 0x00
.equ T64_PI_CART_ADDR, 0x04
.equ T64_PI_RD_LEN, 0x08
.equ T64_PI_WR_LEN, 0x0c
.equ T64_PI_STATUS, 0x10
.equ T64_SRAM_CART_HI, 0x0800
.equ T64_SLOT_LENGTH_RAW, 15

/* RDRAM layout (physical offsets below 0x8000, reached through $s2). */
.equ T64_COMMAND_RDRAM, 0x4000
.equ T64_COMMAND_BYTES, 64
.equ T64_COMMAND_WORDS, 16
.equ T64_SQUARE_RECT, T64_COMMAND_RDRAM + 48
.equ T64_FRAME_RDRAM, 0x4100
.equ T64_FRAME_WORDS, 16
.equ T64_RECEIVE_RDRAM, 0x4200
.equ T64_SLOT_RDRAM, 0x4300
.equ T64_TONE_RDRAM, 0x5000
.equ T64_TONE_WORDS, 512

/* TS-7 and TS-12 values. */
.equ T64_START_X, 56
.equ T64_START_Y, 40
.equ T64_MAX_X, 112
.equ T64_MAX_Y, 80
.equ T64_SLOT_MAGIC_HI, 0x5436
.equ T64_SLOT_MAGIC_LO, 0x3454
.equ T64_PAD_A, 0x80
.equ T64_PAD_B, 0x40
.equ T64_PAD_UP, 0x08
.equ T64_PAD_DOWN, 0x04
.equ T64_PAD_LEFT, 0x02
.equ T64_PAD_RIGHT, 0x01

.equ CH_SPACE, 0x20
.equ CH_NEWLINE, 0x0a
.equ CH_L, 0x4c

/* Copy words from a .rodata label into uncached RDRAM (composite.S). */
.macro COPY_WORDS source, dest, words
  lui $t0, %hi(\source)
  addiu $t0, $t0, %lo(\source)
  ori $t1, $s2, \dest
  addiu $t2, $zero, \words
3:
  lw $t3, 0($t0)
  sw $t3, 0($t1)
  addiu $t0, $t0, 4
  addiu $t1, $t1, 4
  addiu $t2, $t2, -1
  bne $t2, $zero, 3b
  nop
.endm

/* One 16-byte PI DMA against SRAM offset 0 (composite.S PI_DMA): the read
 * length register writes RDRAM to SRAM, the write length register reads SRAM
 * to RDRAM. */
.macro PI_DMA dram, length_reg
  ori $t0, $zero, \dram
  sw $t0, T64_PI_DRAM_ADDR($s3)
  lui $t1, T64_SRAM_CART_HI
  sw $t1, T64_PI_CART_ADDR($s3)
  addiu $t0, $zero, T64_SLOT_LENGTH_RAW
  sw $t0, \length_reg($s3)
2:
  lw $t2, T64_PI_STATUS($s3)
  andi $t2, $t2, 1
  bne $t2, $zero, 2b
  nop
  addiu $t0, $zero, 2
  sw $t0, T64_PI_STATUS($s3)
.endm

/* Wait until an SI DMA ends, then acknowledge it (composite.S). */
.macro SI_WAIT
4:
  lw $t2, T64_SI_STATUS($s1)
  andi $t2, $t2, 1
  bne $t2, $zero, 4b
  nop
  sw $zero, T64_SI_STATUS($s1)
.endm

/* Keep a coordinate register inside 0..max (TS-7 clamp). */
.macro CLAMP reg, max
  bgez \reg, 5f
  nop
  or \reg, $zero, $zero
5:
  slti $t0, \reg, \max + 1
  bne $t0, $zero, 6f
  nop
  addiu \reg, $zero, \max
6:
.endm

/* dest = 1 when the button bit is set in $t3 or the axis test holds: an
 * axis at or above +16 (sign 1) or at or below -16 (sign -1), TS-7. */
.macro DIRECTION dest, bit, axis, sign
  andi \dest, $t3, \bit
  sltu \dest, $zero, \dest
.if \sign > 0
  slti $t0, \axis, 16
  xori $t0, $t0, 1
.else
  slti $t0, \axis, -15
.endif
  or \dest, \dest, $t0
.endm

.section .text.start,"ax",@progbits
.globl _start
_start:
  /* Keep the MI pending bits guest-owned without diverting execution into
   * IPL3's interrupt vector (composite.S): clear Status.IE. */
  mfc0 $t2, $12
  addiu $t3, $zero, -2
  and $t2, $t2, $t3
  mtc0 $t2, $12
  nop

  lui $s0, T64_ISVIEWER_BASE_HI
  lui $s1, T64_SI_BASE_HI
  lui $s2, 0xa000
  lui $s3, T64_PI_BASE_HI
  lui $s4, T64_AI_BASE_HI

  /* ISViewer magic handshake (composite.S); no output when it fails. */
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

  /* TS-9: copy the authored RDP list into uncached RDRAM once; the two words
   * of the square rectangle are rewritten before every submission. TS-8: the
   * tone table stays in RDRAM for every burst. */
  COPY_WORDS rdp_commands, T64_COMMAND_RDRAM, T64_COMMAND_WORDS
  COPY_WORDS tone_table, T64_TONE_RDRAM, T64_TONE_WORDS

  /* TS-8: enable the AI and set DACRATE 1103 and BITRATE 15 (composite.S). */
  addiu $t1, $zero, 1
  sw $t1, T64_AI_CONTROL($s4)
  addiu $t1, $zero, T64_DACRATE
  sw $t1, T64_AI_DACRATE($s4)
  addiu $t1, $zero, T64_BITRATE
  sw $t1, T64_AI_BITRATE($s4)

  /* Enable the documented modeled DP mask once (fill.S); the DP pending bit is
   * then waited for and cleared after every list. */
  lui $t0, T64_MI_BASE_HI
  addiu $t1, $zero, T64_MI_DP_BIT
  sw $t1, T64_MI_MASK($t0)

  /* TS-6: write the joybus frame literal (channel 0 state command, end
   * escape, parse request in byte 63) to the PIF once with a write DMA. */
  COPY_WORDS frame_literal, T64_FRAME_RDRAM, T64_FRAME_WORDS
  ori $t0, $zero, T64_FRAME_RDRAM
  sw $t0, 0($s1)
  lui $t1, T64_PIF_ADDRESS_HI
  ori $t1, $t1, T64_PIF_ADDRESS_LO
  sw $t1, T64_SI_WR64B($s1)
  SI_WAIT

  /* TS-7 initial state. */
  addiu $s5, $zero, T64_START_X
  addiu $s6, $zero, T64_START_Y
  or $s7, $zero, $zero
  or $fp, $zero, $zero
  or $gp, $zero, $zero

  /* TS-12 boot load: read the slot; restore only when the magic matches,
   * x <= 112 and y <= 80, then print the boot line (TS-10). */
  PI_DMA T64_SLOT_RDRAM, T64_PI_WR_LEN
  lw $t0, T64_SLOT_RDRAM($s2)
  lui $t1, T64_SLOT_MAGIC_HI
  ori $t1, $t1, T64_SLOT_MAGIC_LO
  bne $t0, $t1, boot_done
  nop
  lw $t4, T64_SLOT_RDRAM + 4($s2)
  sltiu $t0, $t4, T64_MAX_X + 1
  beq $t0, $zero, boot_done
  nop
  lw $t5, T64_SLOT_RDRAM + 8($s2)
  sltiu $t0, $t5, T64_MAX_Y + 1
  beq $t0, $zero, boot_done
  nop
  or $s5, $t4, $zero
  or $s6, $t5, $zero
  lw $s7, T64_SLOT_RDRAM + 12($s2)
  addiu $a0, $zero, CH_L
  jal print_state
  or $a1, $zero, $zero
boot_done:

  /* TS-3: program the VI, clear any stale VI pending bit, write STATUS last
   * (the VI enable). */
  lui $t0, T64_VI_BASE_HI
  lui $t1, T64_FRAMEBUFFER_HI
  sw $t1, T64_VI_ORIGIN($t0)
  addiu $t1, $zero, T64_SCREEN_WIDTH
  sw $t1, T64_VI_WIDTH($t0)
  addiu $t1, $zero, 2
  sw $t1, T64_VI_INTR($t0)
  addiu $t1, $zero, 525
  sw $t1, T64_VI_V_SYNC($t0)
  addiu $t1, $zero, 3093
  sw $t1, T64_VI_H_SYNC($t0)
  addiu $t1, $zero, 128
  sw $t1, T64_VI_H_START($t0)
  addiu $t1, $zero, 192
  sw $t1, T64_VI_V_START($t0)
  ori $t1, $zero, 0x0400
  sw $t1, T64_VI_X_SCALE($t0)
  sw $t1, T64_VI_Y_SCALE($t0)
  sw $zero, T64_VI_CURRENT($t0)
  addiu $t1, $zero, 0x0303
  sw $t1, T64_VI_STATUS($t0)

  /* TS-5 frame lock: step 1, wait for the VI pending bit with Status.IE clear,
   * then clear it with a VI_CURRENT write. */
frame_loop:
  lui $t0, T64_MI_BASE_HI
wait_vi:
  lw $t1, T64_MI_INTERRUPT($t0)
  andi $t1, $t1, T64_MI_VI_PENDING
  beq $t1, $zero, wait_vi
  nop
  lui $t0, T64_VI_BASE_HI
  sw $zero, T64_VI_CURRENT($t0)

  /* TS-5 step 2 and TS-6: one read DMA of the PIF image; byte 3 is the
   * buttons high byte (A B Z Start Up Down Left Right), bytes 5 and 6 the
   * signed X and Y axes. */
  ori $t0, $zero, T64_RECEIVE_RDRAM
  sw $t0, 0($s1)
  lui $t1, T64_PIF_ADDRESS_HI
  ori $t1, $t1, T64_PIF_ADDRESS_LO
  sw $t1, T64_SI_RD64B($s1)
  SI_WAIT
  addiu $gp, $gp, 1
  lbu $t3, T64_RECEIVE_RDRAM + 3($s2)
  lb $t4, T64_RECEIVE_RDRAM + 5($s2)
  lb $t5, T64_RECEIVE_RDRAM + 6($s2)

  /* TS-7 step 1: B resets the position; otherwise D-pad or stick moves it by
   * 2 pixels per axis (screen y grows downward, stick Y is positive up),
   * clamped to 0..112 and 0..80. */
  andi $t0, $t3, T64_PAD_B
  beq $t0, $zero, move
  nop
  addiu $s5, $zero, T64_START_X
  b press_edge
  addiu $s6, $zero, T64_START_Y
move:
  DIRECTION $t6, T64_PAD_RIGHT, $t4, 1
  DIRECTION $t7, T64_PAD_LEFT, $t4, -1
  subu $t6, $t6, $t7
  sll $t6, $t6, 1
  addu $s5, $s5, $t6
  DIRECTION $t6, T64_PAD_DOWN, $t5, -1
  DIRECTION $t7, T64_PAD_UP, $t5, 1
  subu $t6, $t6, $t7
  sll $t6, $t6, 1
  addu $s6, $s6, $t6
  CLAMP $s5, T64_MAX_X
  CLAMP $s6, T64_MAX_Y

  /* TS-7 step 2 and TS-8: on an A press edge count the press and queue the
   * two descriptors of one burst when the AI is neither busy nor full;
   * previous A becomes this poll's A bit. */
press_edge:
  andi $t6, $t3, T64_PAD_A
  bne $fp, $zero, edge_done
  nop
  beq $t6, $zero, edge_done
  nop
  addiu $s7, $s7, 1
  lw $t0, T64_AI_STATUS($s4)
  lui $t1, T64_AI_BUSY_FULL_HI
  and $t0, $t0, $t1
  bne $t0, $zero, edge_done
  nop
  ori $t0, $zero, T64_TONE_RDRAM
  addiu $t1, $zero, T64_TONE_BYTES
  sw $t0, T64_AI_DRAM_ADDR($s4)
  sw $t1, T64_AI_LEN($s4)
  sw $t0, T64_AI_DRAM_ADDR($s4)
  sw $t1, T64_AI_LEN($s4)
edge_done:
  or $fp, $t6, $zero

  /* TS-9 render: the square rectangle (x, y) to (x + 15, y + 15) inclusive in
   * 10.2 fixed point, then DPC START/END and the DP-done wait (fill.S). */
  sll $t0, $s5, 2
  sll $t1, $s6, 2
  addiu $t2, $t0, 60
  addiu $t3, $t1, 60
  sll $t2, $t2, 12
  or $t2, $t2, $t3
  lui $t3, 0x3600
  or $t2, $t2, $t3
  sw $t2, T64_SQUARE_RECT($s2)
  sll $t0, $t0, 12
  or $t0, $t0, $t1
  sw $t0, T64_SQUARE_RECT + 4($s2)
  lui $t0, T64_DPC_BASE_HI
  ori $t1, $zero, T64_COMMAND_RDRAM
  sw $t1, 0($t0)
  ori $t1, $zero, T64_COMMAND_RDRAM + T64_COMMAND_BYTES
  sw $t1, 4($t0)
  lui $t0, T64_MI_BASE_HI
wait_dp:
  lw $t1, T64_MI_INTERRUPT($t0)
  andi $t1, $t1, T64_MI_DP_PENDING
  beq $t1, $zero, wait_dp
  nop
  addiu $t1, $zero, T64_MI_DP_BIT
  sw $t1, T64_MI_MODE($t0)
  b frame_loop
  nop

  /* TS-11 completion: the run ends at this break. */
.globl t64_title_completion
t64_title_completion:
  break
empty_break:
  break

/* TS-10: $a0 = the tag character, $a1 = 1 to add the poll count. Prints
 * "<tag> xx yy cccccccc[ pp]\n" from $s5, $s6, $s7 and $gp and flushes once
 * (composite.S print_line). */
print_state:
  or $v1, $ra, $zero
  addiu $t2, $s0, T64_ISVIEWER_BUFFER
  sb $a0, 0($t2)
  addiu $t2, $t2, 1
  or $a2, $s5, $zero
  jal emit_field
  addiu $a3, $zero, 1
  or $a2, $s6, $zero
  jal emit_field
  addiu $a3, $zero, 1
  or $a2, $s7, $zero
  jal emit_field
  addiu $a3, $zero, 4
  beq $a1, $zero, print_end
  nop
  or $a2, $gp, $zero
  jal emit_field
  addiu $a3, $zero, 1
print_end:
  addiu $t6, $zero, CH_NEWLINE
  sb $t6, 0($t2)
  addiu $t2, $t2, 1
  addiu $t3, $s0, T64_ISVIEWER_BUFFER
  subu $t6, $t2, $t3
  sh $t6, T64_ISVIEWER_FLUSH_HALFWORD($s0)
  jr $v1
  nop

/* $a2 = value, $a3 = byte count, $t2 = output pointer: writes a space and the
 * low $a3 bytes of $a2 as lower-case hex, high byte first (composite.S
 * emit_hex_byte). */
emit_field:
  addiu $t6, $zero, CH_SPACE
  sb $t6, 0($t2)
  addiu $t2, $t2, 1
  lui $t8, %hi(hex_digits)
  addiu $t8, $t8, %lo(hex_digits)
emit_byte:
  addiu $a3, $a3, -1
  sll $t6, $a3, 3
  srlv $t9, $a2, $t6
  srl $t6, $t9, 4
  andi $t6, $t6, 15
  addu $t6, $t8, $t6
  lbu $t6, 0($t6)
  sb $t6, 0($t2)
  andi $t6, $t9, 15
  addu $t6, $t8, $t6
  lbu $t6, 0($t6)
  sb $t6, 1($t2)
  bne $a3, $zero, emit_byte
  addiu $t2, $t2, 2
  jr $ra
  nop

.section .rodata,"a",@progbits
.align 3
rdp_commands:
  /* TS-9 1: SetColorImage RGBA, 32-bit, width 128, address 0x00100000. */
  .4byte 0x3f18007f, 0x00100000
  /* TS-9 2: SetScissor (0, 0) to (128, 96) in 10.2 coordinates. */
  .4byte 0x2d000000, 0x00200180
  /* TS-9 3: SetOtherModes, fill cycle type. */
  .4byte 0x2f300000, 0x00000000
  /* TS-9 4 and 5: background color, FillRectangle (127, 95) to (0, 0). */
  .4byte 0x37000000, 0x203060ff
  .4byte 0x361fc17c, 0x00000000
  /* TS-9 6 and 7: square color, FillRectangle rewritten per field. */
  .4byte 0x37000000, 0xf0c020ff
  .4byte 0x36000000, 0x00000000
  /* TS-9 8: SyncFull. */
  .4byte 0x29000000, 0x00000000
rdp_commands_end:
.if (rdp_commands_end - rdp_commands) != T64_COMMAND_BYTES
.error "title RDP command list length changed"
.endif

.align 3
tone_table:
  /* TS-8: 512 stereo S16BE frames, +0x2000 for frames f mod 64 < 32 and
   * -0x2000 otherwise, the same sample in both channels. */
.rept 8
.rept 32
  .4byte 0x20002000
.endr
.rept 32
  .4byte 0xe000e000
.endr
.endr
tone_table_end:
.if (tone_table_end - tone_table) != T64_TONE_BYTES
.error "title tone table length changed"
.endif

.align 3
frame_literal:
  /* TS-6 channel 0: TX 1 byte (state command 0x01), RX 4 bytes, the end
   * escape, zero fill to byte 62 and the parse request (composite.S). */
  .byte 0x01, 0x04, 0x01, 0xff, 0xff, 0xff, 0xff
  .byte 0xfe
  .fill 55, 1, 0
  .byte 0x01
frame_literal_end:
.if (frame_literal_end - frame_literal) != T64_FRAME_WORDS * 4
.error "title frame literal length changed"
.endif

hex_digits:
  .ascii "0123456789abcdef"
