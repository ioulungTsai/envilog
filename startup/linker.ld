MEMORY
{
    FLASH (rx)  : ORIGIN = 0x42000000, LENGTH = 8M
    RAM   (rwx) : ORIGIN = 0x3FC80000, LENGTH = 512K
}

SECTIONS
{
    .vectors :
    {
        *(.vectors)
    } > FLASH

    /* Add specific section for literal values */
    .literal :
    {
        *(.literal)
        *(.literal.*)
    } > FLASH

    .text :
    {
        _text_start = .;
        *(.init.literal)
        *(.text.literal)
        *(.text)
        *(.text.*)
        *(.fini.literal)
        *(.fini)
        *(.gnu.version)
        _text_end = .;
    } > FLASH

    .rodata :
    {
        _rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        _rodata_end = .;
    } > FLASH

    .data :
    {
        _data_start = .;
        *(.data)
        *(.data.*)
        _data_end = .;
    } > RAM AT > FLASH

    .bss :
    {
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        _bss_end = .;
    } > RAM
}
