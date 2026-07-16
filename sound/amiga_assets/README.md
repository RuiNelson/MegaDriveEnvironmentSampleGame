# Amiga sound assets

## `boing.samples`

Original Amiga Boing impact sample (8-bit **signed** PCM, silence = 0), from
http://amiga.filfre.net/misc/Chapter2/boing.samples  
(Dale Luck / R.J. Mical demo; archive notes by Jimmy Maher).

## `boing_pcm.bin`

**Generated** — do not hand-edit. Produced by:

```bash
python3 sound/tools/convert_boing_pcm.py \
  --input sound/amiga_assets/boing.samples \
  --output sound/amiga_assets/boing_pcm.bin \
  --source-rate 14037.43 \
  --target-rate 8000
```

Pipeline:

1. Load Amiga signed 8-bit  
2. Remove DC  
3. Trim near-silence  
4. Low-pass (anti-alias)  
5. Resample **14037 Hz → 8000 Hz** (Paula period-255 rate → Z80 DAC rate)  
6. Peak-normalise to 90%  
7. Quantise to YM2612 unsigned 8-bit (silence = `0x80`)

The Z80 driver in `sound/z80/` streams this file at 8 kHz (floor) or ≈12.6 kHz
(wall, Amiga period 160/255 pitch ratio).
