ENTRY(start)

SECTIONS {
    . = 0x10000000;

    .text : {
        KEEP(*(.text.start));
        *(.text .text.*);
    }

    .rodata : ALIGN(4) {
        *(.rodata .rodata.*);
    }

    .data : ALIGN(4) {
        *(.data .data.*);
    }

    .bss : ALIGN(4) {
        *(.bss .bss.* .sbss .sbss.*);

        . = ALIGN(16);
        . += 64 * 1024; /* 64KB */
        __stack_top = .;

        ASSERT(. < 0x18000000, "too large executable");
    }
}
