ENTRY(_start)
SECTIONS
{
    . = 0x40010000;
    .text : { KEEP(*(.text.boot)) *(.text*) *(.rodata*) }
    .vectors ALIGN(32) : { KEEP(*(.vectors)) }
    .data ALIGN(8) : { *(.data*) }
    .bss ALIGN(8) (NOLOAD) : { *(.bss*) *(COMMON) }
    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) *(.note*) }
}
