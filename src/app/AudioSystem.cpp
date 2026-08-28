#include "app/AudioSystem.h"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>

namespace arena::app {
namespace {

constexpr std::uint32_t kSampleRate = 22050U;
constexpr float kMaximumAmplitude = 0.28F;

struct SoundRecipe {
    float durationSeconds;
    float startFrequency;
    float endFrequency;
    float noiseAmount;
};

void _appendTag(std::vector<char>& bytes, const char (&tag)[5]) {
    bytes.insert(bytes.end(), tag, tag + 4);
}

void _appendU16(std::vector<char>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<char>(value & 0xFFU));
    bytes.push_back(static_cast<char>((value >> 8U) & 0xFFU));
}

void _appendU32(std::vector<char>& bytes, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32U; shift += 8U) {
        bytes.push_back(static_cast<char>((value >> shift) & 0xFFU));
    }
}

std::vector<char> _synthesizeWave(const SoundRecipe& recipe) {
    const auto sampleCount = static_cast<std::uint32_t>(
        std::lround(recipe.durationSeconds * static_cast<float>(kSampleRate)));
    const std::uint32_t dataSize = sampleCount * sizeof(std::int16_t);
    std::vector<char> wave;
    wave.reserve(44U + dataSize);

    _appendTag(wave, "RIFF");
    _appendU32(wave, 36U + dataSize);
    _appendTag(wave, "WAVE");
    _appendTag(wave, "fmt ");
    _appendU32(wave, 16U);
    _appendU16(wave, 1U);
    _appendU16(wave, 1U);
    _appendU32(wave, kSampleRate);
    _appendU32(wave, kSampleRate * 2U);
    _appendU16(wave, 2U);
    _appendU16(wave, 16U);
    _appendTag(wave, "data");
    _appendU32(wave, dataSize);

    float phase = 0.0F;
    std::uint32_t noiseState = 0x9E3779B9U;
    for (std::uint32_t index = 0; index < sampleCount; ++index) {
        const float progress = static_cast<float>(index) / static_cast<float>(sampleCount);
        const float frequency = recipe.startFrequency +
                                (recipe.endFrequency - recipe.startFrequency) * progress;
        phase += 2.0F * std::numbers::pi_v<float> * frequency /
                 static_cast<float>(kSampleRate);

        noiseState = noiseState * 1664525U + 1013904223U;
        const float noise = static_cast<float>(noiseState >> 8U) / 8388607.5F - 1.0F;
        const float tone = std::sin(phase);
        const float envelope = (1.0F - progress) * (1.0F - progress);
        const float mixed = tone * (1.0F - recipe.noiseAmount) + noise * recipe.noiseAmount;
        const float scaled = std::clamp(mixed * envelope * kMaximumAmplitude, -1.0F, 1.0F);
        const auto sample = static_cast<std::int16_t>(std::lround(scaled * 32767.0F));
        _appendU16(wave, static_cast<std::uint16_t>(sample));
    }
    return wave;
}

int _priority(GameEvent event) noexcept {
    switch (event) {
        case GameEvent::AttackStarted:
            return 1;
        case GameEvent::DashStarted:
            return 2;
        case GameEvent::EnemyDefeated:
            return 3;
        case GameEvent::PlayerDamaged:
            return 4;
        case GameEvent::GameOver:
            return 5;
    }
    return 0;
}

}  // namespace

AudioSystem::AudioSystem() {
    sounds_[static_cast<std::size_t>(SoundKind::Attack)] =
        _synthesizeWave({0.09F, 620.0F, 210.0F, 0.18F});
    sounds_[static_cast<std::size_t>(SoundKind::Dash)] =
        _synthesizeWave({0.13F, 180.0F, 920.0F, 0.28F});
    sounds_[static_cast<std::size_t>(SoundKind::EnemyDefeated)] =
        _synthesizeWave({0.16F, 440.0F, 980.0F, 0.08F});
    sounds_[static_cast<std::size_t>(SoundKind::PlayerDamaged)] =
        _synthesizeWave({0.18F, 170.0F, 70.0F, 0.42F});
    sounds_[static_cast<std::size_t>(SoundKind::GameOver)] =
        _synthesizeWave({0.55F, 360.0F, 72.0F, 0.06F});
}

AudioSystem::~AudioSystem() {
    // SND_MEMORY playback requires the backing buffer to outlive playback.
    PlaySoundA(nullptr, nullptr, 0);
}

void AudioSystem::playEvents(std::span<const GameEvent> events) const noexcept {
    std::optional<GameEvent> selectedEvent;
    int selectedPriority = 0;
    for (const GameEvent event : events) {
        const int eventPriority = _priority(event);
        if (eventPriority > selectedPriority) {
            selectedEvent = event;
            selectedPriority = eventPriority;
        }
    }
    if (!selectedEvent.has_value()) {
        return;
    }

    SoundKind sound = SoundKind::Attack;
    switch (*selectedEvent) {
        case GameEvent::AttackStarted:
            sound = SoundKind::Attack;
            break;
        case GameEvent::DashStarted:
            sound = SoundKind::Dash;
            break;
        case GameEvent::EnemyDefeated:
            sound = SoundKind::EnemyDefeated;
            break;
        case GameEvent::PlayerDamaged:
            sound = SoundKind::PlayerDamaged;
            break;
        case GameEvent::GameOver:
            sound = SoundKind::GameOver;
            break;
    }

    const std::vector<char>& wave = sounds_[static_cast<std::size_t>(sound)];
    PlaySoundA(wave.data(), nullptr, SND_ASYNC | SND_MEMORY | SND_NODEFAULT);
}

}  // namespace arena::app
