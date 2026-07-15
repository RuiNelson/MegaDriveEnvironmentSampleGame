/**
 * @file main-PC.cpp
 * Dependency-free command-line entry point for the PC executable.
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

namespace {

/** Host bootstrap; all actual game code remains in sample::SampleGame. */
class EnvironmentApplication final : public MegaDriveEnvironment {
  public:
    EnvironmentApplication(std::string romPath, unsigned frameLimit)
        : MegaDriveEnvironment(VDP::InternalTimer, VDP::Integer),
          romPath_(std::move(romPath)),
          frameLimit_(frameLimit),
          gameMemory_(memory()),
          game_(gameMemory_) {
    }

  private:
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

    void vSync() override {
        game_.onVSync();
        ++frameCount_;
        frameLimitReached_ = frameLimit_ != 0 && frameCount_ >= frameLimit_;
    }

    void hSync(int scanline) override {
        game_.onHSync(scanline);
    }

    std::string romPath_;
    unsigned frameLimit_;
    unsigned frameCount_ = 0;
    bool frameLimitReached_ = false;
    sample::platform::PlatformMemory gameMemory_;
    sample::SampleGame game_;
};

} // namespace

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
            // from_chars neither allocates nor accepts trailing junk.
            const auto result = std::from_chars(value.data(), value.data() + value.size(), frameLimit);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
                std::fprintf(stderr, "Invalid value for --frames: %.*s\n", static_cast<int>(value.size()), value.data());
                return 2;
            }
        } else if (argument == "--version" || argument == "-V") {
            std::printf("MegaDriveEnvironmentSampleGame 0.1.0\n");
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
        // Configuration is a standalone UI and intentionally exits afterward.
        runControlsConfig();
        return 0;
    }

    EnvironmentApplication application{std::move(romPath), frameLimit};
    application.setDebugLog(debug);
    application.boot();
    return 0;
}
