// Pruebas de la sintesis de la radio transistor (logica pura: sin dispositivo de audio).
#include "audio/RadioSynth.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace Bazuko;

static int g_failures = 0;
#define CHECK(cond, ...)                                                        \
    do {                                                                        \
        if (!(cond)) {                                                          \
            ++g_failures;                                                       \
            std::printf("FALLO (%s:%d): %s -> ", __FILE__, __LINE__, #cond);    \
            std::printf(__VA_ARGS__);                                           \
            std::printf("\n");                                                  \
        }                                                                       \
    } while (0)

static std::vector<float> Capture(RadioSynth& synth, float seconds) {
    std::vector<float> samples(static_cast<size_t>(48000.0f * seconds));
    // el hilo de audio pide bloques pequenos: se renderiza por trozos de 480
    for (size_t offset = 0; offset < samples.size(); offset += 480)
        synth.Render(samples.data() + offset, std::min<size_t>(480, samples.size() - offset));
    return samples;
}

static double Rms(const std::vector<float>& samples) {
    double sum = 0;
    for (float value : samples) sum += static_cast<double>(value) * value;
    return std::sqrt(sum / static_cast<double>(samples.size()));
}

static float Peak(const std::vector<float>& samples) {
    float peak = 0;
    for (float value : samples) peak = std::max(peak, std::fabs(value));
    return peak;
}

int main() {
    CHECK(RadioProgramFromId("recuerdos") == RadioProgram::Recuerdos && RadioProgramFromId("noticias") == RadioProgram::Noticias
              && RadioProgramFromId("deportes") == RadioProgram::Deportes && RadioProgramFromId("rock") == RadioProgram::Rock
              && RadioProgramFromId("otra") == RadioProgram::None,
          "ids de emisoras");

    RadioSynth off(48000.0f, 1);
    off.SetStation(RadioProgram::Recuerdos);
    const auto silent = Capture(off, 1.0f);
    CHECK(Peak(silent) == 0.0f, "apagada deberia ser silencio");

    const auto render = [](RadioProgram station, float signal, uint32_t seed = 1) {
        RadioSynth synth(48000.0f, seed);
        synth.SetOn(true);
        synth.SetStation(station);
        synth.SetSignal(signal);
        return Capture(synth, 4.0f);
    };
    const auto tuned = render(RadioProgram::Recuerdos, 1.0f);
    const auto lost = render(RadioProgram::Recuerdos, 0.0f);
    std::printf("RMS sintonizada %.4f (pico %.3f), sin senal %.4f (pico %.3f)\n", Rms(tuned), Peak(tuned), Rms(lost), Peak(lost));
    CHECK(Peak(tuned) > 0.005f && Peak(tuned) < 1.0f, "la emisora deberia sonar sin saturar: pico %.3f", Peak(tuned));
    CHECK(Rms(lost) > 0.0 && Peak(lost) * 2.0f < Peak(tuned), "sin senal solo hay estatica: sin los picos de las notas");

    // Cada emisora tiene su propia musica y la misma semilla repite el resultado
    const auto rock = render(RadioProgram::Rock, 1.0f);
    const auto news = render(RadioProgram::Noticias, 1.0f);
    const auto sports = render(RadioProgram::Deportes, 1.0f);
    const auto difference = [](const std::vector<float>& a, const std::vector<float>& b) {
        double sum = 0;
        for (size_t index = 0; index < a.size(); ++index) sum += std::fabs(a[index] - b[index]);
        return sum / static_cast<double>(a.size());
    };
    CHECK(difference(tuned, rock) > 1e-3 && difference(rock, news) > 1e-3 && difference(news, sports) > 1e-3 && difference(tuned, sports) > 1e-3,
          "las emisoras suenan distinto");
    CHECK(difference(tuned, render(RadioProgram::Recuerdos, 1.0f)) == 0.0, "misma semilla, misma salida");

    // La campana del noticiero abre el boletin: suena en el primer medio segundo
    {
        RadioSynth synth(48000.0f, 2);
        synth.SetOn(true);
        synth.SetStation(RadioProgram::Noticias);
        const auto opening = Capture(synth, 0.5f);
        CHECK(Peak(opening) > Rms(tuned), "la campana deberia destacar: pico %.3f", Peak(opening));
    }

    // Menos volumen, menos sonido
    {
        RadioSynth quiet(48000.0f, 1);
        quiet.SetOn(true);
        quiet.SetStation(RadioProgram::Recuerdos);
        quiet.SetVolume(0.2f);
        CHECK(Rms(Capture(quiet, 4.0f)) < Rms(tuned) * 0.5, "con volumen 0,2 deberia sonar mucho menos");
    }

    // Apagar corta el sonido de inmediato
    {
        RadioSynth synth(48000.0f, 3);
        synth.SetOn(true);
        synth.SetStation(RadioProgram::Rock);
        Capture(synth, 1.0f);
        synth.SetOn(false);
        CHECK(Peak(Capture(synth, 0.2f)) == 0.0f, "apagada vuelve el silencio");
    }

    if (g_failures == 0) std::printf("RadioSynth: todo OK\n");
    else std::printf("RadioSynth: %d fallos\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
