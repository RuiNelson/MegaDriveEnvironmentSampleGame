# Building and running

## Requirements

The PC build needs:

- CMake 3.24 or newer;
- a C++23 compiler;
- Python 3;
- SDL3;
- `z80asm`;
- `MegaDriveEnvironment` beside this repository, or an explicit path to it.

The first clean configuration may download dependencies owned by
`MegaDriveEnvironment`.

## PC workflow

The repository entry points work from any current directory:

```bash
./build_pc.sh
./run_pc.sh
./configure_controls.sh
./check.sh
```

`build_pc.sh` configures CMake, builds the asset ROM and compiles the executable.
The asset pipeline reads the checked-in PCM file and writes only below the build
directory; a normal build does not modify the source tree.

Environment variables customize paths and configuration:

```bash
BUILD_TYPE=Release ./build_pc.sh
BUILD_DIR=/tmp/sample-build ./build_pc.sh
MEGADRIVE_ENVIRONMENT_DIR=/path/to/MegaDriveEnvironment ./build_pc.sh
PYTHON3=/path/to/python3 ./build_pc.sh
```

Relative `BUILD_DIR` and `MEGADRIVE_ENVIRONMENT_DIR` values are resolved from
the repository root consistently by the build and run scripts.

The executable accepts:

```text
--config-controls       Configure bindings and exit
--rom FILE              Load another raw asset ROM
--frames N              Exit after N VBlank frames
--debug                 Enable environment logging
--version               Print the project version
--help                  Show all options
```

## CMake presets

Presets are convenient for IDEs and manual development:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

The `release` configure/build preset writes to `build/release`. The `dev`
preset writes to `build/dev` and enables tests. VS Code is configured to use
the development preset.

Direct CMake remains supported:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DMEGADRIVE_ENVIRONMENT_DIR=/path/to/MegaDriveEnvironment
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Set `-DSAMPLE_ENABLE_WARNINGS=OFF` only when compiler diagnostics from a local
toolchain make the development build impractical.

## Real-hardware ROM

Install the GNU M68k compiler and binutils. On macOS:

```bash
brew install m68k-elf-binutils m68k-elf-gcc z80asm
```

Then run:

```bash
./build_megadrive.sh
```

The output is `build/megadrive/MegaDriveEnvironmentSampleGame.bin`, an exact
4 MiB image with a vector table, Sega header, startup, shared game code,
checksum and packed assets.

Tool and path overrides are available:

```bash
PYTHON3=/path/to/python3 ./build_megadrive.sh \
  --output /tmp/sample-game.bin \
  --assembler /path/to/m68k-elf-as \
  --tool-prefix /path/to/toolchain/bin/m68k-elf- \
  --font-data /path/to/FontData.hpp
```

`tools/build_megadrive_rom.py` validates image size, header, vectors, checksum,
code/asset overlap and the byte-for-byte packed payload. It retains generated
assembly, ELF, map and layout charts below `build/megadrive/`.

## Emulator configuration

On macOS, `run_emulator.sh` auto-detects `/Applications/Genesis Plus.app`.
Override the app or use any executable emulator:

```bash
EMULATOR_APP="/Applications/Another Emulator.app" ./run_emulator.sh
EMULATOR=/usr/local/bin/blastem ./run_emulator.sh
ROM_PATH=/tmp/sample-game.bin EMULATOR=/path/to/emulator ./run_emulator.sh
```

`emulator.sh` always rebuilds the ROM before forwarding to `run_emulator.sh`.
Use `run_emulator.sh` directly when an existing ROM can be reused.

## Troubleshooting

- **`MegaDriveEnvironment was not found`:** clone both repositories side by
  side or set `MEGADRIVE_ENVIRONMENT_DIR`.
- **`required tool 'z80asm' was not found`:** install `z80asm`; both targets
  assemble the Z80 audio driver.
- **SDL or fetched dependency errors:** build `MegaDriveEnvironment` itself
  first to confirm its host dependencies, then retry this project with a clean
  build directory.
- **PC executable is not found:** use the same `BUILD_DIR` for `build_pc.sh`,
  `run_pc.sh` and `configure_controls.sh`.
