#include "MegaDriveEnvironmentSampleGame/Memory.hpp"
#include "MegaDriveEnvironmentSampleGame/SampleGame.hpp"
#include "MegaDriveEnvironmentSampleGame/VdpUtils.hpp"

#include <array>
#include <cassert>
#include <cstdint>

namespace {

struct MenuMemory {
    std::uint8_t controllerHigh = 0x7F;
    std::uint8_t controllerLow = 0x73;
    std::uint8_t controllerOutput = 0x40;
    std::uint8_t mode1 = 0;
    std::uint8_t hInterruptCounter = 0;
    std::uint8_t windowHorizontal = 0;
    std::uint8_t windowVertical = 0;
    std::uint16_t lastDataValue = 0;
    unsigned windowDataWrites = 0;
    unsigned planeADataWrites = 0;
    unsigned planeBDataWrites = 0;
    std::array<std::uint16_t, sample::vdp::kPlaneWidth *
                                  sample::vdp::kPlaneHeight> windowCells{};

    std::uint8_t read8(sample::memory::Address address) {
        if (address == 0xA10003) {
            return (controllerOutput & 0x40u) != 0 ? controllerHigh : controllerLow;
        }
        return 0;
    }

    std::uint16_t read16(sample::memory::Address) {
        return 0;
    }

    std::uint32_t read32(sample::memory::Address) {
        return 0;
    }

    void write8(sample::memory::Address address, std::uint8_t value) {
        if (address == 0xA10003) {
            controllerOutput = value;
        }
    }

    void write16(sample::memory::Address address, std::uint16_t value) {
        if (address == sample::vdp::kDataPort) {
            lastDataValue = value;
            if (vramWriteActive) {
                if (vramAddress >= sample::vdp::kWindowPlane &&
                    vramAddress < sample::vdp::kWindowPlane + 0x1000) {
                    ++windowDataWrites;
                    windowCells[(vramAddress - sample::vdp::kWindowPlane) / 2] =
                        value;
                } else if (vramAddress >= sample::vdp::kPlaneA &&
                           vramAddress < sample::vdp::kPlaneA + 0x1000) {
                    ++planeADataWrites;
                } else if (vramAddress >= sample::vdp::kPlaneB &&
                           vramAddress < sample::vdp::kPlaneB + 0x1000) {
                    ++planeBDataWrites;
                }
                vramAddress = static_cast<std::uint16_t>(vramAddress + 2u);
            }
            return;
        }
        if (address != sample::vdp::kControlPort) {
            return;
        }

        if ((value & 0xE000u) == 0x8000u) {
            const auto reg = static_cast<std::uint8_t>((value >> 8) & 0x1Fu);
            const auto data = static_cast<std::uint8_t>(value);
            if (reg == 0x00) {
                mode1 = data;
            } else if (reg == 0x0A) {
                hInterruptCounter = data;
            } else if (reg == 0x11) {
                windowHorizontal = data;
            } else if (reg == 0x12) {
                windowVertical = data;
            }
            commandPending = false;
            vramWriteActive = false;
            return;
        }

        if (!commandPending) {
            commandWord1 = value;
            commandPending = true;
            return;
        }

        const auto code = static_cast<std::uint8_t>(
            ((commandWord1 >> 14) & 0x03u) | ((value >> 2) & 0x3Cu));
        if (code == 0x01) {
            vramAddress = static_cast<std::uint16_t>(
                (commandWord1 & 0x3FFFu) | ((value & 0x03u) << 14));
            vramWriteActive = true;
        } else {
            vramWriteActive = false;
        }
        commandPending = false;
    }

    void write32(sample::memory::Address address, std::uint32_t value) {
        write16(address, static_cast<std::uint16_t>(value >> 16));
        write16(address + 2, static_cast<std::uint16_t>(value));
    }

    void resetPlaneRecording() {
        windowDataWrites = 0;
        planeADataWrites = 0;
        planeBDataWrites = 0;
    }

  private:
    bool commandPending = false;
    bool vramWriteActive = false;
    std::uint16_t commandWord1 = 0;
    std::uint16_t vramAddress = 0;
};

sample::memory::Backend makeBackend(MenuMemory *memory) {
    using sample::memory::Address;
    return sample::memory::Backend{
        [](void *ctx, Address address) -> std::uint8_t {
            return static_cast<MenuMemory *>(ctx)->read8(address);
        },
        [](void *ctx, Address address) -> std::uint16_t {
            return static_cast<MenuMemory *>(ctx)->read16(address);
        },
        [](void *ctx, Address address) -> std::uint32_t {
            return static_cast<MenuMemory *>(ctx)->read32(address);
        },
        [](void *ctx, Address address, std::uint8_t value) {
            static_cast<MenuMemory *>(ctx)->write8(address, value);
        },
        [](void *ctx, Address address, std::uint16_t value) {
            static_cast<MenuMemory *>(ctx)->write16(address, value);
        },
        [](void *ctx, Address address, std::uint32_t value) {
            static_cast<MenuMemory *>(ctx)->write32(address, value);
        },
        memory,
    };
}

void runFrame(sample::SampleGame &game) {
    game.onVSync();
    assert(game.runPendingFrame());
}

} // namespace

int main() {
    MenuMemory memory;
    sample::memory::bind(makeBackend(&memory));
    sample::SampleGame game;
    game.initialize();

    // The opening notice uses the Window plane but does not yet enable the
    // selection menu's raster IRQ.
    assert(memory.windowHorizontal == 20);
    assert(memory.windowVertical == 0x00);
    assert((memory.mode1 & 0x10u) == 0);
    assert(memory.planeBDataWrites >= 40u * 14u);

    // Accept the notice. The resulting selection menu writes all letters to
    // Window (front), leaves Plane A untouched, and enables eight-line HINTs.
    memory.resetPlaneRecording();
    memory.controllerLow = 0x63; // A pressed, active-low
    runFrame(game);
    assert(memory.windowDataWrites > 0);
    assert(memory.planeADataWrites == 0);
    const auto menuTitle = memory.windowCells[8 * sample::vdp::kPlaneWidth + 12];
    assert(((menuTitle >> 13) & 3u) == 1u); // dedicated yellow palette
    assert(memory.hInterruptCounter == 7);
    assert((memory.mode1 & 0x10u) != 0);

    // VBlank restores the first sky band; HBlank advances the gradient.
    game.onVSync();
    assert(memory.lastDataValue == 0x0E86);
    game.onHSync();
    game.onHSync();
    assert(memory.lastDataValue == 0x0E88);
    for (int band = 3; band < 14; ++band) {
        game.onHSync();
    }
    assert((memory.mode1 & 0x10u) == 0); // ocean half needs no HBlank work
    assert(game.runPendingFrame());
    assert((memory.mode1 & 0x10u) != 0);

    // Release A for one frame, then select the first game. Its activation must
    // disable HINT and remove the full-screen Window mapping.
    memory.controllerLow = 0x73;
    runFrame(game);
    memory.controllerLow = 0x63;
    runFrame(game);
    assert((memory.mode1 & 0x10u) == 0);
    assert(memory.windowHorizontal == 0x00);
    assert(memory.windowVertical == 0x00);

    sample::memory::unbind();
    return 0;
}
