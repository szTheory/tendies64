/* Authored MIPS III guest payload; VARIANT is supplied by the pinned as tool. */
.set mips3
.set noreorder
.set noat

/* Guest uses the uncached KSEG1 alias of physical ISViewer 0x13ff0000. */
.equ T64_ISVIEWER_BASE, 0xb3ff0000
.equ T64_ISVIEWER_BUFFER, 0x20
.equ T64_ISVIEWER_FLUSH_HALFWORD, 0x16
.equ T64_ISVIEWER_MAGIC_HI, 0x1234
.equ T64_ISVIEWER_MAGIC_LO, 0x5678

.section .text.start,"ax",@progbits
.globl _start
_start:
.if VARIANT == 1
  break
.elseif VARIANT == 3
empty_loop:
  b empty_loop
  nop
.elseif VARIANT == 5
  .word 0xffffffff
  break
.elseif VARIANT == 6
  b overflow_start
  nop
.else
  b emit_text
  nop
.endif

.if VARIANT != 1 && VARIANT != 3 && VARIANT != 5 && VARIANT != 6
emit_text:
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
  lui $t3, %hi(hello_text)
  addiu $t3, $t3, %lo(hello_text)
  lui $t4, %hi(hello_text_end)
  addiu $t4, $t4, %lo(hello_text_end)
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
  addiu $t6, $zero, hello_text_end - hello_text
  sh $t6, T64_ISVIEWER_FLUSH_HALFWORD($t0)
.if VARIANT == 4
incomplete_loop:
  b incomplete_loop
  nop
.else
  break
.endif
empty_break:
  break

.section .rodata,"a",@progbits
.if VARIANT == 2
hello_text:
  .ascii "Wrong guest transcript\n"
.elseif VARIANT == 4
hello_text:
  .ascii "Tendies64 hello "
.else
hello_text:
  .ascii "Tendies64 hello via open IPL3\n"
.endif
hello_text_end:
.endif

.if VARIANT == 6
overflow_start:
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
  addiu $t3, $zero, 8
  addiu $t4, $zero, 512
  addiu $t5, $zero, 'x'
overflow_chunk:
  addiu $t6, $zero, 0
  addiu $t2, $t0, T64_ISVIEWER_BUFFER
overflow_byte:
  sb $t5, 0($t2)
  addiu $t2, $t2, 1
  addiu $t6, $t6, 1
  bne $t6, $t4, overflow_byte
  nop
  sh $t4, T64_ISVIEWER_FLUSH_HALFWORD($t0)
  addiu $t3, $t3, -1
  bne $t3, $zero, overflow_chunk
  nop
  addiu $t2, $t0, T64_ISVIEWER_BUFFER
  sb $t5, 0($t2)
  addiu $t7, $zero, 1
  sh $t7, T64_ISVIEWER_FLUSH_HALFWORD($t0)
  break
empty_break:
  break
.endif
