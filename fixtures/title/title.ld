OUTPUT_ARCH(mips)
ENTRY(_start)

__t64_rom_clock = 0x0000000f;
__t64_rom_crc2 = 0x00000000;

SECTIONS
{
  . = 0x80000400;
  .text : { KEEP(*(.text.start)) *(.text*) }
  .rodata : { *(.rodata*) }
  __t64_title_payload_end = .;
}

/* The data areas of fixtures/title/title.S start at RDRAM 0x4000. */
ASSERT(__t64_title_payload_end <= 0x80004000, "title payload overlaps the data area")
