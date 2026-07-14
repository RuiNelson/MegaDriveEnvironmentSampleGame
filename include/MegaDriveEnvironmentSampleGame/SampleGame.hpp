#pragma once

#include "system/MegaDriveEnvironment.hpp"

namespace sample {

class SampleGame final : public MegaDriveEnvironment {
  public:
    explicit SampleGame(unsigned frameLimit = 0);

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

    unsigned frameLimit_ = 0;
    unsigned frameCount_ = 0;
    bool frameReady_ = false;
    bool resetWasPressed_ = false;

    int playerX_ = 40;
    int playerY_ = 104;
    int gemX_ = 240;
    int gemY_ = 104;
    unsigned score_ = 0;
};

} // namespace sample

