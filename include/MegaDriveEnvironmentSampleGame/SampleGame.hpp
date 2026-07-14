#pragma once

#include "MegaDriveEnvironmentSampleGame/audio/PsgSoundEffects.hpp"
#include "MegaDriveEnvironmentSampleGame/controllers/ControllerReader.hpp"
#include "MegaDriveEnvironmentSampleGame/game/GameSession.hpp"
#include "MegaDriveEnvironmentSampleGame/platform/megadrive_environment/EnvironmentMemory.hpp"
#include "system/MegaDriveEnvironment.hpp"

#include <string>

namespace sample {

class SampleGame final : public MegaDriveEnvironment {
  public:
    explicit SampleGame(std::string romPath, unsigned frameLimit = 0);

  protected:
    void run() override;
    void vSync() override;

  private:
    void initializeGraphics();
    void update();
    void render();
    void waitForVBlank();

    std::string romPath_;
    unsigned frameLimit_ = 0;
    unsigned frameCount_ = 0;
    bool frameReady_ = false;

    platform::megadrive_environment::EnvironmentMemory gameMemory_;
    controllers::ControllerReader player1Controller_;
    game::GameSession session_;
    audio::PsgSoundEffects soundEffects_;
};

} // namespace sample
