#include "core/GameWorld.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {

constexpr float kTolerance = 0.01F;

void _require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(message.data());
    }
}

arena::WorldConfig _testConfig() {
    arena::WorldConfig config;
    config.width = 400.0F;
    config.height = 300.0F;
    config.playerSpeed = 100.0F;
    config.enemySpeed = 0.0F;
    config.initialEnemies = 0;
    config.maximumEnemies = 16;
    config.automaticSpawning = false;
    config.enemyHealth = 80;
    return config;
}

bool _containsEvent(const arena::GameWorld& world, arena::GameEvent event) {
    return std::ranges::find(world.events(), event) != world.events().end();
}

void _diagonalMovementIsNormalized() {
    arena::GameWorld straightWorld(_testConfig());
    arena::GameWorld diagonalWorld(_testConfig());

    straightWorld.update({{1.0F, 0.0F}, false, false}, 0.1F);
    diagonalWorld.update({{1.0F, 1.0F}, false, false}, 0.1F);

    const arena::Vec2 center{200.0F, 150.0F};
    const float straightDistance = std::sqrt(arena::lengthSquared(
        straightWorld.player().position - center));
    const float diagonalDistance = std::sqrt(arena::lengthSquared(
        diagonalWorld.player().position - center));
    _require(std::abs(straightDistance - diagonalDistance) < kTolerance,
             "Diagonal movement must not be faster than cardinal movement.");
}

void _playerCannotLeaveArena() {
    arena::GameWorld world(_testConfig());
    for (int step = 0; step < 30; ++step) {
        world.update({{-1.0F, -1.0F}, false, false}, 0.1F);
    }

    _require(world.player().position.x >= world.player().radius,
             "Player crossed the left boundary.");
    _require(world.player().position.y >= world.player().radius,
             "Player crossed the top boundary.");
}

void _attackHasCooldownAndAwardsScore() {
    arena::GameWorld world(_testConfig());
    world.spawnEnemy({235.0F, 150.0F});

    world.update({{}, true, false}, 0.01F);
    _require(world.enemies().size() == 1 && world.enemies().front().health == 35,
             "First attack should damage the nearby enemy once.");
    _require(_containsEvent(world, arena::GameEvent::AttackStarted),
             "A valid attack should publish an attack event.");

    world.update({{}, true, false}, 0.01F);
    _require(world.enemies().front().health == 35,
             "Attack cooldown should prevent immediate repeated damage.");
    _require(world.events().empty(), "A blocked attack should not publish an event.");

    world.update({}, 0.25F);
    world.update({}, 0.10F);
    world.update({{}, true, false}, 0.01F);
    _require(world.enemies().empty(), "Second valid attack should defeat the enemy.");
    _require(world.score() == 100, "Defeating one enemy should award 100 points.");
    _require(_containsEvent(world, arena::GameEvent::EnemyDefeated),
             "Defeating an enemy should publish a defeat event.");
}

void _dashMovesAndStartsCooldown() {
    arena::GameWorld world(_testConfig());
    const float initialX = world.player().position.x;
    world.update({{1.0F, 0.0F}, false, true}, 0.01F);

    _require(world.player().position.x > initialX + 100.0F,
             "Dash should create a clear burst of movement.");
    _require(world.player().dashCooldown > 1.0F,
             "Dash should start its cooldown.");
    _require(world.player().invulnerability > 0.0F,
             "Dash should briefly protect the player.");
    _require(_containsEvent(world, arena::GameEvent::DashStarted),
             "A valid dash should publish a dash event.");
}

void _multiKillPublishesOneAggregatedDefeatEvent() {
    arena::WorldConfig config = _testConfig();
    config.enemyHealth = 40;
    arena::GameWorld world(config);
    world.spawnEnemy({225.0F, 150.0F});
    world.spawnEnemy({175.0F, 150.0F});

    world.update({{}, true, false}, 0.01F);
    _require(world.enemies().empty() && world.score() == 200,
             "One attack should be able to defeat multiple nearby enemies.");
    _require(std::ranges::count(world.events(), arena::GameEvent::EnemyDefeated) == 1,
             "A multi-kill should publish one aggregated defeat sound event per update.");
}

void _contactDamagePublishesDamageAndGameOverEvents() {
    arena::GameWorld world(_testConfig());
    world.spawnEnemy(world.player().position);

    bool sawDamage = false;
    bool sawGameOver = false;
    for (int step = 0; step < 40 && !world.gameOver(); ++step) {
        world.update({}, 0.25F);
        sawDamage = sawDamage || _containsEvent(world, arena::GameEvent::PlayerDamaged);
        sawGameOver = sawGameOver || _containsEvent(world, arena::GameEvent::GameOver);
    }

    _require(sawDamage, "Enemy contact should publish a player-damaged event.");
    _require(world.gameOver() && sawGameOver,
             "Lethal contact should publish a game-over event.");
    _require(world.events().size() == 2 &&
                 world.events()[0] == arena::GameEvent::PlayerDamaged &&
                 world.events()[1] == arena::GameEvent::GameOver,
             "Lethal contact events must be emitted once and in causal order.");

    world.update({}, 0.01F);
    _require(world.events().empty(),
             "A post-game update must clear terminal events without repeating them.");
    world.reset();
    _require(world.events().empty(), "Reset must not retain events from the previous run.");
}

void _invalidDeltaTimeIsReported() {
    arena::GameWorld world(_testConfig());
    bool threw = false;
    try {
        world.update({}, 0.0F);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    _require(threw, "Invalid delta time must be reported instead of ignored.");
}

void _invalidWorldAndSpawnInputsAreReported() {
    arena::WorldConfig invalidConfig = _testConfig();
    invalidConfig.width = 20.0F;
    bool invalidWorldThrew = false;
    try {
        arena::GameWorld world(invalidConfig);
    } catch (const std::invalid_argument&) {
        invalidWorldThrew = true;
    }
    _require(invalidWorldThrew, "An arena smaller than its actors must be rejected.");

    arena::GameWorld world(_testConfig());
    bool invalidPositionThrew = false;
    try {
        world.spawnEnemy({std::nanf(""), 10.0F});
    } catch (const std::invalid_argument&) {
        invalidPositionThrew = true;
    }
    _require(invalidPositionThrew, "Non-finite spawn coordinates must be rejected.");

    arena::WorldConfig infiniteSpeedConfig = _testConfig();
    infiniteSpeedConfig.playerSpeed = std::numeric_limits<float>::infinity();
    bool infiniteSpeedThrew = false;
    try {
        arena::GameWorld invalidWorld(infiniteSpeedConfig);
    } catch (const std::invalid_argument&) {
        infiniteSpeedThrew = true;
    }
    _require(infiniteSpeedThrew, "Infinite movement speed must be rejected.");
}

void _spawnTimingDoesNotDependOnFramePartitioning() {
    arena::WorldConfig config = _testConfig();
    config.automaticSpawning = true;
    config.spawnInterval = 0.1F;

    arena::GameWorld singleUpdateWorld(config);
    arena::GameWorld partitionedWorld(config);
    singleUpdateWorld.update({}, 0.25F);
    for (int step = 0; step < 5; ++step) {
        partitionedWorld.update({}, 0.05F);
    }

    _require(singleUpdateWorld.enemies().size() == partitionedWorld.enemies().size(),
             "Equal simulated time must produce the same number of enemies.");
}

void _spawnLimitAndSeparationAreEnforced() {
    arena::WorldConfig config = _testConfig();
    config.maximumEnemies = 2;
    arena::GameWorld world(config);
    world.spawnEnemy({14.0F, 14.0F});
    world.spawnEnemy({14.0F, 14.0F});

    bool limitThrew = false;
    try {
        world.spawnEnemy({100.0F, 100.0F});
    } catch (const std::length_error&) {
        limitThrew = true;
    }
    _require(limitThrew, "The public spawn seam must enforce the configured enemy limit.");

    world.update({}, 0.01F);
    const arena::Vec2 difference = world.enemies()[1].position - world.enemies()[0].position;
    _require(std::sqrt(arena::lengthSquared(difference)) >= 27.99F,
             "Coincident enemies must be separated even at an arena boundary.");
}

}  // namespace

int main() {
    try {
        _diagonalMovementIsNormalized();
        _playerCannotLeaveArena();
        _attackHasCooldownAndAwardsScore();
        _dashMovesAndStartsCooldown();
        _multiKillPublishesOneAggregatedDefeatEvent();
        _contactDamagePublishesDamageAndGameOverEvents();
        _invalidDeltaTimeIsReported();
        _invalidWorldAndSpawnInputsAreReported();
        _spawnTimingDoesNotDependOnFramePartitioning();
        _spawnLimitAndSeparationAreEnforced();
        std::cout << "All arena_core tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
