#!/usr/bin/env python3
"""Build and validate the bootable 32-Mbit Mega Drive ROM for real hardware."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

from asset_pack import ROM_SIZE, AssetLayout


ROM_CHECKSUM_OFFSET = 0x18E
ROM_CHECKSUM_START = 0x200


def run(command: list[str], *, cwd: Path) -> None:
    print("+", " ".join(command))
    subprocess.run(command, cwd=cwd, check=True)


def require_tool(name: str, *, hint: str | None = None) -> str:
    executable = shutil.which(name)
    if executable is None:
        message = f"required tool '{name}' was not found"
        if hint:
            message = f"{message}; {hint}"
        raise RuntimeError(message)
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


def load_layout(path: Path) -> AssetLayout:
    return AssetLayout.from_json(json.loads(path.read_text(encoding="utf-8")))


def validate_rom(rom_path: Path, asset_rom_path: Path, layout: AssetLayout) -> dict[str, int]:
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
    if not ROM_CHECKSUM_START <= reset_vector < layout.pack_offset:
        raise RuntimeError(f"invalid reset vector: 0x{reset_vector:08X}")
    if not ROM_CHECKSUM_START <= hblank_vector < layout.pack_offset:
        raise RuntimeError(f"invalid HBlank IRQ vector: 0x{hblank_vector:08X}")
    if not ROM_CHECKSUM_START <= vblank_vector < layout.pack_offset:
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
    if rom[layout.pack_offset :] != assets[layout.pack_offset :]:
        raise RuntimeError("final ROM asset pack differs from the generated asset ROM")

    return {
        "stack_pointer": stack_pointer,
        "reset_vector": reset_vector,
        "hblank_vector": hblank_vector,
        "vblank_vector": vblank_vector,
        "checksum": stored_checksum,
        "pack_offset": layout.pack_offset,
    }


def escape_assembly_path(path: Path) -> str:
    return str(path.resolve()).replace("\\", "\\\\").replace('"', '\\"')


def generate_blobs_assembly(output_path: Path, pack_binary: Path, layout: AssetLayout) -> None:
    """Emit one .assets section that mirrors the packed tail of the asset ROM."""
    lines = [
        "| Generated from the asset pack. Do not edit.\n",
        '    .section .assets,"a",@progbits\n',
        "    .balign 2\n",
    ]
    # The pack binary is the contiguous tail [pack_offset, rom_end). Symbols for
    # individual blobs are relative labels for debugging only; runtime code uses
    # the absolute offsets from AssetLayout.hpp.
    cursor = 0
    for blob in layout.blobs:
        relative = blob.offset - layout.pack_offset
        if relative < cursor:
            raise RuntimeError("asset pack blobs are not ordered by offset")
        if relative > cursor:
            lines.append(f"    .space {relative - cursor}, 0xFF\n")
            cursor = relative
        symbol = f"asset_{blob.name}"
        lines.append(f"    .globl {symbol}\n")
        lines.append(f"{symbol}:\n")
        # Include the full aligned span from the pack file so section size matches.
        lines.append(
            f'    .incbin "{escape_assembly_path(pack_binary)}",'
            f"{relative},{blob.aligned_size}\n"
        )
        lines.append(f"    .globl {symbol}_end\n")
        lines.append(f"{symbol}_end:\n")
        cursor = relative + blob.aligned_size

    if cursor != layout.pack_size:
        raise RuntimeError("asset pack assembly size does not match layout")

    output_path.write_text("".join(lines), encoding="utf-8")


def generate_rom_ld(output_path: Path, layout: AssetLayout) -> None:
    output_path.write_text(
        f"""OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)
ENTRY(reset_entry)

/* Generated by tools/build_megadrive_rom.py from asset_layout.json. */

SECTIONS
{{
    . = 0x000000;
    .vectors :
    {{
        KEEP(*(.vectors))
    }}
    ASSERT(SIZEOF(.vectors) == 0x100, "invalid Mega Drive vector table size")

    . = 0x000100;
    .rom_header :
    {{
        KEEP(*(.rom_header))
    }}
    ASSERT(SIZEOF(.rom_header) == 0x100, "invalid Mega Drive ROM header size")

    . = 0x000200;
    .startup :
    {{
        *(.startup)
    }}
    .text :
    {{
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.data.rel.ro .data.rel.ro.*)
        *(.data .data.*)
        *(.init_array .init_array.*)
    }}
    . = ALIGN(2);
    __code_end = .;
    ASSERT(__code_end <= 0x{layout.pack_offset:06X}, "game code overlaps the asset pack")

    . = 0x{layout.pack_offset:06X};
    .assets :
    {{
        KEEP(*(.assets))
    }}
    ASSERT(SIZEOF(.assets) == {layout.pack_size}, "unexpected asset pack size")
    __rom_end = .;
    ASSERT(__rom_end == 0x400000, "ROM must end at 4 MiB")

    .bss 0x00FF0000 (NOLOAD) :
    {{
        *(.bss .bss.*)
        *(COMMON)
    }}
    ASSERT(SIZEOF(.bss) == 0, "cartridge code must not require BSS initialization")

    /DISCARD/ :
    {{
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame.*)
        *(.debug .debug.*)
    }}
}}
""",
        encoding="utf-8",
    )


def generate_combined_cpp(output_path: Path, repository: Path) -> None:
    sources = (
        repository / "src" / "Memory-MD.cpp",
        repository / "src" / "BoingBallDemo.cpp",
        repository / "src" / "BoingBallFmSfx.cpp",
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
    tools_dir = repository / "tools"
    if str(tools_dir) not in sys.path:
        sys.path.insert(0, str(tools_dir))

    build_dir = args.build_dir.resolve()
    generated_dir = build_dir / "generated"
    generated_dir.mkdir(parents=True, exist_ok=True)

    asset_rom = args.asset_rom.resolve()
    output_rom = args.output.resolve()
    output_rom.parent.mkdir(parents=True, exist_ok=True)

    layout_header = args.layout_header.resolve()
    layout_json = args.layout_json.resolve()
    pack_binary = args.pack_binary.resolve()
    assets_work = args.assets_work_dir.resolve()

    cxx = require_tool(
        f"{args.tool_prefix}g++",
        hint="on macOS run 'brew install m68k-elf-gcc'",
    )
    assembler = require_tool(
        args.assembler or f"{args.tool_prefix}as",
        hint="on macOS run 'brew install m68k-elf-binutils'",
    )
    linker = require_tool(f"{args.tool_prefix}ld")
    objcopy = require_tool(f"{args.tool_prefix}objcopy")
    require_tool("z80asm", hint="on macOS run 'brew install z80asm'")

    run(
        [
            sys.executable,
            str(repository / "tools" / "build_assets.py"),
            "--output",
            str(asset_rom),
            "--font-data",
            str(args.font_data.resolve()),
            "--work-dir",
            str(assets_work),
            "--layout-header",
            str(layout_header),
            "--layout-json",
            str(layout_json),
            "--pack-binary",
            str(pack_binary),
        ],
        cwd=repository,
    )

    layout = load_layout(layout_json)
    if asset_rom.read_bytes()[layout.pack_offset :] != pack_binary.read_bytes():
        raise RuntimeError("asset ROM pack tail does not match pack binary")

    combined_cpp = generated_dir / "code.cpp"
    code_assembly = generated_dir / "code.s"
    blobs_assembly = generated_dir / "blobs.s"
    rom_ld = generated_dir / "rom.ld"
    header_object = build_dir / "header.o"
    code_object = build_dir / "code.o"
    blobs_object = build_dir / "blobs.o"
    elf_path = build_dir / "MegaDriveEnvironmentSampleGame.elf"
    map_path = build_dir / "MegaDriveEnvironmentSampleGame.map"

    generate_combined_cpp(combined_cpp, repository)
    generate_blobs_assembly(blobs_assembly, pack_binary, layout)
    generate_rom_ld(rom_ld, layout)

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
            "-I",
            str(layout_header.parent),
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
            str(rom_ld),
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
    details = validate_rom(output_rom, asset_rom, layout)
    tiles = layout.blob("tiles")
    z80 = layout.blob("z80_boing_ball_sfx")
    print(
        f"Wrote {output_rom}: {ROM_SIZE} bytes, checksum 0x{checksum:04X}, "
        f"reset 0x{details['reset_vector']:06X}, IRQ4/6 "
        f"0x{details['hblank_vector']:06X}/0x{details['vblank_vector']:06X}, "
        f"assets at 0x{details['pack_offset']:06X}-0x{ROM_SIZE - 1:06X} "
        f"(z80={z80.size}, tiles={tiles.size})"
    )
    print(f"Assembly inputs: {repository / 'megadrive' / 'header.s'}")
    print(f"                 {code_assembly}")
    print(f"                 {blobs_assembly}")


def parse_args() -> argparse.Namespace:
    repository = Path(__file__).resolve().parent.parent
    default_generated = repository / "build" / "generated"
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
        help="raw asset ROM produced by build_assets.py",
    )
    parser.add_argument(
        "--layout-header",
        type=Path,
        default=default_generated / "AssetLayout.hpp",
    )
    parser.add_argument(
        "--layout-json",
        type=Path,
        default=default_generated / "asset_layout.json",
    )
    parser.add_argument(
        "--pack-binary",
        type=Path,
        default=default_generated / "asset_pack.bin",
    )
    parser.add_argument(
        "--assets-work-dir",
        type=Path,
        default=default_generated / "assets",
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
    tools_dir = Path(__file__).resolve().parent
    if str(tools_dir) not in sys.path:
        sys.path.insert(0, str(tools_dir))
    try:
        build(parse_args())
    except (OSError, RuntimeError, subprocess.CalledProcessError, ValueError, KeyError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
