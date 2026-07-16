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

/** Single spherical axis currently driven by the side-wall collisions. */
enum class RotationAxis : std::uint8_t {
    Theta,
    Phi,
};

/**
 * Owns the fixed-point simulation and software tile renderer for the demo.
 *
 * The class has no platform-specific paths. Pixels are packed into native VDP
 * tiles and sent through sample::memory on both PC and real Mega Drive hardware.
 */
class BoingBallDemo final {
  public:
    BoingBallDemo() = default;

    /** Detects 50/60 Hz and uploads the software-generated background patterns. */
    void initialize();

    /** Resets motion and installs the demo palettes, background, text and shadow. */
    void activate();

    /** Advances motion/rotation/FPS, zoom input and collision events. */
    [[nodiscard]] BounceEvents update(bool zoomIn = false, bool zoomOut = false);

    /** Updates sprites/DMA in VBlank, then rasterizes within the VDP beam budget. */
    void render();

    /** Current top-left ball position, exposed for deterministic tests. */
    [[nodiscard]] int ballX() const;
    [[nodiscard]] int ballY() const;
    [[nodiscard]] int ballSize() const;
    [[nodiscard]] RotationAxis rotationAxis() const;

    /** Last complete one-second VBlank sample and detected display refresh. */
    [[nodiscard]] std::uint8_t displayedFps() const;
    [[nodiscard]] std::uint8_t refreshRate() const;

  private:
    static constexpr int kMaximumBallSize = 128;

    /** Freezes zoom/rotation and builds a no-duplicate high-resolution coordinate map. */
    void beginSurfaceRaster();

    /** Builds one tile at the current variable-size sprite cursor. */
    void rasterizeNextBallTile();

    /** Uses the remaining display period without crossing the next VBlank. */
    void rasterizeBallUntilBudget();

    /** Reads the VDP beam position through sample::memory to protect VBlank cadence. */
    [[nodiscard]] bool videoBudgetExpired() const;

    /** Applies a new size while preserving the ball centre. */
    void setBallSize(std::uint8_t size);

    /** Creates the scaled 128x8 dithered ellipse used by four shadow sprites. */
    void uploadShadowTiles();

    /** Creates software-generated wall/floor patterns and the floor name table. */
    void uploadBackgroundTiles();

    /** Maps the software-generated repeating wall grid to visible Plane B. */
    void mapWallGrid();

    /** Replaces the numeric part of the static FPS label. */
    void renderFps();

    /** Output coordinate to unique 128x128 geometry coordinate; $FF is transparent. */
    std::uint8_t sourceCoordinate_[kMaximumBallSize];
    /** Direct palette result for every packed geometry byte used by the sphere. */
    std::uint8_t surfaceColor_[256];
    /** Non-empty tile mask from the last complete surface at occupiedBallSize_. */
    std::uint8_t occupiedTiles_[32];
    int ballXFixed_ = 0;
    int ballYFixed_ = 0;
    int velocityXFixed_ = 0;
    int velocityYFixed_ = 0;
    std::uint8_t thetaPhase_ = 0;
    std::uint8_t phiPhase_ = 0;
    std::uint8_t refreshRate_ = 60;
    std::uint8_t framesInSample_ = 0;
    std::uint8_t sampleAge_ = 0;
    std::uint8_t displayedFps_ = 0;
    std::uint8_t ballSize_ = 96;
    std::uint8_t displayedBallSize_ = 96;
    std::uint8_t rasterBallSize_ = 96;
    std::uint8_t rasterThetaPhase_ = 0;
    std::uint8_t rasterPhiPhase_ = 0;
    std::uint8_t occupiedBallSize_ = 0;
    std::uint8_t displayBank_ = 0;
    std::uint8_t rasterTileDimension_ = 12;
    std::uint8_t rasterBlockX_ = 0;
    std::uint8_t rasterBlockY_ = 0;
    std::uint8_t rasterTileX_ = 0;
    std::uint8_t rasterTileY_ = 0;
    std::uint16_t rasterTileCount_ = 144;
    std::uint16_t rasterNextTile_ = 0;
    bool fpsNeedsRender_ = true;
    bool shadowNeedsUpload_ = true;
    bool surfaceVisible_ = false;
    bool surfaceReadyForDma_ = false;
    bool reuseTransparentTiles_ = false;
    RotationAxis rotationAxis_ = RotationAxis::Theta;
};

} // namespace sample::demo
