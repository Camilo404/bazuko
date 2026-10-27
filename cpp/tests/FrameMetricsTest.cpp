// Pruebas de FrameMetrics (mismas reglas que FrameMetrics.js del juego web).
#include "core/FrameMetrics.hpp"

#include <cmath>
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

int main() {
    FrameMetrics metrics;
    CHECK(!metrics.Sample(0.0), "la primera muestra solo fija la referencia");
    double now = 0.0;
    for (int frame = 0; frame < 200; ++frame) {
        now += 16.0;
        CHECK(metrics.Sample(now), "cuadro %d", frame);
    }
    CHECK(metrics.frames == 200 && std::fabs(metrics.current - 62.5f) < 1e-3f && std::fabs(metrics.average - 62.5f) < 1e-3f,
          "62,5 FPS constantes: %.2f %.2f", metrics.current, metrics.average);

    now += 250.0;   // un tiron de 250 ms
    metrics.Sample(now);
    CHECK(std::fabs(metrics.min - 4.0f) < 1e-3f && metrics.max > 62.0f, "minimo %.2f", metrics.min);
    const FrameMetrics::Recent recent = metrics.RecentStats();
    CHECK(recent.frames == 201 && recent.over100ms == 1, "cuadros %zu, tirones %d", recent.frames, recent.over100ms);
    CHECK(std::fabs(recent.p95 - 16.0f) < 1e-3f && std::fabs(recent.p99 - 16.0f) < 1e-3f, "p95 %.1f p99 %.1f", recent.p95, recent.p99);
    // 201 cuadros: el 1 % son los 3 mas lentos (250, 16 y 16 ms)
    CHECK(std::fabs(recent.low1Percent - 1000.0f / ((250.0f + 16.0f + 16.0f) / 3.0f)) < 0.05f, "1 %% bajos %.2f", recent.low1Percent);

    // En pausa el intervalo se descarta: la primera muestra tras reanudar no cuenta
    metrics.Sample(now + 5000.0, false);
    const size_t before = metrics.frames;
    CHECK(!metrics.Sample(now + 9000.0), "tras la pausa no se mide el intervalo largo");
    CHECK(metrics.frames == before, "los cuadros no cambian");

    // La ventana guarda como maximo 600 cuadros
    for (int frame = 0; frame < 1000; ++frame) {
        now += 10.0;
        metrics.Sample(now + 9000.0);
    }
    CHECK(metrics.RecentStats().frames == 600, "ventana de 600 cuadros: %zu", metrics.RecentStats().frames);

    metrics.Reset();
    CHECK(metrics.frames == 0 && metrics.current == 0.0f && metrics.RecentStats().frames == 0, "Reset");

    if (g_failures == 0) std::printf("FrameMetrics: todo OK\n");
    else std::printf("FrameMetrics: %d fallos\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
