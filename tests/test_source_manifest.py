#!/usr/bin/env python3
"""Keep the shared-source manifest complete for both target builds."""

from __future__ import annotations

import json
from pathlib import Path


REPOSITORY = Path(__file__).resolve().parent.parent
MANIFEST = REPOSITORY / "config" / "shared_sources.json"


def main() -> int:
    entries = json.loads(MANIFEST.read_text(encoding="utf-8"))
    assert isinstance(entries, list) and entries, "manifest must be a non-empty list"
    assert len(entries) == len(set(entries)), "manifest contains duplicate sources"

    listed = {Path(entry) for entry in entries}
    expected = {
        path.relative_to(REPOSITORY)
        for path in (REPOSITORY / "src").glob("*.cpp")
        if not path.stem.endswith(("-PC", "-MD"))
    }
    assert listed == expected, (
        f"shared source manifest mismatch; missing={sorted(expected - listed)}, "
        f"extra={sorted(listed - expected)}"
    )
    assert all((REPOSITORY / source).is_file() for source in listed)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
