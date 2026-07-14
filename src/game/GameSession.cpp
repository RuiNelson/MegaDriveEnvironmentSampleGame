/**
 * @file GameSession.cpp
 * Deterministic, allocation-free game simulation shared by both targets.
 */

#include "MegaDriveEnvironmentSampleGame/game/GameSession.hpp"

namespace sample::game {
namespace {

constexpr int kScreenWidth = 320;
constexpr int kScreenHeight = 224;
constexpr int kHudHeight = 32;
constexpr int kPlayerSize = 16;
constexpr int kGemSize = 8;
constexpr int kEnemySize = 16;
constexpr std::uint32_t kEnemySpeedScale = 24;
constexpr std::uint32_t kEnemyInitialSpeed = 8;

} // namespace

int Entity::x() const {
    return x_;
}

int Entity::y() const {
    return y_;
}

int Entity::width() const {
    return width_;
}

int Entity::height() const {
    return height_;
}

bool Entity::overlaps(const Entity &other) const {
    // Strict inequalities mean merely touching edges is not a collision.
    return x_ < other.x_ + other.width_ && x_ + width_ > other.x_ && y_ < other.y_ + other.height_ &&
           y_ + height_ > other.y_;
}

void Entity::setPosition(int x, int y) {
    x_ = x;
    y_ = y;
}

void Entity::move(int deltaX, int deltaY) {
    x_ += deltaX;
    y_ += deltaY;
}

Player::Player() : Entity(40, 104, kPlayerSize, kPlayerSize) {
}

void Player::reset() {
    setPosition(40, 104);
}

void Player::update(const controllers::ControllerState &controls) {
    constexpr int speed = 2;
    move((controls.right ? speed : 0) - (controls.left ? speed : 0),
         (controls.down ? speed : 0) - (controls.up ? speed : 0));

    // The HUD occupies the top four tile rows and is not part of the playfield.
    int clampedX = x();
    int clampedY = y();
    if (clampedX < 0) {
        clampedX = 0;
    } else if (clampedX > kScreenWidth - width()) {
        clampedX = kScreenWidth - width();
    }
    if (clampedY < kHudHeight) {
        clampedY = kHudHeight;
    } else if (clampedY > kScreenHeight - height()) {
        clampedY = kScreenHeight - height();
    }
    setPosition(clampedX, clampedY);
}

Collectible::Collectible() : Entity(240, 104, kGemSize, kGemSize) {
}

void Collectible::reset() {
    stepX_ = 224;
    stepY_ = 64;
    setPosition(240, 104);
}

void Collectible::moveToNextPosition() {
    // The increments are coprime with their ranges, giving long deterministic
    // cycles. Incremental wrapping avoids division helpers in the ROM build.
    stepX_ = static_cast<std::uint16_t>(stepX_ + 73);
    if (stepX_ >= 280) {
        stepX_ = static_cast<std::uint16_t>(stepX_ - 280);
    }
    stepY_ = static_cast<std::uint16_t>(stepY_ + 47);
    if (stepY_ >= 168) {
        stepY_ = static_cast<std::uint16_t>(stepY_ - 168);
    }
    setPosition(16 + stepX_, kHudHeight + 8 + stepY_);
}

Enemy::Enemy() : Entity(288, 184, kEnemySize, kEnemySize) {
}

void Enemy::reset() {
    chaseProgress_ = 0;
    speed_ = kEnemyInitialSpeed;
    setPosition(288, 184);
}

void Enemy::increaseSpeed() {
    // One unit is 1/24 pixel per frame. Unlike changing an integer frame
    // interval, this makes every collected gem increase the movement rate.
    ++speed_;
}

void Enemy::chase(const Player &player) {
    chaseProgress_ += speed_;

    int stepCount = 0;
    while (chaseProgress_ >= kEnemySpeedScale) {
        chaseProgress_ -= kEnemySpeedScale;
        ++stepCount;
    }
    if (stepCount == 0) {
        return;
    }

    // Clamp each axis to the target so high accumulated speeds cannot overshoot
    // the player. Diagonal pursuit has the same rate on both axes.
    const int distanceX = player.x() - x();
    const int distanceY = player.y() - y();
    const int deltaX =
        distanceX > stepCount ? stepCount : (distanceX < -stepCount ? -stepCount : distanceX);
    const int deltaY =
        distanceY > stepCount ? stepCount : (distanceY < -stepCount ? -stepCount : distanceY);
    move(deltaX, deltaY);
}

bool Events::collectedGem() const {
    return collectedGem_;
}

bool Events::gameOverStarted() const {
    return gameOverStarted_;
}

bool Events::restarted() const {
    return restarted_;
}

GameSession::GameSession() = default;

Events GameSession::update(const controllers::ControllerState &controls) {
    Events events;
    // Reset is edge-triggered to avoid restarting every frame while held. It is
    // accepted during play as well as game over, making it a general new-round
    // command rather than only a game-over acknowledgement.
    const bool resetPressed = controls.a || controls.start;
    if (resetPressed && !resetWasPressed_) {
        reset();
        resetWasPressed_ = true;
        events.restarted_ = true;
        return events;
    }
    resetWasPressed_ = resetPressed;

    if (phase_ == Phase::GameOver) {
        return events;
    }

    // Ordering is intentional: collect after player movement, then advance the
    // enemy and test the potentially updated enemy position for game over.
    player_.update(controls);
    if (player_.overlaps(gem_)) {
        ++score_;
        if (score_ >= 1000) {
            score_ = 0;
        }
        gem_.moveToNextPosition();
        enemy_.increaseSpeed();
        events.collectedGem_ = true;
    }

    enemy_.chase(player_);
    if (player_.overlaps(enemy_)) {
        phase_ = Phase::GameOver;
        events.gameOverStarted_ = true;
    }
    return events;
}

void GameSession::reset() {
    player_.reset();
    gem_.reset();
    enemy_.reset();
    score_ = 0;
    phase_ = Phase::Playing;
}

const Player &GameSession::player() const {
    return player_;
}

const Collectible &GameSession::gem() const {
    return gem_;
}

const Enemy &GameSession::enemy() const {
    return enemy_;
}

std::uint16_t GameSession::score() const {
    return score_;
}

Phase GameSession::phase() const {
    return phase_;
}

} // namespace sample::game
