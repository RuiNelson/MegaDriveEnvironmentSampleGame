#include "MegaDriveEnvironmentSampleGame/game/GameSession.hpp"

#include <cassert>

int main() {
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
