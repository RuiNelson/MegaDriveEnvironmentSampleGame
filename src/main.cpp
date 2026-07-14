#include "MegaDriveEnvironmentSampleGame/SampleGame.hpp"
#include "config/controls/ControlsConfigUI.hpp"

#include <charconv>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

#ifndef SAMPLE_ASSET_ROM_PATH
#define SAMPLE_ASSET_ROM_PATH "sample_game_assets.bin"
#endif

int main(int argc, char **argv) {
    unsigned frameLimit = 0;
    bool debug = false;
    bool configureControls = false;
    std::string romPath = SAMPLE_ASSET_ROM_PATH;

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
        runControlsConfig();
        return 0;
    }

    sample::SampleGame game{std::move(romPath), frameLimit};
    game.setDebugLog(debug);
    game.boot();
    return 0;
}
