/**
 * @file main-PC.cpp
 * @brief Dependency-free command-line entry point for the PC (host) executable.
 *
 * ## Role in the dual-target architecture
 *
 * This translation unit is compiled only for the PC target. It boots the same
 * shared `sample::SampleGame` used on real hardware, but drives it through
 * `MegaDriveEnvironment` instead of a freestanding 68000 IRQ vector table:
 *
 * | Concern            | PC (`main-PC.cpp`)              | Mega Drive (`main-MD.cpp`)      |
 * | ------------------ | ------------------------------- | ------------------------------- |
 * | Process entry      | `main(argc, argv)`              | `game_main()` via `header.s`    |
 * | Memory backend     | `memory::bind(SystemMemory)`    | Direct 68000 bus (`Memory-MD`)  |
 * | VBlank callback    | `EnvironmentApplication::vSync` | `game_vsync` → IRQ6             |
 * | HBlank callback    | `EnvironmentApplication::hSync` | `game_hsync` → IRQ4             |
 * | Main loop          | `run()` + `runVDPInterrupts()`  | `wait_for_interrupt()` STOP loop|
 *
 * All gameplay, input protocol, audio sequencing and VDP rendering remain in
 * the shared library; this file only parses host CLI options, constructs the
 * environment subclass and starts `boot()`.
 *
 * ## Host-only features
 *
 * - Asset ROM path override (`--rom`) for alternate packed asset images.
 * - Frame limit (`--frames`) for smoke tests and CI without a human closing the window.
 * - Debug logging toggle for `MegaDriveEnvironment`.
 * - Standalone controls configuration UI (`--config-controls`).
 * - Version / help output for packaging and scripts.
 *
 * CLI parsing intentionally uses only the standard library (`string_view`,
 * `from_chars`, `printf`/`fprintf`) so the sample does not grow a command-line
 * processing dependency (see CLAUDE.md conventions).
 */

#include "MegaDriveEnvironmentSampleGame/SampleGame.hpp"
#include "MegaDriveEnvironmentSampleGame/Memory.hpp"
#include "config/controls/ControlsConfigUI.hpp"
#include "system/MegaDriveEnvironment.hpp"

#include <charconv>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

#ifndef SAMPLE_ASSET_ROM_PATH
// CMake normally supplies an absolute generated-ROM path. This fallback keeps
// manually compiled binaries usable from a directory containing the asset ROM.
#define SAMPLE_ASSET_ROM_PATH "sample_game_assets.bin"
#endif

#ifndef SAMPLE_GAME_VERSION
// Injected by the build system for release builds; development is the default
// string when compiling this file outside CMake.
#define SAMPLE_GAME_VERSION "development"
#endif

namespace {

/**
 * @brief Host bootstrap that wires MegaDriveEnvironment IRQs to SampleGame.
 *
 * Inherits the full SDL/window/VDP host runtime from `MegaDriveEnvironment` and
 * overrides only the three hooks required by this sample:
 *
 * - `run()`     — load assets, initialize the game, pump IRQs until quit/limit;
 * - `vSync()`   — one shared game tick per emulated VBlank;
 * - `hSync()`   — batched horizontal-scroll work (line index tracked in SampleGame).
 *
 * Actual game rules, rendering and hardware protocols stay inside
 * `sample::SampleGame`; this class must not reimplement them.
 */
class EnvironmentApplication final : public MegaDriveEnvironment {
  public:
    /**
     * @brief Constructs the host application and embeds the shared game.
     *
     * Uses internal VDP timing (`VDP::InternalTimer`) and integer scaling
     * (`VDP::Integer`) so the window matches typical developer expectations
     * without fractional pixel filters.
     *
     * @param romPath    Path to the raw, headerless asset ROM (tiles, Z80, PCM).
     * @param frameLimit If non-zero, stop after that many completed VBlank ticks
     *                   (used by automated tests). Zero means run until quit.
     */
    EnvironmentApplication(std::string romPath, unsigned frameLimit)
        : MegaDriveEnvironment(VDP::InternalTimer, VDP::Integer),
          romPath_(std::move(romPath)),
          frameLimit_(frameLimit) {
        // Route all sample::memory free functions through the environment map.
        sample::memory::bind(memory());
    }

    ~EnvironmentApplication() override {
        sample::memory::unbind();
    }

  private:
    /**
     * @brief Main host loop body invoked by `MegaDriveEnvironment::boot()`.
     *
     * Sequence:
     * 1. Map the asset ROM into the emulated 68000 address space.
     * 2. Run shared one-shot initialization (controllers, audio, VDP, scene).
     * 3. Drain any VDP interrupts queued during that long init so the first
     *    `vSync`/`hSync` reflects a fresh displayed frame, not stale IRQs.
     * 4. Until the user quits or `--frames` is exhausted: dispatch pending
     *    VDP interrupts (which call `vSync`/`hSync`) and `pace()` the host
     *    to real-time video timing.
     */
    void run() override {
        loadROM(romPath_);
        game_.initialize();

        // Initialization can take several host frames while VRAM is cleared.
        // Discard those old events so the first callback represents a fresh
        // displayed frame rather than replaying initialization-time IRQs.
        VDP::Interrupt interrupt;
        while (vdp().popInterrupt(interrupt)) {
        }

        while (!shouldQuit() && !frameLimitReached_) {
            runVDPInterrupts();
            pace();
        }
    }

    /**
     * @brief Emulated VBlank hook; advances one full shared game frame.
     *
     * Called from the environment when the VDP raises a vertical interrupt.
     * Forwards into `SampleGame::onVSync()` (identical contract to hardware
     * IRQ6) and updates the optional frame-limit counter used by CI.
     */
    void vSync() override {
        game_.onVSync();
        ++frameCount_;
        frameLimitReached_ = frameLimit_ != 0 && frameCount_ >= frameLimit_;
    }

    /**
     * @brief Emulated HBlank hook for batched horizontal-scroll work.
     *
     * The environment reports the scanline that raised HINT, but the shared
     * game ignores it and advances its own first-line counter so PC and real
     * hardware stay identical (hardware HINT has no line payload at all).
     */
    void hSync(int /*scanline*/) override {
        game_.onHSync();
    }

    /** Filesystem path of the raw asset ROM passed to `loadROM`. */
    std::string romPath_;
    /**
     * Maximum VBlank callbacks before exit; `0` disables the limit.
     * Set from `--frames N` on the command line.
     */
    unsigned frameLimit_;
    /** Number of `vSync` invocations completed so far. */
    unsigned frameCount_ = 0;
    /** Sticky flag set when `frameCount_` reaches `frameLimit_`. */
    bool frameLimitReached_ = false;
    /** Shared game instance; identical class to the Mega Drive stack object. */
    sample::SampleGame game_;
};

} // namespace

/**
 * @brief Process entry point for the PC executable.
 *
 * Parses a small fixed set of flags without external CLI libraries, then either
 * runs the controls configurator or boots `EnvironmentApplication`.
 *
 * @param argc Argument count including the program name.
 * @param argv Argument vector; options documented by `--help`.
 * @return `0` on success or after help/version/config; `2` on usage errors.
 *
 * Supported options:
 * - `--config-controls` / `--configControls` — edit bindings UI, then exit.
 * - `--rom FILE` — load a different raw asset ROM instead of the build default.
 * - `--frames N` — exit after N VBlank ticks (unsigned decimal, no junk suffix).
 * - `--debug` — enable MegaDriveEnvironment debug logging.
 * - `-V` / `--version` — print version and exit.
 * - `-h` / `--help` — print usage and exit.
 */
int main(int argc, char **argv) {
    unsigned frameLimit = 0;
    bool debug = false;
    bool configureControls = false;
    std::string romPath = SAMPLE_ASSET_ROM_PATH;

    // Parsing is deliberately implemented with standard-library primitives so
    // the sample does not need a command-line processing dependency.
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--debug") {
            debug = true;
        } else if (argument == "--config-controls" || argument == "--configControls") {
            configureControls = true;
        } else if (argument == "--rom") {
            if (index + 1 >= argc) {
                std::fprintf(stderr, "Missing value for --rom\n");
                return 2;
            }
            romPath = argv[++index];
        } else if (argument == "--frames") {
            if (index + 1 >= argc) {
                std::fprintf(stderr, "Missing value for --frames\n");
                return 2;
            }
            const std::string_view value{argv[++index]};
            // from_chars neither allocates nor accepts trailing junk: the entire
            // token must convert to unsigned, or the option is rejected.
            const auto result = std::from_chars(value.data(), value.data() + value.size(), frameLimit);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
                std::fprintf(stderr, "Invalid value for --frames: %.*s\n", static_cast<int>(value.size()), value.data());
                return 2;
            }
        } else if (argument == "--version" || argument == "-V") {
            std::printf("MegaDriveEnvironmentSampleGame %s\n", SAMPLE_GAME_VERSION);
            return 0;
        } else if (argument == "--help" || argument == "-h") {
            std::printf("Usage: mega_drive_environment_sample_game [OPTIONS]\n\n"
                        "Options:\n"
                        "  --config-controls, --configControls\n"
                        "                         Configure keyboard and gamepad bindings, then exit\n"
                        "  --rom FILE             Load a different raw asset ROM\n"
                        "  --frames N             Exit after N frames (smoke tests and CI)\n"
                        "  --debug                Enable MegaDriveEnvironment debug logging\n"
                        "  -V, --version          Print the program version\n"
                        "  -h, --help             Show this help\n");
            return 0;
        } else {
            std::fprintf(stderr, "Unknown option: %.*s\n", static_cast<int>(argument.size()), argument.data());
            std::fprintf(stderr, "Try --help for usage.\n");
            return 2;
        }
    }

    if (configureControls) {
        // Configuration is a standalone UI and intentionally exits afterward;
        // it does not start the game or load the asset ROM.
        runControlsConfig();
        return 0;
    }

    EnvironmentApplication application{std::move(romPath), frameLimit};
    application.setDebugLog(debug);
    // boot() owns the SDL window lifetime and eventually calls run().
    application.boot();
    return 0;
}
