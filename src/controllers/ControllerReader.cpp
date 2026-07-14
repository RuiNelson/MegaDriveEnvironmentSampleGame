#include "MegaDriveEnvironmentSampleGame/controllers/ControllerReader.hpp"

namespace sample::controllers {
namespace {

constexpr memory::Address kPlayer1DataPort = 0xA10003;
constexpr memory::Address kPlayer2DataPort = 0xA10005;
constexpr memory::Address kPlayer1ControlPort = 0xA10009;
constexpr memory::Address kPlayer2ControlPort = 0xA1000B;
constexpr std::uint8_t kThHigh = 0x40;
constexpr std::uint8_t kThLow = 0x00;

} // namespace

ControllerReader::ControllerReader(memory::Memory &memory, Player player)
    : memory_(memory),
      dataPort_(player == Player::One ? kPlayer1DataPort : kPlayer2DataPort),
      controlPort_(player == Player::One ? kPlayer1ControlPort : kPlayer2ControlPort) {
}

void ControllerReader::initialize() {
    memory_.write8(controlPort_, kThHigh); // bit 6 is an output
    memory_.write8(dataPort_, kThHigh);
}

ControllerState ControllerReader::read() {
    memory_.write8(dataPort_, kThHigh);
    const std::uint8_t high = memory_.read8(dataPort_);

    memory_.write8(dataPort_, kThLow);
    const std::uint8_t low = memory_.read8(dataPort_);

    // Keep TH high between samples, as expected by a standard controller.
    memory_.write8(dataPort_, kThHigh);

    ControllerState state;
    state.connected = (low & 0x0C) == 0; // grounded signature in the TH-low phase
    state.up = pressed(high, 0);
    state.down = pressed(high, 1);
    state.left = pressed(high, 2);
    state.right = pressed(high, 3);
    state.b = pressed(high, 4);
    state.c = pressed(high, 5);
    state.a = pressed(low, 4);
    state.start = pressed(low, 5);
    return state;
}

bool ControllerReader::pressed(std::uint8_t value, unsigned bit) {
    return (value & (1u << bit)) == 0;
}

} // namespace sample::controllers
