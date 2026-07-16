"""Assemble Z80 sources with z80asm (https://www.nongnu.org/z80asm/)."""

from __future__ import annotations

import argparse
import shutil
import subprocess
from pathlib import Path


def require_z80asm() -> str:
    executable = shutil.which("z80asm")
    if executable is None:
        raise RuntimeError(
            "required tool 'z80asm' was not found; "
            "on macOS run 'brew install z80asm' "
            "(https://www.nongnu.org/z80asm/)"
        )
    return executable


def assemble_z80(source: Path, output: Path, *, list_file: Path | None = None) -> bytes:
    """Assemble one Z80 source file to a raw binary and return its bytes."""
    source = source.resolve()
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    command = [
        require_z80asm(),
        "-i",
        str(source),
        "-o",
        str(output),
    ]
    if list_file is not None:
        list_file = list_file.resolve()
        list_file.parent.mkdir(parents=True, exist_ok=True)
        command.append(f"--list={list_file}")

    subprocess.run(command, cwd=source.parent, check=True)
    if not output.is_file():
        raise RuntimeError(f"z80asm did not produce {output}")

    data = output.read_bytes()
    if not data:
        raise RuntimeError(f"assembled Z80 binary is empty: {output}")
    return data


def parse_args() -> argparse.Namespace:
    repository = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source",
        type=Path,
        default=repository / "z80" / "boing_ball_sfx.s",
        help="Z80 assembly source",
    )
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="raw binary output path",
    )
    parser.add_argument(
        "--list",
        type=Path,
        help="optional listing file path",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    data = assemble_z80(args.source, args.output, list_file=args.list)
    print(f"Wrote {args.output}: {len(data)} bytes from {args.source}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
