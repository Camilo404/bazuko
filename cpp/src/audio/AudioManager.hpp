#pragma once

#include <memory>

namespace Bazuko {

enum class AudioCue {
    Pickup,
    Treasure,
    Sell,
    Full,
    Empty
};

class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    bool IsReady() const;
    bool IsMuted() const;
    void SetMuted(bool muted);
    float GetVolume() const;
    void SetVolume(float volume);
    void Play(AudioCue cue);
    void Update(bool moving, bool running);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Bazuko
