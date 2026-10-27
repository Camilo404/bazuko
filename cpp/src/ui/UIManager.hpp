#pragma once
#include <array>
#include <string>
#include <cstddef>
#include <vector>
#include <glm/glm.hpp>
#include "../renderer/Camera.hpp"
#include "../renderer/Renderer.hpp"
#include "../core/Window.hpp"
#include "../world/CityLayout.hpp"
#include "../world/Simulation.hpp"
#include "../audio/AudioManager.hpp"

struct GLFWwindow;

namespace Bazuko {

struct CraftingOption {
    std::string id;
    std::string name;
    std::string ingredients;
    bool craftable = false;
};

class UIManager {
public:
    UIManager(GLFWwindow* window);
    ~UIManager();

    void BeginFrame();
    void EndFrame();
    void TogglePause();
    void HandleEscape();
    void ToggleCraftMenu();
    bool IsPaused() const { return m_Paused; }
    std::string ConsumeCraftRequest();
    bool ConsumeSaveRequest();

    void RenderHUD(float dt, const glm::vec3& playerPos, Camera& camera, Renderer& renderer, Window& window,
                   const CityLayout& city, const Simulation& simulation,
                   AudioManager& audio,
                   const std::array<float, 7>& needs, float money, float inventoryWeight, float inventoryCapacity,
                   float dayProgress, size_t pedestrians, size_t traffic, const std::string& inventorySummary,
                   const std::string& diseaseSummary, int level, int xp, const std::string& objective,
                   size_t missionProgress, size_t missionTarget,
                   const std::vector<CraftingOption>& craftingOptions);

private:
    void SetupDarkUrbanBogotaTheme();
    bool m_Paused = false;
    bool m_CraftMenuOpen = false;
    bool m_RadarVisible = true;
    bool m_SaveRequest = false;
    std::string m_CraftRequest;
};

} // namespace Bazuko
