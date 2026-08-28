#pragma once

#include "core/GameWorld.h"

#include <array>
#include <span>
#include <vector>

namespace arena::app {

class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    void playEvents(std::span<const GameEvent> events) const noexcept;

private:
    enum class SoundKind : std::size_t {
        Attack,
        Dash,
        EnemyDefeated,
        PlayerDamaged,
        GameOver,
        Count,
    };

    std::array<std::vector<char>, static_cast<std::size_t>(SoundKind::Count)> sounds_;
};

}  // namespace arena::app
