#!/usr/bin/env python3
"""Compatibility shim: asset ROM tests live in test_build_assets.py."""

from __future__ import annotations

import runpy
import sys
from pathlib import Path


if __name__ == "__main__":
    script = Path(__file__).with_name("test_build_assets.py")
    # Replace argv[0] so error messages name the real test module.
    sys.argv = [str(script), *sys.argv[1:]]
    runpy.run_path(str(script), run_name="__main__")
