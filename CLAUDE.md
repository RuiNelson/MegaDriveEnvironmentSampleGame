# CLAUDE.md

Guidance for agents working in `MegaDriveEnvironmentSampleGame`.

## Purpose

This repository is a deliberately small example game for
`MegaDriveEnvironment`. Keep the code readable and focused on showing how a
consumer integrates the environment, configures the VDP, reads a controller,
updates game state, and renders sprites and text.

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
- Do not commit build outputs, fetched dependencies, screenshots, or caches.

