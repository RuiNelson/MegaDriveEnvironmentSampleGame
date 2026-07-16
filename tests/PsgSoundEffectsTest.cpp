#include "MegaDriveEnvironmentSampleGame/PsgSoundEffects.hpp"

#include <array>
#include <cassert>

namespace {

struct RecordingMemory {
    std::uint8_t read8(sample::memory::Address) {
        return 0;
    }

    std::uint16_t read16(sample::memory::Address) {
        return 0;
    }

    std::uint32_t read32(sample::memory::Address) {
        return 0;
    }

    void write8(sample::memory::Address address, std::uint8_t value) {
        assert(address == 0xC00011);
        writes[writeCount++] = value;
    }

    void write16(sample::memory::Address, std::uint16_t) {
    }

    void write32(sample::memory::Address, std::uint32_t) {
    }

    std::array<std::uint8_t, 128> writes{};
    std::size_t writeCount = 0;
};

template <typename T>
sample::memory::Backend makeBackend(T *self) {
    using sample::memory::Address;
    return sample::memory::Backend{
        [](void *ctx, Address address) -> std::uint8_t {
            return static_cast<T *>(ctx)->read8(address);
        },
        [](void *ctx, Address address) -> std::uint16_t {
            return static_cast<T *>(ctx)->read16(address);
        },
        [](void *ctx, Address address) -> std::uint32_t {
            return static_cast<T *>(ctx)->read32(address);
        },
        [](void *ctx, Address address, std::uint8_t value) {
            static_cast<T *>(ctx)->write8(address, value);
        },
        [](void *ctx, Address address, std::uint16_t value) {
            static_cast<T *>(ctx)->write16(address, value);
        },
        [](void *ctx, Address address, std::uint32_t value) {
            static_cast<T *>(ctx)->write32(address, value);
        },
        self,
    };
}

} // namespace

int main() {
    RecordingMemory memory;
    sample::memory::bind(makeBackend(&memory));
    sample::audio::PsgSoundEffects sounds;

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

    sounds.playBallFloorBounce();
    const auto floorStart = memory.writeCount;
    assert((memory.writes[floorStart - 2] & 0xF8) == 0xE0);
    assert((memory.writes[floorStart - 1] & 0xF0) == 0xF0);
    for (int frame = 0; frame < 6; ++frame) {
        sounds.update();
    }
    assert(memory.writes[memory.writeCount - 1] == 0xFF);

    sounds.playBallWallBounce();
    const auto wallStart = memory.writeCount;
    assert(memory.writes[wallStart - 4] == 0xFF);
    assert((memory.writes[wallStart - 3] & 0xF0) == 0x80);
    assert((memory.writes[wallStart - 1] & 0xF0) == 0x90);
    sample::memory::unbind();
    return 0;
}
