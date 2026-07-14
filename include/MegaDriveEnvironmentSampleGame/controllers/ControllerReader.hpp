#pragma once

#include "MegaDriveEnvironmentSampleGame/memory/Memory.hpp"

namespace sample::controllers {

enum class Player : std::uint8_t {
    One,
    Two,
};

struct ControllerState {
    bool connected = false;
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
    ControllerReader(memory::Memory &memory, Player player);

    /** Configures TH as an output and leaves it high. Call once during startup. */
    void initialize();

    /** Samples both TH phases and restores TH high before returning. */
    [[nodiscard]] ControllerState read();

  private:
    [[nodiscard]] static bool pressed(std::uint8_t value, unsigned bit);

    memory::Memory &memory_;
    memory::Address dataPort_;
    memory::Address controlPort_;
};

} // namespace sample::controllers
