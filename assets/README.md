# Assets

## `boing_pcm.bin`

Unsigned 8-bit PCM for the YM2612 DAC (silence = `0x80`), converted from the
classic Amiga Boing impact sample.

| Source | Format |
|--------|--------|
| Amiga `boing.samples` | 8-bit **signed** (Paula, silence = 0) |
| This file | 8-bit **unsigned** (YM2612 `$2A`, silence = `0x80`) |

Conversion (required by the chip / ymfm: `internal = (data ^ 0x80) << 1`):

```text
ym_byte = (amiga_signed + 128) & 0xFF
```

Playback rates match the Amiga demo’s Paula periods (NTSC colour clock):

| Hit | Amiga period | ≈ sample rate |
|-----|--------------|---------------|
| Floor | 255 | ~14.0 kHz |
| Wall  | 160 | ~22.4 kHz |

Source archive: http://amiga.filfre.net/misc/Chapter2/ (Dale Luck / R.J. Mical
Boing demo samples; reconstruction notes by Jimmy Maher).
