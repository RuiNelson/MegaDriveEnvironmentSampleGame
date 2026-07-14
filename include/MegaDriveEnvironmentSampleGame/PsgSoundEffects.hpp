#pragma once

/**
 * @file PsgSoundEffects.hpp
 * @brief Portable, frame-driven SN76489 sound-effect sequencer.
 */

#include "MegaDriveEnvironmentSampleGame/Memory.hpp"

namespace sample::audio {

/**
 * Plays the sample game's short effects through the Mega Drive PSG port.
 *
 * The class writes bytes to address $C00011 through memory::Memory. The same
 * calls therefore reach MegaDriveEnvironment's PSG emulation on the host and
 * the physical SN76489-compatible chip on real hardware. Call update() exactly
 * once per game frame so effect durations remain deterministic.
 */
class PsgSoundEffects final {
  public:
    /** Stores a non-owning reference to the target's memory bus. */
    explicit PsgSoundEffects(memory::Memory &memory);

    /** Silences every PSG channel and resets the active sequence. */
    void initialize();

    /** Advances the active effect by one video frame. */
    void update();

    /** Starts a short rising two-note tone, replacing any current effect. */
    void playGemCollected();

    /** Starts a descending tone with a white-noise burst. */
    void playGameOver();

    /** Starts the short two-note restart confirmation. */
    void playRestart();

  private:
    enum class Effect : std::uint8_t {
        None,
        GemCollected,
        GameOver,
        Restart,
    };

    /** Writes one SN76489 command/data byte to the memory-mapped PSG port. */
    void write(std::uint8_t value);

    /** Programs tone channel 0 with a 10-bit divider and 4-bit attenuation. */
    void setTone(std::uint16_t period, std::uint8_t attenuation);

    /** Programs noise channel 3 using the PSG's three-bit noise control word. */
    void setNoise(std::uint8_t control, std::uint8_t attenuation);

    /** Mutes all four channels and marks the sequencer idle. */
    void silence();

    /** Target bus; must outlive this object. */
    memory::Memory &memory_;
    /** Sequence whose frame transitions update() currently executes. */
    Effect effect_ = Effect::None;
    /** Zero-based elapsed-frame counter for effect_. */
    std::uint8_t frame_ = 0;
};

} // namespace sample::audio
