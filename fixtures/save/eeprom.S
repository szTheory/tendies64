/* Authored MIPS III guest payload for the Phase 9 eeprom-rw diagnostic. No SDK
 * or third-party code is copied. Register setup, the interrupt mask and the
 * ISViewer print idiom follow fixtures/pad/poll.S.
 *
 * The program runs thirteen steps against the modeled SI on joybus channel 4
 * (the cartridge channel) of an explicit EEPROM profile. Each transaction
 * copies one literal 64-byte frame to uncached RDRAM, moves it to the modeled
 * PIF RAM with a write DMA, executes it with a read DMA, spins until the DMA
 * completes, acknowledges the interrupt and prints one line:
 *
 *   <tag> <rx byte, 2 hex> <rx data, hex>\n
 *
 * The rx byte is byte 5 of the returned PIF image (the receive count with any
 * flag bits or-ed in); the data are the receive bytes that follow the
 * transmit bytes. Transactions in order: I1 (info), W1 (write block 0x01,
 * data 11 to 18), I2 (info, busy), R1 (read, busy), W2 (write block 0x02,
 * busy), a wait, I3 (info, idle), R2 (read block 0x01), R5 (read block 0x02,
 * never written), W3 (write block 0x41, data 41 to 48), a second wait, R3
 * (read block 0x01) and R4 (read block 0x41). Each wait spins until the CP0
 * Count difference from the value read immediately before the RD64B start of
 * the preceding write is at least 220,000 Count units (440,000 guest ticks:
 * Count advances once per two guest ticks in the model), which exceeds the
 * 375,000-tick busy window by a wide margin.
 *
 * Variants selected by assembler defsyms (each changes exactly one thing):
 *   T64_EEPROM_16K  the header text reads 16k instead of 4k and nothing else
 *   T64_SAVE_WRONG  the last data byte of the first write is 0x19, not 0x18
 * Editing this file re-pins every eeprom-rw row, so it is authored once with
 * its only variants. Every figure here is a software-profile policy value,
 * not a hardware measurement.
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
.equ T64_WAIT_COUNT_HI, 3
.equ T64_WAIT_COUNT_LO, 0x5b60 /* 0x35b60 = 220,000 Count units */
.equ frame_literal_length, 64
.ifdef T64_EEPROM_16K
.equ header_length, 11
.else
.equ header_length, 10
.endif

.equ CH_SPACE, 0x20
.equ CH_NEWLINE, 0x0a

.macro RUN_TXN frame
  lui $a1, %hi(\frame)
  addiu $a1, $a1, %lo(\frame)
  jal do_txn
  nop
.endm

.macro PRINT_TXN tag, tx_count, rx_count
  lui $a0, %hi(\tag)
  addiu $a0, $a0, %lo(\tag)
  addiu $a2, $zero, \rx_count
  addiu $a3, $zero, \tx_count
  jal print_line
  nop
.endm

/* Spin until the unsigned Count difference from the anchor in $s4 is at
 * least 220,000. */
.macro WAIT_FROM_ANCHOR
  lui $t3, T64_WAIT_COUNT_HI
  ori $t3, $t3, T64_WAIT_COUNT_LO
1:
  mfc0 $t0, $9
  subu $t1, $t0, $s4
  sltu $t2, $t1, $t3
  bne $t2, $zero, 1b
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

  /* Header line: copy the literal text to the ISViewer buffer and flush. */
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

  RUN_TXN frame_i
  PRINT_TXN tag_i1, 1, 3
  RUN_TXN frame_w1
  or $s4, $s7, $zero
  PRINT_TXN tag_w1, 10, 1
  RUN_TXN frame_i
  PRINT_TXN tag_i2, 1, 3
  RUN_TXN frame_r1
  PRINT_TXN tag_r1, 2, 8
  RUN_TXN frame_w2
  PRINT_TXN tag_w2, 10, 1
  WAIT_FROM_ANCHOR
  RUN_TXN frame_i
  PRINT_TXN tag_i3, 1, 3
  RUN_TXN frame_r1
  PRINT_TXN tag_r2, 2, 8
  RUN_TXN frame_r5
  PRINT_TXN tag_r5, 2, 8
  RUN_TXN frame_w3
  or $s4, $s7, $zero
  PRINT_TXN tag_w3, 10, 1
  WAIT_FROM_ANCHOR
  RUN_TXN frame_r1
  PRINT_TXN tag_r3, 2, 8
  RUN_TXN frame_r4
  PRINT_TXN tag_r4, 2, 8
.globl t64_save_completion
t64_save_completion:
  break
empty_break:
  break

/* One transaction: $a1 = frame literal. Copies it to uncached RDRAM, moves it
 * to the PIF RAM (write DMA), reads CP0 Count immediately before the read DMA
 * start into $s7, starts the read DMA into the receive buffer, and spins and
 * acknowledges after each DMA. */
do_txn:
  or $t0, $a1, $zero
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

  mfc0 $s7, $9
  addiu $t0, $zero, T64_RECEIVE_RDRAM
  sw $t0, 0($s1)
  sw $t1, T64_SI_RD64B($s1)
wait_read:
  lw $t2, T64_SI_STATUS($s1)
  andi $t2, $t2, 1
  bne $t2, $zero, wait_read
  nop
  sw $zero, T64_SI_STATUS($s1)
  jr $ra
  nop

/* $a0 = tag text (2 bytes), $a2 = receive data byte count, $a3 = transmit
 * byte count. Prints "<tag> <byte 5> <data>\n" and flushes once. */
print_line:
  or $s6, $ra, $zero
  addiu $t2, $s0, T64_ISVIEWER_BUFFER
  lbu $t6, 0($a0)
  sb $t6, 0($t2)
  lbu $t6, 1($a0)
  sb $t6, 1($t2)
  addiu $t6, $zero, CH_SPACE
  sb $t6, 2($t2)
  addiu $t2, $t2, 3
  lbu $a0, 5($s3)
  jal emit_hex_byte
  nop
  addiu $t6, $zero, CH_SPACE
  sb $t6, 0($t2)
  addiu $t2, $t2, 1
  addu $t5, $s3, $a3
  addiu $t5, $t5, 6
  or $s5, $a2, $zero
print_data:
  lbu $a0, 0($t5)
  jal emit_hex_byte
  nop
  addiu $t5, $t5, 1
  addiu $s5, $s5, -1
  bne $s5, $zero, print_data
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
/* Frame images: bytes 0 to 3 are zero (channels 0 to 3 skipped), byte 4 the
 * transmit count, byte 5 the receive count, then the transmit bytes, the 0xff
 * receive fill, the end escape 0xfe, zero fill, and byte 63 = 0x01 (parse). */
frame_i:
  .byte 0x00, 0x00, 0x00, 0x00, 0x01, 0x03
  .byte 0x00
  .byte 0xff, 0xff, 0xff
  .byte 0xfe
  .fill 52, 1, 0
  .byte 0x01
frame_i_end:
.if (frame_i_end - frame_i) != frame_literal_length
.error "eeprom-rw info frame length changed"
.endif
frame_w1:
  .byte 0x00, 0x00, 0x00, 0x00, 0x0a, 0x01
  .byte 0x05, 0x01, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17
.ifdef T64_SAVE_WRONG
  .byte 0x19
.else
  .byte 0x18
.endif
  .byte 0xff
  .byte 0xfe
  .fill 45, 1, 0
  .byte 0x01
frame_w1_end:
.if (frame_w1_end - frame_w1) != frame_literal_length
.error "eeprom-rw first write frame length changed"
.endif
frame_w2:
  .byte 0x00, 0x00, 0x00, 0x00, 0x0a, 0x01
  .byte 0x05, 0x02, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28
  .byte 0xff
  .byte 0xfe
  .fill 45, 1, 0
  .byte 0x01
frame_w2_end:
.if (frame_w2_end - frame_w2) != frame_literal_length
.error "eeprom-rw second write frame length changed"
.endif
frame_w3:
  .byte 0x00, 0x00, 0x00, 0x00, 0x0a, 0x01
  .byte 0x05, 0x41, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48
  .byte 0xff
  .byte 0xfe
  .fill 45, 1, 0
  .byte 0x01
frame_w3_end:
.if (frame_w3_end - frame_w3) != frame_literal_length
.error "eeprom-rw third write frame length changed"
.endif
frame_r1:
  .byte 0x00, 0x00, 0x00, 0x00, 0x02, 0x08
  .byte 0x04, 0x01
  .byte 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
  .byte 0xfe
  .fill 46, 1, 0
  .byte 0x01
frame_r1_end:
.if (frame_r1_end - frame_r1) != frame_literal_length
.error "eeprom-rw read block 0x01 frame length changed"
.endif
frame_r5:
  .byte 0x00, 0x00, 0x00, 0x00, 0x02, 0x08
  .byte 0x04, 0x02
  .byte 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
  .byte 0xfe
  .fill 46, 1, 0
  .byte 0x01
frame_r5_end:
.if (frame_r5_end - frame_r5) != frame_literal_length
.error "eeprom-rw read block 0x02 frame length changed"
.endif
frame_r4:
  .byte 0x00, 0x00, 0x00, 0x00, 0x02, 0x08
  .byte 0x04, 0x41
  .byte 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
  .byte 0xfe
  .fill 46, 1, 0
  .byte 0x01
frame_r4_end:
.if (frame_r4_end - frame_r4) != frame_literal_length
.error "eeprom-rw read block 0x41 frame length changed"
.endif

tag_i1:
  .ascii "I1"
tag_w1:
  .ascii "W1"
tag_i2:
  .ascii "I2"
tag_r1:
  .ascii "R1"
tag_w2:
  .ascii "W2"
tag_i3:
  .ascii "I3"
tag_r2:
  .ascii "R2"
tag_r5:
  .ascii "R5"
tag_w3:
  .ascii "W3"
tag_r3:
  .ascii "R3"
tag_r4:
  .ascii "R4"
header_text:
.ifdef T64_EEPROM_16K
  .ascii "EEPROM 16k\n"
.else
  .ascii "EEPROM 4k\n"
.endif
header_text_end:
.if (header_text_end - header_text) != header_length
.error "eeprom-rw header text length changed"
.endif
hex_digits:
  .ascii "0123456789abcdef"
