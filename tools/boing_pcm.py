"""Convert the Amiga Boing impact sample for the YM2612 DAC.

Amiga Paula samples are 8-bit **signed** (0 = silence).
YM2612 register $2A expects 8-bit **unsigned** (0x80 = silence).

Hardware (and ymfm) applies:  internal = (data ^ 0x80) << 1
so the correct host conversion is simply::

    ym_byte = (amiga_signed_byte + 128) & 0xFF
"""

from __future__ import annotations

from pathlib import Path


def amiga_signed_to_ym2612_unsigned(raw: bytes) -> bytes:
    """Map Amiga signed PCM to YM2612 DAC unsigned PCM."""
    out = bytearray(len(raw))
    for i, byte in enumerate(raw):
        signed = byte if byte < 128 else byte - 256
        out[i] = (signed + 128) & 0xFF
    return bytes(out)


def load_boing_pcm(path: Path) -> bytes:
    """Load PCM ready for DAC playback from a file on disk.

    Accepts:
      - original Amiga ``boing.samples`` (signed 8-bit, mean near 0)
      - already-converted unsigned (mean near 128, centre 0x80)
    """
    raw = path.read_bytes()
    if not raw:
        raise ValueError(f"empty PCM file: {path}")

    as_signed = [b if b < 128 else b - 256 for b in raw]
    mean_signed = sum(as_signed) / len(as_signed)

    # Signed Amiga source: convert. Unsigned centre-128 asset: use as-is.
    if abs(mean_signed) < 16:
        return amiga_signed_to_ym2612_unsigned(raw)
    return raw
