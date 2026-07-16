#pragma once

/**
 * @file GameConfig.hpp
 * Common gameplay constants intended as the first customization point.
 */

#include "MegaDriveEnvironmentSampleGame/StdCompat.hpp"

namespace sample::game::config {

inline constexpr int kScreenWidth = 320;
inline constexpr int kScreenHeight = 224;
inline constexpr int kHudHeight = 32;

inline constexpr int kPlayerSize = 16;
inline constexpr int kPlayerStartX = 40;
inline constexpr int kPlayerStartY = 104;
inline constexpr int kPlayerSpeed = 2;

inline constexpr int kGemSize = 8;
inline constexpr int kGemStartX = 240;
inline constexpr int kGemStartY = 104;
inline constexpr std::uint16_t kGemSequenceStartX = 224;
inline constexpr std::uint16_t kGemSequenceStartY = 64;
inline constexpr std::uint16_t kGemSequenceWidth = 280;
inline constexpr std::uint16_t kGemSequenceHeight = 168;
inline constexpr std::uint16_t kGemSequenceStepX = 73;
inline constexpr std::uint16_t kGemSequenceStepY = 47;
inline constexpr int kGemMarginX = 16;
inline constexpr int kGemMarginY = 8;

inline constexpr int kEnemySize = 16;
inline constexpr int kEnemyStartX = 288;
inline constexpr int kEnemyStartY = 184;
inline constexpr std::uint32_t kEnemySpeedScale = 24;
inline constexpr std::uint32_t kEnemyInitialSpeed = 8;
inline constexpr std::uint32_t kEnemySpeedIncrease = 1;

inline constexpr std::uint16_t kScoreLimit = 1000;

} // namespace sample::game::config
