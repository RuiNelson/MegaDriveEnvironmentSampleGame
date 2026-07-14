#pragma once

/**
 * @file ControllerReader.hpp
 * Shared reader for standard three-button Mega Drive controllers.
 */

#include "MegaDriveEnvironmentSampleGame/memory/Memory.hpp"

namespace sample::controllers {

/** Physical controller port to sample. */
enum class Player : std::uint8_t {
    One,
    Two,
};

/** Logical button state captured during one complete two-phase sample. */
struct ControllerState {
    /** Whether the TH-low response has the standard controller signature. */
    bool connected = false;
    /** Direction and action fields are true while the button is held. */
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool a = false;
    bool b = false;
    bool c = false;
    bool start = false;
};

/**
 * Reads a standard three-button Mega Drive controller through memory-mapped
 * I/O. The implementation is shared by MegaDriveEnvironment and real hardware;
 * only the supplied memory::Memory backend changes.
 */
class ControllerReader {
  public:
    /**
     * Selects the data and control ports for one player.
     *
     * `memory` is retained by reference and must outlive this reader.
     */
    ControllerReader(memory::Memory &memory, Player player);

    /** Configures TH as an output and leaves it high. Call once during startup. */
    void initialize();

    /**
     * Samples both TH phases and restores TH high before returning.
     *
     * The returned booleans convert the bus's active-low bits into the natural
     * `true == pressed` convention.
     */
    [[nodiscard]] ControllerState read();

  private:
    /** Tests one active-low input bit. */
    [[nodiscard]] static bool pressed(std::uint8_t value, unsigned bit);

    /** Bus backend shared by the PC and cartridge builds. */
    memory::Memory &memory_;
    /** Selected controller's memory-mapped data and direction registers. */
    memory::Address dataPort_;
    memory::Address controlPort_;
};

} // namespace sample::controllers
