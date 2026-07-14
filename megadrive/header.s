| Hand-written Mega Drive vector table, cartridge header and startup code.
| vasm standard/GNU-as compatible syntax.

    .section .vectors,"a",@progbits
    .globl rom_vectors
rom_vectors:
    .long 0x00FFFFFC          | Initial supervisor stack pointer
    .long reset_entry         | Reset
    .long exception_handler   | Bus error
    .long exception_handler   | Address error
    .long exception_handler   | Illegal instruction
    .long exception_handler   | Division by zero
    .long exception_handler   | CHK
    .long exception_handler   | TRAPV
    .long exception_handler   | Privilege violation
    .long exception_handler   | Trace
    .long exception_handler   | Line 1010 emulator
    .long exception_handler   | Line 1111 emulator
    .long exception_handler
    .long exception_handler
    .long exception_handler
    .long exception_handler   | Uninitialized interrupt
    .rept 8
    .long exception_handler   | Reserved
    .endr
    .long irq_spurious
    .long irq_level_1
    .long irq_level_2
    .long irq_level_3
    .long irq_level_4          | HBlank autovector
    .long irq_level_5
    .long irq_level_6          | VBlank autovector
    .long irq_level_7
    .rept 16
    .long exception_handler    | TRAP #0-#15
    .endr
    .rept 16
    .long exception_handler    | Reserved
    .endr

    .section .rom_header,"a",@progbits
    .globl sega_rom_header
sega_rom_header:
header_console:
    .ascii "SEGA MEGA DRIVE "
header_copyright:
    .ascii "(C)RNS 2026.JUL"
    .space 16 - ($ - header_copyright), 0x20
header_domestic_name:
    .ascii "MEGADRIVE ENVIRONMENT SAMPLE GAME"
    .space 48 - ($ - header_domestic_name), 0x20
header_overseas_name:
    .ascii "MEGADRIVE ENVIRONMENT SAMPLE GAME"
    .space 48 - ($ - header_overseas_name), 0x20
header_serial:
    .ascii "GM 00000000-00"
header_checksum:
    .word 0                    | Patched by build_megadrive_rom.py
header_io_support:
    .ascii "J"
    .space 16 - ($ - header_io_support), 0x20
header_rom_start:
    .long 0x00000000
header_rom_end:
    .long 0x003FFFFF
header_ram_start:
    .long 0x00FF0000
header_ram_end:
    .long 0x00FFFFFF
header_backup_ram:
    .space 12, 0x20
header_modem:
    .space 12, 0x20
header_notes:
    .ascii "C++23 SAMPLE; TILES STORED AT END OF ROM"
    .space 40 - ($ - header_notes), 0x20
header_regions:
    .ascii "JUE"
    .space 16 - ($ - header_regions), 0x20

    .section .startup,"ax",@progbits
    .balign 2
    .globl reset_entry
reset_entry:
    move.w  #0x2700, %sr       | Supervisor mode; mask IRQs during startup

    | Unlock the VDP on consoles equipped with the TMSS security register.
    move.b  0x00A10001, %d0
    andi.b  #0x0F, %d0
    beq.s   1f
    move.l  #0x53454741, 0x00A14000
1:
    jsr     game_main
2:
    bra.s   2b

exception_handler:
    stop    #0x2700
    bra.s   exception_handler

irq_spurious:
irq_level_1:
irq_level_2:
irq_level_3:
irq_level_5:
irq_level_7:
    rte

| Reading VDP status acknowledges both external VDP interrupt sources.
irq_level_4:
irq_level_6:
    move.w  0x00C00004, %d0
    rte
