"""Load converted Boing PCM for the asset pack (see convert_boing_pcm.py)."""

from __future__ import annotations

from pathlib import Path


# Must match tools/convert_boing_pcm.py DEFAULT_TARGET_RATE and Z80 delays.
TARGET_RATE_HZ = 8000


def load_boing_pcm(path: Path) -> bytes:
    """Load already-converted YM2612 unsigned PCM (centre 0x80)."""
    raw = path.read_bytes()
    if not raw:
        raise ValueError(f"empty PCM file: {path}")
    # Guard against accidentally packing the raw Amiga signed file.
    as_signed = [b if b < 128 else b - 256 for b in raw]
    mean_signed = sum(as_signed) / len(as_signed)
    if abs(mean_signed) < 8:
        raise ValueError(
            f"{path} looks like Amiga signed PCM; run tools/convert_boing_pcm.py first"
        )
    return raw
