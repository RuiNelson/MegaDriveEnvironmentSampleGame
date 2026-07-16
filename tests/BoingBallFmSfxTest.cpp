#include "MegaDriveEnvironmentSampleGame/BoingBallFmSfx.hpp"

#include "AssetLayout.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

namespace {

class RecordingMemory final : public sample::memory::Memory {
  public:
    explicit RecordingMemory(std::vector<std::uint8_t> rom) : rom_(std::move(rom)) {
        rom_.resize(sample::assets::kRomSize, 0xFF);
        z80Ram_.fill(0xCC);
    }

    std::uint8_t read8(sample::memory::Address address) override {
        if (address >= sample::audio::BoingBallFmSfx::kZ80RamBase &&
            address < sample::audio::BoingBallFmSfx::kZ80RamBase + z80Ram_.size()) {
            return z80Ram_[address - sample::audio::BoingBallFmSfx::kZ80RamBase];
        }
        if (address < rom_.size()) {
            return rom_[static_cast<std::size_t>(address)];
        }
        return 0;
    }

    std::uint16_t read16(sample::memory::Address address) override {
        if (address == sample::audio::BoingBallFmSfx::kZ80BusRequest) {
            // Emulate an immediately acknowledged bus request.
            return busRequested_ ? 0x0000 : 0x0100;
        }
        return static_cast<std::uint16_t>((read8(address) << 8) | read8(address + 1));
    }

    std::uint32_t read32(sample::memory::Address address) override {
        return (static_cast<std::uint32_t>(read16(address)) << 16) | read16(address + 2);
    }

    void write8(sample::memory::Address address, std::uint8_t value) override {
        if (address >= sample::audio::BoingBallFmSfx::kZ80RamBase &&
            address < sample::audio::BoingBallFmSfx::kZ80RamBase + z80Ram_.size()) {
            z80Ram_[address - sample::audio::BoingBallFmSfx::kZ80RamBase] = value;
            ++z80RamWrites;
            return;
        }
        writes.push_back({address, value});
    }

    void write16(sample::memory::Address address, std::uint16_t value) override {
        if (address == sample::audio::BoingBallFmSfx::kZ80BusRequest) {
            busRequested_ = (value & 0x0100) != 0;
            controlWords.push_back({address, value});
            return;
        }
        if (address == sample::audio::BoingBallFmSfx::kZ80Reset) {
            resetHeld_ = (value & 0x0100) == 0;
            controlWords.push_back({address, value});
            return;
        }
        write8(address, static_cast<std::uint8_t>(value >> 8));
        write8(address + 1, static_cast<std::uint8_t>(value));
    }

    void write32(sample::memory::Address address, std::uint32_t value) override {
        write16(address, static_cast<std::uint16_t>(value >> 16));
        write16(address + 2, static_cast<std::uint16_t>(value));
    }

    struct WordWrite {
        sample::memory::Address address;
        std::uint16_t value;
    };

    struct ByteWrite {
        sample::memory::Address address;
        std::uint8_t value;
    };

    std::vector<std::uint8_t> rom_;
    std::array<std::uint8_t, 0x2000> z80Ram_{};
    std::vector<WordWrite> controlWords;
    std::vector<ByteWrite> writes;
    std::size_t z80RamWrites = 0;
    bool busRequested_ = true;
    bool resetHeld_ = true;
};

std::vector<std::uint8_t> makeRomWithDriverMarker() {
    std::vector<std::uint8_t> rom(sample::assets::kRomSize, 0xFF);
    const auto offset = sample::assets::kZ80BoingBallSfxOffset;
    const auto size = sample::assets::kZ80BoingBallSfxSize;
    assert(offset + size <= rom.size());
    for (unsigned index = 0; index < size; ++index) {
        rom[offset + index] = static_cast<std::uint8_t>(0xA0 + (index & 0x0F));
    }
    return rom;
}

} // namespace

int main() {
    RecordingMemory memory{makeRomWithDriverMarker()};
    sample::audio::BoingBallFmSfx sfx{memory};

    assert(!sfx.ready());
    sfx.initialize();
    assert(sfx.ready());

    // Driver bytes were copied from the cartridge image into Z80 RAM.
    assert(memory.z80RamWrites >= sample::assets::kZ80BoingBallSfxSize);
    for (unsigned index = 0; index < sample::assets::kZ80BoingBallSfxSize; ++index) {
        const auto expected = static_cast<std::uint8_t>(0xA0 + (index & 0x0F));
        assert(memory.z80Ram_[index] == expected);
    }
    assert(memory.z80Ram_[sample::audio::BoingBallFmSfx::kMailboxOffset] ==
           sample::audio::BoingBallFmSfx::kCommandIdle);

    // Accurate bus controller sequence: /BUSREQ, then /RESET released so the
    // 68K can see Z80 RAM, a reset pulse after the copy, then bus release.
    bool sawBusRequest = false;
    bool sawBusRelease = false;
    bool sawResetHold = false;
    bool sawResetRun = false;
    int firstBusRequest = -1;
    int firstResetRun = -1;
    int firstBusReleaseAfterLoad = -1;
    for (int index = 0; index < static_cast<int>(memory.controlWords.size()); ++index) {
        const auto &word = memory.controlWords[static_cast<std::size_t>(index)];
        if (word.address == sample::audio::BoingBallFmSfx::kZ80BusRequest &&
            word.value == 0x0100) {
            sawBusRequest = true;
            if (firstBusRequest < 0) {
                firstBusRequest = index;
            }
        }
        if (word.address == sample::audio::BoingBallFmSfx::kZ80BusRequest &&
            word.value == 0x0000) {
            sawBusRelease = true;
            if (firstBusReleaseAfterLoad < 0 && firstResetRun >= 0) {
                firstBusReleaseAfterLoad = index;
            }
        }
        if (word.address == sample::audio::BoingBallFmSfx::kZ80Reset && word.value == 0x0000) {
            sawResetHold = true;
        }
        if (word.address == sample::audio::BoingBallFmSfx::kZ80Reset && word.value == 0x0100) {
            sawResetRun = true;
            if (firstResetRun < 0) {
                firstResetRun = index;
            }
        }
    }
    assert(sawBusRequest);
    assert(sawBusRelease);
    assert(sawResetHold);
    assert(sawResetRun);
    assert(firstBusRequest >= 0);
    assert(firstResetRun > firstBusRequest);
    assert(firstBusReleaseAfterLoad > firstResetRun);

    memory.z80Ram_[sample::audio::BoingBallFmSfx::kMailboxOffset] = 0xEE;
    sfx.playFloorBounce();
    assert(memory.z80Ram_[sample::audio::BoingBallFmSfx::kMailboxOffset] ==
           sample::audio::BoingBallFmSfx::kCommandFloor);

    sfx.playWallBounce();
    assert(memory.z80Ram_[sample::audio::BoingBallFmSfx::kMailboxOffset] ==
           sample::audio::BoingBallFmSfx::kCommandWall);

    return 0;
}
