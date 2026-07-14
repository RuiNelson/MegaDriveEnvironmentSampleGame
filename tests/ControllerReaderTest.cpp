#include "MegaDriveEnvironmentSampleGame/controllers/ControllerReader.hpp"

#include <cassert>
#include <cstdint>

namespace {

class ControllerMemory final : public sample::memory::Memory {
  public:
    std::uint8_t high = 0x7F;
    std::uint8_t low = 0x73;
    sample::memory::Address lastControlPort = 0;
    sample::memory::Address lastDataPort = 0;
    std::uint8_t dataOutput = 0x40;

    std::uint8_t read8(sample::memory::Address address) override {
        lastDataPort = address;
        return (dataOutput & 0x40) != 0 ? high : low;
    }

    std::uint16_t read16(sample::memory::Address) override {
        return 0;
    }

    std::uint32_t read32(sample::memory::Address) override {
        return 0;
    }

    void write8(sample::memory::Address address, std::uint8_t value) override {
        if (address == 0xA10009 || address == 0xA1000B) {
            lastControlPort = address;
        } else {
            lastDataPort = address;
            dataOutput = value;
        }
    }

    void write16(sample::memory::Address, std::uint16_t) override {
    }

    void write32(sample::memory::Address, std::uint32_t) override {
    }
};

} // namespace

int main() {
    ControllerMemory memory;
    sample::controllers::ControllerReader player1{memory, sample::controllers::Player::One};
    player1.initialize();
    assert(memory.lastControlPort == 0xA10009);
    assert(memory.lastDataPort == 0xA10003);

    // Active-low: press Up, Left, B, C in TH-high and A, Start in TH-low.
    memory.high = static_cast<std::uint8_t>(0x7F & ~0x01 & ~0x04 & ~0x10 & ~0x20);
    memory.low = static_cast<std::uint8_t>(0x73 & ~0x10 & ~0x20);
    const auto state = player1.read();
    assert(state.connected);
    assert(state.up && !state.down && state.left && !state.right);
    assert(state.a && state.b && state.c && state.start);
    assert(memory.dataOutput == 0x40);

    sample::controllers::ControllerReader player2{memory, sample::controllers::Player::Two};
    player2.initialize();
    assert(memory.lastControlPort == 0xA1000B);
    assert(memory.lastDataPort == 0xA10005);
}
