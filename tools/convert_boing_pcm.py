#!/usr/bin/env python3
"""Convert the Amiga Boing sample to YM2612 DAC PCM.

Amiga Paula plays ``boing.samples`` as signed 8-bit PCM. The Mega Drive YM2612
DAC (register $2A) wants **unsigned** 8-bit with centre 0x80, at a fixed rate
the Z80 can stream cleanly.

This script:

  1. Loads Amiga signed 8-bit PCM
  2. Removes DC offset
  3. Low-pass filters (anti-alias) before downsampling
  4. Resamples from the Amiga playback rate (Paula period 255 → ~14037 Hz on
     NTSC) to a stable Mega Drive DAC rate (default 8000 Hz)
  5. Peak-normalises gently and quantises to unsigned 8-bit (silence = 0x80)

Example::

    python3 tools/convert_boing_pcm.py \\
        --input sound/amiga_assets/boing.samples \\
        --output sound/amiga_assets/boing_pcm.bin
"""

from __future__ import annotations

import argparse
import math
import struct
import sys
from pathlib import Path


# NTSC Amiga colour clock / period 255 — rate the Boing demo uses for floor hits.
AMIGA_NTSC_CLOCK = 3_579_545
AMIGA_FLOOR_PERIOD = 255
DEFAULT_SOURCE_RATE = AMIGA_NTSC_CLOCK / AMIGA_FLOOR_PERIOD  # ≈ 14037.43 Hz

# Comfortable for a tight Z80 DAC loop with margin for banked ROM reads.
DEFAULT_TARGET_RATE = 8000.0


def load_amiga_signed_pcm(path: Path) -> list[float]:
    """Load Amiga signed 8-bit PCM as floats in [-1, 1]."""
    raw = path.read_bytes()
    if not raw:
        raise ValueError(f"empty sample file: {path}")
    samples: list[float] = []
    for byte in raw:
        signed = byte if byte < 128 else byte - 256
        samples.append(signed / 128.0)
    return samples


def remove_dc(samples: list[float]) -> list[float]:
    mean = sum(samples) / len(samples)
    return [s - mean for s in samples]


def trim_silence(samples: list[float], threshold: float = 0.02, pad: int = 64) -> list[float]:
    """Drop near-silent head/tail; keep a little padding for the envelope."""
    abs_s = [abs(s) for s in samples]
    first = 0
    while first < len(abs_s) and abs_s[first] < threshold:
        first += 1
    last = len(abs_s)
    while last > first and abs_s[last - 1] < threshold:
        last -= 1
    first = max(0, first - pad)
    last = min(len(samples), last + pad)
    if last <= first:
        return samples
    return samples[first:last]


def lowpass_fir(samples: list[float], cutoff_hz: float, sample_rate: float, taps: int = 63) -> list[float]:
    """Simple windowed-sinc low-pass (Hanning). Pure Python, no numpy."""
    if cutoff_hz <= 0 or cutoff_hz >= sample_rate * 0.5:
        return list(samples)
    if taps % 2 == 0:
        taps += 1
    fc = cutoff_hz / sample_rate
    mid = taps // 2
    kernel: list[float] = []
    for n in range(taps):
        x = n - mid
        if x == 0:
            h = 2.0 * fc
        else:
            h = math.sin(2.0 * math.pi * fc * x) / (math.pi * x)
        # Hanning window
        w = 0.5 - 0.5 * math.cos(2.0 * math.pi * n / (taps - 1))
        kernel.append(h * w)
    gain = sum(kernel)
    kernel = [k / gain for k in kernel]

    # Reflect-pad edges to avoid end clicks
    pad = mid
    padded = samples[pad:0:-1] + samples + samples[-2 : -pad - 2 : -1]
    out: list[float] = []
    for i in range(len(samples)):
        acc = 0.0
        base = i
        for k, coeff in enumerate(kernel):
            acc += padded[base + k] * coeff
        out.append(acc)
    return out


def resample_linear(samples: list[float], source_rate: float, target_rate: float) -> list[float]:
    """Linear resample after caller has low-passed for downsampling."""
    if abs(source_rate - target_rate) < 1e-6:
        return list(samples)
    if len(samples) < 2:
        return list(samples)
    ratio = source_rate / target_rate
    out_len = max(1, int(round(len(samples) / ratio)))
    out: list[float] = []
    for i in range(out_len):
        src = i * ratio
        i0 = int(src)
        i1 = min(i0 + 1, len(samples) - 1)
        frac = src - i0
        out.append(samples[i0] * (1.0 - frac) + samples[i1] * frac)
    return out


def normalize_peak(samples: list[float], peak: float = 0.92) -> list[float]:
    """Peak-normalise below full scale to avoid DAC clipping grit."""
    max_abs = max((abs(s) for s in samples), default=0.0)
    if max_abs < 1e-9:
        return list(samples)
    scale = peak / max_abs
    return [max(-1.0, min(1.0, s * scale)) for s in samples]


def float_to_ym2612_unsigned(samples: list[float]) -> bytes:
    """Quantise [-1, 1] to YM2612 DAC unsigned bytes (0x80 = silence)."""
    out = bytearray(len(samples))
    for i, sample in enumerate(samples):
        # Map -1..1 → 0..255 with centre 128
        value = int(round(sample * 127.0 + 128.0))
        if value < 0:
            value = 0
        elif value > 255:
            value = 255
        out[i] = value
    return bytes(out)


def convert_amiga_to_ym_dac(
    samples: list[float],
    *,
    source_rate: float,
    target_rate: float,
) -> bytes:
    samples = remove_dc(samples)
    samples = trim_silence(samples)
    # Anti-alias: cut a little below Nyquist of the *target* rate.
    cutoff = min(source_rate * 0.45, target_rate * 0.45)
    samples = lowpass_fir(samples, cutoff_hz=cutoff, sample_rate=source_rate, taps=95)
    samples = resample_linear(samples, source_rate=source_rate, target_rate=target_rate)
    # Second gentle low-pass at target rate to tame linear-resample images.
    samples = lowpass_fir(samples, cutoff_hz=target_rate * 0.42, sample_rate=target_rate, taps=47)
    samples = normalize_peak(samples, peak=0.90)
    return float_to_ym2612_unsigned(samples)


def parse_args() -> argparse.Namespace:
    repository = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        type=Path,
        default=repository / "sound" / "amiga_assets" / "boing.samples",
        help="Amiga signed 8-bit PCM (boing.samples)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=repository / "sound" / "amiga_assets" / "boing_pcm.bin",
        help="YM2612 unsigned 8-bit PCM output",
    )
    parser.add_argument(
        "--source-rate",
        type=float,
        default=DEFAULT_SOURCE_RATE,
        help=f"Amiga playback rate in Hz (default period-255 ≈ {DEFAULT_SOURCE_RATE:.2f})",
    )
    parser.add_argument(
        "--target-rate",
        type=float,
        default=DEFAULT_TARGET_RATE,
        help=f"YM2612 DAC stream rate in Hz (default {DEFAULT_TARGET_RATE:.0f})",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not args.input.is_file():
        print(f"error: input not found: {args.input}", file=sys.stderr)
        return 1

    signed = load_amiga_signed_pcm(args.input)
    pcm = convert_amiga_to_ym_dac(
        signed,
        source_rate=args.source_rate,
        target_rate=args.target_rate,
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(pcm)

    duration = len(pcm) / args.target_rate
    print(
        f"Wrote {args.output}: {len(pcm)} bytes, "
        f"{args.source_rate:.1f} Hz → {args.target_rate:.1f} Hz, "
        f"duration {duration:.3f}s, centre={sum(pcm) / len(pcm):.1f}"
    )
    # Emit a tiny sidecar the Z80 build can stay in sync with (optional).
    meta = args.output.with_suffix(".rate.txt")
    meta.write_text(f"{int(round(args.target_rate))}\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
