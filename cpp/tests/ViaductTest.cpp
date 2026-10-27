// Pruebas del viaducto elevado y de las capas de movimiento (logica pura).
#include "world/Viaduct.hpp"

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
    using namespace Viaduct;

    // Alturas: a ras fuera del tablero, 7,82 m en la cresta, sin saltos y con pendiente acotada
    CHECK(Height(-200.0f) == kBase && Height(kEndZ) == kBase && Height(kEndZ + 5.0f) == kBase, "fuera del tablero va a ras");
    CHECK(std::fabs(Height(0.0f) - 7.82f) < 0.001f, "la cresta mide 7,82 m: %.3f", Height(0.0f));
    float previous = Height(kStartZ - 5.0f), worstJump = 0.0f, worstSlope = 0.0f;
    for (float z = kStartZ - 5.0f; z <= kEndZ + 5.0f; z += 0.05f) {
        const float h = Height(z);
        worstJump = std::max(worstJump, std::fabs(h - previous));
        worstSlope = std::max(worstSlope, std::fabs(Slope(z)));
        previous = h;
    }
    CHECK(worstJump < 0.03f, "salto de altura entre dos pasos de 5 cm: %.3f", worstJump);
    CHECK(worstSlope < 0.40f, "pendiente maxima: %.3f", worstSlope);
    for (float z = kStartZ; z < kCrestStartZ; z += 0.5f) CHECK(Height(z + 0.5f) >= Height(z), "la rampa norte sube de forma continua en %.1f", z);

    // Subir al tablero solo por el extremo bajo; por debajo se pasa en la cresta
    float y = 0.0f;
    bool onDeck = false;
    onDeck = StepLayer(onDeck, 0.0f, kStartZ - 1.0f, y);
    CHECK(!onDeck && y == 0.0f, "antes de la rampa se esta en la calle");
    onDeck = StepLayer(onDeck, 0.0f, kStartZ + 0.5f, y);
    CHECK(onDeck && y > 0.0f && y <= kBoardHeight, "se sube al entrar por el extremo bajo (y=%.2f)", y);
    for (float z = kStartZ + 0.5f; z < kEndZ - 0.5f; z += 0.1f) {
        onDeck = StepLayer(onDeck, 0.0f, z, y);
        CHECK(onDeck && std::fabs(y - Height(z)) < 0.001f, "se mantiene sobre el tablero hasta el final (z=%.1f)", z);
        if (!onDeck) break;
    }
    onDeck = StepLayer(onDeck, 0.0f, kEndZ + 0.5f, y);
    CHECK(!onDeck && y == 0.0f, "al salir por el extremo sur vuelve a la calle");

    bool below = false;
    below = StepLayer(below, 0.0f, 0.0f, y);
    CHECK(!below && y == 0.0f, "bajo la cresta se pasa por debajo, en la calle");
    below = StepLayer(below, 0.0f, kStartZ + 8.0f, y);
    CHECK(!below, "no se sube a la rampa por la mitad");
    CHECK(BlockedOnGround(0.0f, kStartZ + 4.0f) && !BlockedOnGround(0.0f, kStartZ + 0.5f) && !BlockedOnGround(0.0f, 0.0f),
          "el muro bajo de la rampa bloquea, el extremo y la cresta no");
    CHECK(!BlockedOnGround(kHalfWidth + 1.0f, kStartZ + 4.0f), "fuera del tablero no hay muro");

    // Barandas
    CHECK(BlockedOnDeck(kWalkHalfWidth, 0.0f, 0.35f) && !BlockedOnDeck(0.0f, 0.0f, 0.35f) && !BlockedOnDeck(5.0f, 0.0f, 0.35f), "las barandas limitan el paso");

    // Pilares: no invaden las calles transversales y no quedan pegados entre si
    const std::vector<std::pair<float, float>> streets{{-35.0f, -23.0f}, {9.0f, 21.0f}, {33.0f, 45.0f}, {77.0f, 89.0f}};
    const auto pillars = PillarPositions(streets);
    CHECK(pillars.size() >= 8, "hay pilares (%zu)", pillars.size());
    for (const glm::vec2& pillar : pillars) {
        for (const auto& street : streets) CHECK(pillar.y < street.first - 2.0f || pillar.y > street.second + 2.0f, "un pilar invade una calle en z=%.1f", pillar.y);
        CHECK(std::fabs(pillar.x) == 4.5f, "los pilares quedan bajo los carriles");
    }

    if (g_failures == 0) std::printf("Viaduct: todo OK\n");
    return g_failures == 0 ? 0 : 1;
}
