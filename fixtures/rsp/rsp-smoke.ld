OUTPUT_ARCH(mips)
ENTRY(_start)

__t64_rom_clock = 0x0000000f;
__t64_rom_crc2 = 0x00000000;

SECTIONS
{
  . = 0x80000400;
  .text : { KEEP(*(.text.start)) *(.text*) }
  .rodata : { *(.rodata*) }
}
