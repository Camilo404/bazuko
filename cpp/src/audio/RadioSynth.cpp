#include "RadioSynth.hpp"

#include <algorithm>
#include <cmath>

namespace Bazuko {

namespace {
// El filtro pasa banda deja la musica muy baja: se compensa para que se oiga junto al resto del juego
constexpr float kMakeupGain = 3.0f;
}

RadioProgram RadioProgramFromId(const std::string& id) {
    if (id == "recuerdos") return RadioProgram::Recuerdos;
    if (id == "noticias") return RadioProgram::Noticias;
    if (id == "deportes") return RadioProgram::Deportes;
    if (id == "rock") return RadioProgram::Rock;
    return RadioProgram::None;
}

RadioSynth::RadioSynth(float sampleRate, uint32_t seed)
    : m_SampleRate(sampleRate), m_Random(seed), m_StepLength(static_cast<long>(sampleRate * 0.18f)) {
    constexpr float kPi = 3.14159265358979f;
    const float w0 = 2.0f * kPi * 1450.0f / sampleRate;
    const float alpha = std::sin(w0) / (2.0f * 1.1f);
    const float a0 = 1.0f + alpha;
    m_B0 = alpha / a0;
    m_B2 = -alpha / a0;
    m_A1 = -2.0f * std::cos(w0) / a0;
    m_A2 = (1.0f - alpha) / a0;
    m_Voices.reserve(32);
}

void RadioSynth::Note(float frequency, float duration, float gain, Wave wave, float startSeconds) {
    if (gain <= 0.0001f) return;
    Voice voice;
    voice.frequency = frequency;
    voice.duration = duration;
    voice.gain = gain;
    voice.wave = wave;
    voice.delaySamples = static_cast<long>(startSeconds * m_SampleRate);
    m_Voices.push_back(voice);
}

void RadioSynth::PlayStep(int step) {
    const float signal = m_Signal.load();
    if (signal < 0.15f) return;   // muy poca senal para oir musica clara
    const float vol = signal * m_Volume.load() * 0.18f;
    switch (static_cast<RadioProgram>(m_Station.load())) {
    case RadioProgram::Recuerdos: {
        // 540 AM: cumbia / bolero lo-fi: bajo trotón y acordes de organo vintage
        if (step % 2 == 0) {
            static const float bass[4] = {110.0f, 130.8f, 146.8f, 164.8f};
            Note(bass[(step / 4) % 4], 0.22f, vol * 1.2f, Wave::Triangle, 0.0f);
        }
        if (step % 4 == 1 || step % 4 == 3) {
            static const float chords[4][3] = {{220.0f, 261.6f, 329.6f}, {293.7f, 349.2f, 440.0f}, {329.6f, 392.0f, 493.9f}, {220.0f, 261.6f, 329.6f}};
            for (float frequency : chords[(step / 8) % 4]) Note(frequency, 0.14f, vol * 0.6f, Wave::Sine, 0.0f);
        }
        break;
    }
    case RadioProgram::Noticias:
        // 890 AM: campana de inicio de boletin y cadencia de locutor
        if (step % 32 == 0) {
            Note(523.25f, 0.6f, vol * 1.5f, Wave::Sine, 0.0f);
            Note(659.25f, 0.6f, vol * 1.2f, Wave::Sine, 0.15f);
            Note(783.99f, 0.9f, vol * 1.5f, Wave::Sine, 0.3f);
        } else if (step % 32 < 20 && step % 2 == 0) {
            const float talk = 160.0f + static_cast<float>(step % 7) * 25.0f + std::sin(static_cast<float>(step)) * 15.0f;
            Note(talk, 0.08f, vol * 0.85f, Wave::Triangle, 0.0f);
        }
        break;
    case RadioProgram::Deportes:
        // 1040 AM: corneta de estadio y locutor eufórico
        if (step % 16 == 0) {
            Note(440.0f, 0.15f, vol * 1.3f, Wave::Sawtooth, 0.0f);
            Note(554.37f, 0.15f, vol * 1.3f, Wave::Sawtooth, 0.15f);
            Note(659.25f, 0.35f, vol * 1.6f, Wave::Sawtooth, 0.3f);
        } else if (step % 3 == 0) {
            Note(220.0f + static_cast<float>(step % 5) * 35.0f, 0.09f, vol * 0.9f, Wave::Sawtooth, 0.0f);
        }
        break;
    case RadioProgram::Rock: {
        // 98.5 FM: rock en espanol, bajo con overdrive
        static const float riff[6] = {146.8f, 146.8f, 174.6f, 196.0f, 164.8f, 146.8f};
        const float note = riff[step % 6];
        Note(note, 0.15f, vol * 1.1f, Wave::Sawtooth, 0.0f);
        if (step % 4 == 2) Note(note * 2.0f, 0.1f, vol * 0.7f, Wave::Triangle, 0.0f);
        break;
    }
    case RadioProgram::None: break;
    }
}

float RadioSynth::NextVoiceSample() {
    constexpr float kTwoPi = 6.28318530717959f;
    float sum = 0.0f;
    for (Voice& voice : m_Voices) {
        if (voice.delaySamples > 0) {
            --voice.delaySamples;
            continue;
        }
        const float t = static_cast<float>(voice.age) / m_SampleRate;
        // exponentialRampToValueAtTime(0.0001) desde `gain` durante `duration`
        const float envelope = voice.gain * std::pow(0.0001f / voice.gain, std::min(1.0f, t / voice.duration));
        float wave;
        switch (voice.wave) {
        case Wave::Sine: wave = std::sin(voice.phase * kTwoPi); break;
        case Wave::Triangle: wave = 4.0f * std::fabs(voice.phase - 0.5f) - 1.0f; break;
        default: wave = 2.0f * voice.phase - 1.0f; break;
        }
        sum += wave * envelope;
        voice.phase += voice.frequency / m_SampleRate;
        if (voice.phase >= 1.0f) voice.phase -= 1.0f;
        ++voice.age;
    }
    return sum;
}

float RadioSynth::Bandpass(float input) {
    const float output = m_B0 * input + m_B2 * m_X2 - m_A1 * m_Y1 - m_A2 * m_Y2;
    m_X2 = m_X1;
    m_X1 = input;
    m_Y2 = m_Y1;
    m_Y1 = output;
    return output;
}

void RadioSynth::Render(float* out, size_t frames) {
    if (!m_On.load()) {
        std::fill(out, out + frames, 0.0f);
        m_Voices.clear();
        m_StepCounter = 0;
        return;
    }
    const float volume = m_Volume.load();
    const float signal = std::clamp(m_Signal.load(), 0.0f, 1.0f);
    const float staticTarget = (1.0f - signal * 0.85f) * 0.12f * volume;
    const float smoothing = 1.0f - std::exp(-1.0f / (0.05f * m_SampleRate));   // setTargetAtTime(tau = 50 ms)
    std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
    for (size_t index = 0; index < frames; ++index) {
        if (m_StepCounter <= 0) {
            PlayStep(m_Step++);
            m_StepCounter = m_StepLength;
        }
        --m_StepCounter;
        m_StaticLevel += (staticTarget - m_StaticLevel) * smoothing;
        const float mix = NextVoiceSample() + noise(m_Random) * 0.35f * m_StaticLevel;
        out[index] = Bandpass(mix) * volume * kMakeupGain;
    }
    m_Voices.erase(std::remove_if(m_Voices.begin(), m_Voices.end(), [&](const Voice& voice) {
        return voice.delaySamples <= 0 && voice.age >= static_cast<long>(voice.duration * m_SampleRate);
    }), m_Voices.end());
}

} // namespace Bazuko
