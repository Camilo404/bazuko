// Verifica la regla de aparicion segura sobre la ciudad real: desde el centro de cada edificio (y desde puntos
// repartidos por todo el mapa) la posicion resultante debe ser caminable, estar libre y tener salida.
// Necesita un contexto GL (ventana oculta) porque Simulation crea mallas. Devuelve 0 si todo pasa.
#include "core/Paths.hpp"
#include "renderer/GL.hpp"
#include "world/CityLayout.hpp"
#include "world/SafeSpawn.hpp"
#include "world/Station.hpp"
#include "world/Viaduct.hpp"
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

    // Marco del original: la baldosa (i, j) tiene su centro en (2 i + 1 - ORIGIN_X, 2 j + 1 - ORIGIN_Z)
    const float minX = -CityLayout::ORIGIN_X, maxX = CityLayout::WIDTH * CityLayout::TILE_SIZE - CityLayout::ORIGIN_X;
    const float minZ = -CityLayout::ORIGIN_Z, maxZ = CityLayout::HEIGHT * CityLayout::TILE_SIZE - CityLayout::ORIGIN_Z;
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
            const glm::vec3 center((x + 0.5f) * CityLayout::TILE_SIZE - CityLayout::ORIGIN_X, 0.0f, (z + 0.5f) * CityLayout::TILE_SIZE - CityLayout::ORIGIN_Z);
            if (isFree(center.x, center.z)) continue;
            if (++tileCount % 3 != 0) continue;
            verify(center, "baldosa bloqueada");
        }
    }
    // Puntos aleatorios por todo el mapa, incluso fuera de el
    std::mt19937 random(7);
    std::uniform_real_distribution<float> anyX(minX - 20.0f, maxX + 20.0f), anyZ(minZ - 20.0f, maxZ + 20.0f);
    for (int index = 0; index < 400; ++index) verify({anyX(random), 0.0f, anyZ(random)}, "punto aleatorio");
    // La posicion de inicio tambien debe ser segura por si sola
    verify(spawn, "punto de inicio");
    std::printf("%d posiciones revisadas, %d movidas (maximo %.1f m), peor tiempo %.1f ms, fallos %d\n", checked, moved, worstDistance, worstMs, failures);
    const bool spawnSafe = isFree(spawn.x, spawn.z) && CanEscapeFrom({spawn.x, spawn.z}, isFree);
    if (!spawnSafe) std::printf("FALLO: el punto de inicio de la ciudad no es seguro\n");
    // Ampliacion sur y hitos (ola 1): el origen del mundo no se mueve y los hitos tienen el comportamiento esperado
    int layoutFailures = 0;
    const auto expect = [&](bool condition, const char* what) {
        if (!condition) { ++layoutFailures; std::printf("FALLO (mapa): %s\n", what); }
    };
    expect(CityLayout::HEIGHT == 161 + CityLayout::NORTH_ROWS && CityLayout::ORIGIN_X == 1.0f && CityLayout::ORIGIN_Z == 75.0f,
           "el mapa es el del original: 161 filas mas 37 al norte para la estacion, con su marco de coordenadas");
    expect(std::fabs(spawn.x - 112.98f) < 0.01f && std::fabs(spawn.z - 97.84f) < 0.01f && city.IsWalkable(spawn.x, spawn.z),
           "el inicio esta junto al cambuche del original (112,98; 97,84) y es transitable");
    expect(!city.IsWalkable(60.0f, 262.0f) && !city.IsWalkable(110.0f, 262.0f), "el lago no es transitable (el corredor del puente conserva la calle)");
    expect(city.IsWalkable(75.0f, 100.0f), "el tablero y la calle bajo el viaducto siguen siendo transitables a pie");
    expect(city.IsWalkable(154.0f, 95.0f), "el lote de la gasolinera es transitable");
    expect(city.IsBlocked(48.0f, 166.0f, 0.3f), "la tienda D69 es solida");
    expect(city.GetLandmarks().size() == 6, "hay seis hitos para el radar (gasolinera, D69, pasarela, estacion, Colpatria y lago)");
    expect(city.IsBlocked(81.0f, 168.0f, 0.3f) && city.IsBlocked(98.0f, 168.0f, 0.3f), "la torre Colpatria y su anexo son solidos");
    expect(std::fabs(city.GetGroundHeight(75.0f, -30.0f) - 0.92f) < 0.001f && city.GetGroundHeight(75.0f, -61.0f) == 0.0f
           && city.GetGroundHeight(75.0f, -54.0f) > 0.0f && city.GetGroundHeight(80.0f, -30.0f) == 0.0f,
           "la isla de la estacion sube por rampas y solo existe entre los carriles");
    expect(!city.IsWalkable(75.0f, maxZ + 1.0f), "el borde sur es un muro");
    // Recorrer a pie la isla de la estacion por su eje (de la rampa norte a la sur y de vuelta): nada la bloquea y la
    // cota sube y baja sin saltos
    {
        bool blocked = false;
        float previousHeight = 0.0f, worstStep = 0.0f, peak = 0.0f;
        for (int pass = 0; pass < 2; ++pass) {
            for (float step = 0.0f; step <= 36.0f; step += 0.1f) {
                const float z = pass == 0 ? Station::kNorthEnd + 0.5f + step : Station::kSouthEnd - 0.5f - step;
                const float height = city.GetGroundHeight(Station::kCenterX, z);
                if (!city.IsWalkable(Station::kCenterX, z) || city.IsBlocked(Station::kCenterX, z, 0.35f) || simulation.IsBlocked({Station::kCenterX, height, z}, 0.35f)) blocked = true;
                if (step > 0.0f) worstStep = std::max(worstStep, std::fabs(height - previousHeight));
                peak = std::max(peak, height);
                previousHeight = height;
            }
        }
        expect(!blocked, "la isla de la estacion se recorre de punta a punta sin bloqueos");
        expect(worstStep < 0.05f && std::fabs(peak - Station::kPlatformHeight) < 0.001f, "la isla sube y baja por rampas suaves hasta 0,92 m");
    }
    // Elementos fijos de la calle: farolas y postes de semaforo sobre andenes libres (ni calzada, ni tablero), puestos
    // sobre suelo transitable y con el vendedor accesible
    {
        int badLamps = 0, badPoles = 0, badVendors = 0;
        const auto lamps = simulation.StreetLampPositions();
        for (const glm::vec3& lamp : lamps) {
            if (!city.IsSidewalk(lamp.x, lamp.z) || city.IsRoad(lamp.x, lamp.z) || Viaduct::Covers(lamp.x, lamp.z)) {
                ++badLamps; std::printf("  farola mal ubicada en (%.1f, %.1f)\n", lamp.x, lamp.z);
            }
        }
        const auto poles = simulation.TrafficSignalPoles();
        for (const glm::vec3& pole : poles) {
            if (!city.IsSidewalk(pole.x, pole.z) || city.IsBlocked(pole.x, pole.z, 0.2f)) {
                ++badPoles; std::printf("  semaforo mal ubicado en (%.1f, %.1f)\n", pole.x, pole.z);
            }
        }
        for (const StreetVendor& vendor : simulation.GetStreetVendors()) {
            const bool stallOk = city.IsWalkable(vendor.position.x, vendor.position.z) && !city.IsRoad(vendor.position.x, vendor.position.z);
            const bool sellerOk = city.IsWalkable(vendor.serverPosition.x, vendor.serverPosition.z) && !city.IsBlocked(vendor.serverPosition.x, vendor.serverPosition.z, 0.2f);
            if (!stallOk || !sellerOk) { ++badVendors; std::printf("  puesto '%s' mal ubicado en (%.1f, %.1f) puesto=%d vendedor=%d\n", vendor.name.c_str(), vendor.position.x, vendor.position.z, stallOk, sellerOk); }
        }
        std::printf("Elementos fijos: %zu farolas, %zu semaforos, %zu puestos; mal ubicados: %d/%d/%d\n",
                    lamps.size(), poles.size(), simulation.GetStreetVendors().size(), badLamps, badPoles, badVendors);
        expect(lamps.size() >= 30 && !poles.empty(), "hay farolas repartidas y semaforos");
        expect(badLamps == 0 && badPoles == 0 && badVendors == 0, "farolas, semaforos y puestos estan en sitios validos");
    }
    std::printf("Ampliacion del mapa: %d fallos\n", layoutFailures);
    failures += layoutFailures;
    glfwDestroyWindow(window);
    glfwTerminate();
    return failures == 0 && spawnSafe ? 0 : 1;
}

