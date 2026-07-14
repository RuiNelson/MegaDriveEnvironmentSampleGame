#pragma once

#include "MegaDriveEnvironmentSampleGame/controllers/ControllerReader.hpp"
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
    void moveGem();
    void reset();

    std::string romPath_;
    unsigned frameLimit_ = 0;
    unsigned frameCount_ = 0;
    bool frameReady_ = false;
    bool resetWasPressed_ = false;

    platform::megadrive_environment::EnvironmentMemory gameMemory_;
    controllers::ControllerReader player1Controller_;

    int playerX_ = 40;
    int playerY_ = 104;
    int gemX_ = 240;
    int gemY_ = 104;
    unsigned score_ = 0;
};

} // namespace sample
