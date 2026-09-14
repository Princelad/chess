#include "sfx.h"

#include <algorithm>
#include <cmath>

namespace chess::client {

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr double kSampleRate = SoundManager::sampleRate();

// Concatenate single-note samples with a short silence gap between them.
std::vector<std::int16_t> synthSequence(
    const std::vector<std::pair<float, float>>& notes, float gap)
{
    const int gapSamples = static_cast<int>(gap * kSampleRate);
    std::vector<std::int16_t> out;
    for (std::size_t i = 0; i < notes.size(); ++i) {
        auto tone = SoundManager::synthTone(notes[i].first, notes[i].second, 18.f);
        out.insert(out.end(), tone.begin(), tone.end());
        if (i + 1 < notes.size())
            out.insert(out.end(), gapSamples, std::int16_t{0});
    }
    return out;
}
} // anonymous namespace

Sfx classifyMove(const Board& before, const Move& m)
{
    Board after = before;
    after.makeMove(m);
    if (evaluateGameState(after) == GameState::Checkmate) return Sfx::Checkmate;
    if (chess::inCheck(after, after.sideToMove())) return Sfx::Check;
    if (m.isCapture() || m.isEnPassant()) return Sfx::Capture;
    return Sfx::Move;
}

std::vector<std::int16_t> SoundManager::synthTone(float freq, float seconds,
                                               float decayPerSecond)
{
    const int samples = static_cast<int>(seconds * kSampleRate);
    std::vector<std::int16_t> out;
    out.reserve(static_cast<std::size_t>(samples));
    const float amplitude = 0.5f * 32767.f;
    for (int i = 0; i < samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(cSampleRate);
        const float env = std::exp(-decayPerSecond * t);
        const float tone = std::sin(2.f * kPi * freq * t);
        out.push_back(static_cast<std::int16_t>(amplitude * env * tone));
    }
    return out;
}

std::vector<std::int16_t> SoundManager::synthTone(float freq, float seconds,
                                               float fadeOutStart,
                                               float decayPerSecond)
{
    const int samples = static_cast<int>(seconds * kSampleRate);
    const int fadePoint = static_cast<int>(fadeOutStart * kSampleRate);
    std::vector<std::int16_t> tone = synthTone(freq, seconds, decayPerSecond);
    for (int i = fadePoint; i < samples; ++i) {
        const float frac =
            1.f - static_cast<float>(i - fadePoint) /
                      static_cast<float>(std::max(1, samples - fadePoint));
        tone[i] = static_cast<std::int16_t>(tone[i] * frac);
    }
    return tone;
}

sf::SoundBuffer SoundManager::makeToneBuffer(float freq, float seconds,
                                             float decayPerSecond)
{
    auto samples = synthTone(freq, seconds, decayPerSecond);
    sf::SoundBuffer buffer;
    const bool ok = buffer.loadFromSamples(
        samples.data(), static_cast<std::uint64_t>(samples.size()), 1,
        cSampleRate, {sf::SoundChannel::Mono});
    if (!ok) buffer = sf::SoundBuffer{};
    return buffer;
}

SoundManager::SoundManager() = default;

void SoundManager::play(Sfx kind)
{
    if (kind == Sfx::None || !enabled_) return;
    ensureBuilt();
    sound_.setBuffer(bufferFor(kind));
    sound_.setVolume(currentVolume());
    sound_.play();
}

void SoundManager::ensureBuilt()
{
    if (built_) return;
    moveBuffer_ = makeToneBuffer(330.f, 0.09f, 15.f);
    captureBuffer_ = makeToneBuffer(150.f, 0.12f, 22.f);

    auto checkSamples = synthSequence({{440.f, 0.07f}, {587.f, 0.10f}}, 0.02f);
    const bool checkOk = checkBuffer_.loadFromSamples(
        checkSamples.data(), static_cast<std::uint64_t>(checkSamples.size()), 1,
        cSampleRate, {sf::SoundChannel::Mono});
    (void)checkOk;

    auto mateSamples = synthSequence({{440.f, 0.09f}, {349.f, 0.09f}, {262.f, 0.20f}},
                                     0.03f);
    const bool mateOk = mateBuffer_.loadFromSamples(
        mateSamples.data(), static_cast<std::uint64_t>(mateSamples.size()), 1,
        cSampleRate, {sf::SoundChannel::Mono});
    (void)mateOk;

    built_ = true;
}

const sf::SoundBuffer& SoundManager::bufferFor(Sfx kind) const
{
    switch (kind) {
        case Sfx::Capture: return captureBuffer_;
        case Sfx::Check: return checkBuffer_;
        case Sfx::Checkmate: return mateBuffer_;
        case Sfx::Move:
        case Sfx::None:
        default: return moveBuffer_;
    }
}

} // namespace chess::client