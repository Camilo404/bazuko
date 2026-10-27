// Verifica la regla de aparicion segura sobre la ciudad real: desde el centro de cada edificio (y desde puntos
// repartidos por todo el mapa) la posicion resultante debe ser caminable, estar libre y tener salida.
// Necesita un contexto GL (ventana oculta) porque Simulation crea mallas. Devuelve 0 si todo pasa.
#include "core/Paths.hpp"
#include "renderer/GL.hpp"
#include "world/CityLayout.hpp"
#include "world/SafeSpawn.hpp"
#include "world/Simulation.hpp"

#include <GLFW/glfw3.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>

using namespace Bazuko;

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Paths::Init(0, nullptr);
    if (!glfwInit()) { std::printf("no se pudo iniciar GLFW\n"); return 2; }
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(64, 64, "spawn-test", nullptr, nullptr);
    if (!window) { std::printf("no se pudo crear el contexto GL\n"); return 2; }
    glfwMakeContextCurrent(window);
    if (gladLoadGL(glfwGetProcAddress) == 0) { std::printf("fallo GLAD\n"); return 2; }

    CityLayout city;
    Simulation simulation(city, 42);
    const FreeCheck isFree = [&](float x, float z) { return city.IsWalkable(x, z) && !simulation.IsBlocked({x, 0.0f, z}, 0.5f); };
    const glm::vec3 spawn = city.GetSpawnPoint();

    const float halfWidth = CityLayout::WIDTH * CityLayout::TILE_SIZE * 0.5f;
    const float halfHeight = CityLayout::HEIGHT * CityLayout::TILE_SIZE * 0.5f;
    int checked = 0, moved = 0, failures = 0;
    double worstMs = 0.0, worstDistance = 0.0;
    const auto verify = [&](const glm::vec3& wanted, const char* what) {
        const auto start = std::chrono::steady_clock::now();
        const glm::vec3 result = FindSafePlayerPosition(wanted, isFree, spawn);
        worstMs = std::max(worstMs, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
        ++checked;
        const double distance = std::hypot(result.x - wanted.x, result.z - wanted.z);
        if (distance > 0.05) ++moved;
        worstDistance = std::max(worstDistance, distance);
        if (!isFree(result.x, result.z) || !CanEscapeFrom({result.x, result.z}, isFree)) {
            ++failures;
            std::printf("FALLO (%s): (%.1f, %.1f) -> (%.1f, %.1f) sigue bloqueada o sin salida\n", what, wanted.x, wanted.z, result.x, result.z);
        }
    };

    // Centro de edificios (uno de cada tres para acotar el tiempo) y de todos los tipos de baldosa no caminable
    int tileCount = 0;
    for (int z = 0; z < CityLayout::HEIGHT; ++z) {
        for (int x = 0; x < CityLayout::WIDTH; ++x) {
            const glm::vec3 center((x + 0.5f) * CityLayout::TILE_SIZE - halfWidth, 0.0f, (z + 0.5f) * CityLayout::TILE_SIZE - halfHeight);
            if (isFree(center.x, center.z)) continue;
            if (++tileCount % 3 != 0) continue;
            verify(center, "baldosa bloqueada");
        }
    }
    // Puntos aleatorios por todo el mapa, incluso fuera de el
    std::mt19937 random(7);
    std::uniform_real_distribution<float> anyX(-halfWidth - 20.0f, halfWidth + 20.0f), anyZ(-halfHeight - 20.0f, halfHeight + 20.0f);
    for (int index = 0; index < 400; ++index) verify({anyX(random), 0.0f, anyZ(random)}, "punto aleatorio");
    // La posicion de inicio tambien debe ser segura por si sola
    verify(spawn, "punto de inicio");
    std::printf("%d posiciones revisadas, %d movidas (maximo %.1f m), peor tiempo %.1f ms, fallos %d\n", checked, moved, worstDistance, worstMs, failures);
    const bool spawnSafe = isFree(spawn.x, spawn.z) && CanEscapeFrom({spawn.x, spawn.z}, isFree);
    if (!spawnSafe) std::printf("FALLO: el punto de inicio de la ciudad no es seguro\n");
    glfwDestroyWindow(window);
    glfwTerminate();
    return failures == 0 && spawnSafe ? 0 : 1;
}

