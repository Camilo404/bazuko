// Prueba de trafico sobre la simulacion real: detecta carros que quedan detenidos demasiado tiempo
// (atascos en intersecciones). Necesita un contexto GL (ventana oculta) porque Simulation crea mallas.
// Uso: bazuko_traffic_test [segundos] [semilla]   -> devuelve 0 si no hay atascos.
#include "core/Paths.hpp"
#include "renderer/GL.hpp"
#include "world/CityLayout.hpp"
#include "world/Simulation.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace Bazuko;

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const float totalSeconds = argc > 1 ? static_cast<float>(std::atof(argv[1])) : 900.0f;
    const uint32_t seed = argc > 2 ? static_cast<uint32_t>(std::atoi(argv[2])) : 42u;
    Paths::Init(0, nullptr);

    if (!glfwInit()) { std::printf("no se pudo iniciar GLFW\n"); return 2; }
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(64, 64, "traffic-test", nullptr, nullptr);
    if (!window) { std::printf("no se pudo crear el contexto GL\n"); return 2; }
    glfwMakeContextCurrent(window);
    if (gladLoadGL(glfwGetProcAddress) == 0) { std::printf("fallo GLAD\n"); return 2; }

    CityLayout city;
    Simulation simulation(city, seed);
    const glm::vec3 farPlayer(1000.0f, 0.0f, 1000.0f);   // sin peatones ni jugador cerca de los carros

    constexpr float dt = 1.0f / 30.0f;
    constexpr float stuckSeconds = 45.0f;   // un semaforo en rojo dura ~22 s; con cola, algo mas
    const size_t count = simulation.GetTrafficCount();
    std::vector<float> stopped(count, 0.0f), worst(count, 0.0f);
    std::vector<char> reported(count, 0);
    float movedTotal = 0.0f;
    int stuckEvents = 0;
    float busStopped = 0.0f, busWorst = 0.0f;
    long overlapFrames = 0, frames = 0;
    for (float t = 0.0f; t < totalSeconds; t += dt) {
        simulation.Update(dt, farPlayer);
        const auto& cars = simulation.GetTrafficVehicles();
        // el bus se detiene 4 s en la estacion y por semaforos/peatones no; mas de 45 s seguidos es un atasco
        busStopped = simulation.GetBusSpeed() < 0.05f ? busStopped + dt : 0.0f;
        busWorst = std::max(busWorst, busStopped);
        // carros cuyos centros estan a menos de 2 m (la longitud es 4.3 m) estan atravesados entre si
        ++frames;
        bool anyOverlap = false;
        for (size_t a = 0; a < count && !anyOverlap; ++a)
            for (size_t b = a + 1; b < count; ++b)
                if (glm::distance(cars[a].position, cars[b].position) < 2.0f) {
                    anyOverlap = true;
                    if (std::getenv("TRAFFIC_VERBOSE") && static_cast<int>(t * 30.0f) % 30 == 0)
                        std::printf("  solape t=%.0f carros %zu(ruta %zu) y %zu(ruta %zu) en (%.1f, %.1f) v=%.1f/%.1f%c", t, a, cars[a].routeIndex, b, cars[b].routeIndex,
                                    cars[a].position.x, cars[a].position.z, cars[a].speed, cars[b].speed, 10);
                    break;
                }
        if (anyOverlap) ++overlapFrames;
        for (size_t i = 0; i < count; ++i) {
            movedTotal += glm::distance(cars[i].position, cars[i].previousPosition);
            stopped[i] = cars[i].speed < 0.1f ? stopped[i] + dt : 0.0f;
            worst[i] = std::max(worst[i], stopped[i]);
            if (stopped[i] > stuckSeconds && !reported[i]) {
                reported[i] = 1;
                ++stuckEvents;
                std::printf("t=%.0fs: carro %zu detenido %.0f s en (%.1f, %.1f) rumbo (%.1f, %.1f)\n", t, i, stopped[i],
                            cars[i].position.x, cars[i].position.z, cars[i].direction.x, cars[i].direction.z);
                for (size_t j = 0; j < count; ++j) {
                    if (j == i || glm::distance(cars[j].position, cars[i].position) > 14.0f) continue;
                    std::printf("    cerca: carro %zu ruta %zu en (%.1f, %.1f) rumbo (%.1f, %.1f) v=%.1f detenido %.0f s\n", j, cars[j].routeIndex,
                                cars[j].position.x, cars[j].position.z, cars[j].direction.x, cars[j].direction.z, cars[j].speed, cars[j].stalledTime);
                }
                for (const auto& parked : simulation.GetParkedCars())
                    if (glm::distance(parked.position, cars[i].position) < 14.0f)
                        std::printf("    cerca: estacionado en (%.1f, %.1f)\n", parked.position.x, parked.position.z);
            }
            if (stopped[i] <= 1.0f) reported[i] = 0;
        }
    }
    float globalWorst = 0.0f;
    for (float w : worst) globalWorst = std::max(globalWorst, w);
    std::printf("bus: espera maxima %.0f s; carros atravesados en %.2f%% del tiempo\n", busWorst,
                100.0 * static_cast<double>(overlapFrames) / static_cast<double>(std::max(1L, frames)));
    if (busWorst > stuckSeconds) { std::printf("el bus se quedo detenido %.0f s%c", busWorst, 10); ++stuckEvents; }
    std::printf("semilla %u, %.0f s, %zu carros: recorrido total %.0f m, espera maxima %.0f s, atascos %d\n",
                seed, totalSeconds, count, movedTotal, globalWorst, stuckEvents);
    glfwDestroyWindow(window);
    glfwTerminate();
    return stuckEvents == 0 ? 0 : 1;
}
