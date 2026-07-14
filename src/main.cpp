#include "MegaDriveEnvironmentSampleGame/SampleGame.hpp"

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
    std::string romPath = SAMPLE_ASSET_ROM_PATH;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--debug") {
            debug = true;
        } else if (argument == "--rom" && index + 1 < argc) {
            romPath = argv[++index];
        } else if (argument == "--frames" && index + 1 < argc) {
            const std::string_view value{argv[++index]};
            const auto result = std::from_chars(value.data(), value.data() + value.size(), frameLimit);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
                std::fprintf(stderr, "Invalid value for --frames: %.*s\n", static_cast<int>(value.size()), value.data());
                return 2;
            }
        } else if (argument == "--help") {
            std::printf(
                "Usage: mega_drive_environment_sample_game [--debug] [--frames N] [--rom FILE]\n");
            return 0;
        } else {
            std::fprintf(stderr, "Unknown option: %.*s\n", static_cast<int>(argument.size()), argument.data());
            return 2;
        }
    }

    sample::SampleGame game{std::move(romPath), frameLimit};
    game.setDebugLog(debug);
    game.boot();
    return 0;
}
