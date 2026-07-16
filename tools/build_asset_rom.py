#!/usr/bin/env python3
"""Compatibility entry point for the asset pack builder.

Historically this script only wrote VDP tiles. It now forwards to
``build_assets.py``, which packs tiles, the Boing Ball Z80 driver, and any
future blobs into the same raw 32-Mbit image.
"""

from __future__ import annotations

import sys
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from build_assets import main  # noqa: E402


if __name__ == "__main__":
    raise SystemExit(main())
