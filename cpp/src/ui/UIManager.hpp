#pragma once
#include <array>
#include <chrono>
#include <memory>
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

class Texture;

struct RadioStation {
    std::string id;
    std::string name;
    std::string frequency;
    std::string genre;
    std::string program;
    std::string streamUrl;
};

struct CraftingOption {
    std::string id;
    std::string name;
    std::string ingredients;
    bool craftable = false;
};

struct InventoryOption {
    size_t itemIndex = 0;
    std::string id;
    std::string name;
    int quantity = 0;
    float weight = 0.0f;
    bool usable = false;
};

// Producto de una tienda (La Esquinita o un vendedor ambulante)
struct ShopOption {
    std::string key;             // id del objeto, o "costal"
    std::string name;
    std::string description;
    int price = 0;
    bool enabled = false;
    bool owned = false;
};

// Fila del menu de la recicladora
struct RecyclerRow {
    std::string name;
    int quantity = 0;
    int unitPrice = 0;
    int totalPrice = 0;
};

struct DealerOption {
    std::string id;
    std::string name;
    int price = 0;
    bool affordable = false;
    bool canCarry = false;
};

struct TalentOption {
    std::string id;
    std::string name;
    std::string description;
    std::string requirement;
    int cost = 0;
    bool unlocked = false;
    bool available = false;
};

struct CambuchePieceOption {
    std::string id;
    std::string name;
    std::string desc;
    std::string reqText;
    bool isBuilt = false;
    bool isLocked = false;
    bool canBuild = false;
};

class UIManager {
public:
    UIManager(GLFWwindow* window);
    ~UIManager();

    void BeginFrame();
    void EndFrame();
    void TogglePause();
    void HandleEscape();
    void HandleRadioShortcut();
    void OpenRadioMenu(bool returnToPause = true);
    void OpenPauseSection(int section);
    void OpenDealerMenu();
    // Tiendas y recicladora: se abren como secciones de la pausa (la partida se detiene mientras se compra).
    // `itemKeys` limita los productos (vendedores ambulantes); vacio = toda la tienda.
    void OpenShopMenu(const std::string& title, const std::string& subtitle, const std::vector<std::string>& itemKeys = {});
    void OpenRecyclerMenu();
    bool IsShopMenuOpen() const { return m_Paused && m_PauseSection == 5; }
    bool IsRecyclerMenuOpen() const { return m_Paused && m_PauseSection == 6; }
    const std::vector<std::string>& ShopFilter() const { return m_ShopFilter; }
    // Datos vivos de los menus de lugares; main los actualiza cada cuadro antes de RenderHUD.
    void SetPlaceMenuData(const std::vector<ShopOption>& shop, const std::vector<RecyclerRow>& recycler,
                          float money, float weight, float capacity);
    // Progreso (0..1) de la clasificacion de basura con Q; negativo = no se esta clasificando.
    void SetSortProgress(float fraction) { m_SortProgress = fraction; }
    std::string ConsumeShopPurchaseRequest();
    bool ConsumeRecyclerSellRequest();
    void ToggleCraftMenu();
    bool IsPaused() const { return m_Paused; }
    bool IsDeathScreenOpen() const { return m_DeathScreenOpen; }
    void ShowDeathScreen(const std::string& causes, bool vehicleImpact);
    bool ConsumeRespawnRequest();
    void CompleteDeathRecovery();
    std::string ConsumeCraftRequest();
    size_t ConsumeInventoryUseRequest();
    size_t ConsumeInventoryDropRequest();
    bool ConsumeInventorySortRequest();
    std::string ConsumeTalentUnlockRequest();
    std::string ConsumeDealerPurchaseRequest();
    bool ConsumePetRequest();
    void SetCompanionCustomization(const std::string& name, int coat);
    std::string GetCompanionName() const;
    int GetCompanionCoat() const { return m_CompanionCoat; }
    void RenderLoadingScreen(Window& window, float progress, const std::string& stageText, const std::string& tipText = "");

    void OpenCambucheMenu() { m_CambucheMenuOpen = true; }
    void CloseCambucheMenu() { m_CambucheMenuOpen = false; }
    bool IsCambucheMenuOpen() const { return m_CambucheMenuOpen; }
    std::string ConsumeCambucheBuildRequest() { return std::exchange(m_CambucheBuildRequest, {}); }
    bool ConsumeSleepRequest() { return std::exchange(m_SleepRequest, false); }
    void RenderCambucheMenu(const std::vector<CambuchePieceOption>& pieces, bool hasBase, bool isNight);

    void RenderHUD(float dt, const glm::vec3& playerPos, Camera& camera, Renderer& renderer, Window& window,
                   const CityLayout& city, const Simulation& simulation,
                   AudioManager& audio,
                   const std::string& playerName, const std::string& playerOrigin,
                   const std::array<float, 7>& needs, float money, float inventoryWeight, float inventoryCapacity,
                   float dayProgress, size_t pedestrians, size_t traffic, const std::string& inventorySummary,
                   const std::string& diseaseSummary, int level, int xp, const std::string& objective,
                   size_t missionProgress, size_t missionTarget,
                   const std::vector<CraftingOption>& craftingOptions,
                   const std::vector<InventoryOption>& inventoryOptions, int talentPoints,
                   const std::vector<TalentOption>& talentOptions, bool companionIsFriend,
                   const std::vector<DealerOption>& dealerOptions,
                   bool boostActive = false, float boostTimer = 0.0f, float psychosisSeverity = 0.0f);

    bool ConsumeSaveRequest();
    bool ConsumeNewGameRequest();
    bool ConsumeQuitRequest();
    bool ConsumeCharacterCreatorRequest();
    void RequestCharacterCreator() { m_CharacterCreatorRequest = true; }
    void SetSaveNotification(const std::string& message);
    void SetPickupNotification(const std::string& message, bool rare = false);
    void RenderQuitScreen(Window& window);

private:
    void SetupDarkUrbanBogotaTheme();
    void LoadLoadingArtwork(int index);
    void DrawLoadingArtwork();
    void CloseRadioMenu();
    bool m_Paused = false;
    bool m_DeathScreenOpen = false;
    bool m_RespawnRequest = false;
    bool m_DeathByVehicleImpact = false;
    std::string m_DeathCauses;
    bool m_RadioMenuOpen = false;
    bool m_RadioReturnsToPause = false;
    bool m_CraftMenuOpen = false;
    bool m_CambucheMenuOpen = false;
    std::string m_CambucheBuildRequest;
    bool m_SleepRequest = false;
    int m_PauseSection = 0;
    bool m_RadarVisible = true;
    bool m_SaveRequest = false;
    bool m_NewGameRequest = false;
    bool m_QuitRequest = false;
    bool m_CharacterCreatorRequest = false;
    std::string m_SaveNotification;
    float m_SaveNotificationTimer = 0.0f;
    std::string m_PickupNotification;
    float m_PickupNotificationTimer = 0.0f;
    bool m_PickupNotificationRare = false;
    float m_WhisperTimer = 5.0f;
    std::string m_ActiveWhisper;
    float m_ActiveWhisperTimer = 0.0f;
    float m_WhisperPosX = 0.5f;
    float m_WhisperPosY = 0.35f;
    std::string m_CraftRequest;
    size_t m_InventoryUseRequest = static_cast<size_t>(-1);
    size_t m_InventoryDropRequest = static_cast<size_t>(-1);
    bool m_InventorySortRequest = false;
    std::string m_TalentUnlockRequest;
    std::string m_DealerPurchaseRequest;
    float m_SortProgress = -1.0f;
    std::string m_ShopTitle;
    std::string m_ShopSubtitle;
    std::vector<std::string> m_ShopFilter;
    std::string m_ShopPurchaseRequest;
    bool m_RecyclerSellRequest = false;
    std::vector<ShopOption> m_ShopOptions;
    std::vector<RecyclerRow> m_RecyclerRows;
    float m_PlaceMoney = 0.0f;
    float m_PlaceWeight = 0.0f;
    float m_PlaceCapacity = 0.0f;
    bool m_PetRequest = false;
    std::array<char, 32> m_CompanionName{{'E', 'l', ' ', 'C', 'h', 'a', 't', 'o', '\0'}};
    int m_CompanionCoat = 0;
    std::vector<RadioStation> m_RadioStations;
    size_t m_SelectedRadioStation = 0;
    std::array<std::shared_ptr<Texture>, 4> m_LoadingBackgrounds{};
    std::array<std::shared_ptr<Texture>, 4> m_LoadingForegrounds{};
    std::array<bool, 4> m_LoadingArtworkLoadAttempted{};
    std::array<int, 4> m_LoadingArtworkDeck{};
    size_t m_LoadingArtworkDeckPosition = 4;
    int m_LoadingArtwork = -1;
    int m_PreviousLoadingArtwork = -1;
    std::chrono::steady_clock::time_point m_LoadingArtworkChangedAt{};
};

} // namespace Bazuko
