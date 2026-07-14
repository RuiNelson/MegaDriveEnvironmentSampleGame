# CLAUDE.md

Guidance for agents working in `MegaDriveEnvironmentSampleGame`.

## Purpose

This repository is a deliberately small dual-target game for the local
`MegaDriveEnvironment` and real Mega Drive hardware. Keep the shared game code
portable and the target-specific implementations explicit. The environment
target currently demonstrates VDP setup, controller input, game state, sprites,
and text; the real-hardware target is being added subsystem by subsystem.

## Build

The `MegaDriveEnvironment` repository must be available next to this one, or
its location must be supplied explicitly:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/mega_drive_environment_sample_game
```

```bash
cmake -S . -B build \
  -DMEGADRIVE_ENVIRONMENT_DIR=/path/to/MegaDriveEnvironment
```

## Conventions

- Use C++23.
- Include only public `MegaDriveEnvironment` headers.
- Keep hardware-facing helpers in `VdpUtils`; keep game rules in `SampleGame`.
- Keep shared platform contracts under `memory/` and target implementations
  under `platform/megadrive_environment/` or `platform/megadrive/`.
- Controller code must use `controllers/ControllerReader` and memory-mapped I/O,
  not SDL or `Controllers::getCurrentState()` directly.
- `tools/build_asset_rom.py` owns the raw asset ROM layout. Keep it headerless
  and manifest-free until the project explicitly introduces a ROM header.
- Never execute `HardwareMemory` on the host; it dereferences the real 68000
  address map directly.
- Do not commit build outputs, fetched dependencies, screenshots, or caches.
