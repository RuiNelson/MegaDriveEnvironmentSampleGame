#include "MegaDriveEnvironmentSampleGame/SampleGame.hpp"

#include <charconv>
#include <cstdio>
#include <string_view>

int main(int argc, char **argv) {
    unsigned frameLimit = 0;
    bool debug = false;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--debug") {
            debug = true;
        } else if (argument == "--frames" && index + 1 < argc) {
            const std::string_view value{argv[++index]};
            const auto result = std::from_chars(value.data(), value.data() + value.size(), frameLimit);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
                std::fprintf(stderr, "Invalid value for --frames: %.*s\n", static_cast<int>(value.size()), value.data());
                return 2;
            }
        } else if (argument == "--help") {
            std::printf("Usage: mega_drive_environment_sample_game [--debug] [--frames N]\n");
            return 0;
        } else {
            std::fprintf(stderr, "Unknown option: %.*s\n", static_cast<int>(argument.size()), argument.data());
            return 2;
        }
    }

    sample::SampleGame game{frameLimit};
    game.setDebugLog(debug);
    game.boot();
    return 0;
}

