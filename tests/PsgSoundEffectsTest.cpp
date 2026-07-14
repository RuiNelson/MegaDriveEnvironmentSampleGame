#include "MegaDriveEnvironmentSampleGame/audio/PsgSoundEffects.hpp"

#include <array>
#include <cassert>

namespace {

class RecordingMemory final : public sample::memory::Memory {
  public:
    std::uint8_t read8(sample::memory::Address) override {
        return 0;
    }

    std::uint16_t read16(sample::memory::Address) override {
        return 0;
    }

    std::uint32_t read32(sample::memory::Address) override {
        return 0;
    }

    void write8(sample::memory::Address address, std::uint8_t value) override {
        assert(address == 0xC00011);
        writes[writeCount++] = value;
    }

    void write16(sample::memory::Address, std::uint16_t) override {
    }

    void write32(sample::memory::Address, std::uint32_t) override {
    }

    std::array<std::uint8_t, 64> writes{};
    std::size_t writeCount = 0;
};

} // namespace

int main() {
    RecordingMemory memory;
    sample::audio::PsgSoundEffects sounds(memory);

    sounds.initialize();
    assert(memory.writeCount == 4);
    assert(memory.writes[0] == 0x9F);
    assert(memory.writes[3] == 0xFF);

    sounds.playGemCollected();
    assert(memory.writes[4] == 0xFF);
    assert((memory.writes[5] & 0xF0) == 0x80);
    assert((memory.writes[7] & 0xF0) == 0x90);
    for (int frame = 0; frame < 8; ++frame) {
        sounds.update();
    }
    assert(memory.writes[memory.writeCount - 1] == 0xFF);

    sounds.playGameOver();
    const auto gameOverStart = memory.writeCount;
    assert((memory.writes[gameOverStart - 2] & 0xF8) == 0xE0);
    assert((memory.writes[gameOverStart - 1] & 0xF0) == 0xF0);
    return 0;
}
