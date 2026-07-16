"""Load the Boing Ball impact PCM asset for the pack builder."""

from __future__ import annotations

from pathlib import Path

# Playback rate assumed by z80/boing_ball_sfx.s delays.
TARGET_RATE_HZ = 8000


def load_boing_pcm(path: Path) -> bytes:
    """Load unsigned 8-bit YM2612 DAC PCM (centre 0x80)."""
    raw = path.read_bytes()
    if not raw:
        raise ValueError(f"empty PCM file: {path}")
    return raw
