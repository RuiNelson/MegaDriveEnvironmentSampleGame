; Boing Ball demo sound driver for the Mega Drive Z80.
; Assembled with z80asm (https://www.nongnu.org/z80asm/).
;
; Plays the original Amiga Boing impact sample (garage-door slam) through the
; YM2612 DAC, streaming from cartridge ROM via the Z80 32 KiB bank window.
;
; Z80 RAM layout (shared with the 68000 via $A00000):
;   $0000..     this program
;   $1E00       command mailbox (68K writes, Z80 clears)
;                 0 = idle
;                 1 = floor bounce  (Amiga period 255)
;                 2 = wall bounce   (Amiga period 160)
;   $1E01       status (Z80 writes 1 when ready)
;   $1E02/$1E03 sample bank (word, little-endian) = cart_addr >> 15
;   $1E04/$1E05 sample pointer in bank window (word LE, $8000+)
;   $1E06/$1E07 sample length in bytes (word LE)
;
; YM2612 is memory-mapped on the Z80 bus at $4000-$4003.
; Bank register is written bit-serially at $6000.

	org	0

YM_ADDR:	equ	0x4000
YM_DATA:	equ	0x4001
Z80_BANK:	equ	0x6000

CMD_MAILBOX:	equ	0x1E00
STATUS:		equ	0x1E01
PCM_BANK:	equ	0x1E02
PCM_PTR:	equ	0x1E04
PCM_LEN:	equ	0x1E06

CMD_FLOOR:	equ	1
CMD_WALL:	equ	2

; Playback delays (inner DJNZ counts). Tuned for ~14 kHz / ~22 kHz on a
; 3.58 MHz Z80 with a tight DAC write path (no busy-wait per sample).
DELAY_FLOOR:	equ	18
DELAY_WALL:	equ	11

start:
	di
	ld	sp, 0x1E00
	xor	a
	ld	(CMD_MAILBOX), a
	call	init_dac
	ld	a, 1
	ld	(STATUS), a

main_loop:
	ld	a, (CMD_MAILBOX)
	or	a
	jr	z, main_loop
	ld	b, a
	xor	a
	ld	(CMD_MAILBOX), a
	ld	a, b
	cp	CMD_FLOOR
	jr	z, do_floor
	cp	CMD_WALL
	jr	z, do_wall
	jr	main_loop

do_floor:
	ld	c, DELAY_FLOOR
	call	play_pcm
	jr	main_loop

do_wall:
	ld	c, DELAY_WALL
	call	play_pcm
	jr	main_loop

; ---------------------------------------------------------------------------
; YM2612 helpers
; ---------------------------------------------------------------------------

ym_wait:
	ld	a, (YM_ADDR)
	rla
	jr	c, ym_wait
	ret

; Write E to register A on FM part 0.
ym_write:
	push	af
	call	ym_wait
	pop	af
	ld	(YM_ADDR), a
	ld	a, e
	ld	(YM_DATA), a
	ret

; ---------------------------------------------------------------------------
; DAC init — silence FM channels, enable DAC on channel 6 path.
; ---------------------------------------------------------------------------

init_dac:
	ld	a, 0x22
	ld	e, 0x00
	call	ym_write		; LFO off
	ld	a, 0x27
	ld	e, 0x00
	call	ym_write		; timers off

	; Key off all six channels
	ld	b, 0
.koff:
	ld	a, 0x28
	ld	e, b
	call	ym_write
	inc	b
	ld	a, b
	cp	3
	jr	c, .koff
	ld	b, 4
.koff2:
	ld	a, 0x28
	ld	e, b
	call	ym_write
	inc	b
	ld	a, b
	cp	7
	jr	c, .koff2

	; Centre silence on the DAC data port, then enable DAC mode.
	ld	a, 0x2A
	ld	e, 0x80
	call	ym_write
	ld	a, 0x2B
	ld	e, 0x80
	call	ym_write		; DAC enable
	ret

; ---------------------------------------------------------------------------
; Bank window: shift 9 bits (68K A15..A23) into $6000, LSB first.
; HL = bank number (cart_addr >> 15)
; ---------------------------------------------------------------------------

set_bank:
	ld	b, 9
.sb_loop:
	ld	a, l
	and	1
	ld	(Z80_BANK), a
	srl	h
	rr	l
	djnz	.sb_loop
	ret

; ---------------------------------------------------------------------------
; Stream PCM from the banked cartridge window to the YM2612 DAC.
; C = per-sample delay (DJNZ count)
; ---------------------------------------------------------------------------

play_pcm:
	; Install bank for the sample
	ld	hl, (PCM_BANK)
	call	set_bank

	ld	hl, (PCM_PTR)
	ld	de, (PCM_LEN)
	ld	a, d
	or	e
	ret	z

	; Point YM address latch at DAC data ($2A) once per stream.
	call	ym_wait
	ld	a, 0x2A
	ld	(YM_ADDR), a

.sample:
	ld	a, (hl)
	inc	hl
	ld	(YM_DATA), a

	; Pace the stream. Outer count in B from C.
	ld	b, c
.delay:
	djnz	.delay

	dec	de
	ld	a, d
	or	e
	jr	nz, .sample

	; Return DAC to centre so the next hit starts clean.
	ld	a, 0x80
	ld	(YM_DATA), a
	ret
