#include "MegaDriveEnvironmentSampleGame/BoingBallDemo.hpp"
#include "MegaDriveEnvironmentSampleGame/VdpUtils.hpp"

#include <cassert>
#include <cstddef>

namespace {

class RecordingMemory final : public sample::memory::Memory {
  public:
    explicit RecordingMemory(bool pal = false) : pal_(pal) {
    }

    std::uint8_t read8(sample::memory::Address address) override {
        return address == 0xA10001 && pal_ ? 0x40 : 0;
    }
    std::uint16_t read16(sample::memory::Address) override {
        return 0;
    }
    std::uint32_t read32(sample::memory::Address) override {
        return 0;
    }

    void write8(sample::memory::Address, std::uint8_t) override {
    }
    void write16(sample::memory::Address address, std::uint16_t value) override {
        if (address >= kBufferStart && address < kBufferEnd) {
            ++bufferWordWrites;
            if (address < minimumBufferAddress) {
                minimumBufferAddress = address;
            }
            if (address > maximumBufferAddress) {
                maximumBufferAddress = address;
            }
        }
        if (address == sample::vdp::kControlPort && value == 0x5000) {
            sawBallDmaDestination = true;
        }
        if (address == sample::vdp::kControlPort && value == 0x0080) {
            sawDmaCommand = true;
        }
    }
    void write32(sample::memory::Address, std::uint32_t) override {
    }

    void resetRecording() {
        bufferWordWrites = 0;
        minimumBufferAddress = 0xFFFFFFFF;
        maximumBufferAddress = 0;
        sawBallDmaDestination = false;
        sawDmaCommand = false;
    }

    static constexpr sample::memory::Address kBufferStart = 0xFF1000;
    static constexpr sample::memory::Address kBufferEnd = 0xFF2200;
    bool pal_;
    std::size_t bufferWordWrites = 0;
    sample::memory::Address minimumBufferAddress = 0xFFFFFFFF;
    sample::memory::Address maximumBufferAddress = 0;
    bool sawBallDmaDestination = false;
    bool sawDmaCommand = false;
};

} // namespace

int main() {
    RecordingMemory ntscMemory;
    sample::demo::BoingBallDemo demo{ntscMemory};
    demo.initialize();
    assert(demo.refreshRate() == 60);
    assert(demo.displayedFps() == 30);

    demo.activate();
    assert(demo.ballX() == 8);
    assert(demo.ballY() == 80);

    ntscMemory.resetRecording();
    demo.render();
    // 144 tiles x 16 words exactly fill $FF1000-$FF21FF before one DMA.
    assert(ntscMemory.bufferWordWrites == 144 * 16);
    assert(ntscMemory.minimumBufferAddress == RecordingMemory::kBufferStart);
    assert(ntscMemory.maximumBufferAddress == RecordingMemory::kBufferEnd - 2);
    assert(ntscMemory.sawBallDmaDestination);
    assert(ntscMemory.sawDmaCommand);

    // Tile persistence halves the raster cost: motion renders on the first
    // update without rebuilding tiles, then the second update rebuilds them.
    (void)demo.update();
    ntscMemory.resetRecording();
    demo.render();
    assert(ntscMemory.bufferWordWrites == 0);
    (void)demo.update();
    demo.render();
    assert(ntscMemory.bufferWordWrites == 144 * 16);

    // A 60-VBlank NTSC sample contains thirty software-rasterized images.
    demo.activate();
    for (int frame = 0; frame < 60; ++frame) {
        (void)demo.update();
    }
    assert(demo.displayedFps() == 30);

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
    assert(demo.displayedFps() == 30);

    RecordingMemory palMemory{true};
    sample::demo::BoingBallDemo palDemo{palMemory};
    palDemo.initialize();
    palDemo.activate();
    assert(palDemo.refreshRate() == 50);
    for (int frame = 0; frame < 50; ++frame) {
        (void)palDemo.update();
    }
    assert(palDemo.displayedFps() == 25);
    return 0;
}
