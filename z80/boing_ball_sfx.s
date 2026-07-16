; Boing Ball demo sound driver for the Mega Drive Z80.
; Assembled with z80asm (https://www.nongnu.org/z80asm/).
;
; Streams assets/boing_pcm.bin (from tools/convert_boing_pcm.py) through the
; YM2612 channel-6 DAC. Input is unsigned 8-bit @ 8000 Hz, centre 0x80
; (Amiga signed @ ~14037 Hz, low-passed and resampled).
;
; Z80 RAM (visible to the 68000 at $A00000):
;   $1E00       command: 0 idle, 1 floor (8 kHz), 2 wall (faster ≈ period 160)
;   $1E01       status: 1 = ready
;   $1E02/03    PCM bank  = cart_addr >> 15          (little-endian word)
;   $1E04/05    PCM ptr   = $8000 | (cart_addr & $7FFF)
;   $1E06/07    PCM length in bytes
;
; Hardware map used (memory only):
;   $4000/$4001  YM2612 part 0  (global + ch1-3)
;   $4002/$4003  YM2612 part 1  (ch4-6, including DAC pan $B6)
;   $6000        Z80 bank latch (9 bits, LSB first = 68K A15)

	org	0

YM0_ADDR:	equ	0x4000
YM0_DATA:	equ	0x4001
YM1_ADDR:	equ	0x4002
YM1_DATA:	equ	0x4003
Z80_BANK:	equ	0x6000

CMD_MAILBOX:	equ	0x1E00
STATUS:		equ	0x1E01
PCM_BANK:	equ	0x1E02
PCM_PTR:	equ	0x1E04
PCM_LEN:	equ	0x1E06

CMD_FLOOR:	equ	1
CMD_WALL:	equ	2

; PCM is fixed at 8000 Hz (see convert_boing_pcm.py). Loop body ≈ 56 T-states
; plus DJNZ delay. Target: floor 8000 Hz (≈448 T), wall ≈ 8000*(255/160) Hz
; (Amiga period ratio, higher pitch / shorter hit).
;   DELAY=n → 56 + 13*(n-1)+8 T
DELAY_FLOOR:	equ	30		; ≈441 T → ~8.1 kHz
DELAY_WALL:	equ	18		; ≈285 T → ~12.6 kHz (≈8k * 255/160)

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

ym_wait:
	ld	a, (YM0_ADDR)
	rla
	jr	c, ym_wait
	ret

; Write E to register A on FM part 0 ($4000/$4001).
ym0_write:
	push	af
	call	ym_wait
	pop	af
	ld	(YM0_ADDR), a
	ld	a, e
	ld	(YM0_DATA), a
	ret

; Write E to register A on FM part 1 ($4002/$4003).
ym1_write:
	push	af
	call	ym_wait
	pop	af
	ld	(YM1_ADDR), a
	ld	a, e
	ld	(YM1_DATA), a
	ret

; ---------------------------------------------------------------------------
; DAC setup. Critical: enable pan L+R on channel 6 ($B6 in part 1). Without
; those bits the YM2612 mixes DAC as silence (ymfm checks ch_output on 0x102).
; ---------------------------------------------------------------------------

init_dac:
	ld	a, 0x22
	ld	e, 0x00
	call	ym0_write			; LFO off
	ld	a, 0x27
	ld	e, 0x00
	call	ym0_write			; timers off, ch3 normal

	; Key-off every FM channel (0,1,2,4,5,6 — slot bits clear).
	ld	e, 0x00
	ld	a, 0x28
	call	ym0_write
	ld	e, 0x01
	ld	a, 0x28
	call	ym0_write
	ld	e, 0x02
	ld	a, 0x28
	call	ym0_write
	ld	e, 0x04
	ld	a, 0x28
	call	ym0_write
	ld	e, 0x05
	ld	a, 0x28
	call	ym0_write
	ld	e, 0x06
	ld	a, 0x28
	call	ym0_write

	; Channel 6 stereo enable (part 1, reg $B6): L|R = %11xxxxxx
	ld	a, 0xB6
	ld	e, 0xC0
	call	ym1_write

	; Centre the DAC and enable it (global regs on part 0).
	ld	a, 0x2A
	ld	e, 0x80
	call	ym0_write			; silence (unsigned centre)
	ld	a, 0x2B
	ld	e, 0x80
	call	ym0_write			; DAC enable
	ret

; ---------------------------------------------------------------------------
; Bank = cart_addr >> 15. Nine writes to $6000, bit0 each time, A15 first.
; HL = bank number.
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
; Stream PCM: C = delay count (Amiga period analogue).
; Sample data is unsigned, centre 0x80.
; ---------------------------------------------------------------------------

play_pcm:
	ld	hl, (PCM_BANK)
	call	set_bank

	ld	hl, (PCM_PTR)
	ld	de, (PCM_LEN)
	ld	a, d
	or	e
	ret	z

	; Latch DAC data register once; keep streaming data bytes after.
	call	ym_wait
	ld	a, 0x2A
	ld	(YM0_ADDR), a

.sample:
	ld	a, (hl)			; unsigned PCM
	inc	hl
	ld	(YM0_DATA), a

	ld	b, c
.delay:
	djnz	.delay

	dec	de
	ld	a, d
	or	e
	jr	nz, .sample

	ld	a, 0x80
	ld	(YM0_DATA), a		; rest at centre
	ret
