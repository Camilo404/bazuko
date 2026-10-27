// Pruebas del ciclo de luz segun la hora y el clima (logica pura).
#include "world/Atmosphere.hpp"

#include <cstdio>

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

static float Brightness(const glm::vec3& value) { return value.r + value.g + value.b; }

int main() {
    const Atmosphere noon = ComputeAtmosphere(12.0f, 0.0f, 0.0f);
    const Atmosphere night = ComputeAtmosphere(0.0f, 0.0f, 0.0f);
    const Atmosphere dusk = ComputeAtmosphere(18.2f, 0.0f, 0.0f);
    const Atmosphere dawn = ComputeAtmosphere(6.2f, 0.0f, 0.0f);

    CHECK(Brightness(noon.sunColor) > 3.0f * Brightness(night.sunColor), "el mediodia debe ser mucho mas claro que la noche");
    CHECK(Brightness(noon.fogColor) > 3.0f * Brightness(night.fogColor), "cielo de dia mas claro que de noche");
    CHECK(Brightness(noon.ambientColor) > Brightness(night.ambientColor), "ambiente de dia mas claro");
    CHECK(night.sunColor.b > night.sunColor.r, "la luz nocturna es azulada");
    CHECK(dusk.sunColor.r > dusk.sunColor.b, "el atardecer es calido");
    CHECK(Brightness(dawn.fogColor) > Brightness(night.fogColor) && Brightness(dawn.fogColor) < Brightness(noon.fogColor), "el amanecer queda entre noche y dia");
    CHECK(Brightness(night.sunColor) > 0.05f && Brightness(night.ambientColor) > 0.3f, "la noche no debe quedar a oscuras del todo");

    CHECK(night.lampPower > 0.99f && noon.lampPower < 0.01f, "las farolas se encienden de noche y se apagan de dia: %.2f / %.2f", night.lampPower, noon.lampPower);
    CHECK(dusk.lampPower > 0.1f && dusk.lampPower < 0.99f, "al atardecer se encienden poco a poco: %.2f", dusk.lampPower);
    CHECK(ComputeAtmosphere(12.0f, 1.0f, 0.0f).lampPower > noon.lampPower, "con lluvia oscurece y se encienden antes");

    const Atmosphere rainy = ComputeAtmosphere(12.0f, 1.0f, 0.0f);
    CHECK(Brightness(rainy.sunColor) < Brightness(noon.sunColor), "la lluvia reduce el sol");
    CHECK(rainy.fogDensity > noon.fogDensity, "la lluvia aumenta la niebla");
    const Atmosphere misty = ComputeAtmosphere(12.0f, 0.0f, 1.0f);
    CHECK(misty.fogDensity > 2.0f * noon.fogDensity, "la niebla debe notarse");
    const Atmosphere mistyNight = ComputeAtmosphere(0.0f, 0.0f, 1.0f);
    CHECK(Brightness(mistyNight.fogColor) < Brightness(misty.fogColor), "la niebla de noche es mas oscura que la de dia");

    // Continuidad: sin saltos bruscos al recorrer el dia
    ComputeAtmosphere(0.0f, 0.0f, 0.0f);
    Atmosphere previous = ComputeAtmosphere(0.0f, 0.3f, 0.2f);
    float worstStep = 0.0f;
    for (float hour = 0.0f; hour <= 24.0f; hour += 0.05f) {
        const Atmosphere current = ComputeAtmosphere(hour, 0.3f, 0.2f);
        worstStep = std::max({worstStep, Brightness(current.sunColor) - Brightness(previous.sunColor), Brightness(previous.sunColor) - Brightness(current.sunColor),
                              Brightness(current.fogColor) - Brightness(previous.fogColor), Brightness(previous.fogColor) - Brightness(current.fogColor)});
        previous = current;
    }
    CHECK(worstStep < 0.12f, "cambio brusco de luz entre dos pasos de 3 minutos: %.3f", worstStep);

    if (g_failures == 0) std::printf("Atmosphere: todo OK\n");
    return g_failures == 0 ? 0 : 1;
}
