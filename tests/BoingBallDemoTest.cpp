#include "MegaDriveEnvironmentSampleGame/BoingBallDemo.hpp"
#include "MegaDriveEnvironmentSampleGame/VdpUtils.hpp"

#include <cassert>
#include <cstddef>

namespace {

struct RecordingMemory {
    explicit RecordingMemory(bool pal = false) : pal_(pal) {}

    std::uint8_t read8(sample::memory::Address address) {
        return address == 0xA10001 && pal_ ? 0x40 : 0;
    }
    std::uint16_t read16(sample::memory::Address address) {
        return address == 0xC00008 ? static_cast<std::uint16_t>(verticalCounter << 8) : 0;
    }
    std::uint32_t read32(sample::memory::Address) {
        return 0;
    }

    void write8(sample::memory::Address, std::uint8_t) {
    }
    void write16(sample::memory::Address address, std::uint16_t value) {
        if (address >= kBufferStart && address < kBufferEnd) {
            ++bufferWordWrites;
            for (int shift = 0; shift < 16; shift += 4) {
                sawRasterIndex[(value >> shift) & 0x0F] = true;
            }
            if (address < minimumBufferAddress) {
                minimumBufferAddress = address;
            }
            if (address > maximumBufferAddress) {
                maximumBufferAddress = address;
            }
        }
        if (address == sample::vdp::kDataPort) {
            if (vramWriteActive) {
                const auto floorStart = static_cast<std::uint16_t>(
                    sample::vdp::kWindowPlane +
                    20 * sample::vdp::kPlaneWidth * 2);
                const auto floorEnd = static_cast<std::uint16_t>(
                    sample::vdp::kWindowPlane +
                    28 * sample::vdp::kPlaneWidth * 2);
                if (vramAddress >= floorStart && vramAddress < floorEnd &&
                    value != 0) {
                    ++floorNameTableWrites;
                }
                vramAddress = static_cast<std::uint16_t>(vramAddress + 2);
            }
            return;
        }
        if (address != sample::vdp::kControlPort) {
            return;
        }
        if ((value & 0xFFFCu) == 0x0080u) {
            sawDmaCommand = true;
            const auto dmaWords = static_cast<std::uint16_t>(
                dmaLengthLow | (static_cast<std::uint16_t>(dmaLengthHigh) << 8));
            if (dmaWords > maximumDmaWordCount) {
                maximumDmaWordCount = dmaWords;
            }
        }
        if ((value & 0xE000u) == 0x8000u) {
            const auto reg = static_cast<std::uint8_t>((value >> 8) & 0x1Fu);
            if (reg == 0x00) {
                mode1Register = static_cast<std::uint8_t>(value);
            } else if (reg == 0x01) {
                mode2Register = static_cast<std::uint8_t>(value);
                sawDisplayDisabled |= mode2Register == 0x34;
            } else if (reg == 0x13) {
                dmaLengthLow = static_cast<std::uint8_t>(value);
            } else if (reg == 0x14) {
                dmaLengthHigh = static_cast<std::uint8_t>(value);
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
        vramAddress = static_cast<std::uint16_t>(
            (commandWord1 & 0x3FFFu) | ((value & 0x03u) << 14));
        vramWriteActive = code == 0x01;
        commandPending = false;
    }
    void write32(sample::memory::Address address, std::uint32_t value) {
        write16(address, static_cast<std::uint16_t>(value >> 16));
        write16(address + 2, static_cast<std::uint16_t>(value));
    }

    void resetRecording() {
        bufferWordWrites = 0;
        minimumBufferAddress = 0xFFFFFFFF;
        maximumBufferAddress = 0;
        sawDmaCommand = false;
        maximumDmaWordCount = 0;
        floorNameTableWrites = 0;
        commandPending = false;
        vramWriteActive = false;
        for (auto &seen : sawRasterIndex) {
            seen = false;
        }
    }

    static constexpr sample::memory::Address kBufferStart = 0xFF1000;
    static constexpr sample::memory::Address kBufferEnd = 0xFF3000;
    bool pal_;
    std::uint8_t verticalCounter = 0;
    std::size_t bufferWordWrites = 0;
    sample::memory::Address minimumBufferAddress = 0xFFFFFFFF;
    sample::memory::Address maximumBufferAddress = 0;
    bool sawDmaCommand = false;
    std::uint8_t dmaLengthLow = 0;
    std::uint8_t dmaLengthHigh = 0;
    std::uint8_t mode1Register = 0;
    std::uint8_t mode2Register = 0;
    bool sawDisplayDisabled = false;
    std::uint16_t maximumDmaWordCount = 0;
    std::size_t floorNameTableWrites = 0;
    bool sawRasterIndex[16]{};
    bool commandPending = false;
    bool vramWriteActive = false;
    std::uint16_t commandWord1 = 0;
    std::uint16_t vramAddress = 0;
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
    RecordingMemory ntscMemory;
    sample::memory::bind(makeBackend(&ntscMemory));
    sample::vdp::initialize();
    assert((ntscMemory.mode1Register & 0x10u) == 0); // HBlank IRQ disabled
    sample::demo::BoingBallDemo demo;
    demo.initialize();
    assert(demo.refreshRate() == 60);
    assert(demo.displayedFps() == 0);
    // Nine wall tiles and 320 perspective-floor tiles are generated in
    // software before being uploaded; no pre-authored graphics are sampled.
    assert(ntscMemory.bufferWordWrites == (9 + 320) * 16);

    // Reproduce the menu owning and clearing the Window name table. Activation
    // must rebuild all 320 cells of the lower perspective floor.
    sample::vdp::fillPlaneArea(sample::vdp::kWindowPlane, 0, 0, 40, 28,
                               sample::vdp::tileDescriptor(0));
    ntscMemory.resetRecording();
    demo.activate();
    assert(ntscMemory.floorNameTableWrites == 8u * 40u);
    assert(ntscMemory.sawDisplayDisabled);
    assert(ntscMemory.mode2Register == 0x74);
    assert(demo.ballX() == 8);
    assert(demo.ballY() == 80);
    assert(demo.ballSize() == 96);
    assert(demo.rotationAxis() == sample::demo::RotationAxis::Theta);

    // The first (right) wall switches to phi-only rotation; returning to the
    // left wall switches back to theta-only rotation.
    sample::demo::BounceEvents rightWallEvents;
    for (int frame = 0; frame < 300 && !rightWallEvents.hitWall; ++frame) {
        rightWallEvents = demo.update();
    }
    assert(rightWallEvents.hitWall);
    assert(demo.rotationAxis() == sample::demo::RotationAxis::Phi);
    sample::demo::BounceEvents leftWallEvents;
    for (int frame = 0; frame < 300 && !leftWallEvents.hitWall; ++frame) {
        leftWallEvents = demo.update();
    }
    assert(leftWallEvents.hitWall);
    assert(demo.rotationAxis() == sample::demo::RotationAxis::Theta);
    demo.activate();

    ntscMemory.resetRecording();
    demo.render();
    // The default 96-pixel ball generates exactly 12x12 tiles: no fixed
    // 128-pixel transparent canvas is rasterized.
    assert(ntscMemory.bufferWordWrites == 144 * 16);
    assert(ntscMemory.minimumBufferAddress == RecordingMemory::kBufferStart);
    assert(ntscMemory.maximumBufferAddress == RecordingMemory::kBufferStart + 144 * 32 - 2);
    assert(!ntscMemory.sawDmaCommand);
    assert(ntscMemory.sawRasterIndex[1] || ntscMemory.sawRasterIndex[2] ||
           ntscMemory.sawRasterIndex[3]);
    assert(ntscMemory.sawRasterIndex[4] || ntscMemory.sawRasterIndex[5] ||
           ntscMemory.sawRasterIndex[6]);
    assert(!ntscMemory.sawRasterIndex[8] && !ntscMemory.sawRasterIndex[9] &&
           !ntscMemory.sawRasterIndex[10]);

    // The completed Work RAM surface is DMA'd on the next VBlank, then a new
    // surface is built using whatever beam-time budget remains.
    ntscMemory.resetRecording();
    demo.render();
    assert(ntscMemory.sawDmaCommand);
    assert(ntscMemory.maximumDmaWordCount == 144 * 16);
    // Fully transparent corner tiles remain zero across rotations and are
    // skipped after the occupancy mask has been learned from the first surface.
    assert(ntscMemory.bufferWordWrites < 144 * 16);
    assert(ntscMemory.bufferWordWrites > 0);

    // Zoom changes continuously by one pixel per held-button VBlank.
    demo.activate();
    for (int frame = 0; frame < 32; ++frame) {
        (void)demo.update(true, false);
    }
    assert(demo.ballSize() == 128);
    for (int frame = 0; frame < 120; ++frame) {
        (void)demo.update(false, true);
    }
    assert(demo.ballSize() == 8);

    // A maximum-size 8192-byte surface is split across VBlanks. No individual
    // DMA may exceed the 5120-byte NTSC H40 budget reserved by the demo.
    demo.activate();
    for (int frame = 0; frame < 32; ++frame) {
        (void)demo.update(true, false);
    }
    demo.render(); // finish the frozen 96-pixel surface
    demo.render(); // upload it and rasterize the requested 128-pixel surface
    ntscMemory.resetRecording();
    demo.render();
    assert(ntscMemory.sawDmaCommand);
    assert(ntscMemory.maximumDmaWordCount == 160 * 16);
    ntscMemory.resetRecording();
    demo.render();
    assert(ntscMemory.sawDmaCommand);
    assert(ntscMemory.maximumDmaWordCount == 96 * 16);

    // The 8-pixel display size uses exactly one tile. The first render finishes
    // the already-frozen 96-pixel surface; the second starts the new size.
    demo.activate();
    for (int frame = 0; frame < 120; ++frame) {
        (void)demo.update(false, true);
    }
    ntscMemory.resetRecording();
    demo.render();
    ntscMemory.resetRecording();
    demo.render();
    assert(ntscMemory.bufferWordWrites == 16);

    // With an unconstrained mock beam counter, PC-like execution completes a
    // surface per callback after the one-frame pipeline fill.
    demo.activate();
    for (int frame = 0; frame < 60; ++frame) {
        (void)demo.update();
        demo.render();
    }
    assert(demo.displayedFps() == 59);

    int floorHits = 0;
    int wallHits = 0;
    for (int frame = 0; frame < 2000; ++frame) {
        const auto events = demo.update();
        floorHits += events.hitFloor ? 1 : 0;
        wallHits += events.hitWall ? 1 : 0;
        assert(demo.ballX() >= 8);
        assert(demo.ballX() <= 216);
        assert(demo.ballY() <= 112);
    }
    assert(floorHits > 0);
    assert(wallHits > 0);
    assert(demo.displayedFps() == 59);

    // A real-MD-like beam deadline yields after one bounded tile;
    // clearing the deadline lets the exact same renderer resume next VBlank.
    sample::memory::unbind();
    RecordingMemory budgetMemory;
    sample::memory::bind(makeBackend(&budgetMemory));
    sample::demo::BoingBallDemo budgetDemo;
    budgetDemo.initialize();
    budgetDemo.activate();
    budgetMemory.resetRecording();
    budgetMemory.verticalCounter = 192;
    budgetDemo.render();
    assert(budgetMemory.bufferWordWrites == 16);
    assert(!budgetMemory.sawDmaCommand);
    budgetMemory.resetRecording();
    budgetMemory.verticalCounter = 0;
    budgetDemo.render();
    assert(budgetMemory.bufferWordWrites == (144 - 1) * 16);
    budgetMemory.resetRecording();
    budgetDemo.render();
    assert(budgetMemory.sawDmaCommand);

    sample::memory::unbind();
    RecordingMemory palMemory{true};
    sample::memory::bind(makeBackend(&palMemory));
    sample::demo::BoingBallDemo palDemo;
    palDemo.initialize();
    palDemo.activate();
    assert(palDemo.refreshRate() == 50);
    for (int frame = 0; frame < 50; ++frame) {
        (void)palDemo.update();
        palDemo.render();
    }
    assert(palDemo.displayedFps() == 49);
    sample::memory::unbind();
    return 0;
}
