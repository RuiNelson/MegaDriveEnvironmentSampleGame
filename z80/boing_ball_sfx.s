; Boing Ball demo FM sound driver for the Mega Drive Z80.
; Assembled with z80asm (https://www.nongnu.org/z80asm/).
;
; Z80 RAM layout (shared with the 68000 via $A00000):
;   $0000..  this program
;   $1E00    command mailbox (68K writes, Z80 clears)
;              0 = idle
;              1 = floor bounce
;              2 = wall bounce
;   $1E01    status (Z80 writes 1 when ready)
;
; YM2612 is memory-mapped on the Z80 bus at $4000-$4003.
;
; Timbre aims at the classic Amiga "Boing" demo: a heavy, short metallic slam
; (the original was famously a garage-door sample) — weighty and resonant,
; not a beep and not a white-noise burst.

	org	0

YM_ADDR:	equ	0x4000
YM_DATA:	equ	0x4001
; Keep the mailbox well below the stack (SP starts at $1E00).
CMD_MAILBOX:	equ	0x1E00
STATUS:		equ	0x1E01

CMD_FLOOR:	equ	1
CMD_WALL:	equ	2

start:
	di
	; Stack grows down from just below the mailbox so nested YM helpers
	; cannot overwrite the 68K command byte.
	ld	sp, 0x1E00
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
; Clobbers A — callers that need A preserved must push/pop around this.
ym_wait:
	ld	a, (YM_ADDR)
	rla
	jr	c, ym_wait
	ret

; Write E to register A on FM part 0.
; Must preserve A across the busy wait: ym_wait loads status into A.
ym_write:
	push	af
	call	ym_wait
	pop	af
	ld	(YM_ADDR), a
	ld	a, e
	ld	(YM_DATA), a
	ret

; Key-on / key-off channel 0. D = 0xF0 (on) or 0x00 (off).
ym_key:
	ld	a, 0x28
	ld	e, d
	jp	ym_write

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

; Busy-wait. BC = iteration count.
delay_bc:
	dec	bc
	ld	a, b
	or	c
	jr	nz, delay_bc
	ret

; ---------------------------------------------------------------------------
; Voice: heavy metallic slam (algorithm 4, mild feedback).
;
; Two FM pairs give body + a soft upper resonance without pure noise.
; Op1 modulates Op2 (weight), Op3 modulates Op4 (short metallic edge).
; Feedback on the first pair is kept moderate so it stays "door" not "buzz".
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

	; Feedback 4, algorithm 4 → (M1→C1) + (M2→C2)
	ld	a, 0xB0
	ld	e, 0x24
	call	ym_write
	ld	a, 0xB4
	ld	e, 0xC0
	call	ym_write		; L/R pan centre

	; --- Slot 1 (modulator): low-ratio rattle of the door ---
	ld	a, 0x30
	ld	e, 0x02
	call	ym_write		; DT=0 MUL=2
	ld	a, 0x40
	ld	e, 0x1C
	call	ym_write		; TL moderate (not harsh)
	ld	a, 0x50
	ld	e, 0x1F
	call	ym_write		; AR=31
	ld	a, 0x60
	ld	e, 0x10
	call	ym_write		; D1R
	ld	a, 0x70
	ld	e, 0x0C
	call	ym_write		; D2R
	ld	a, 0x80
	ld	e, 0x8F
	call	ym_write		; SL=8 RR=15

	; --- Slot 2 (carrier): heavy fundamental ---
	ld	a, 0x34
	ld	e, 0x01
	call	ym_write		; MUL=1
	ld	a, 0x44
	ld	e, 0x06
	call	ym_write		; TL almost full (body)
	ld	a, 0x54
	ld	e, 0x1F
	call	ym_write		; AR=31
	ld	a, 0x64
	ld	e, 0x0A
	call	ym_write		; slower decay = weight
	ld	a, 0x74
	ld	e, 0x07
	call	ym_write
	ld	a, 0x84
	ld	e, 0x5A
	call	ym_write		; SL=5 RR=10 (lingering thud)

	; --- Slot 3 (modulator): soft upper metal ---
	ld	a, 0x38
	ld	e, 0x03
	call	ym_write		; MUL=3
	ld	a, 0x48
	ld	e, 0x28
	call	ym_write		; quieter modulator
	ld	a, 0x58
	ld	e, 0x1F
	call	ym_write
	ld	a, 0x68
	ld	e, 0x14
	call	ym_write
	ld	a, 0x78
	ld	e, 0x10
	call	ym_write
	ld	a, 0x88
	ld	e, 0xAF
	call	ym_write		; SL=10 RR=15 (dies fast)

	; --- Slot 4 (carrier): short bright edge, kept quiet ---
	ld	a, 0x3C
	ld	e, 0x01
	call	ym_write
	ld	a, 0x4C
	ld	e, 0x18
	call	ym_write		; quieter than main body
	ld	a, 0x5C
	ld	e, 0x1F
	call	ym_write
	ld	a, 0x6C
	ld	e, 0x12
	call	ym_write
	ld	a, 0x7C
	ld	e, 0x0E
	call	ym_write
	ld	a, 0x8C
	ld	e, 0x9F
	call	ym_write
	ret

; ---------------------------------------------------------------------------
; Effects — short downward pitch glides sell the heavy falling mass.
; ---------------------------------------------------------------------------

; Floor: deep garage-door slam, longer body.
sfx_floor:
	; Start ~55 Hz (block 2, fnum high enough for a thick thud)
	ld	b, 2
	ld	hl, 0x280
	call	ym_pitch
	ld	d, 0xF0
	call	ym_key

	ld	bc, 0x0C00
	call	delay_bc
	ld	b, 2
	ld	hl, 0x200
	call	ym_pitch		; drop

	ld	bc, 0x1000
	call	delay_bc
	ld	b, 2
	ld	hl, 0x180
	call	ym_pitch		; drop again

	ld	bc, 0x1400
	call	delay_bc
	ld	b, 1
	ld	hl, 0x300
	call	ym_pitch		; settle lower octave

	ld	bc, 0x1800
	call	delay_bc
	ld	d, 0x00
	call	ym_key
	ld	bc, 0x1000
	call	delay_bc
	ret

; Wall: same family, a bit higher and shorter — still heavy, not a click.
sfx_wall:
	ld	b, 2
	ld	hl, 0x320
	call	ym_pitch
	ld	d, 0xF0
	call	ym_key

	ld	bc, 0x0A00
	call	delay_bc
	ld	b, 2
	ld	hl, 0x280
	call	ym_pitch

	ld	bc, 0x0E00
	call	delay_bc
	ld	b, 2
	ld	hl, 0x200
	call	ym_pitch

	ld	bc, 0x1000
	call	delay_bc
	ld	d, 0x00
	call	ym_key
	ld	bc, 0x0C00
	call	delay_bc
	ret
