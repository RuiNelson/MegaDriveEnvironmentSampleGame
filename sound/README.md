# Sound

Boing Ball audio is split by responsibility:

- `z80/` — the Z80 DAC driver assembled into the asset image;
- `amiga_assets/` — original signed sample and checked-in 8 kHz YM2612 PCM;
- `tools/` — dependency-free assembly and conversion helpers.

Normal builds consume `amiga_assets/boing_pcm.bin` without modifying it. See
[`docs/ASSETS.md`](../docs/ASSETS.md) before regenerating the PCM or extending
the packed sound data.
