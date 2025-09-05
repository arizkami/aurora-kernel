SECTIONS
{
    . = 0x140000000;
    .text : { *(.text*) }
    .rdata : { *(.rdata*) *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
    /DISCARD/ : { *(.note*) *(.comment*) }
}
