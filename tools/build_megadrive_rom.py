#!/usr/bin/env python3
"""Build and validate the bootable 32-Mbit Mega Drive ROM for real hardware."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

from build_asset_rom import ROM_SIZE, TILE_SIZE, build_tile_data


ROM_CHECKSUM_OFFSET = 0x18E
ROM_CHECKSUM_START = 0x200
TILE_COUNT = 101
TILE_DATA_SIZE = TILE_COUNT * TILE_SIZE
TILE_DATA_OFFSET = ROM_SIZE - TILE_DATA_SIZE


def run(command: list[str], *, cwd: Path) -> None:
    print("+", " ".join(command))
    subprocess.run(command, cwd=cwd, check=True)


def require_tool(name: str) -> str:
    executable = shutil.which(name)
    if executable is None:
        hint = "on macOS run 'brew install m68k-elf-binutils m68k-elf-gcc'"
        raise RuntimeError(f"required tool '{name}' was not found; {hint}")
    return executable


def sega_checksum(rom: bytes | bytearray) -> int:
    if len(rom) < ROM_CHECKSUM_START:
        raise ValueError("ROM is too small for a Mega Drive checksum")
    total = 0
    for offset in range(ROM_CHECKSUM_START, len(rom), 2):
        high = rom[offset]
        low = rom[offset + 1] if offset + 1 < len(rom) else 0
        total = (total + (high << 8) + low) & 0xFFFF
    return total


def write_checksum(rom_path: Path) -> int:
    rom = bytearray(rom_path.read_bytes())
    checksum = sega_checksum(rom)
    rom[ROM_CHECKSUM_OFFSET : ROM_CHECKSUM_OFFSET + 2] = checksum.to_bytes(2, "big")
    rom_path.write_bytes(rom)
    return checksum


def validate_rom(rom_path: Path, asset_rom_path: Path) -> dict[str, int]:
    rom = rom_path.read_bytes()
    assets = asset_rom_path.read_bytes()

    if len(rom) != ROM_SIZE:
        raise RuntimeError(f"final ROM is {len(rom)} bytes; expected {ROM_SIZE}")
    if len(assets) != ROM_SIZE:
        raise RuntimeError(f"asset ROM is {len(assets)} bytes; expected {ROM_SIZE}")
    if rom[0x100:0x110] != b"SEGA MEGA DRIVE ":
        raise RuntimeError("Sega console identifier is missing at 0x100")

    stack_pointer = int.from_bytes(rom[0:4], "big")
    reset_vector = int.from_bytes(rom[4:8], "big")
    hblank_vector = int.from_bytes(rom[28 * 4 : 29 * 4], "big")
    vblank_vector = int.from_bytes(rom[30 * 4 : 31 * 4], "big")
    stored_checksum = int.from_bytes(
        rom[ROM_CHECKSUM_OFFSET : ROM_CHECKSUM_OFFSET + 2], "big"
    )
    calculated_checksum = sega_checksum(rom)

    if not 0x00FF0000 <= stack_pointer <= 0x00FFFFFF:
        raise RuntimeError(f"invalid initial stack pointer: 0x{stack_pointer:08X}")
    if not ROM_CHECKSUM_START <= reset_vector < TILE_DATA_OFFSET:
        raise RuntimeError(f"invalid reset vector: 0x{reset_vector:08X}")
    if not ROM_CHECKSUM_START <= hblank_vector < TILE_DATA_OFFSET:
        raise RuntimeError(f"invalid HBlank IRQ vector: 0x{hblank_vector:08X}")
    if not ROM_CHECKSUM_START <= vblank_vector < TILE_DATA_OFFSET:
        raise RuntimeError(f"invalid VBlank IRQ vector: 0x{vblank_vector:08X}")
    if int.from_bytes(rom[0x1A0:0x1A4], "big") != 0:
        raise RuntimeError("ROM start field is not zero")
    if int.from_bytes(rom[0x1A4:0x1A8], "big") != ROM_SIZE - 1:
        raise RuntimeError("ROM end field does not describe a 32-Mbit ROM")
    if stored_checksum != calculated_checksum:
        raise RuntimeError(
            f"checksum mismatch: header=0x{stored_checksum:04X}, "
            f"calculated=0x{calculated_checksum:04X}"
        )
    if rom[TILE_DATA_OFFSET:] != assets[TILE_DATA_OFFSET:]:
        raise RuntimeError("final ROM tile blob differs from the generated asset ROM")

    return {
        "stack_pointer": stack_pointer,
        "reset_vector": reset_vector,
        "hblank_vector": hblank_vector,
        "vblank_vector": vblank_vector,
        "checksum": stored_checksum,
        "tile_offset": TILE_DATA_OFFSET,
    }


def escape_assembly_path(path: Path) -> str:
    return str(path.resolve()).replace("\\", "\\\\").replace('"', '\\"')


def generate_blobs_assembly(output_path: Path, blob_path: Path) -> None:
    output_path.write_text(
        "| Generated from the raw asset ROM. Do not edit.\n"
        '    .section .assets,"a",@progbits\n'
        "    .balign 2\n"
        "    .globl asset_tiles\n"
        "asset_tiles:\n"
        f'    .incbin "{escape_assembly_path(blob_path)}"\n'
        "    .globl asset_tiles_end\n"
        "asset_tiles_end:\n",
        encoding="utf-8",
    )


def generate_combined_cpp(output_path: Path, repository: Path) -> None:
    sources = (
        repository / "src" / "Memory-MD.cpp",
        repository / "src" / "ControllerReader.cpp",
        repository / "src" / "GameSession.cpp",
        repository / "src" / "PsgSoundEffects.cpp",
        repository / "src" / "VdpUtils.cpp",
        repository / "src" / "SampleGame.cpp",
        repository / "src" / "main-MD.cpp",
    )
    lines = ["// Generated compilation unit. Do not edit.\n"]
    for source in sources:
        lines.append(f'#include "{escape_assembly_path(source)}"\n')
    output_path.write_text("".join(lines), encoding="utf-8")


def build(args: argparse.Namespace) -> None:
    repository = Path(__file__).resolve().parent.parent
    build_dir = args.build_dir.resolve()
    generated_dir = build_dir / "generated"
    generated_dir.mkdir(parents=True, exist_ok=True)

    asset_rom = args.asset_rom.resolve()
    output_rom = args.output.resolve()
    output_rom.parent.mkdir(parents=True, exist_ok=True)

    cxx = require_tool(f"{args.tool_prefix}g++")
    assembler = require_tool(args.assembler or f"{args.tool_prefix}as")
    linker = require_tool(f"{args.tool_prefix}ld")
    objcopy = require_tool(f"{args.tool_prefix}objcopy")

    run(
        [
            sys.executable,
            str(repository / "tools" / "build_asset_rom.py"),
            "--output",
            str(asset_rom),
            "--font-data",
            str(args.font_data.resolve()),
        ],
        cwd=repository,
    )

    expected_tiles = build_tile_data(args.font_data.resolve())
    if asset_rom.read_bytes()[TILE_DATA_OFFSET:] != expected_tiles:
        raise RuntimeError("asset ROM verification failed before real-hardware build")

    combined_cpp = generated_dir / "code.cpp"
    code_assembly = generated_dir / "code.s"
    blobs_assembly = generated_dir / "blobs.s"
    blob_binary = generated_dir / "assets.bin"
    header_object = build_dir / "header.o"
    code_object = build_dir / "code.o"
    blobs_object = build_dir / "blobs.o"
    elf_path = build_dir / "MegaDriveEnvironmentSampleGame.elf"
    map_path = build_dir / "MegaDriveEnvironmentSampleGame.map"

    generate_combined_cpp(combined_cpp, repository)
    blob_binary.write_bytes(asset_rom.read_bytes()[TILE_DATA_OFFSET:])
    generate_blobs_assembly(blobs_assembly, blob_binary)

    run(
        [
            cxx,
            "-std=c++23",
            "-m68000",
            "-Os",
            "-DMEGADRIVE=1",
            "-ffreestanding",
            "-fno-exceptions",
            "-fno-rtti",
            "-fno-threadsafe-statics",
            "-fno-use-cxa-atexit",
            "-fno-unwind-tables",
            "-fno-asynchronous-unwind-tables",
            "-fomit-frame-pointer",
            "-fno-stack-protector",
            "-fdata-sections",
            "-ffunction-sections",
            "-fno-pic",
            "-fno-pie",
            "-fno-common",
            "-fno-ident",
            "-I",
            str(repository / "include"),
            "-S",
            str(combined_cpp),
            "-o",
            str(code_assembly),
        ],
        cwd=repository,
    )
    assembly_inputs = (
        (repository / "megadrive" / "header.s", header_object),
        (code_assembly, code_object),
        (blobs_assembly, blobs_object),
    )
    for source, object_path in assembly_inputs:
        run(
            [assembler, "-m68000", "-o", str(object_path), str(source)],
            cwd=repository,
        )
    run(
        [
            linker,
            "--gc-sections",
            "-T",
            str(repository / "megadrive" / "rom.ld"),
            "-Map",
            str(map_path),
            "-o",
            str(elf_path),
            str(header_object),
            str(code_object),
            str(blobs_object),
        ],
        cwd=repository,
    )
    run(
        [
            objcopy,
            "-O",
            "binary",
            "--gap-fill",
            "0xFF",
            str(elf_path),
            str(output_rom),
        ],
        cwd=repository,
    )

    checksum = write_checksum(output_rom)
    details = validate_rom(output_rom, asset_rom)
    print(
        f"Wrote {output_rom}: {ROM_SIZE} bytes, checksum 0x{checksum:04X}, "
        f"reset 0x{details['reset_vector']:06X}, IRQ4/6 "
        f"0x{details['hblank_vector']:06X}/0x{details['vblank_vector']:06X}, tiles at "
        f"0x{details['tile_offset']:06X}-0x{ROM_SIZE - 1:06X}"
    )
    print(f"Assembly inputs: {repository / 'megadrive' / 'header.s'}")
    print(f"                 {code_assembly}")
    print(f"                 {blobs_assembly}")


def parse_args() -> argparse.Namespace:
    repository = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=repository / "build" / "megadrive",
        help="directory for generated assembly, ELF and map files",
    )
    parser.add_argument(
        "--asset-rom",
        type=Path,
        default=repository / "build" / "sample_game_assets.bin",
        help="raw asset-only ROM produced by build_asset_rom.py",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=repository / "build" / "megadrive" / "MegaDriveEnvironmentSampleGame.bin",
        help="final bootable ROM for real hardware",
    )
    parser.add_argument(
        "--font-data",
        type=Path,
        default=(
            repository.parent
            / "MegaDriveEnvironment"
            / "include"
            / "MegaDriveEnvironment"
            / "util"
            / "font"
            / "FontData.hpp"
        ),
        help="path to MegaDriveEnvironment's FontData.hpp",
    )
    parser.add_argument(
        "--assembler",
        help="GNU M68k assembler executable (default: <tool-prefix>as)",
    )
    parser.add_argument(
        "--tool-prefix",
        default="m68k-elf-",
        help="GNU cross-tool prefix (default: m68k-elf-)",
    )
    return parser.parse_args()


def main() -> int:
    try:
        build(parse_args())
    except (OSError, RuntimeError, subprocess.CalledProcessError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
