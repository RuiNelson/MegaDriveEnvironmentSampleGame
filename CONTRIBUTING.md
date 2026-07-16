# Contributing

Thanks for improving the sample. Changes should keep it easy to read while
preserving the same shared game path on PC and Mega Drive.

## Set up

Clone `MegaDriveEnvironment` beside this repository, install the dependencies
in [Building and running](docs/BUILDING.md), then run:

```bash
./check.sh
```

That command builds the PC target and runs all C++ and Python tests.

## Change guidelines

- Use C++23 and keep shared code allocation-free.
- Keep public headers flat under `include/MegaDriveEnvironmentSampleGame/` and
  implementations flat under `src/`.
- Name target-only sources with `-PC` or `-MD`; leave shared sources unsuffixed.
- Add every shared implementation to `config/shared_sources.json`.
- Route runtime hardware access through `sample::memory` free functions.
- Keep controller logic independent of SDL and host APIs.
- Keep command-line parsing dependency-free in `src/main-PC.cpp`.
- Use generated asset constants instead of hard-coded packed ROM offsets.
- Add deterministic tests for new game rules and hardware command sequences.
- Do not commit build outputs, caches, local controls, screenshots or generated
  ROM layout files.

Keep commits focused. Avoid reformatting unrelated files, especially the large
software renderer, in a behavioral change.

## Before submitting

Run:

```bash
./check.sh
git diff --check
```

If the cross toolchain is installed, also run `./build_megadrive.sh`. Mention
when that validation was unavailable.

For asset changes, inspect generated sizes and bank boundaries in the build
summary. For Work RAM changes, update [Memory model](docs/MEMORY_MODEL.md) and
the relevant bounds tests.

## Where changes belong

- gameplay model and rules: `GameSession`;
- game-wide orchestration and screens: `SampleGame`;
- reusable VDP commands: `VdpUtils`;
- controller bus protocol: `ControllerReader`;
- PCM/Z80/FM or PSG behavior: the matching audio class and `sound/` sources;
- host bootstrap/CLI: `main-PC.cpp`;
- hardware startup/IRQs: `main-MD.cpp` and `megadrive/header.s`;
- asset packing and validation: `tools/` plus regression tests.

When unsure, prefer shared code with a narrow interface over a second target-
specific implementation.
