#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "AudioManager.hpp"

#include <array>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>

namespace Bazuko {

namespace {

enum class SoundSlot : size_t {
    Pickup,
    Treasure,
    Sell,
    Full,
    Empty,
    Steps,
    Ambient,
    Count
};

std::string FindAudioPath(const char* filename) {
    const std::array<std::filesystem::path, 5> candidates{
        std::filesystem::path("public/audio") / filename,
        std::filesystem::path("../public/audio") / filename,
        std::filesystem::path("../../public/audio") / filename,
        std::filesystem::path("assets/audio") / filename,
        std::filesystem::path("audio") / filename
    };
    for (const auto& path : candidates) {
        if (std::filesystem::exists(path)) return path.string();
    }
    return {};
}

} // namespace

struct AudioManager::Impl {
    struct Sound {
        ma_sound value{};
        bool initialized = false;
    };

    ma_engine engine{};
    bool ready = false;
    bool muted = false;
    float masterVolume = 0.7f;
    bool stepsPlaying = false;
    std::array<Sound, static_cast<size_t>(SoundSlot::Count)> sounds{};

    Impl() {
        const ma_result result = ma_engine_init(nullptr, &engine);
        if (result != MA_SUCCESS) {
            std::cerr << "Audio: no se pudo inicializar el dispositivo: " << ma_result_description(result) << '\n';
            return;
        }
        ready = true;
        ma_engine_set_volume(&engine, masterVolume);

        const std::array<std::pair<SoundSlot, const char*>, static_cast<size_t>(SoundSlot::Count)> files{{
            {SoundSlot::Pickup, "pickup.wav"},
            {SoundSlot::Treasure, "treasure.wav"},
            {SoundSlot::Sell, "sell.wav"},
            {SoundSlot::Full, "full.wav"},
            {SoundSlot::Empty, "empty.wav"},
            {SoundSlot::Steps, "steps.wav"},
            {SoundSlot::Ambient, "ambient.wav"}
        }};
        for (const auto& [slot, filename] : files) {
            const std::string path = FindAudioPath(filename);
            if (path.empty()) {
                std::cerr << "Audio: no se encontro public/audio/" << filename << '\n';
                continue;
            }
            Sound& sound = sounds[static_cast<size_t>(slot)];
            const ma_result loadResult = ma_sound_init_from_file(&engine, path.c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, &sound.value);
            if (loadResult != MA_SUCCESS) {
                std::cerr << "Audio: no se pudo cargar " << path << ": " << ma_result_description(loadResult) << '\n';
                continue;
            }
            sound.initialized = true;
        }

        const std::array<float, static_cast<size_t>(SoundSlot::Count)> volumes{0.6f, 0.9f, 0.8f, 0.5f, 0.4f, 0.5f, 0.25f};
        for (size_t index = 0; index < sounds.size(); ++index) {
            if (sounds[index].initialized) ma_sound_set_volume(&sounds[index].value, volumes[index]);
        }

        Sound& steps = sounds[static_cast<size_t>(SoundSlot::Steps)];
        if (steps.initialized) ma_sound_set_looping(&steps.value, MA_TRUE);
        Sound& ambient = sounds[static_cast<size_t>(SoundSlot::Ambient)];
        if (ambient.initialized) {
            ma_sound_set_looping(&ambient.value, MA_TRUE);
            ma_sound_set_volume(&ambient.value, 0.25f);
            ma_sound_start(&ambient.value);
        }
    }

    ~Impl() {
        if (ready) {
            for (Sound& sound : sounds) if (sound.initialized) ma_sound_uninit(&sound.value);
            ma_engine_uninit(&engine);
        }
    }
};

AudioManager::AudioManager() : m_Impl(std::make_unique<Impl>()) {}
AudioManager::~AudioManager() = default;

bool AudioManager::IsReady() const { return m_Impl && m_Impl->ready; }
bool AudioManager::IsMuted() const { return !m_Impl || m_Impl->muted; }
float AudioManager::GetVolume() const { return m_Impl ? m_Impl->masterVolume : 0.0f; }

void AudioManager::SetVolume(float volume) {
    if (!m_Impl || !m_Impl->ready) return;
    m_Impl->masterVolume = std::clamp(volume, 0.0f, 1.0f);
    ma_engine_set_volume(&m_Impl->engine, m_Impl->muted ? 0.0f : m_Impl->masterVolume);
}

void AudioManager::SetMuted(bool muted) {
    if (!m_Impl || !m_Impl->ready) return;
    m_Impl->muted = muted;
    ma_engine_set_volume(&m_Impl->engine, muted ? 0.0f : m_Impl->masterVolume);
}

void AudioManager::Play(AudioCue cue) {
    if (!m_Impl || !m_Impl->ready || m_Impl->muted) return;
    SoundSlot slot = SoundSlot::Pickup;
    switch (cue) {
        case AudioCue::Pickup: slot = SoundSlot::Pickup; break;
        case AudioCue::Treasure: slot = SoundSlot::Treasure; break;
        case AudioCue::Sell: slot = SoundSlot::Sell; break;
        case AudioCue::Full: slot = SoundSlot::Full; break;
        case AudioCue::Empty: slot = SoundSlot::Empty; break;
    }
    auto& sound = m_Impl->sounds[static_cast<size_t>(slot)];
    if (!sound.initialized) return;
    ma_sound_stop(&sound.value);
    ma_sound_seek_to_pcm_frame(&sound.value, 0);
    ma_sound_start(&sound.value);
}

void AudioManager::Update(bool moving, bool running) {
    if (!m_Impl || !m_Impl->ready) return;
    auto& steps = m_Impl->sounds[static_cast<size_t>(SoundSlot::Steps)];
    if (!steps.initialized) return;
    if (moving && !m_Impl->stepsPlaying) {
        ma_sound_set_pitch(&steps.value, running ? 1.4f : 1.0f);
        ma_sound_start(&steps.value);
        m_Impl->stepsPlaying = true;
    } else if (!moving && m_Impl->stepsPlaying) {
        ma_sound_stop(&steps.value);
        m_Impl->stepsPlaying = false;
    }
    if (moving && m_Impl->stepsPlaying) ma_sound_set_pitch(&steps.value, running ? 1.4f : 1.0f);
}

} // namespace Bazuko
