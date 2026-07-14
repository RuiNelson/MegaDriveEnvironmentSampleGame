#pragma once

#include "MegaDriveEnvironmentSampleGame/memory/Memory.hpp"

namespace sample::audio {

class PsgSoundEffects final {
  public:
    explicit PsgSoundEffects(memory::Memory &memory);

    void initialize();
    void update();
    void playGemCollected();
    void playGameOver();
    void playRestart();

  private:
    enum class Effect : std::uint8_t {
        None,
        GemCollected,
        GameOver,
        Restart,
    };

    void write(std::uint8_t value);
    void setTone(std::uint16_t period, std::uint8_t attenuation);
    void setNoise(std::uint8_t control, std::uint8_t attenuation);
    void silence();

    memory::Memory &memory_;
    Effect effect_ = Effect::None;
    std::uint8_t frame_ = 0;
};

} // namespace sample::audio
