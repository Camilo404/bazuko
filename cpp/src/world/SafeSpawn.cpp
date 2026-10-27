#include "SafeSpawn.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <unordered_set>

namespace Bazuko {

namespace {

int64_t Pack(int x, int z) { return (static_cast<int64_t>(x) << 32) ^ static_cast<uint32_t>(z); }

} // namespace

bool CanEscapeFrom(const glm::vec2& start, const FreeCheck& isFree, float openRadius) {
    if (!isFree(start.x, start.y)) return false;
    // Busqueda en anchura sobre una cuadricula de 1 m (con diagonales); basta con que la region sea amplia
    // y se aleje del punto de partida: un patio cerrado, una caja o un hueco entre edificios no cuentan.
    constexpr int kOpenCells = 600;
    constexpr int kLimit = 4000;
    std::unordered_set<int64_t> visited;
    std::queue<std::pair<int, int>> pending;
    const int startX = static_cast<int>(std::lround(start.x));
    const int startZ = static_cast<int>(std::lround(start.y));
    pending.push({startX, startZ});
    visited.insert(Pack(startX, startZ));
    float farthest = 0.0f;
    int reached = 1;
    while (!pending.empty() && reached < kLimit) {
        const auto [x, z] = pending.front();
        pending.pop();
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dz == 0) continue;
                const int nx = x + dx;
                const int nz = z + dz;
                if (!visited.insert(Pack(nx, nz)).second) continue;
                if (!isFree(static_cast<float>(nx), static_cast<float>(nz))) continue;
                // una diagonal no puede colarse entre dos esquinas ocupadas
                if (dx != 0 && dz != 0 && !isFree(static_cast<float>(x + dx), static_cast<float>(z))
                    && !isFree(static_cast<float>(x), static_cast<float>(z + dz))) continue;
                pending.push({nx, nz});
                ++reached;
                farthest = std::max(farthest, std::hypot(static_cast<float>(nx - startX), static_cast<float>(nz - startZ)));
            }
        }
        if (reached >= kOpenCells && farthest >= openRadius) return true;
    }
    return reached >= kOpenCells && farthest >= openRadius;
}

glm::vec3 FindSafePlayerPosition(const glm::vec3& wanted, const FreeCheck& isFree, const glm::vec3& fallback,
                                 float searchRadius) {
    const auto safe = [&](float x, float z) { return isFree(x, z) && CanEscapeFrom({x, z}, isFree); };
    if (safe(wanted.x, wanted.z)) return wanted;
    constexpr float kPi = 3.14159265f;
    for (float radius = 0.5f; radius <= searchRadius; radius += 0.5f) {
        const int steps = std::max(8, static_cast<int>(std::ceil(radius * 8.0f)));
        for (int step = 0; step < steps; ++step) {
            const float angle = 2.0f * kPi * static_cast<float>(step) / static_cast<float>(steps);
            const float x = wanted.x + std::cos(angle) * radius;
            const float z = wanted.z + std::sin(angle) * radius;
            if (safe(x, z)) return {x, wanted.y, z};
        }
    }
    return fallback;
}

} // namespace Bazuko
