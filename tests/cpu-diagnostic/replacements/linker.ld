ENTRY(entrypoint)

SECTIONS {
    . = 0xFFFFFFFF80000400;
    loaded_rom_start = .;

    __boot_start = .;

    .t64_entrypoint 0xFFFFFFFF80000400 : {
        KEEP(*(.t64_entrypoint))
    }
    .t64_park 0xFFFFFFFF80000420 : {
        KEEP(*(.t64_park))
    }

    .boot : {
        *(.boot)
        . = ALIGN(16);
    }

    .text : {
        *(.text .text.*)
        . = ALIGN(16);
    }

    .rodata : {
        *(.rodata .rodata.*)
        . = ALIGN(16);
    }

    .data : {
        *(.data .data.*)
        . = ALIGN(16);
    }

    .t64_selector : ALIGN(4) {
        __t64_selector_start = .;
        KEEP(*(.t64_selector))
        __t64_selector_end = .;
        . = ALIGN(16);
    }

    __binary_size = . - __boot_start;
    loaded_rom_end = .;

    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        . = ALIGN(16);
        __bss_end = .;
    }

    __cpu_heap_limit = 0xFFFFFFFF80300000;
    __cpu_stack_bottom = 0xFFFFFFFF80300000;
    __cpu_stack_top = 0xFFFFFFFF80400000;
    __cpu_rdram_start = 0xFFFFFFFF80000000;
    __cpu_rdram_end = 0xFFFFFFFF80400000;
    __rom_end = . - __boot_start + 0xB0001000;

    ASSERT(ADDR(.t64_entrypoint) == 0xFFFFFFFF80000400,
           "CPU diagnostic entrypoint address drifted")
    ASSERT(entrypoint == ADDR(.t64_entrypoint),
           "CPU diagnostic entrypoint symbol is outside its fixed section")
    ASSERT(SIZEOF(.t64_entrypoint) == 16,
           "CPU diagnostic entrypoint must remain four instructions")
    ASSERT(ADDR(.t64_park) == 0xFFFFFFFF80000420,
           "CPU diagnostic park address drifted")
    ASSERT(park == ADDR(.t64_park),
           "CPU diagnostic park symbol is outside its fixed section")
    ASSERT(SIZEOF(.t64_park) == 8,
           "CPU diagnostic park must remain a jump and delay-slot nop")
    ASSERT(__bss_start >= __cpu_rdram_start,
           "CPU diagnostic BSS starts outside profile RDRAM")
    ASSERT(__bss_end >= __bss_start,
           "CPU diagnostic BSS end precedes its start")
    ASSERT(__bss_end <= __cpu_heap_limit,
           "CPU diagnostic BSS overlaps the heap")
    ASSERT(__bss_end <= __cpu_stack_bottom,
           "CPU diagnostic BSS overlaps the reserved stack")
    ASSERT(__bss_end <= __cpu_rdram_end,
           "CPU diagnostic BSS exceeds profile RDRAM")
    ASSERT(__cpu_heap_limit >= __cpu_rdram_start,
           "CPU diagnostic heap begins outside profile RDRAM")
    ASSERT(__cpu_heap_limit <= __cpu_stack_bottom,
           "CPU diagnostic heap overlaps the reserved stack")
    ASSERT(__cpu_stack_bottom >= __cpu_rdram_start,
           "CPU diagnostic stack begins outside profile RDRAM")
    ASSERT(__cpu_stack_bottom <= __cpu_stack_top,
           "CPU diagnostic stack bounds are reversed")
    ASSERT(__cpu_stack_top <= __cpu_rdram_end,
           "CPU diagnostic stack exceeds profile RDRAM")

    /DISCARD/ : {
        *(.MIPS.*)
        *(.comment)
        *(.mdebug.*)
        *(.pdr)
        *(.reginfo)
        *(.got)
    }
}
