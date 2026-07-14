#include "MegaDriveEnvironmentSampleGame/GameSession.hpp"

#include <cassert>

int main() {
    // The base rate is 8/24 pixel per frame. Every acceleration adds exactly
    // one more pixel of travel over a 24-frame window.
    sample::game::Player pursuitTarget;
    sample::game::Enemy baseEnemy;
    sample::game::Enemy onceAcceleratedEnemy;
    sample::game::Enemy twiceAcceleratedEnemy;
    onceAcceleratedEnemy.increaseSpeed();
    twiceAcceleratedEnemy.increaseSpeed();
    twiceAcceleratedEnemy.increaseSpeed();
    for (int frame = 0; frame < 24; ++frame) {
        baseEnemy.chase(pursuitTarget);
        onceAcceleratedEnemy.chase(pursuitTarget);
        twiceAcceleratedEnemy.chase(pursuitTarget);
    }
    assert(baseEnemy.x() == 280);
    assert(onceAcceleratedEnemy.x() == 279);
    assert(twiceAcceleratedEnemy.x() == 278);
    twiceAcceleratedEnemy.reset();
    for (int frame = 0; frame < 24; ++frame) {
        twiceAcceleratedEnemy.chase(pursuitTarget);
    }
    assert(twiceAcceleratedEnemy.x() == 280);

    sample::game::GameSession session;
    sample::controllers::ControllerState controls;

    controls.right = true;
    (void)session.update(controls);
    assert(session.player().x() == 42);
    assert(session.player().y() == 104);

    (void)session.update(controls);
    (void)session.update(controls);
    assert(session.enemy().x() == 287);
    assert(session.enemy().y() == 183);

    bool collectedGem = false;
    for (int frame = 0; frame < 150 && !collectedGem; ++frame) {
        collectedGem = session.update(controls).collectedGem();
    }
    assert(collectedGem);
    assert(session.score() == 1);

    controls = {};
    bool gameOverStarted = false;
    for (int frame = 0; frame < 1000 && session.phase() == sample::game::Phase::Playing; ++frame) {
        gameOverStarted = session.update(controls).gameOverStarted();
    }
    assert(gameOverStarted);
    assert(session.phase() == sample::game::Phase::GameOver);

    controls.start = true;
    const auto restartEvents = session.update(controls);
    assert(restartEvents.restarted());
    assert(session.phase() == sample::game::Phase::Playing);
    assert(session.score() == 0);
    assert(session.player().x() == 40);
    assert(session.enemy().x() == 288);
    return 0;
}
