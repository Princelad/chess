#pragma once

#include <chess/board.h>
#include <chess/move.h>
#include <chess/movegen.h>

#include <SFML/Audio.hpp>

#include <cstdint>
#include <vector>

namespace chess::client {

enum class Sfx {
    None = 0,
    Move,
    Capture,
    Check,
    Checkmate,
};

// Pure classification: which clip a completed move warrants. Order of
// precedence: Checkmate > Check > Capture > Move. No I/O, unit-tested.
Sfx classifyMove(const Board& before, const Move& m);

// Hands out self-contained, procedurally synthesized sound clips (16-bit
// mono PCM @ 44100 Hz). Buffers are built lazily on first play; the class
// works without an audio device (synthesis only, safe in tests).
class SoundManager {
public:
    SoundManager();

    void playMove(const Board& before, const Move& m) { play(classifyMove(before, m)); }
    void play(Sfx kind);

    void setEnabled(bool enabled) { enabled_ = enabled; sound_.setVolume(currentVolume()); }
    void setVolume(int percent) { volume_ = percent; sound_.setVolume(currentVolume()); }
    bool enabled() const { return enabled_; }
    int volume() const { return volume_; }

// Synthesis helpers, exposed for testing.
    static constexpr int sampleRate() { return cSampleRate; }
    static std::vector<std::int16_t> synthTone(float freq, float seconds,
                                               float decayPerSecond);
    static sf::SoundBuffer makeToneBuffer(float freq, float seconds,
                                          float decayPerSecond);

    bool ready() const { return built_; }

private:
    static constexpr int cSampleRate = 44100;

    void ensureBuilt();
    float currentVolume() const { return enabled_ ? static_cast<float>(volume_) : 0.f; }
    const sf::SoundBuffer& bufferFor(Sfx kind) const;

    sf::SoundBuffer moveBuffer_;
    sf::SoundBuffer captureBuffer_;
    sf::SoundBuffer checkBuffer_;
    sf::SoundBuffer mateBuffer_;
    sf::Sound sound_{moveBuffer_};
    bool built_ = false;
    bool enabled_ = true;
    int volume_ = 100;
};

} // namespace chess::client