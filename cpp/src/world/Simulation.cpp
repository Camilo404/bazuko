#include "Simulation.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <queue>
#include <unordered_map>

namespace Bazuko {

namespace {

std::vector<glm::vec3> MakeTrafficCircuit(float left, float right, float top, float bottom) {
    constexpr float radius = 1.0f;
    constexpr float spacing = 0.45f;
    std::vector<glm::vec3> points;
    const auto appendLine = [&](const glm::vec3& start, const glm::vec3& end) {
        const float length = glm::distance(start, end);
        const int steps = std::max(1, static_cast<int>(std::ceil(length / spacing)));
        for (int step = points.empty() ? 0 : 1; step <= steps; ++step)
            points.push_back(glm::mix(start, end, static_cast<float>(step) / steps));
    };
    const auto appendArc = [&](float centerX, float centerZ, float startAngle) {
        const float arcLength = glm::half_pi<float>() * radius;
        const int steps = std::max(1, static_cast<int>(std::ceil(arcLength / spacing)));
        for (int step = points.empty() ? 0 : 1; step <= steps; ++step) {
            const float angle = startAngle + glm::half_pi<float>() * static_cast<float>(step) / steps;
            points.emplace_back(centerX + std::cos(angle) * radius, 0.0f,
                                centerZ + std::sin(angle) * radius);
        }
    };
    appendLine({left + radius, 0.0f, top}, {right - radius, 0.0f, top});
    appendArc(right - radius, top + radius, -glm::half_pi<float>());
    appendLine({right, 0.0f, top + radius}, {right, 0.0f, bottom - radius});
    appendArc(right - radius, bottom - radius, 0.0f);
    appendLine({right - radius, 0.0f, bottom}, {left + radius, 0.0f, bottom});
    appendArc(left + radius, bottom - radius, glm::half_pi<float>());
    appendLine({left, 0.0f, bottom - radius}, {left, 0.0f, top + radius});
    appendArc(left + radius, top + radius, glm::pi<float>());
    return points;
}

float GetCircuitLength(const std::vector<glm::vec3>& points) {
    float length = 0.0f;
    for (size_t index = 1; index < points.size(); ++index) length += glm::distance(points[index - 1], points[index]);
    return length + glm::distance(points.back(), points.front());
}

void SampleCircuit(const std::vector<glm::vec3>& points, const std::vector<float>& cumulative,
                   float routeLength, float distance, glm::vec3& position, glm::vec3& direction,
                   size_t& nextWaypoint) {
    float remaining = std::fmod(distance, routeLength);
    if (remaining < 0.0f) remaining += routeLength;
    const auto upper = std::upper_bound(cumulative.begin(), cumulative.end(), remaining);
    const size_t index = upper == cumulative.begin() ? 0 : static_cast<size_t>(upper - cumulative.begin() - 1);
    nextWaypoint = (index + 1) % points.size();
    direction = glm::normalize(points[nextWaypoint] - points[index]);
    position = points[index] + direction * (remaining - cumulative[index]);
}

}

Simulation::Simulation(const CityLayout& city, uint32_t seed) : m_City(city), m_Random(seed) {
    m_PedestrianBody = Mesh::CreateCube({0.34f, 1.05f, 0.25f}, {0.48f, 0.34f, 0.25f, 1.0f});
    m_PedestrianHead = Mesh::CreateCube({0.24f, 0.25f, 0.24f}, {0.62f, 0.46f, 0.34f, 1.0f});
    const std::array<std::string, 4> assetRoots{"public/models/", "../public/models/", "../../public/models/", "../../../public/models/"};
    std::string modelsRoot;
    for (const auto& root : assetRoots) if (std::filesystem::exists(root + "animaciones-prueba.glb")) { modelsRoot = root; break; }
    if (!modelsRoot.empty()) {
        m_PedestrianAnimation = Model::LoadFromFile(modelsRoot + "animaciones-prueba.glb");
        const std::array<std::array<std::string, 4>, 12> outfits{{
            {{"hombre-joven", "chaqueta", "tenis-rotos", "corto"}},
            {{"hombre-joven", "buzo", "tenis", "crespo"}},
            {{"hombre-adulto", "buzo", "zapatos", "desordenado"}},
            {{"hombre-adulto", "camisa", "botas", "largo"}},
            {{"hombre-mayor", "saco", "botas", "rapado"}},
            {{"hombre-mayor", "overol", "tenis", "candado"}},
            {{"mujer-joven", "sudadera", "tenis", "largo"}},
            {{"mujer-joven", "blusa", "botas", "crespo"}},
            {{"mujer-adulta", "blusa", "tenis", "corto"}},
            {{"mujer-adulta", "sudadera", "botas", "melena"}},
            {{"mujer-mayor", "saco", "botas", "melena"}},
            {{"mujer-mayor", "sudadera", "tenis-rotos", "rapado"}}
        }};
        std::unordered_map<std::string, std::shared_ptr<Model>> modelCache;
        for (const auto& outfit : outfits) {
            PedestrianAppearance appearance;
            for (size_t partIndex = 0; partIndex < outfit.size(); ++partIndex) {
                const std::string filename = partIndex == 0 ? "cuerpo.gltf" : outfit[partIndex] + ".gltf";
                const std::string path = modelsRoot + "personaje/" + outfit[0] + "/" + filename;
                auto found = modelCache.find(path);
                std::shared_ptr<Model> model;
                if (found != modelCache.end()) model = found->second;
                else {
                    model = Model::LoadFromFile(path);
                    if (model && m_PedestrianAnimation) model->SetAnimationSource(*m_PedestrianAnimation);
                    if (model) modelCache.emplace(path, model);
                }
                appearance.parts.push_back(std::move(model));
            }
            if (!appearance.parts.empty() && appearance.parts.front()) {
                appearance.walkClip = appearance.parts.front()->FindAnimationClip("Walk");
                appearance.idleClip = appearance.parts.front()->FindAnimationClip("Idle");
                m_PedestrianAppearances.push_back(std::move(appearance));
            }
        }
    }
    m_CarBody = Mesh::CreateRoundedBox({4.3f, 0.78f, 1.76f}, 0.22f, 3);
    m_CarCabin = Mesh::CreateRoundedBox({2.25f, 0.62f, 1.62f}, 0.20f, 3, {0.035f, 0.045f, 0.06f, 1.0f});
    m_CarGlass = Mesh::CreateCube({0.08f, 0.48f, 1.52f}, {0.22f, 0.32f, 0.42f, 1.0f});
    m_CarRoof = Mesh::CreateRoundedBox({1.7f, 0.08f, 1.56f}, 0.04f, 2);
    m_CarWheel = Mesh::CreateCylinder(0.33f, 0.24f, 18, {0.067f, 0.067f, 0.067f, 1.0f});
    m_CarRim = Mesh::CreateCylinder(0.20f, 0.25f, 12, {0.54f, 0.56f, 0.59f, 1.0f});
    m_CarLamp = Mesh::CreateCube({0.07f, 0.16f, 0.34f}, {1.0f, 0.88f, 0.64f, 1.0f});
    m_BusBody = Mesh::CreateRoundedBox({2.5f, 2.8f, 9.0f}, 0.18f, 3, {0.70f, 0.15f, 0.12f, 1.0f});
    m_BusWindows = Mesh::CreateCube({0.05f, 0.95f, 8.0f}, {0.08f, 0.12f, 0.16f, 1.0f});
    m_BusWindshield = Mesh::CreateCube({2.25f, 1.0f, 0.05f}, {0.08f, 0.12f, 0.16f, 1.0f});
    m_BusJoint = Mesh::CreateCylinder(1.2f, 0.4f, 16, {0.10f, 0.10f, 0.10f, 1.0f});
    m_BusWheel = Mesh::CreateCylinder(0.5f, 0.3f, 14, {0.067f, 0.067f, 0.067f, 1.0f});
    m_BusRim = Mesh::CreateCylinder(0.22f, 0.32f, 12, {0.54f, 0.56f, 0.59f, 1.0f});
    m_BusPlatform = Mesh::CreateCube({2.6f, 0.9f, 16.0f}, {0.60f, 0.60f, 0.58f, 1.0f});
    m_BusStationGlass = Mesh::CreateCube({0.05f, 1.6f, 16.0f}, {0.66f, 0.78f, 0.84f, 1.0f});
    m_BusCanopy = Mesh::CreateCube({3.2f, 0.12f, 17.0f}, {0.75f, 0.22f, 0.18f, 1.0f});
    m_BusStationPost = Mesh::CreateCylinder(0.07f, 2.4f, 8, {0.68f, 0.68f, 0.67f, 1.0f});
    m_BusStationSign = Mesh::CreateCube({2.8f, 0.7f, 0.12f}, {0.70f, 0.12f, 0.10f, 1.0f});
    m_CarTaxiSign = Mesh::CreateCube({0.6f, 0.18f, 0.3f}, {1.0f, 0.92f, 0.69f, 1.0f});
    m_StreetLampPole = Mesh::CreateCube({0.10f, 3.2f, 0.10f}, {0.22f, 0.23f, 0.24f, 1.0f});
    m_StreetLampHead = Mesh::CreateCube({0.44f, 0.10f, 0.18f}, {0.88f, 0.76f, 0.48f, 1.0f});
    m_MissionMarker = Mesh::CreateCube({0.42f, 1.5f, 0.42f}, {0.92f, 0.64f, 0.12f, 1.0f});
    m_PickupMeshes = {
        Mesh::CreateCube({0.24f, 0.26f, 0.24f}, {0.68f, 0.72f, 0.75f, 1.0f}),
        Mesh::CreateCube({0.20f, 0.32f, 0.20f}, {0.22f, 0.58f, 0.34f, 1.0f}),
        Mesh::CreateCube({0.38f, 0.18f, 0.28f}, {0.57f, 0.42f, 0.25f, 1.0f}),
        Mesh::CreateCube({0.36f, 0.28f, 0.28f}, {0.37f, 0.39f, 0.42f, 1.0f}),
        Mesh::CreateCube({0.30f, 0.14f, 0.12f}, {0.75f, 0.47f, 0.18f, 1.0f}),
        Mesh::CreateCube({0.28f, 0.15f, 0.16f}, {0.67f, 0.55f, 0.39f, 1.0f}),
        Mesh::CreateCube({0.20f, 0.34f, 0.20f}, {0.22f, 0.48f, 0.76f, 1.0f}),
        Mesh::CreateCube({0.18f, 0.24f, 0.18f}, {0.78f, 0.67f, 0.28f, 1.0f})
    };
    for (int z = 2; z < CityLayout::HEIGHT - 2; z += 7) {
        for (int x = 2; x < CityLayout::WIDTH - 2; x += 7) {
            bool placed = false;
            for (int dz = 0; dz < 7 && !placed; ++dz) for (int dx = 0; dx < 7 && !placed; ++dx) {
                const int tx = x + dx, tz = z + dz;
                if (m_City.GetTileAt(tx, tz) != TileType::Sidewalk) continue;
                const bool adjacentToRoad = m_City.GetTileAt(tx + 1, tz) == TileType::Road || m_City.GetTileAt(tx - 1, tz) == TileType::Road ||
                    m_City.GetTileAt(tx, tz + 1) == TileType::Road || m_City.GetTileAt(tx, tz - 1) == TileType::Road;
                if (!adjacentToRoad) continue;
                m_StreetLamps.emplace_back((tx + 0.5f) * CityLayout::TILE_SIZE - CityLayout::WIDTH, 0.0f,
                    (tz + 0.5f) * CityLayout::TILE_SIZE - CityLayout::HEIGHT);
                placed = true;
            }
        }
    }
    const auto worldTile = [](float tile, int count) {
        return (tile + 0.5f) * CityLayout::TILE_SIZE - count * CityLayout::TILE_SIZE * 0.5f;
    };
    const auto laneX = [&](int first, int last, int side) {
        return worldTile((first + last) * 0.5f + side * (last - first + 1) * 0.25f, CityLayout::WIDTH);
    };
    const auto laneZ = [&](int first, int last, int side) {
        return worldTile((first + last) * 0.5f + side * (last - first + 1) * 0.25f, CityLayout::HEIGHT);
    };
    const std::array<std::pair<int, int>, 4> carreras{{
        {18, 23}, {36, 41}, {84, 89}, {106, 111}
    }};
    const std::array<std::pair<int, int>, 2> caracas{{{53, 56}, {63, 66}}};
    const std::array<std::pair<int, int>, 5> calles{{
        {16, 21}, {38, 43}, {60, 65}, {72, 77}, {94, 99}
    }};
    const float top = laneZ(calles[0].first, calles[0].second, 1);
    const float bottom = laneZ(calles[1].first, calles[1].second, -1);
    const float farBottom = laneZ(calles.back().first, calles.back().second, -1);
    const float upperMiddle = laneZ(calles[1].first, calles[1].second, 1);
    const std::array<std::array<float, 4>, 6> routeBounds{{
        {{laneX(carreras[0].first, carreras[0].second, 1), laneX(caracas[0].first, caracas[0].second, -1), top, bottom}},
        {{laneX(caracas[1].first, caracas[1].second, 1), laneX(carreras[1].first, carreras[1].second, -1), top, bottom}},
        {{laneX(carreras[1].first, carreras[1].second, 1), laneX(carreras[2].first, carreras[2].second, -1), top, farBottom}},
        {{laneX(carreras[2].first, carreras[2].second, 1), laneX(carreras[3].first, carreras[3].second, -1), top, farBottom}},
        {{laneX(carreras[0].first, carreras[0].second, 1), laneX(caracas[0].first, caracas[0].second, -1), upperMiddle, farBottom}},
        {{laneX(caracas[1].first, caracas[1].second, 1), laneX(carreras[1].first, carreras[1].second, -1), upperMiddle, farBottom}}
    }};
    m_BusStationX = worldTile((57.0f + 62.0f) * 0.5f, CityLayout::WIDTH);
    m_BusPosition = {m_BusStationX + 3.5f, 0.02f,
                     worldTile(static_cast<float>(CityLayout::HEIGHT - 2), CityLayout::HEIGHT) - 9.1f};
    m_BusStartZ = m_BusPosition.z;
    m_BusEndZ = worldTile(1.0f, CityLayout::HEIGHT) + 9.1f;
    m_BusStationZ = worldTile(21.5f, CityLayout::HEIGHT);
    for (const auto& bounds : routeBounds) {
        const float left = std::min(bounds[0], bounds[1]);
        const float right = std::max(bounds[0], bounds[1]);
        const float routeTop = std::min(bounds[2], bounds[3]);
        const float routeBottom = std::max(bounds[2], bounds[3]);
        if (right - left <= 2.0f || routeBottom - routeTop <= 2.0f) continue;
        m_TrafficRoutes.push_back(MakeTrafficCircuit(left, right, routeTop, routeBottom));
    }
    for (size_t routeIndex = 0; routeIndex < m_TrafficRoutes.size(); ++routeIndex) {
        const auto& route = m_TrafficRoutes[routeIndex];
        const float routeLength = GetCircuitLength(route);
        const size_t count = std::min<size_t>(4, static_cast<size_t>(routeLength / 12.0f));
        for (size_t index = 0; index < count; ++index) {
            TrafficVehicle vehicle;
            vehicle.route = route;
            vehicle.routeIndex = routeIndex;
            vehicle.routeLength = routeLength;
            vehicle.routeCumulative.resize(route.size());
            for (size_t point = 1; point < route.size(); ++point)
                vehicle.routeCumulative[point] = vehicle.routeCumulative[point - 1] + glm::distance(route[point - 1], route[point]);
            const float distance = (static_cast<float>(index) + 0.4f) * routeLength / count;
            vehicle.distance = distance;
            vehicle.nextDistance = distance;
            SampleCircuit(vehicle.route, vehicle.routeCumulative, vehicle.routeLength, distance,
                          vehicle.position, vehicle.direction, vehicle.nextWaypoint);
            vehicle.horizontalLane = std::abs(vehicle.direction.x) > std::abs(vehicle.direction.z);
            vehicle.laneCoordinate = vehicle.horizontalLane ? vehicle.position.z : vehicle.position.x;
            vehicle.yaw = std::atan2(-vehicle.direction.z, vehicle.direction.x);
            vehicle.cruiseSpeed = 3.5f + m_Unit(m_Random) * 1.5f;
            vehicle.speed = 0.0f;
            vehicle.bus = false;
            vehicle.length = 4.3f;
            vehicle.width = 1.76f;
            const std::array<glm::vec3, 8> paints{{
                {0.91f, 0.91f, 0.90f}, {0.73f, 0.75f, 0.77f}, {0.11f, 0.12f, 0.13f}, {0.48f, 0.08f, 0.08f},
                {0.12f, 0.24f, 0.42f}, {0.30f, 0.35f, 0.23f}, {0.55f, 0.56f, 0.57f}, {0.82f, 0.79f, 0.70f}
            }};
            vehicle.taxi = m_Unit(m_Random) < 0.3f;
            vehicle.color = vehicle.taxi ? glm::vec3(0.949f, 0.765f, 0.0f)
                : paints[static_cast<size_t>(m_Unit(m_Random) * paints.size()) % paints.size()];
            m_Traffic.push_back(std::move(vehicle));
            m_CarCooldowns.push_back(0.0f);
        }
    }
}

bool Simulation::FindSidewalkPoint(glm::vec3& point, const glm::vec3& center, float radius) {
    for (int attempt = 0; attempt < 60; ++attempt) {
        const float angle = m_Unit(m_Random) * glm::two_pi<float>();
        const float distance = std::sqrt(m_Unit(m_Random)) * radius;
        glm::vec3 candidate = center + glm::vec3(std::cos(angle) * distance, 0.0f, std::sin(angle) * distance);
        if (m_City.IsSidewalk(candidate.x, candidate.z) && !m_City.IsBlocked(candidate.x, candidate.z, 0.25f)) {
            point = candidate;
            return true;
        }
    }
    return false;
}

bool Simulation::FindSidewalkPath(const glm::vec3& start, const glm::vec3& target, std::vector<glm::vec3>& path) const {
    const auto toTile = [](float coordinate, int count) {
        return static_cast<int>(std::floor((coordinate + count * CityLayout::TILE_SIZE * 0.5f) / CityLayout::TILE_SIZE));
    };
    const int startX = toTile(start.x, CityLayout::WIDTH);
    const int startZ = toTile(start.z, CityLayout::HEIGHT);
    const int goalX = toTile(target.x, CityLayout::WIDTH);
    const int goalZ = toTile(target.z, CityLayout::HEIGHT);
    if (startX < 0 || startX >= CityLayout::WIDTH || startZ < 0 || startZ >= CityLayout::HEIGHT ||
        goalX < 0 || goalX >= CityLayout::WIDTH || goalZ < 0 || goalZ >= CityLayout::HEIGHT) return false;
    const auto traversable = [&](int x, int z) {
        const TileType tile = m_City.GetTileAt(x, z);
        return tile == TileType::Sidewalk || tile == TileType::Plaza || tile == TileType::Grass;
    };
    if (!traversable(startX, startZ) || !traversable(goalX, goalZ)) return false;
    const auto index = [](int x, int z) { return z * CityLayout::WIDTH + x; };
    std::vector<int> previous(CityLayout::WIDTH * CityLayout::HEIGHT, -1);
    std::queue<int> frontier;
    const int startIndex = index(startX, startZ);
    const int goalIndex = index(goalX, goalZ);
    previous[startIndex] = startIndex;
    frontier.push(startIndex);
    constexpr int offsets[4][2]{{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    while (!frontier.empty() && previous[goalIndex] < 0) {
        const int current = frontier.front();
        frontier.pop();
        const int x = current % CityLayout::WIDTH;
        const int z = current / CityLayout::WIDTH;
        for (const auto& offset : offsets) {
            const int nextX = x + offset[0];
            const int nextZ = z + offset[1];
            if (nextX < 0 || nextX >= CityLayout::WIDTH || nextZ < 0 || nextZ >= CityLayout::HEIGHT || !traversable(nextX, nextZ)) continue;
            const int next = index(nextX, nextZ);
            if (previous[next] >= 0) continue;
            previous[next] = current;
            frontier.push(next);
        }
    }
    if (previous[goalIndex] < 0 || goalIndex == startIndex) return false;
    std::vector<int> reversed;
    for (int current = goalIndex; current != startIndex; current = previous[current]) reversed.push_back(current);
    path.clear();
    path.reserve(reversed.size());
    for (auto it = reversed.rbegin(); it != reversed.rend(); ++it) {
        const int x = *it % CityLayout::WIDTH;
        const int z = *it / CityLayout::WIDTH;
        path.emplace_back((x + 0.5f) * CityLayout::TILE_SIZE - CityLayout::WIDTH * CityLayout::TILE_SIZE * 0.5f,
                          0.0f,
                          (z + 0.5f) * CityLayout::TILE_SIZE - CityLayout::HEIGHT * CityLayout::TILE_SIZE * 0.5f);
    }
    return !path.empty();
}

bool Simulation::CollectPickup(size_t index, PickupItem& pickup) {
    if (index >= m_Pickups.size()) return false;
    pickup = m_Pickups[index];
    m_Pickups.erase(m_Pickups.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

void Simulation::RestorePickup(const PickupItem& pickup) { m_Pickups.push_back(pickup); }

size_t Simulation::GetNearbyCar(const glm::vec3& position, float radius) const {
    size_t nearest = m_Traffic.size();
    float nearestDistance = radius;
    for (size_t i = 0; i < m_Traffic.size(); ++i) {
        if (m_Traffic[i].bus || m_CarCooldowns[i] > 0.0f) continue;
        const float distance = glm::distance(glm::vec2(position.x, position.z), glm::vec2(m_Traffic[i].position.x, m_Traffic[i].position.z));
        if (distance < nearestDistance) { nearest = i; nearestDistance = distance; }
    }
    return nearest;
}

bool Simulation::IsNearShop(const glm::vec3& position, float radius) const {
    const float halfWidth = CityLayout::WIDTH * CityLayout::TILE_SIZE * 0.5f;
    const float halfHeight = CityLayout::HEIGHT * CityLayout::TILE_SIZE * 0.5f;
    const int tileX = static_cast<int>(std::floor((position.x + halfWidth) / CityLayout::TILE_SIZE));
    const int tileZ = static_cast<int>(std::floor((position.z + halfHeight) / CityLayout::TILE_SIZE));
    const int tileRange = static_cast<int>(std::ceil(radius / CityLayout::TILE_SIZE)) + 1;
    const float radiusSquared = radius * radius;
    for (int z = std::max(0, tileZ - tileRange); z <= std::min(CityLayout::HEIGHT - 1, tileZ + tileRange); ++z) {
        for (int x = std::max(0, tileX - tileRange); x <= std::min(CityLayout::WIDTH - 1, tileX + tileRange); ++x) {
            if (m_City.GetTileAt(x, z) != TileType::Building) continue;
            const float wx = (x + 0.5f) * CityLayout::TILE_SIZE - halfWidth;
            const float wz = (z + 0.5f) * CityLayout::TILE_SIZE - halfHeight;
            const float dx = position.x - wx;
            const float dz = position.z - wz;
            if (dx * dx + dz * dz < radiusSquared) return true;
        }
    }
    return false;
}

bool Simulation::IsNearRecycler(const glm::vec3& position, float radius) const {
    const glm::vec3 recyclerPosition(0.0f, 0.0f, 0.0f);
    return glm::distance(glm::vec2(position.x, position.z), glm::vec2(recyclerPosition.x, recyclerPosition.z)) < radius;
}

bool Simulation::IsNearMissionNpc(const glm::vec3& position, float radius) const {
    return glm::distance(glm::vec2(position.x, position.z), glm::vec2(m_MissionNpcPosition.x, m_MissionNpcPosition.z)) < radius;
}

bool Simulation::StealCar(size_t index) {
    if (index >= m_Traffic.size() || m_Traffic[index].bus || m_CarCooldowns[index] > 0.0f) return false;
    m_CarCooldowns[index] = 40.0f;
    return true;
}

void Simulation::Update(float dt, const glm::vec3& playerPosition) {
    m_Clock = std::fmod(m_Clock + dt / 600.0f, 1.0f);
    m_DayProgress = m_Clock;
    UpdatePedestrians(dt, playerPosition);
    UpdateTraffic(dt, playerPosition);
    UpdateBus(dt, playerPosition);
    m_Elapsed += dt;
    UpdatePickups(dt, playerPosition);
}

void Simulation::UpdatePickups(float dt, const glm::vec3& playerPosition) {
    m_PickupSpawnTimer -= dt;
    if (m_Pickups.size() >= 32 || m_PickupSpawnTimer > 0.0f) return;
    for (int attempt = 0; attempt < 30; ++attempt) {
        const float angle = m_Unit(m_Random) * glm::two_pi<float>();
        const float distance = 4.0f + m_Unit(m_Random) * 24.0f;
        glm::vec3 point = playerPosition + glm::vec3(std::cos(angle) * distance, 0.0f, std::sin(angle) * distance);
        if (!m_City.IsWalkable(point.x, point.z) || m_City.IsRoad(point.x, point.z)) continue;
        bool overlapping = false;
        for (const auto& pickup : m_Pickups) if (glm::distance(pickup.position, point) < 2.0f) overlapping = true;
        if (overlapping) continue;
        m_Pickups.push_back({static_cast<size_t>(m_Unit(m_Random) * m_PickupMeshes.size()) % m_PickupMeshes.size(), point});
        m_PickupSpawnTimer = 1.8f;
        return;
    }
    m_PickupSpawnTimer = 0.5f;
}

std::vector<PickupItem> Simulation::CollectPickups(const glm::vec3& position, float radius) {
    std::vector<PickupItem> collected;
    for (auto pickup = m_Pickups.begin(); pickup != m_Pickups.end();) {
        if (glm::distance(pickup->position, position) <= radius) {
            collected.push_back(*pickup);
            pickup = m_Pickups.erase(pickup);
        } else ++pickup;
    }
    return collected;
}

void Simulation::UpdatePedestrians(float dt, const glm::vec3& playerPosition) {
    m_PedestrianSpawnTimer -= dt;
    const size_t populationLimit = IsNight() ? 9 : 22;
    if (m_Pedestrians.size() < populationLimit && m_PedestrianSpawnTimer <= 0.0f) {
        glm::vec3 spawn;
        if (FindSidewalkPoint(spawn, playerPosition, 48.0f)) {
            Pedestrian pedestrian;
            pedestrian.position = spawn;
            glm::vec3 destination;
            if (FindSidewalkPoint(destination, spawn, 22.0f) && FindSidewalkPath(spawn, destination, pedestrian.route)) {
                pedestrian.target = pedestrian.route.front();
                pedestrian.nextWaypoint = 1;
            } else {
                pedestrian.target = spawn;
            }
            pedestrian.speed = 0.75f + m_Unit(m_Random) * 0.7f;
            pedestrian.scale = 0.94f + m_Unit(m_Random) * 0.16f;
            pedestrian.animationOffset = m_Unit(m_Random) * 2.0f;
            if (!m_PedestrianAppearances.empty()) {
                std::uniform_int_distribution<size_t> appearanceDistribution(0, m_PedestrianAppearances.size() - 1);
                pedestrian.appearance = appearanceDistribution(m_Random);
            }
            pedestrian.color = {0.22f + m_Unit(m_Random) * 0.55f, 0.18f + m_Unit(m_Random) * 0.48f, 0.15f + m_Unit(m_Random) * 0.42f};
            m_Pedestrians.push_back(pedestrian);
        }
        m_PedestrianSpawnTimer = 0.6f;
    }

    for (auto& pedestrian : m_Pedestrians) {
        pedestrian.moving = false;
        pedestrian.animationTime += dt;
        glm::vec3 direction = pedestrian.target - pedestrian.position;
        direction.y = 0.0f;
        float distance = glm::length(direction);
        if (distance < 0.45f) {
            if (pedestrian.nextWaypoint < pedestrian.route.size()) {
                pedestrian.target = pedestrian.route[pedestrian.nextWaypoint++];
            } else {
                glm::vec3 destination;
                pedestrian.route.clear();
                pedestrian.nextWaypoint = 0;
                if (FindSidewalkPoint(destination, pedestrian.position, 24.0f) && FindSidewalkPath(pedestrian.position, destination, pedestrian.route)) {
                    pedestrian.target = pedestrian.route.front();
                    pedestrian.nextWaypoint = 1;
                } else pedestrian.target = pedestrian.position;
            }
            direction = pedestrian.target - pedestrian.position;
            direction.y = 0.0f;
            distance = glm::length(direction);
            if (distance < 0.001f) continue;
        }
        direction /= distance;
        const glm::vec3 next = pedestrian.position + direction * (pedestrian.speed * dt);
        if (m_City.IsSidewalk(next.x, next.z) && !m_City.IsBlocked(next.x, next.z, 0.18f)) {
            pedestrian.position = next;
            pedestrian.yaw = std::atan2(direction.x, direction.z);
            pedestrian.moving = true;
        } else {
            pedestrian.route.clear();
            pedestrian.nextWaypoint = 0;
            glm::vec3 destination;
            if (FindSidewalkPoint(destination, pedestrian.position, 12.0f) && FindSidewalkPath(pedestrian.position, destination, pedestrian.route)) {
                pedestrian.target = pedestrian.route.front();
                pedestrian.nextWaypoint = 1;
            } else pedestrian.target = pedestrian.position;
        }
    }
    std::erase_if(m_Pedestrians, [&](const Pedestrian& pedestrian) { return glm::distance(pedestrian.position, playerPosition) > 72.0f; });
}

void Simulation::UpdateTraffic(float dt, const glm::vec3& playerPosition) {
    if (dt <= 0.0f) return;
    constexpr float braking = 5.0f;
    constexpr float acceleration = 1.8f;
    constexpr float gap = 1.8f;
    constexpr float probeStep = 0.5f;
    float remaining = std::min(dt, 0.5f);
    while (remaining > 1e-8f) {
        const float stepTime = std::min(remaining, 0.05f);
        for (auto& vehicle : m_Traffic) {
            float clearance = std::numeric_limits<float>::infinity();
            const float routeLength = vehicle.routeLength;
            for (const auto& other : m_Traffic) {
                if (&vehicle == &other || vehicle.routeIndex != other.routeIndex) continue;
                const float routeGap = std::fmod(other.distance - vehicle.distance + routeLength, routeLength);
                clearance = std::min(clearance, std::max(0.0f, routeGap - (vehicle.length + other.length) * 0.5f - gap));
            }
            const float lookAhead = std::max(5.0f, vehicle.speed * vehicle.speed / (2.0f * braking) + vehicle.speed * 0.2f + 2.0f);
            const float radius = lookAhead + std::hypot(vehicle.length * 0.5f + 0.8f, vehicle.width * 0.5f + 0.55f);
            const glm::vec2 carPosition(vehicle.position.x, vehicle.position.z);
            const auto personInCarAt = [&](const glm::vec3& actorPosition, const glm::vec3& probePosition, const glm::vec3& probeDirection) {
                const glm::vec2 delta(actorPosition.x - probePosition.x, actorPosition.z - probePosition.z);
                const glm::vec2 forward(probeDirection.x, probeDirection.z);
                const float along = glm::dot(delta, forward);
                const float sideways = glm::dot(delta, glm::vec2(-forward.y, forward.x));
                return std::abs(along) < vehicle.length * 0.5f + 0.8f
                    && std::abs(sideways) < vehicle.width * 0.5f + 0.55f;
            };
            for (float ahead = 0.0f; ahead <= std::min(lookAhead, clearance); ahead += probeStep) {
                glm::vec3 probePosition, probeDirection;
                size_t probeWaypoint = 0;
                SampleCircuit(vehicle.route, vehicle.routeCumulative, routeLength, vehicle.distance + ahead,
                              probePosition, probeDirection, probeWaypoint);
                const glm::vec2 playerOffset(playerPosition.x - carPosition.x, playerPosition.z - carPosition.y);
                if (glm::dot(playerOffset, playerOffset) <= radius * radius
                    && personInCarAt(playerPosition, probePosition, probeDirection)) {
                    clearance = std::max(0.0f, ahead - probeStep);
                    break;
                }
                bool stoppedForPedestrian = false;
                for (const auto& pedestrian : m_Pedestrians) {
                    const glm::vec2 offset(pedestrian.position.x - carPosition.x, pedestrian.position.z - carPosition.y);
                    if (glm::dot(offset, offset) > radius * radius) continue;
                    if (personInCarAt(pedestrian.position, probePosition, probeDirection)) {
                        clearance = std::max(0.0f, ahead - probeStep);
                        stoppedForPedestrian = true;
                        break;
                    }
                }
                if (stoppedForPedestrian) break;
            }
            const float desiredSpeed = std::min(vehicle.cruiseSpeed, std::sqrt(2.0f * braking * clearance));
            const float rate = desiredSpeed < vehicle.speed ? braking : acceleration;
            const float speed = std::clamp(desiredSpeed, vehicle.speed - rate * stepTime, vehicle.speed + rate * stepTime);
            const float travel = std::min(std::max(0.0f, speed * stepTime), clearance);
            vehicle.nextDistance = std::fmod(vehicle.distance + travel, routeLength);
            vehicle.nextSpeed = travel / stepTime;
        }
        for (auto& vehicle : m_Traffic) {
            vehicle.distance = vehicle.nextDistance;
            vehicle.speed = vehicle.nextSpeed;
            vehicle.wheelRotation -= vehicle.speed * stepTime / 0.33f;
            SampleCircuit(vehicle.route, vehicle.routeCumulative, vehicle.routeLength, vehicle.distance,
                          vehicle.position, vehicle.direction, vehicle.nextWaypoint);
            vehicle.horizontalLane = std::abs(vehicle.direction.x) > std::abs(vehicle.direction.z);
            vehicle.laneCoordinate = vehicle.horizontalLane ? vehicle.position.z : vehicle.position.x;
            vehicle.yaw = std::atan2(-vehicle.direction.z, vehicle.direction.x);
        }
        remaining -= stepTime;
    }
    for (size_t index = 0; index < m_CarCooldowns.size(); ++index) m_CarCooldowns[index] = std::max(0.0f, m_CarCooldowns[index] - dt);
}

void Simulation::UpdateBus(float dt, const glm::vec3& playerPosition) {
    constexpr float acceleration = 1.1f;
    constexpr float braking = 4.0f;
    float remaining = std::min(std::max(dt, 0.0f), 0.5f);
    while (remaining > 1e-8f) {
        const float step = std::min(remaining, 0.05f);
        remaining -= step;
        if (m_BusDwellRemaining > 0.0f) {
            m_BusDwellRemaining = std::max(0.0f, m_BusDwellRemaining - step);
            continue;
        }
        float clearance = m_BusPosition.z - m_BusEndZ;
        if (!m_BusServedStation) clearance = std::min(clearance, m_BusPosition.z - m_BusStationZ);
        const auto actorClearance = [&](const glm::vec3& actor) {
            if (std::abs(actor.x - m_BusPosition.x) > 2.52f * 0.5f + 0.55f) return std::numeric_limits<float>::infinity();
            if (actor.z > m_BusPosition.z + 18.2f * 0.5f + 0.55f) return std::numeric_limits<float>::infinity();
            return std::max(0.0f, m_BusPosition.z - actor.z - 18.2f * 0.5f - 0.8f);
        };
        clearance = std::min(clearance, actorClearance(playerPosition));
        for (const auto& pedestrian : m_Pedestrians) clearance = std::min(clearance, actorClearance(pedestrian.position));
        const float desiredSpeed = std::min(5.0f, std::sqrt(2.0f * braking * std::max(0.0f, clearance)));
        const float rate = desiredSpeed < m_BusSpeed ? braking : acceleration;
        m_BusSpeed = std::clamp(desiredSpeed, m_BusSpeed - rate * step, m_BusSpeed + rate * step);
        const float travel = std::min(m_BusSpeed * step, std::max(0.0f, clearance));
        m_BusPosition.z -= travel;
        m_BusSpeed = travel / step;
        m_BusWheelRotation += travel / 0.5f;
        if (!m_BusServedStation && m_BusPosition.z <= m_BusStationZ + 1e-8f) {
            m_BusPosition.z = m_BusStationZ;
            m_BusServedStation = true;
            m_BusSpeed = 0.0f;
            m_BusDwellRemaining = 4.0f;
        }
        if (m_BusPosition.z <= m_BusEndZ + 1e-8f) {
            m_BusPosition.z = m_BusStartZ;
            m_BusServedStation = false;
            m_BusSpeed = 0.0f;
        }
    }
}

bool Simulation::IsBlocked(const glm::vec3& position, float radius) const {
    for (const auto& vehicle : m_Traffic) {
        const glm::vec2 delta(position.x - vehicle.position.x, position.z - vehicle.position.z);
        if (glm::length(delta) < radius + vehicle.length * 0.36f) return true;
    }
    return false;
}

void Simulation::Render(Renderer& renderer, float time, const glm::vec3& focusPosition) const {
    const glm::mat4 stationRoot = glm::translate(glm::mat4(1.0f), {m_BusStationX, 0.0f, m_BusStationZ});
    renderer.Submit(*m_BusPlatform, glm::translate(stationRoot, {0.0f, 0.45f, 0.0f}));
    renderer.Submit(*m_BusCanopy, glm::translate(stationRoot, {0.0f, 3.3f, 0.0f}));
    renderer.Submit(*m_BusStationGlass, glm::translate(stationRoot, {-1.25f, 1.7f, 0.0f}), {0.66f, 0.78f, 0.84f, 0.35f}, false, false, nullptr, 2);
    renderer.Submit(*m_BusStationGlass, glm::translate(stationRoot, {1.25f, 1.7f, 0.0f}), {0.66f, 0.78f, 0.84f, 0.35f}, false, false, nullptr, 2);
    renderer.Submit(*m_BusStationSign, glm::translate(stationRoot, {0.0f, 3.8f, 7.0f}));
    for (float z = -8.0f; z <= 8.0f; z += 4.0f) {
        for (float x : {-1.2f, 1.2f}) {
            glm::mat4 post = glm::translate(stationRoot, {x, 2.1f, z});
            post = glm::rotate(post, -glm::half_pi<float>(), {1.0f, 0.0f, 0.0f});
            renderer.Submit(*m_BusStationPost, post);
        }
    }
    for (const auto& position : m_StreetLamps) {
        glm::mat4 pole = glm::translate(glm::mat4(1.0f), position + glm::vec3(0.0f, 1.6f, 0.0f));
        renderer.Submit(*m_StreetLampPole, pole);
        glm::mat4 head = glm::translate(glm::mat4(1.0f), position + glm::vec3(0.18f, 3.18f, 0.0f));
        renderer.Submit(*m_StreetLampHead, head, glm::vec4(1.0f, 0.89f, 0.65f, 1.0f));
    }
    glm::mat4 missionNpc = glm::translate(glm::mat4(1.0f), m_MissionNpcPosition);
    missionNpc = glm::scale(missionNpc, glm::vec3(1.08f));
    if (!m_PedestrianAppearances.empty()) {
        const auto& appearance = m_PedestrianAppearances.front();
        for (const auto& model : appearance.parts)
            if (model) model->Draw(renderer, missionNpc, glm::vec4(1.0f), time, appearance.idleClip >= 0, appearance.idleClip);
    }
    if (!m_MissionStarted && !m_MissionComplete) {
        glm::mat4 marker = glm::translate(glm::mat4(1.0f), m_MissionNpcPosition + glm::vec3(0.0f, 2.0f + std::sin(time * 2.0f) * 0.12f, 0.0f));
        renderer.Submit(*m_MissionMarker, marker);
    }
    for (const auto& pickup : m_Pickups) {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), pickup.position + glm::vec3(0.0f, 0.22f + std::sin(time * 2.0f + pickup.position.x) * 0.04f, 0.0f));
        model = glm::rotate(model, time * 0.65f, {0.0f, 1.0f, 0.0f});
        renderer.Submit(*m_PickupMeshes[pickup.itemIndex], model);
    }
    for (const auto& vehicle : m_Traffic) {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), vehicle.position);
        model = glm::rotate(model, vehicle.yaw, {0.0f, 1.0f, 0.0f});
        glm::mat4 body = glm::translate(model, {0.0f, 0.62f, 0.0f});
        renderer.Submit(*m_CarBody, body, glm::vec4(vehicle.color, 1.0f));
        glm::mat4 cabin = glm::translate(model, {-0.25f, 1.25f, 0.0f});
        renderer.Submit(*m_CarCabin, cabin);
        glm::mat4 windshield = glm::translate(model, {0.80f, 1.25f, 0.0f});
        renderer.Submit(*m_CarGlass, windshield);
        glm::mat4 roof = glm::translate(model, {-0.3f, 1.58f, 0.0f});
        renderer.Submit(*m_CarRoof, roof, glm::vec4(vehicle.color, 1.0f));
        if (vehicle.taxi) {
            glm::mat4 taxiSign = glm::translate(model, {-0.3f, 1.72f, 0.0f});
            renderer.Submit(*m_CarTaxiSign, taxiSign);
        }
        for (float x : {-1.35f, 1.35f}) for (float z : {-0.76f, 0.76f}) {
            glm::mat4 wheel = glm::translate(model, {x, 0.33f, z});
            wheel = glm::rotate(wheel, vehicle.wheelRotation, {0.0f, 0.0f, 1.0f});
            renderer.Submit(*m_CarWheel, wheel);
            renderer.Submit(*m_CarRim, wheel);
        }
        for (float z : {-0.58f, 0.58f}) {
            glm::mat4 lamp = glm::translate(model, {2.12f, 0.66f, z});
            renderer.Submit(*m_CarLamp, lamp);
        }
    }
    const float busOpacity = std::clamp(std::min((m_BusStartZ - m_BusPosition.z) / 6.0f,
                                                  (m_BusPosition.z - m_BusEndZ) / 6.0f), 0.0f, 1.0f);
    if (busOpacity > 0.0f) {
        const int alphaMode = busOpacity < 1.0f ? 2 : 0;
        const glm::vec4 busRed(0.70f, 0.15f, 0.12f, busOpacity);
        const glm::vec4 busWindow(0.08f, 0.12f, 0.16f, busOpacity);
        glm::mat4 busRoot = glm::translate(glm::mat4(1.0f), m_BusPosition);
        for (float segmentZ : {-4.6f, 4.6f}) {
            renderer.Submit(*m_BusBody, glm::translate(busRoot, {0.0f, 1.75f, segmentZ}), busRed, false, false, nullptr, alphaMode);
            for (float side : {-1.0f, 1.0f}) {
                renderer.Submit(*m_BusWindows, glm::translate(busRoot, {side * 1.26f, 2.3f, segmentZ}), busWindow, false, false, nullptr, alphaMode);
            }
            for (float wheelZ : {segmentZ - 3.0f, segmentZ + 3.0f}) {
                for (float side : {-1.15f, 1.15f}) {
                    glm::mat4 wheel = glm::translate(busRoot, {side, 0.5f, wheelZ});
                    wheel = glm::rotate(wheel, glm::half_pi<float>(), {0.0f, 1.0f, 0.0f});
                    wheel = glm::rotate(wheel, m_BusWheelRotation, {0.0f, 0.0f, 1.0f});
                    renderer.Submit(*m_BusWheel, wheel, glm::vec4(1.0f, 1.0f, 1.0f, busOpacity), false, false, nullptr, alphaMode);
                    renderer.Submit(*m_BusRim, wheel, glm::vec4(1.0f, 1.0f, 1.0f, busOpacity), false, false, nullptr, alphaMode);
                }
            }
        }
        renderer.Submit(*m_BusJoint, glm::translate(busRoot, {0.0f, 1.8f, 0.0f}), glm::vec4(1.0f, 1.0f, 1.0f, busOpacity), false, false, nullptr, alphaMode);
        glm::mat4 windshield = glm::translate(busRoot, {0.0f, 2.0f, -9.1f});
        renderer.Submit(*m_BusWindshield, windshield, busWindow, false, false, nullptr, alphaMode);
    }
    size_t visiblePedestrians = 0;
    for (const auto& pedestrian : m_Pedestrians) {
        const glm::vec2 offset(pedestrian.position.x - focusPosition.x, pedestrian.position.z - focusPosition.z);
        if (glm::dot(offset, offset) > 42.0f * 42.0f || visiblePedestrians >= 12) continue;
        ++visiblePedestrians;
        glm::mat4 character = glm::translate(glm::mat4(1.0f), pedestrian.position);
        character = glm::rotate(character, pedestrian.yaw, {0.0f, 1.0f, 0.0f});
        character = glm::scale(character, glm::vec3(pedestrian.scale));
        if (pedestrian.appearance >= m_PedestrianAppearances.size()) continue;
        const auto& appearance = m_PedestrianAppearances[pedestrian.appearance];
        const int clip = pedestrian.moving ? appearance.walkClip : appearance.idleClip;
        for (size_t partIndex = 0; partIndex < appearance.parts.size(); ++partIndex) {
            const auto& model = appearance.parts[partIndex];
            if (!model) continue;
            glm::vec4 tint(1.0f);
            if (partIndex == 1) tint = glm::vec4(pedestrian.color, 1.0f);
            if (partIndex == 2) tint = glm::vec4(0.68f,0.65f,0.61f,1.0f);
            if (partIndex == 3) tint = glm::vec4(0.66f,0.58f,0.47f,1.0f);
            model->Draw(renderer, character, tint, pedestrian.animationTime + pedestrian.animationOffset, clip >= 0, clip);
        }
    }
    (void)time;
}

} // namespace Bazuko
