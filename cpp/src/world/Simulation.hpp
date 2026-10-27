#pragma once

#include "CityLayout.hpp"
#include "../renderer/Model.hpp"
#include <glm/glm.hpp>
#include <cstdint>
#include <algorithm>
#include <random>
#include <utility>
#include <vector>

namespace Bazuko {

struct Pedestrian {
    glm::vec3 position{0.0f};
    glm::vec3 target{0.0f};
    float speed = 1.1f;
    float scale = 1.0f;
    float yaw = 0.0f;
    float animationTime = 0.0f;
    float animationOffset = 0.0f;
    size_t appearance = 0;
    bool moving = false;
    glm::vec3 color{0.65f, 0.55f, 0.42f};
    std::vector<glm::vec3> route;
    size_t nextWaypoint = 0;
};

struct PedestrianAppearance {
    std::vector<std::shared_ptr<Model>> parts;
    int walkClip = -1;
    int idleClip = -1;
};

struct TrafficVehicle {
    glm::vec3 position{0.0f};
    glm::vec3 direction{1.0f, 0.0f, 0.0f};
    float speed = 5.0f;
    float cruiseSpeed = 5.0f;
    float length = 3.0f;
    float width = 1.35f;
    float yaw = 0.0f;
    glm::vec3 color{0.75f, 0.18f, 0.12f};
    bool bus = false;
    bool taxi = false;
    bool horizontalLane = true;
    float wheelRotation = 0.0f;
    float laneCoordinate = 0.0f;
    size_t routeIndex = 0;
    float distance = 0.0f;
    float routeLength = 0.0f;
    float nextDistance = 0.0f;
    float nextSpeed = 0.0f;
    std::vector<glm::vec3> route;
    std::vector<float> routeCumulative;
    size_t nextWaypoint = 0;
};

struct PickupItem {
    size_t itemIndex = 0;
    glm::vec3 position{0.0f};
};

class Simulation {
public:
    explicit Simulation(const CityLayout& city, uint32_t seed = 42);
    void Update(float dt, const glm::vec3& playerPosition);
    bool IsBlocked(const glm::vec3& position, float radius) const;
    void Render(Renderer& renderer, float time, const glm::vec3& focusPosition) const;
    std::vector<PickupItem> CollectPickups(const glm::vec3& position, float radius);
    bool CollectPickup(size_t index, PickupItem& pickup);
    void RestorePickup(const PickupItem& pickup);
    const std::vector<PickupItem>& GetPickups() const { return m_Pickups; }
    size_t GetNearbyCar(const glm::vec3& position, float radius) const;
    bool IsNearShop(const glm::vec3& position, float radius) const;
    bool IsNearRecycler(const glm::vec3& position, float radius) const;
    bool StealCar(size_t index);
    float GetDayProgress() const { return m_DayProgress; }
    void SetDayProgress(float progress) { m_Clock = std::clamp(progress, 0.0f, 1.0f); m_DayProgress = m_Clock; }
    bool IsNight() const { return m_DayProgress > 0.78f || m_DayProgress < 0.22f; }
    size_t GetPedestrianCount() const { return m_Pedestrians.size(); }
    size_t GetTrafficCount() const { return m_Traffic.size(); }
    size_t GetMissionProgress() const { return m_MissionProgress; }
    size_t GetMissionTarget() const { return m_MissionTarget; }
    bool IsMissionStarted() const { return m_MissionStarted; }
    bool IsMissionComplete() const { return m_MissionComplete; }
    const glm::vec3& GetMissionNpcPosition() const { return m_MissionNpcPosition; }
    bool IsNearMissionNpc(const glm::vec3& position, float radius) const;
    void StartMission() { m_MissionStarted = true; }
    void CompleteMission() { m_MissionComplete = true; }
    bool CompleteMissionItem() { if (m_MissionProgress >= m_MissionTarget) return false; ++m_MissionProgress; return true; }
    void SetMissionProgress(size_t progress) { m_MissionProgress = std::min(progress, m_MissionTarget); }

private:
    bool FindSidewalkPoint(glm::vec3& point, const glm::vec3& center, float radius);
    bool FindSidewalkPath(const glm::vec3& start, const glm::vec3& target, std::vector<glm::vec3>& path) const;
    void UpdatePedestrians(float dt, const glm::vec3& playerPosition);
    void UpdateTraffic(float dt, const glm::vec3& playerPosition);
    void UpdateBus(float dt, const glm::vec3& playerPosition);
    void UpdatePickups(float dt, const glm::vec3& playerPosition);

    const CityLayout& m_City;
    std::mt19937 m_Random;
    std::uniform_real_distribution<float> m_Unit{0.0f, 1.0f};
    std::vector<Pedestrian> m_Pedestrians;
    std::vector<TrafficVehicle> m_Traffic;
    std::vector<PickupItem> m_Pickups;
    std::vector<float> m_CarCooldowns;
    std::vector<std::vector<glm::vec3>> m_TrafficRoutes;
    std::shared_ptr<Mesh> m_PedestrianBody;
    std::shared_ptr<Mesh> m_PedestrianHead;
    std::vector<PedestrianAppearance> m_PedestrianAppearances;
    std::shared_ptr<Model> m_PedestrianAnimation;
    std::shared_ptr<Mesh> m_CarBody;
    std::shared_ptr<Mesh> m_CarCabin;
    std::shared_ptr<Mesh> m_CarGlass;
    std::shared_ptr<Mesh> m_CarRoof;
    std::shared_ptr<Mesh> m_CarWheel;
    std::shared_ptr<Mesh> m_CarRim;
    std::shared_ptr<Mesh> m_CarLamp;
    std::shared_ptr<Mesh> m_CarTaxiSign;
    std::shared_ptr<Mesh> m_BusBody;
    std::shared_ptr<Mesh> m_BusWindows;
    std::shared_ptr<Mesh> m_BusWindshield;
    std::shared_ptr<Mesh> m_BusJoint;
    std::shared_ptr<Mesh> m_BusWheel;
    std::shared_ptr<Mesh> m_BusRim;
    std::shared_ptr<Mesh> m_BusPlatform;
    std::shared_ptr<Mesh> m_BusStationGlass;
    std::shared_ptr<Mesh> m_BusCanopy;
    std::shared_ptr<Mesh> m_BusStationPost;
    std::shared_ptr<Mesh> m_BusStationSign;
    std::shared_ptr<Mesh> m_PickupMesh;
    std::vector<std::shared_ptr<Mesh>> m_PickupMeshes;
    std::shared_ptr<Mesh> m_StreetLampPole;
    std::shared_ptr<Mesh> m_StreetLampHead;
    std::vector<glm::vec3> m_StreetLamps;
    glm::vec3 m_BusPosition{0.0f};
    float m_BusStartZ = 0.0f;
    float m_BusEndZ = 0.0f;
    float m_BusStationZ = 0.0f;
    float m_BusSpeed = 0.0f;
    float m_BusDwellRemaining = 0.0f;
    float m_BusWheelRotation = 0.0f;
    float m_BusStationX = 0.0f;
    bool m_BusServedStation = false;
    std::shared_ptr<Mesh> m_MissionMarker;
    glm::vec3 m_MissionNpcPosition{3.0f, 0.0f, 0.0f};
    float m_PickupSpawnTimer = 0.0f;
    float m_Elapsed = 0.0f;
    size_t m_MissionProgress = 0;
    size_t m_MissionTarget = 5;
    bool m_MissionStarted = false;
    bool m_MissionComplete = false;
    float m_Clock = 0.30f;
    float m_DayProgress = 0.30f;
    float m_PedestrianSpawnTimer = 0.0f;
};

} // namespace Bazuko
