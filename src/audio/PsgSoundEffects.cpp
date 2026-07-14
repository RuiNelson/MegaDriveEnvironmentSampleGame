#include "MegaDriveEnvironmentSampleGame/audio/PsgSoundEffects.hpp"

namespace sample::audio {
namespace {

constexpr memory::Address kPsgPort = 0xC00011;

} // namespace

PsgSoundEffects::PsgSoundEffects(memory::Memory &memory) : memory_(memory) {
}

void PsgSoundEffects::initialize() {
    effect_ = Effect::None;
    frame_ = 0;
    silence();
}

void PsgSoundEffects::update() {
    if (effect_ == Effect::None) {
        return;
    }

    ++frame_;
    switch (effect_) {
    case Effect::GemCollected:
        if (frame_ == 4) {
            setTone(0x070, 2);
        } else if (frame_ >= 8) {
            silence();
        }
        break;
    case Effect::GameOver:
        if (frame_ == 8) {
            setTone(0x240, 4);
        } else if (frame_ == 16) {
            setTone(0x320, 6);
        } else if (frame_ >= 28) {
            silence();
        }
        break;
    case Effect::Restart:
        if (frame_ == 4) {
            setTone(0x090, 3);
        } else if (frame_ >= 8) {
            silence();
        }
        break;
    case Effect::None:
        break;
    }
}

void PsgSoundEffects::playGemCollected() {
    effect_ = Effect::GemCollected;
    frame_ = 0;
    write(0xFF); // silence noise left by another effect
    setTone(0x0C0, 2);
}

void PsgSoundEffects::playGameOver() {
    effect_ = Effect::GameOver;
    frame_ = 0;
    setTone(0x180, 4);
    setNoise(0x06, 3); // slow white-noise burst
}

void PsgSoundEffects::playRestart() {
    effect_ = Effect::Restart;
    frame_ = 0;
    write(0xFF);
    setTone(0x120, 3);
}

void PsgSoundEffects::write(std::uint8_t value) {
    memory_.write8(kPsgPort, value);
}

void PsgSoundEffects::setTone(std::uint16_t period, std::uint8_t attenuation) {
    period = static_cast<std::uint16_t>(period & 0x03FFu);
    write(static_cast<std::uint8_t>(0x80u | (period & 0x0Fu)));
    write(static_cast<std::uint8_t>((period >> 4) & 0x3Fu));
    write(static_cast<std::uint8_t>(0x90u | (attenuation & 0x0Fu)));
}

void PsgSoundEffects::setNoise(std::uint8_t control, std::uint8_t attenuation) {
    write(static_cast<std::uint8_t>(0xE0u | (control & 0x07u)));
    write(static_cast<std::uint8_t>(0xF0u | (attenuation & 0x0Fu)));
}

void PsgSoundEffects::silence() {
    write(0x9F);
    write(0xBF);
    write(0xDF);
    write(0xFF);
    effect_ = Effect::None;
    frame_ = 0;
}

} // namespace sample::audio
