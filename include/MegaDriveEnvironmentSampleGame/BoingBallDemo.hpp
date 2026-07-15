#pragma once

/**
 * @file BoingBallDemo.hpp
 * Allocation-free software-rasterized 3D ball demo shared by both targets.
 */

#include "MegaDriveEnvironmentSampleGame/Memory.hpp"

namespace sample::demo {

/** Collision edges emitted by one fixed-point simulation update. */
struct BounceEvents {
    bool hitFloor = false;
    bool hitWall = false;
};

/**
 * Owns the fixed-point simulation and software tile renderer for the demo.
 *
 * The class has no platform-specific paths. Pixels are packed into native VDP
 * tiles and sent through memory::Memory on both PC and real Mega Drive hardware.
 */
class BoingBallDemo final {
  public:
    /** Retains `memory` by reference; the backend must outlive the demo. */
    explicit BoingBallDemo(memory::Memory &memory);

    /** Builds the bounded per-pixel sphere lookup and detects 50/60 Hz. */
    void initialize();

    /** Resets motion and installs the demo palettes, background, text and shadow. */
    void activate();

    /** Advances motion/rotation/FPS and reports new floor/wall impacts. */
    [[nodiscard]] BounceEvents update();

    /** Rasterizes through Work RAM/DMA when needed and updates linked sprites. */
    void render();

    /** Current top-left ball position, exposed for deterministic tests. */
    [[nodiscard]] int ballX() const;
    [[nodiscard]] int ballY() const;

    /** Last complete one-second VBlank sample and detected display refresh. */
    [[nodiscard]] std::uint8_t displayedFps() const;
    [[nodiscard]] std::uint8_t refreshRate() const;

  private:
    struct SurfacePoint {
        /** Red shade in the high nibble and blue shade in the low nibble. */
        std::uint8_t colors;
        /** Approximate texture longitude in 32 angular steps. */
        std::uint8_t longitude;
        /** Approximate texture latitude in 32 angular steps. */
        std::uint8_t latitude;
    };

    // The 48x48 logical raster is expanded 2x2 into a 96x96 image: exactly
    // 30% of the 320-pixel display width, at one quarter of the 3D pixel cost.
    static constexpr int kLogicalBallSize = 48;
    static constexpr int kDisplayedBallSize = kLogicalBallSize * 2;
    static constexpr int kSurfacePointCount = kLogicalBallSize * kLogicalBallSize;

    /** Integer square root used only while building the fixed geometry lookup. */
    [[nodiscard]] static int integerSquareRoot(int value);

    /** Small division-free atan approximation used only during initialization. */
    [[nodiscard]] static std::uint8_t approximateAngle(int coordinate, int depth);

    /** Returns one pre-shaded checker pixel for the current theta/phi rotation. */
    [[nodiscard, gnu::always_inline]] inline std::uint8_t rasterizedPixel(int x, int y) const {
        const auto &point = surface_[y * kLogicalBallSize + x];
        const bool red = (((point.longitude + thetaPhase_) ^
                           (point.latitude + phiPhase_)) & 4u) == 0;
        return red ? static_cast<std::uint8_t>(point.colors >> 4)
                   : static_cast<std::uint8_t>(point.colors & 0x0Fu);
    }

    /** Builds nine expanded 4x4 sprite blocks in the reserved Work RAM buffer. */
    void rasterizeBallTiles();

    /** Creates the fixed 96x8 dithered ellipse used by three shadow sprites. */
    void uploadShadowTiles();

    /** Creates the static perspective grid and Window-plane name table. */
    void uploadFloorGrid();

    /** Replaces the numeric part of the static FPS label. */
    void renderFps();

    memory::Memory &memory_;
    // initialize() writes every entry; omitting value-initialization prevents
    // a freestanding compiler from introducing an unavailable memset call.
    SurfacePoint surface_[kSurfacePointCount];
    int ballXFixed_ = 0;
    int ballYFixed_ = 0;
    int velocityXFixed_ = 0;
    int velocityYFixed_ = 0;
    std::uint8_t thetaPhase_ = 0;
    std::uint8_t phiPhase_ = 0;
    std::uint8_t refreshRate_ = 60;
    std::uint8_t framesInSample_ = 0;
    std::uint8_t sampleAge_ = 0;
    std::uint8_t displayedFps_ = 60;
    bool fpsNeedsRender_ = true;
    bool rasterizeThisFrame_ = true;
    bool oddAnimationFrame_ = false;
    bool oddPhiFrame_ = false;
};

} // namespace sample::demo
