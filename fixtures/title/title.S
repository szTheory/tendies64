/* Authored MIPS III guest payload of the Phase 12 title (plan 12-03, API-06).
 * Behavior authority: fixtures/title/title-spec.md (TS-1 to TS-16); the section
 * of each block is named in its comment. No SDK or third-party code is copied.
 * Idiom sources, all authored fixtures of this repository:
 *   interrupt mask, base registers, ISViewer handshake   fixtures/composite/composite.S
 *   RDP command copy, DPC START/END, DP mask and wait    fixtures/rdp/fill.S via composite.S
 *   VI register block                                    fixtures/composite/composite.S
 * The RDP words, the colors and the frame-lock loop are literals authored here.
 *
 * The guest never takes an interrupt (Status.IE is cleared and no handler
 * exists), never counts instructions to wait and uses no Z, texture, RSP
 * microcode, TLB or library: it is locked to the video field by polling the MI
 * VI pending bit (TS-5). Every figure is a software-profile policy value, not a
 * hardware measurement.
 */
.set mips3
.set noreorder
.set noat

/* Device bases (uncached KSEG1 aliases of the physical addresses). */
.equ T64_ISVIEWER_BASE_HI, 0xb3ff
.equ T64_ISVIEWER_MAGIC_HI, 0x1234
.equ T64_ISVIEWER_MAGIC_LO, 0x5678
.equ T64_DPC_BASE_HI, 0xa410
.equ T64_MI_BASE_HI, 0xa430
.equ T64_VI_BASE_HI, 0xa440
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

/* RDRAM layout (physical offsets below 0x8000, reached through $s2). */
.equ T64_COMMAND_RDRAM, 0x4000
.equ T64_COMMAND_BYTES, 64
.equ T64_COMMAND_WORDS, 16
.equ T64_SQUARE_RECT, T64_COMMAND_RDRAM + 48

/* TS-7 start position. */
.equ T64_START_X, 56
.equ T64_START_Y, 40

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
  lui $s2, 0xa000

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
   * of the square rectangle are rewritten before every submission. */
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

  /* Enable the documented modeled DP mask once (fill.S); the DP pending bit is
   * then waited for and cleared after every list. */
  lui $t0, T64_MI_BASE_HI
  addiu $t1, $zero, T64_MI_DP_BIT
  sw $t1, T64_MI_MASK($t0)

  /* TS-7 initial state. */
  addiu $s5, $zero, T64_START_X
  addiu $s6, $zero, T64_START_Y

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
