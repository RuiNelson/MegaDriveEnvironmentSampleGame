; Boing Ball demo FM sound driver for the Mega Drive Z80.
; Assembled with z80asm (https://www.nongnu.org/z80asm/).
;
; Z80 RAM layout (shared with the 68000 via $A00000):
;   $0000..  this program
;   $1FF0    command mailbox (68K writes, Z80 clears)
;              0 = idle
;              1 = floor bounce
;              2 = wall bounce
;   $1FF1    status (Z80 writes 1 when ready)
;
; YM2612 is memory-mapped on the Z80 bus at $4000-$4003.

	org	0

YM_ADDR:	equ	0x4000
YM_DATA:	equ	0x4001
CMD_MAILBOX:	equ	0x1FF0
STATUS:		equ	0x1FF1

CMD_FLOOR:	equ	1
CMD_WALL:	equ	2

start:
	di
	ld	sp, 0x2000
	xor	a
	ld	(CMD_MAILBOX), a
	call	init_ym
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
	call	sfx_floor
	jr	main_loop

do_wall:
	call	sfx_wall
	jr	main_loop

; ---------------------------------------------------------------------------
; YM2612 helpers (part 0 only)
; ---------------------------------------------------------------------------

; Wait until the chip is not busy (status bit 7 clear).
ym_wait:
	ld	a, (YM_ADDR)
	rla
	jr	c, ym_wait
	ret

; Write E to register A on FM part 0.
ym_write:
	call	ym_wait
	ld	(YM_ADDR), a
	ld	a, e
	ld	(YM_DATA), a
	ret

; Key-on / key-off channel 0. D = 0xF0 (on) or 0x00 (off).
ym_key:
	ld	a, 0x28
	ld	e, d
	call	ym_write
	ret

; ---------------------------------------------------------------------------
; Voice setup: algorithm 7, operator 1 only (simple carrier).
; Good enough for short percussive hits without multi-op FM tuning.
; ---------------------------------------------------------------------------

init_ym:
	ld	a, 0x22
	ld	e, 0x00
	call	ym_write		; LFO off
	ld	a, 0x27
	ld	e, 0x00
	call	ym_write		; timers off, ch3 normal
	ld	d, 0x00
	call	ym_key			; key off ch0

	ld	a, 0xB0
	ld	e, 0x07
	call	ym_write		; feedback 0, algorithm 7
	ld	a, 0xB4
	ld	e, 0xC0
	call	ym_write		; L/R enable

	; Operator 1: audible carrier
	ld	a, 0x30
	ld	e, 0x01
	call	ym_write		; DT=0 MUL=1
	ld	a, 0x40
	ld	e, 0x04
	call	ym_write		; TL nearly full
	ld	a, 0x50
	ld	e, 0x1F
	call	ym_write		; AR=31
	ld	a, 0x60
	ld	e, 0x12
	call	ym_write		; D1R
	ld	a, 0x70
	ld	e, 0x0F
	call	ym_write		; D2R
	ld	a, 0x80
	ld	e, 0x2F
	call	ym_write		; SL=2 RR=15 (percussive)

	; Operators 2-4: silent
	ld	a, 0x34
	ld	e, 0x01
	call	ym_write
	ld	a, 0x38
	ld	e, 0x01
	call	ym_write
	ld	a, 0x3C
	ld	e, 0x01
	call	ym_write
	ld	a, 0x44
	ld	e, 0x7F
	call	ym_write
	ld	a, 0x48
	ld	e, 0x7F
	call	ym_write
	ld	a, 0x4C
	ld	e, 0x7F
	call	ym_write
	ret

; Set channel 0 block/f-number. B = block (0-7), HL = f-number (0-0x3FF).
ym_pitch:
	ld	a, h
	and	0x07
	ld	c, a			; f-number bits 8-10
	ld	a, b
	and	0x07
	add	a, a
	add	a, a
	add	a, a			; block << 3
	or	c
	ld	e, a
	ld	a, 0xA4
	call	ym_write		; block / fnum high
	ld	a, 0xA0
	ld	e, l
	call	ym_write		; fnum low
	ret

; ---------------------------------------------------------------------------
; Effects
; ---------------------------------------------------------------------------

; Low thump for floor impact.
sfx_floor:
	ld	b, 3			; block 3
	ld	hl, 0x21D		; ~110 Hz
	call	ym_pitch
	ld	a, 0x40
	ld	e, 0x08
	call	ym_write		; slightly softer body
	ld	d, 0xF0
	call	ym_key
	ld	bc, 0x1800
	call	delay_bc
	ld	d, 0x00
	call	ym_key
	ld	bc, 0x0C00
	call	delay_bc
	ret

; Higher dry knock for wall impact.
sfx_wall:
	ld	b, 4			; block 4
	ld	hl, 0x32C		; ~330 Hz
	call	ym_pitch
	ld	a, 0x40
	ld	e, 0x02
	call	ym_write		; brighter / louder
	ld	d, 0xF0
	call	ym_key
	ld	bc, 0x0E00
	call	delay_bc
	ld	d, 0x00
	call	ym_key
	ld	bc, 0x0800
	call	delay_bc
	ret

; Busy-wait. BC = iteration count (roughly a few milliseconds at Z80 clock).
delay_bc:
	dec	bc
	ld	a, b
	or	c
	jr	nz, delay_bc
	ret
