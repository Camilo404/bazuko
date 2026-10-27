// Pruebas de la regla de aparicion segura (logica pura: sin ciudad ni GPU).
#include "world/SafeSpawn.hpp"

#include <chrono>
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

// Mundo de prueba de 300 x 300 m con un borde cerrado:
//  - edificio macizo en [40,70] x [40,70]
//  - recinto cerrado (patio libre de 11 x 11 con muros de 2 m) en [100,114] x [100,114]
static bool World(float x, float z) {
    if (x < 1 || z < 1 || x > 299 || z > 299) return false;
    if (x >= 40 && x <= 70 && z >= 40 && z <= 70) return false;
    const bool inEnclosure = x >= 100 && x <= 114 && z >= 100 && z <= 114;
    if (inEnclosure) {
        const bool wall = x < 102 || x > 112 || z < 102 || z > 112;
        return !wall;
    }
    return true;
}

// World mas un callejon de 1 m de ancho y 40 m de largo (x = 150) cerrado por todos lados salvo el principio
static bool Alley(float x, float z) {
    if (!World(x, z)) return false;
    const bool alleyBox = x >= 147 && x <= 153 && z >= 150 && z <= 192;
    if (!alleyBox) return true;
    return std::fabs(x - 150.0f) < 0.6f && z >= 152.0f && z <= 190.0f && false;   // todo macizo salvo la franja (abajo)
}

// Callejon real: la franja central existe pero esta cerrada en ambos extremos
static bool DeadEnd(float x, float z) {
    if (!World(x, z)) return false;
    const bool box = x >= 147 && x <= 153 && z >= 150 && z <= 192;
    if (!box) return true;
    return std::fabs(x - 150.0f) < 0.6f && z >= 152.0f && z <= 190.0f;
}

int main() {
    const glm::vec3 fallback(10.0f, 0.0f, 10.0f);
    const FreeCheck world = World;
    (void)Alley;

    // Una posicion libre y abierta no se toca (y conserva la altura)
    {
        const glm::vec3 wanted(20.0f, 1.5f, 20.0f);
        const glm::vec3 result = FindSafePlayerPosition(wanted, world, fallback);
        CHECK(result == wanted, "posicion libre movida a %.1f,%.1f", result.x, result.z);
    }

    // Dentro de un edificio: se saca al exterior, lo mas cerca posible, y desde ahi se puede salir
    {
        const glm::vec3 wanted(55.0f, 0.0f, 55.0f);
        const glm::vec3 result = FindSafePlayerPosition(wanted, world, fallback);
        CHECK(World(result.x, result.z) && CanEscapeFrom({result.x, result.z}, world), "sigue bloqueada: %.1f,%.1f", result.x, result.z);
        CHECK(std::hypot(result.x - wanted.x, result.z - wanted.z) < 17.0f, "deberia salir por el lado mas cercano: %.1f m",
              std::hypot(result.x - wanted.x, result.z - wanted.z));
    }

    // Dentro de un recinto cerrado (libre pero sin salida): tambien se mueve al exterior
    {
        const glm::vec3 wanted(107.0f, 0.0f, 107.0f);
        CHECK(World(wanted.x, wanted.z) && !CanEscapeFrom({wanted.x, wanted.z}, world), "el recinto es libre pero no tiene salida");
        const glm::vec3 result = FindSafePlayerPosition(wanted, world, fallback);
        const bool outside = result.x < 100 || result.x > 114 || result.z < 100 || result.z > 114;
        CHECK(outside && CanEscapeFrom({result.x, result.z}, world), "sigue encerrada: %.1f,%.1f", result.x, result.z);
    }

    // Callejon sin salida de 1 m: no es un lugar seguro
    {
        const FreeCheck deadEnd = DeadEnd;
        const glm::vec3 wanted(150.0f, 0.0f, 170.0f);
        CHECK(deadEnd(wanted.x, wanted.z) && !CanEscapeFrom({wanted.x, wanted.z}, deadEnd), "el callejon cerrado no tiene salida");
        const glm::vec3 result = FindSafePlayerPosition(wanted, deadEnd, fallback);
        CHECK(CanEscapeFrom({result.x, result.z}, deadEnd), "deberia dejarla en un sitio con salida: %.1f,%.1f", result.x, result.z);
    }

    // Fuera del mapa: vuelve al punto de respaldo; en el borde, a una posicion valida cercana
    {
        const glm::vec3 result = FindSafePlayerPosition({-500.0f, 0.0f, 700.0f}, world, fallback);
        CHECK(result == fallback, "fuera del mapa y a mas de 80 m: usa el respaldo (%.1f,%.1f)", result.x, result.z);
        const glm::vec3 edge = FindSafePlayerPosition({-3.0f, 0.0f, 20.0f}, world, fallback);
        CHECK(World(edge.x, edge.z) && CanEscapeFrom({edge.x, edge.z}, world), "borde del mapa");
    }

    // Un mundo totalmente macizo cae en el respaldo
    {
        const FreeCheck solid = [](float, float) { return false; };
        CHECK(FindSafePlayerPosition({5.0f, 0.0f, 5.0f}, solid, fallback) == fallback, "mundo macizo");
    }

    // Costo: buscar desde el centro de un edificio grande tarda poco
    {
        const auto start = std::chrono::steady_clock::now();
        for (int run = 0; run < 20; ++run) FindSafePlayerPosition({55.0f, 0.0f, 55.0f}, world, fallback);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 20.0;
        std::printf("busqueda desde el centro de un edificio: %.2f ms\n", ms);
        CHECK(ms < 50.0, "demasiado lento: %.1f ms", ms);
    }

    if (g_failures == 0) std::printf("SafeSpawn: todo OK\n");
    else std::printf("SafeSpawn: %d fallos\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
