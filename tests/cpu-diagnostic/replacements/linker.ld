ENTRY(entrypoint)

SECTIONS {
    . = 0xFFFFFFFF80000400;
    loaded_rom_start = .;

    __boot_start = .;
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
    __rom_end = . - __boot_start + 0xB0001000;

    /DISCARD/ : {
        *(.MIPS.*)
        *(.comment)
        *(.mdebug.*)
        *(.pdr)
        *(.reginfo)
        *(.got)
    }
}
