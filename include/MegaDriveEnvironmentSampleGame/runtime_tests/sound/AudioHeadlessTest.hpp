#pragma once

// Host-only diagnostics owned by MegaDriveEnvironmentSampleGame.

#include <string>

struct AudioHeadlessOptions {
    std::string wavPath;
};

int runAudioHeadlessTest(const AudioHeadlessOptions &options);
