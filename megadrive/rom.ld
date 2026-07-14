OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(reset_entry)

SECTIONS
{
    . = 0x000000;
    .vectors :
    {
        KEEP(*(.vectors))
    }
    ASSERT(SIZEOF(.vectors) == 0x100, "invalid Mega Drive vector table size")

    . = 0x000100;
    .rom_header :
    {
        KEEP(*(.rom_header))
    }
    ASSERT(SIZEOF(.rom_header) == 0x100, "invalid Mega Drive ROM header size")

    . = 0x000200;
    .startup :
    {
        *(.startup)
    }
    .text :
    {
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.data.rel.ro .data.rel.ro.*)
        *(.data .data.*)
        *(.init_array .init_array.*)
    }
    . = ALIGN(2);
    __code_end = .;
    ASSERT(__code_end <= 0x3FF360, "game code overlaps the tile blobs")

    . = 0x3FF360;
    .assets :
    {
        KEEP(*(.assets))
    }
    ASSERT(SIZEOF(.assets) == 3232, "unexpected tile blob size")
    __rom_end = .;
    ASSERT(__rom_end == 0x400000, "ROM must end at 4 MiB")

    .bss 0x00FF0000 (NOLOAD) :
    {
        *(.bss .bss.*)
        *(COMMON)
    }
    ASSERT(SIZEOF(.bss) == 0, "cartridge code must not require BSS initialization")

    /DISCARD/ :
    {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame.*)
        *(.debug .debug.*)
    }
}
