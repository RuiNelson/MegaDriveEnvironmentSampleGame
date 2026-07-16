"""Prepare the Amiga Boing impact sample for YM2612 DAC playback."""

from __future__ import annotations

from pathlib import Path


def load_boing_pcm(path: Path) -> bytes:
    """Load unsigned 8-bit PCM (0x80 centre) for the Boing Ball impact.

    Accepts either the pre-converted ``boing_pcm.bin`` (unsigned) or the
    original Amiga ``boing.samples`` (signed 8-bit) and normalises to unsigned.
    """
    raw = path.read_bytes()
    if not raw:
        raise ValueError(f"empty PCM file: {path}")

    # Heuristic: original Amiga file averages near zero when signed; unsigned
    # centre-at-0x80 averages near 128.
    as_signed = [b if b < 128 else b - 256 for b in raw]
    mean_signed = sum(as_signed) / len(as_signed)
    if abs(mean_signed) < 16:
        return bytes((b + 128) & 0xFF for b in as_signed)
    return raw
