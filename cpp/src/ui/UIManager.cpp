#include "UIManager.hpp"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include "../core/Input.hpp"
#include "../core/Log.hpp"
#include "../core/Paths.hpp"
#include "../renderer/Texture.hpp"
#include "SignStyle.hpp"
#include <array>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>
#include <utility>

namespace Bazuko {

namespace {

constexpr float LoadingArtworkDuration = 7.5f;
constexpr float LoadingArtworkTransitionDuration = 0.9f;

ImTextureID LoadingTextureId(const Texture& texture) {
    return (ImTextureID)(intptr_t)texture.GetID();
}

void AddLoadingArtworkImage(ImDrawList* drawList, const Texture& texture, const ImVec2& displaySize,
                            bool foreground, float alpha) {
    ImVec2 min(0.0f, 0.0f);
    ImVec2 max(displaySize.x, displaySize.y);
    if (foreground) {
        const float boxWidth = displaySize.x * 0.74f;
        const float boxHeight = displaySize.y * 0.99f;
        const float scale = std::min(boxWidth / static_cast<float>(texture.GetWidth()),
                                     boxHeight / static_cast<float>(texture.GetHeight()));
        const float imageWidth = texture.GetWidth() * scale;
        const float imageHeight = texture.GetHeight() * scale;
        const float boxLeft = displaySize.x * 0.28f;
        min.x = boxLeft + (boxWidth - imageWidth) * 0.5f;
        max.x = min.x + imageWidth;
        max.y = displaySize.y * 1.03f;
        min.y = max.y - imageHeight;
    } else {
        const float displayAspect = displaySize.x / displaySize.y;
        const float imageAspect = static_cast<float>(texture.GetWidth()) / static_cast<float>(texture.GetHeight());
        ImVec2 uvMin(0.0f, 0.0f);
        ImVec2 uvMax(1.0f, 1.0f);
        if (imageAspect > displayAspect) {
            const float visibleWidth = displayAspect / imageAspect;
            uvMin.x = (1.0f - visibleWidth) * 0.5f;
            uvMax.x = uvMin.x + visibleWidth;
        } else {
            const float visibleHeight = imageAspect / displayAspect;
            uvMin.y = (1.0f - visibleHeight) * 0.5f;
            uvMax.y = uvMin.y + visibleHeight;
        }
        drawList->AddImage(LoadingTextureId(texture), min, max, uvMin, uvMax,
                           IM_COL32(255, 255, 255, static_cast<int>(255.0f * alpha)));
        return;
    }
    drawList->AddImage(LoadingTextureId(texture), min, max, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                       IM_COL32(255, 255, 255, static_cast<int>(255.0f * alpha)));
}

} // namespace

UIManager::UIManager(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Evitar que ImGui fuerce el cursor visible cada cuadro
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    SignUI::LoadFonts();
    SetupDarkUrbanBogotaTheme();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");

    const std::string configPath = Paths::Find("assets/radio_stations.json");
    std::ifstream configFile(configPath);
    if (!configFile) {
        LogWarn("No se pudo abrir la configuracion de emisoras: {}", configPath);
        return;
    }

    try {
        const nlohmann::json config = nlohmann::json::parse(configFile);
        for (const auto& entry : config.at("stations")) {
            RadioStation station{
                entry.at("id").get<std::string>(),
                entry.at("name").get<std::string>(),
                entry.at("frequency").get<std::string>(),
                entry.at("genre").get<std::string>(),
                entry.at("program").get<std::string>(),
                entry.value("url", std::string{})
            };
            m_RadioStations.push_back(std::move(station));
        }
        if (m_RadioStations.empty()) {
            LogWarn("La configuracion de emisoras no contiene estaciones: {}", configPath);
        }
    } catch (const std::exception& error) {
        m_RadioStations.clear();
        LogError("No se pudo leer la configuracion de emisoras {}: {}", configPath, error.what());
    }
}

UIManager::~UIManager() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void UIManager::SetupDarkUrbanBogotaTheme() {
    // Mismo aspecto que el juego web: ver SignStyle.cpp
    SignUI::ApplyTheme();
}

void UIManager::BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void UIManager::EndFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void UIManager::TogglePause() {
    if (m_DeathScreenOpen) return;
    m_Paused = !m_Paused;
    m_RadioMenuOpen = false;
    m_RadioReturnsToPause = false;
    m_CraftMenuOpen = false;
    m_PauseSection = 0;
    Input::SetCursorLocked(!m_Paused);
}

void UIManager::HandleEscape() {
    if (m_DeathScreenOpen) return;
    if (m_RadioMenuOpen) {
        CloseRadioMenu();
        return;
    }
    if (m_CraftMenuOpen) {
        m_CraftMenuOpen = false;
        return;
    }
    if (m_CambucheMenuOpen) {
        m_CambucheMenuOpen = false;
        Input::SetCursorLocked(true);
        return;
    }
    if (m_Paused && m_PauseSection != 0) {
        TogglePause();
        return;
    }
    TogglePause();
}

void UIManager::HandleRadioShortcut() {
    if (m_RadioMenuOpen) {
        CloseRadioMenu();
    } else if (!m_Paused) {
        OpenRadioMenu(false);
    }
}

void UIManager::OpenRadioMenu(bool returnToPause) {
    if (m_DeathScreenOpen) return;
    m_Paused = true;
    m_RadioMenuOpen = true;
    m_RadioReturnsToPause = returnToPause;
    m_CraftMenuOpen = false;
    m_PauseSection = 0;
    Input::SetCursorLocked(false);
}

void UIManager::CloseRadioMenu() {
    m_RadioMenuOpen = false;
    if (!m_RadioReturnsToPause) TogglePause();
    m_RadioReturnsToPause = false;
}

void UIManager::ToggleCraftMenu() {
    if (!m_Paused && !m_DeathScreenOpen) m_CraftMenuOpen = !m_CraftMenuOpen;
}

void UIManager::OpenPauseSection(int section) {
    if (m_DeathScreenOpen) return;
    const int requestedSection = std::clamp(section, 0, 3);
    if (m_Paused && m_PauseSection == requestedSection) {
        TogglePause();
        return;
    }
    m_Paused = true;
    m_PauseSection = requestedSection;
    m_RadioMenuOpen = false;
    m_RadioReturnsToPause = false;
    m_CraftMenuOpen = false;
    Input::SetCursorLocked(false);
}

void UIManager::OpenDealerMenu() {
    if (m_DeathScreenOpen) return;
    m_Paused = true;
    m_PauseSection = 4;
    m_RadioMenuOpen = false;
    m_RadioReturnsToPause = false;
    m_CraftMenuOpen = false;
    Input::SetCursorLocked(false);
}

void UIManager::OpenShopMenu(const std::string& title, const std::string& subtitle, const std::vector<std::string>& itemKeys) {
    if (m_DeathScreenOpen) return;
    m_Paused = true;
    m_PauseSection = 5;
    m_ShopTitle = title;
    m_ShopSubtitle = subtitle;
    m_ShopFilter = itemKeys;
    m_ShopPurchaseRequest.clear();
    m_RadioMenuOpen = false;
    m_RadioReturnsToPause = false;
    m_CraftMenuOpen = false;
    Input::SetCursorLocked(false);
}

void UIManager::OpenRecyclerMenu() {
    if (m_DeathScreenOpen) return;
    m_Paused = true;
    m_PauseSection = 6;
    m_RecyclerSellRequest = false;
    m_RadioMenuOpen = false;
    m_RadioReturnsToPause = false;
    m_CraftMenuOpen = false;
    Input::SetCursorLocked(false);
}

void UIManager::SetPlaceMenuData(const std::vector<ShopOption>& shop, const std::vector<RecyclerRow>& recycler,
                                 float money, float weight, float capacity) {
    m_ShopOptions = shop;
    m_RecyclerRows = recycler;
    m_PlaceMoney = money;
    m_PlaceWeight = weight;
    m_PlaceCapacity = capacity;
}

std::string UIManager::ConsumeShopPurchaseRequest() {
    std::string request = std::move(m_ShopPurchaseRequest);
    m_ShopPurchaseRequest.clear();
    return request;
}

bool UIManager::ConsumeRecyclerSellRequest() {
    const bool requested = m_RecyclerSellRequest;
    m_RecyclerSellRequest = false;
    return requested;
}

void UIManager::ShowDeathScreen(const std::string& causes, bool vehicleImpact) {
    if (m_DeathScreenOpen) return;
    m_DeathScreenOpen = true;
    m_DeathByVehicleImpact = vehicleImpact;
    m_DeathCauses = causes;
    m_RespawnRequest = false;
    m_Paused = true;
    m_RadioMenuOpen = false;
    m_RadioReturnsToPause = false;
    m_CraftMenuOpen = false;
    m_PauseSection = 0;
    Input::SetCursorLocked(false);
}

bool UIManager::ConsumeRespawnRequest() {
    const bool requested = m_RespawnRequest;
    m_RespawnRequest = false;
    return requested;
}

void UIManager::CompleteDeathRecovery() {
    m_DeathScreenOpen = false;
    m_DeathByVehicleImpact = false;
    m_DeathCauses.clear();
    m_Paused = false;
    m_PauseSection = 0;
    Input::SetCursorLocked(true);
}

std::string UIManager::ConsumeCraftRequest() {
    std::string request = std::move(m_CraftRequest);
    m_CraftRequest.clear();
    return request;
}

size_t UIManager::ConsumeInventoryUseRequest() {
    const size_t request = m_InventoryUseRequest;
    m_InventoryUseRequest = static_cast<size_t>(-1);
    return request;
}

size_t UIManager::ConsumeInventoryDropRequest() {
    const size_t request = m_InventoryDropRequest;
    m_InventoryDropRequest = static_cast<size_t>(-1);
    return request;
}

bool UIManager::ConsumeInventorySortRequest() {
    const bool requested = m_InventorySortRequest;
    m_InventorySortRequest = false;
    return requested;
}

std::string UIManager::ConsumeTalentUnlockRequest() {
    std::string request = std::move(m_TalentUnlockRequest);
    m_TalentUnlockRequest.clear();
    return request;
}

std::string UIManager::ConsumeDealerPurchaseRequest() {
    std::string request = std::move(m_DealerPurchaseRequest);
    m_DealerPurchaseRequest.clear();
    return request;
}

bool UIManager::ConsumePetRequest() {
    const bool requested = m_PetRequest;
    m_PetRequest = false;
    return requested;
}

void UIManager::SetCompanionCustomization(const std::string& name, int coat) {
    m_CompanionName.fill('\0');
    const size_t count = std::min(name.size(), m_CompanionName.size() - 1);
    std::copy_n(name.begin(), count, m_CompanionName.begin());
    m_CompanionCoat = std::clamp(coat, 0, 3);
}

std::string UIManager::GetCompanionName() const {
    return m_CompanionName.data();
}

bool UIManager::ConsumeSaveRequest() {
    const bool requested = m_SaveRequest;
    m_SaveRequest = false;
    return requested;
}

bool UIManager::ConsumeNewGameRequest() {
    const bool requested = m_NewGameRequest;
    m_NewGameRequest = false;
    return requested;
}

bool UIManager::ConsumeQuitRequest() {
    const bool requested = m_QuitRequest;
    m_QuitRequest = false;
    return requested;
}

bool UIManager::ConsumeCharacterCreatorRequest() {
    const bool requested = m_CharacterCreatorRequest;
    m_CharacterCreatorRequest = false;
    return requested;
}

void UIManager::RenderQuitScreen(Window& window) {
    window.PollEvents();
    glClearColor(0.05f, 0.05f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    BeginFrame();

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(520.0f, io.DisplaySize.x - 40.0f), 0), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 24.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.88f, 0.58f, 0.15f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.10f, 0.98f));

    if (ImGui::Begin("QuitModal", nullptr, flags)) {
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("B A Z U K O").x * 1.5f) * 0.5f);
        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextColored(ImVec4(0.96f, 0.72f, 0.16f, 1.0f), "B A Z U K O");
        ImGui::SetWindowFontScale(1.0f);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.40f, 0.85f, 0.45f, 1.0f), "Guardando partida...");
        ImGui::Spacing();
        ImGui::Text("Tus datos, inventario y apariencia se han guardado con exito.");
        ImGui::TextDisabled("Cerrando aplicacion con seguridad. ¡Hasta pronto, sobreviviente!");
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    EndFrame();
    window.SwapBuffers();
}

void UIManager::SetSaveNotification(const std::string& message) {
    m_SaveNotification = message;
    m_SaveNotificationTimer = 3.5f;
}

void UIManager::SetPickupNotification(const std::string& message, bool rare) {
    m_PickupNotification = message;
    m_PickupNotificationTimer = 2.5f;
    m_PickupNotificationRare = rare;
}

void UIManager::LoadLoadingArtwork(int index) {
    if (index < 0 || index >= static_cast<int>(m_LoadingArtworkLoadAttempted.size()) ||
        m_LoadingArtworkLoadAttempted[static_cast<size_t>(index)]) {
        return;
    }
    m_LoadingArtworkLoadAttempted[static_cast<size_t>(index)] = true;

    const std::string root = "public/images/pantalla-carga/";
    const std::string backgroundPath = Paths::Find(root + "fondo_pantalla_carga_" + std::to_string(index + 1) + ".webp");
    const std::string foregroundPath = Paths::Find(root + "personaje_pantalla_carga_" + std::to_string(index + 1) + ".webp");
    std::error_code error;
    if (!std::filesystem::exists(backgroundPath, error) || error) {
        LogWarn("No se encontro el fondo de la ilustracion de carga: {}", backgroundPath);
        return;
    }
    if (!std::filesystem::exists(foregroundPath, error) || error) {
        LogWarn("No se encontro el personaje de la ilustracion de carga: {}", foregroundPath);
        return;
    }

    m_LoadingBackgrounds[static_cast<size_t>(index)] = Texture::LoadCached(backgroundPath, false);
    m_LoadingForegrounds[static_cast<size_t>(index)] = Texture::LoadCached(foregroundPath, false);
}

void UIManager::DrawLoadingArtwork() {
    const auto now = std::chrono::steady_clock::now();
    if (m_LoadingArtwork < 0 ||
        std::chrono::duration<float>(now - m_LoadingArtworkChangedAt).count() >= LoadingArtworkDuration) {
        if (m_LoadingArtworkDeckPosition >= m_LoadingArtworkDeck.size()) {
            for (size_t i = 0; i < m_LoadingArtworkDeck.size(); ++i) {
                m_LoadingArtworkDeck[i] = static_cast<int>(i);
            }
            static std::mt19937 randomEngine(
                static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));
            std::shuffle(m_LoadingArtworkDeck.begin(), m_LoadingArtworkDeck.end(), randomEngine);
            if (m_LoadingArtworkDeck.size() > 1 &&
                m_LoadingArtworkDeck.front() == m_LoadingArtwork) {
                std::swap(m_LoadingArtworkDeck[0], m_LoadingArtworkDeck[1]);
            }
            m_LoadingArtworkDeckPosition = 0;
        }
        m_PreviousLoadingArtwork = m_LoadingArtwork;
        m_LoadingArtwork = m_LoadingArtworkDeck[m_LoadingArtworkDeckPosition++];
        LoadLoadingArtwork(m_LoadingArtwork);
        m_LoadingArtworkChangedAt = std::chrono::steady_clock::now();
    }

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    const auto drawPair = [&](int index, float alpha) {
        if (index < 0) return;
        const size_t artworkIndex = static_cast<size_t>(index);
        if (m_LoadingBackgrounds[artworkIndex] && m_LoadingForegrounds[artworkIndex]) {
            AddLoadingArtworkImage(drawList, *m_LoadingBackgrounds[artworkIndex], displaySize, false, alpha);
            AddLoadingArtworkImage(drawList, *m_LoadingForegrounds[artworkIndex], displaySize, true, alpha);
        }
    };

    const float elapsed = std::chrono::duration<float>(now - m_LoadingArtworkChangedAt).count();
    const float transition = std::clamp(elapsed / LoadingArtworkTransitionDuration, 0.0f, 1.0f);
    const size_t currentIndex = static_cast<size_t>(m_LoadingArtwork);
    const bool currentArtworkAvailable =
        m_LoadingBackgrounds[currentIndex] && m_LoadingForegrounds[currentIndex];
    if (!currentArtworkAvailable && m_PreviousLoadingArtwork >= 0) {
        drawPair(m_PreviousLoadingArtwork, 1.0f);
    } else {
        if (m_PreviousLoadingArtwork >= 0 && transition < 1.0f) {
            drawPair(m_PreviousLoadingArtwork, 1.0f - transition);
        }
        drawPair(m_LoadingArtwork, transition);
    }

    const ImU32 transparent = IM_COL32(5, 8, 13, 0);
    const ImU32 dark = IM_COL32(5, 8, 13, 210);
    drawList->AddRectFilledMultiColor(ImVec2(0.0f, displaySize.y * 0.40f), displaySize,
                                      transparent, transparent, dark, dark);
    drawList->AddRectFilledMultiColor(ImVec2(0.0f, 0.0f), ImVec2(displaySize.x * 0.72f, displaySize.y),
                                      IM_COL32(5, 8, 13, 165), IM_COL32(5, 8, 13, 0),
                                      IM_COL32(5, 8, 13, 0), IM_COL32(5, 8, 13, 165));
}

void UIManager::RenderLoadingScreen(Window& window, float progress, const std::string& stageText, const std::string& tipText) {
    window.PollEvents();
    glClearColor(0.06f, 0.07f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    BeginFrame();
    DrawLoadingArtwork();

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(48.0f, io.DisplaySize.y - 36.0f), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowSize(ImVec2(std::max(1.0f, std::min(500.0f, io.DisplaySize.x - 64.0f)), 0.0f), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    if (ImGui::Begin("LoadingModal", nullptr, flags)) {
        ImGui::SetWindowFontScale(1.55f);
        ImGui::TextColored(ImVec4(0.96f, 0.72f, 0.16f, 1.0f), "B A Z U K O");
        ImGui::SetWindowFontScale(1.0f);

        ImGui::TextDisabled("SUPERVIVENCIA URBANA · BOGOTÁ");
        ImGui::Spacing();
        ImGui::TextWrapped("%s", stageText.c_str());

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.88f, 0.58f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.14f, 0.17f, 0.90f));
        char progressOverlay[32];
        snprintf(progressOverlay, sizeof(progressOverlay), "%.0f %%", std::clamp(progress, 0.0f, 1.0f) * 100.0f);
        ImGui::ProgressBar(std::clamp(progress, 0.0f, 1.0f), ImVec2(-1.0f, 16.0f), progressOverlay);
        ImGui::PopStyleColor(2);

        if (!tipText.empty()) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.88f, 0.86f, 0.80f, 1.0f));
            ImGui::TextWrapped("%s", tipText.c_str());
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();

    ImGui::PopStyleVar();

    EndFrame();
    window.SwapBuffers();
}

void UIManager::RenderHUD(float dt, const glm::vec3& playerPos, Camera& camera, Renderer& renderer, Window& window,
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
                          bool boostActive, float boostTimer, float psychosisSeverity) {
    if (m_SaveNotificationTimer > 0.0f) {
        m_SaveNotificationTimer -= (dt > 0.0f ? dt : 0.016f);
        if (m_SaveNotificationTimer <= 0.0f) m_SaveNotification.clear();
    }
    if (m_PickupNotificationTimer > 0.0f) {
        m_PickupNotificationTimer -= (dt > 0.0f ? dt : 0.016f);
        if (m_PickupNotificationTimer <= 0.0f) m_PickupNotification.clear();
    }
    static const char* needNames[] = {"Hambre", "Sed", "Energia", "Salud", "Higiene", "Animo", "Vicio"};
    static const ImVec4 needColors[] = {
        {0.82f, 0.62f, 0.12f, 1.0f}, {0.20f, 0.48f, 0.88f, 1.0f},
        {0.63f, 0.38f, 0.80f, 1.0f}, {0.77f, 0.20f, 0.18f, 1.0f},
        {0.20f, 0.65f, 0.82f, 1.0f}, {0.61f, 0.31f, 0.75f, 1.0f},
        {0.90f, 0.48f, 0.12f, 1.0f}
    };
    static const float warningThresholds[] = {30.0f, 30.0f, 30.0f, 25.0f, 30.0f, 30.0f, 20.0f};
    const ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove;
    const int totalMinutes = static_cast<int>(dayProgress * 1440.0f) % 1440;
    const int hour = totalMinutes / 60;
    const int minute = totalMinutes % 60;
    const float fps = dt > 0.0f ? 1.0f / dt : 0.0f;

    if (m_DeathScreenOpen) {
        SignUI::DrawBackdrop(0.85f);
        const ImVec4 blueText(0.84f, 0.89f, 1.0f, 1.0f);
        if (SignUI::BeginMenu("Fin de la partida", m_DeathByVehicleImpact ? "Te atropellaron" : "Te quedaste sin salud",
                              "La partida permanece pausada hasta que elijas recuperarte.", SignUI::Icon::Heart, nullptr, 520.0f, 440.0f)) {
            ImGui::PushStyleColor(ImGuiCol_Text, blueText);
            ImGui::TextWrapped("Estado al caer: %s.", m_DeathCauses.empty() ? "salud agotada" : m_DeathCauses.c_str());
            ImGui::Spacing();
            ImGui::TextWrapped("Volverás al cambuche con salud y necesidades recuperadas al menos al 60 %%. Se perderá la mitad de los tipos de objetos del inventario y se curarán las enfermedades.");
            ImGui::Spacing();
            if (m_DeathByVehicleImpact) {
                ImGui::TextWrapped("Mira ambos sentidos antes de cruzar; los vehículos necesitan espacio para frenar.");
            } else {
                ImGui::TextWrapped("Come, bebe y descansa para mantener tus necesidades.");
            }
            ImGui::PopStyleColor();
            SignUI::BeginFooter();
            if (SignUI::PrimaryButton("Volver al cambuche")) m_RespawnRequest = true;
        }
        SignUI::EndMenu();
        return;
    }

    if (!m_Paused && !m_PickupNotification.empty() && m_PickupNotificationTimer > 0.0f) {
        const float alpha = std::clamp(m_PickupNotificationTimer / 0.5f, 0.0f, 1.0f);
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f,
                                      ImGui::GetIO().DisplaySize.y * 0.78f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.82f * alpha);
        if (ImGui::Begin("PickupNotification", nullptr, hudFlags | ImGuiWindowFlags_NoInputs)) {
            const ImVec4 color = m_PickupNotificationRare
                ? ImVec4(1.0f, 0.78f, 0.25f, alpha)
                : ImVec4(0.94f, 0.94f, 0.90f, alpha);
            ImGui::TextColored(color, "%s", m_PickupNotification.c_str());
        }
        ImGui::End();
    }

    if (m_CraftMenuOpen && !m_Paused) {
        SignUI::DrawBackdrop(0.45f);
        const ImVec4 blueText(0.84f, 0.89f, 1.0f, 1.0f);
        bool closeRequested = false;
        if (SignUI::BeginMenu("Mesa de crafteo", "Mesa de crafteo", "Selecciona una receta · C o Esc para cerrar",
                              SignUI::Icon::Gear, &closeRequested)) {
            ImGui::PushStyleColor(ImGuiCol_Text, blueText);
            ImGui::TextWrapped("Los ingredientes se descuentan solo cuando se puede fabricar.");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            int rowIndex = 0;
            for (const auto& option : craftingOptions) {
                ImGui::PushID(option.id.c_str());
                if (SignUI::BeginRow("receta", rowIndex++)) {
                    const float buttonWidth = SignUI::RowButtonWidth("Craftear");
                    const float rowTop = SignUI::RowBegin();
                    ImGui::PushFont(SignUI::GetFonts().monoBold14);
                    ImGui::Text("%s", option.name.c_str());
                    ImGui::PopFont();
                    SignUI::RowCluster(rowTop, ImGui::GetWindowWidth() - 10.0f - buttonWidth);
                    if (SignUI::RowButton("Craftear", option.craftable, true)) m_CraftRequest = option.id;
                    SignUI::RowBelow(rowTop);
                    ImGui::PushStyleColor(ImGuiCol_Text, blueText);
                    ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 18.0f - buttonWidth);
                    ImGui::TextWrapped("Ingredientes: %s", option.ingredients.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::PopStyleColor();
                }
                SignUI::EndRow();
                ImGui::PopID();
            }
            ImGui::Spacing();
            SignUI::NoteText("Hierba seca aparece en las recetas originales, pero no tiene resultado definido en items.js.");
            SignUI::BeginFooter();
            if (SignUI::PrimaryButton("Cerrar")) closeRequested = true;
        }
        SignUI::EndMenu();
        if (closeRequested) m_CraftMenuOpen = false;
    }

    if (m_Paused) {
        using SignUI::Icon;
        SignUI::DrawBackdrop(0.45f);
        const SignUI::Fonts& fonts = SignUI::GetFonts();
        const ImVec4 blueText(0.84f, 0.89f, 1.0f, 1.0f);
        const ImVec4 gold(0.93f, 0.84f, 0.53f, 1.0f);
        const auto fmt = [](const char* pattern, auto... values) {
            char buffer[160];
            std::snprintf(buffer, sizeof(buffer), pattern, values...);
            return std::string(buffer);
        };
        std::string pauseTitle = "Partida en pausa";
        std::string pauseSubtitle = "Vagabundo / Habitante de Calle · Bogotá";
        Icon pauseIcon = Icon::Pause;
        if (m_RadioMenuOpen) {
            pauseTitle = "Radio · Sintonizador";
            pauseSubtitle = "Emisoras AM/FM del barrio";
            pauseIcon = Icon::Radio;
        } else if (m_PauseSection == 1) {
            pauseTitle = "Inventario · Costal";
            pauseSubtitle = fmt("Peso %.1f / %.0f kg", inventoryWeight, inventoryCapacity);
            pauseIcon = Icon::Bag;
        } else if (m_PauseSection == 2) {
            pauseTitle = "Árbol de talentos";
            pauseSubtitle = fmt("Puntos disponibles: %d", talentPoints);
            pauseIcon = Icon::Star;
        } else if (m_PauseSection == 3) {
            pauseTitle = "El Chato · Compañero";
            pauseSubtitle = m_CompanionName.data();
            pauseIcon = Icon::Paw;
        } else if (m_PauseSection == 4) {
            pauseTitle = "Dealer · El Cartucho";
            pauseSubtitle = "Sustancias disponibles";
            pauseIcon = Icon::Pill;
        } else if (m_PauseSection == 5) {
            pauseTitle = m_ShopTitle;
            pauseSubtitle = m_ShopSubtitle;
            pauseIcon = Icon::Bag;
        } else if (m_PauseSection == 6) {
            pauseTitle = "Recicladora El Progreso";
            pauseSubtitle = "Pagan de contado. Comida, remedios y sustancias no se reciben.";
            pauseIcon = Icon::Gear;
        }
        // Dinero (y carga) a la derecha del boton principal del pie
        const auto footerMoney = [&](bool showWeight) {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            float x = max.x + 16.0f;
            const float y = (min.y + max.y) * 0.5f - 7.0f;
            const auto put = [&](const char* text, ImU32 color) {
                draw->AddText(fonts.monoBold13, 13.0f, ImVec2(x, y), color, text);
                x += fonts.monoBold13->CalcTextSizeA(13.0f, FLT_MAX, 0.0f, text).x;
            };
            char buffer[64];
            put("Tienes ", IM_COL32(255, 255, 255, 255));
            std::snprintf(buffer, sizeof(buffer), "$ %.0f", m_PlaceMoney);
            put(buffer, IM_COL32(246, 196, 0, 255));
            if (showWeight) {
                x += 14.0f;
                std::snprintf(buffer, sizeof(buffer), "Carga %.1f / %.0f kg", m_PlaceWeight, m_PlaceCapacity);
                put(buffer, IM_COL32(214, 228, 255, 255));
            }
        };
        // ancho de la columna derecha de una fila: cantidad + botones
        const auto rowRight = [](float contentWidth) { return ImGui::GetWindowWidth() - 10.0f - contentWidth; };
        bool closeRequested = false;
        if (SignUI::BeginMenu(pauseTitle.c_str(), pauseTitle.c_str(), pauseSubtitle.c_str(), pauseIcon, &closeRequested)) {
            if (m_RadioMenuOpen) {
                ImGui::PushStyleColor(ImGuiCol_Text, blueText);
                ImGui::TextWrapped("Selecciona una emisora. La reproducción de audio en línea se conectará en una etapa posterior.");
                ImGui::PopStyleColor();
                if (m_RadioStations.empty()) {
                    ImGui::TextWrapped("No hay emisoras cargadas. Revisa assets/radio_stations.json y los mensajes de inicio.");
                }
                for (size_t index = 0; index < m_RadioStations.size(); ++index) {
                    const RadioStation& station = m_RadioStations[index];
                    const bool selected = m_SelectedRadioStation == index;
                    if (SignUI::BeginRow(station.id.c_str(), static_cast<int>(index))) {
                        const float buttonWidth = SignUI::RowButtonWidth(selected ? "Sintonizada" : "Sintonizar");
                        const float rowTop = SignUI::RowBegin();
                        ImGui::PushFont(fonts.monoBold14);
                        ImGui::Text("%s  ·  %s", station.frequency.c_str(), station.name.c_str());
                        ImGui::PopFont();
                        SignUI::RowCluster(rowTop, rowRight(buttonWidth));
                        if (SignUI::RowButton(selected ? "Sintonizada" : "Sintonizar", !selected, selected)) m_SelectedRadioStation = index;
                        SignUI::RowBelow(rowTop);
                        ImGui::PushStyleColor(ImGuiCol_Text, blueText);
                        ImGui::PushTextWrapPos(rowRight(buttonWidth + 8.0f));
                        ImGui::TextWrapped("%s · %s", station.genre.c_str(), station.program.c_str());
                        ImGui::PopTextWrapPos();
                        ImGui::PopStyleColor();
                    }
                    SignUI::EndRow();
                }
                if (!m_RadioStations.empty()) {
                    const RadioStation& selected = m_RadioStations[m_SelectedRadioStation];
                    ImGui::Spacing();
                    ImGui::TextColored(gold, "Sintonizada: %s (%s)", selected.name.c_str(), selected.frequency.c_str());
                    SignUI::NoteText(selected.streamUrl.empty() ? "Emisora ficticia: sin URL configurada." : selected.streamUrl.c_str());
                    SignUI::NoteText("La URL se guarda como configuración; todavía no se reproduce audio en línea.");
                }
                SignUI::BeginFooter();
                if (SignUI::PrimaryButton(m_RadioReturnsToPause ? "Volver a pausa" : "Cerrar radio")) CloseRadioMenu();
            } else if (m_PauseSection == 1) {
                if (inventoryOptions.empty()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, blueText);
                    ImGui::TextWrapped("El costal está vacío. Rebusca en las bolsas para encontrar objetos.");
                    ImGui::PopStyleColor();
                } else {
                    SignUI::FlexReset();
                    if (SignUI::Button("Ordenar por nombre", Icon::Book)) m_InventorySortRequest = true;
                    ImGui::Spacing();
                    int rowIndex = 0;
                    for (const auto& item : inventoryOptions) {
                        ImGui::PushID(static_cast<int>(item.itemIndex));
                        if (SignUI::BeginRow("fila", rowIndex++)) {
                            const float useWidth = SignUI::RowButtonWidth("Usar");
                            const float dropWidth = SignUI::RowButtonWidth("Soltar");
                            const float cluster = 46.0f + useWidth + dropWidth + 16.0f;
                            const float rowTop = SignUI::RowBegin();
                            ImGui::PushFont(fonts.monoBold14);
                            ImGui::Text("%s", item.name.c_str());
                            ImGui::PopFont();
                            ImGui::SameLine(rowRight(cluster));
                            ImGui::SetCursorPosY(rowTop + 7.0f);
                            ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.0f, 1.0f), "x%d", item.quantity);
                            SignUI::RowCluster(rowTop, rowRight(useWidth + dropWidth + 8.0f));
                            if (SignUI::RowButton("Usar", item.usable, true)) m_InventoryUseRequest = item.itemIndex;
                            ImGui::SameLine(0.0f, 8.0f);
                            if (SignUI::RowButton("Soltar")) m_InventoryDropRequest = item.itemIndex;
                            SignUI::RowBelow(rowTop);
                            ImGui::PushStyleColor(ImGuiCol_Text, blueText);
                            ImGui::Text("%.1f kg c/u", item.weight);
                            ImGui::PopStyleColor();
                        }
                        SignUI::EndRow();
                        ImGui::PopID();
                    }
                }
                SignUI::BeginFooter();
                if (SignUI::PrimaryButton("Cerrar inventario")) TogglePause();
            } else if (m_PauseSection == 2) {
                ImGui::PushStyleColor(ImGuiCol_Text, blueText);
                ImGui::TextWrapped("Invierte puntos ganados al subir de nivel. Cada talento puede desbloquearse una sola vez.");
                ImGui::PopStyleColor();
                ImGui::Spacing();
                const std::array<const char*, 3> branches{"Recolección", "Supervivencia", "Vida en la calle"};
                const std::array<const char*, 3> branchIds{"collector", "surv", "crime"};
                for (size_t branch = 0; branch < branches.size(); ++branch) {
                    if (!SignUI::BeginSection(branches[branch], Icon::Star)) { SignUI::EndSection(); continue; }
                    int rowIndex = 0;
                    for (const auto& talent : talentOptions) {
                        if (talent.id.rfind(branchIds[branch], 0) != 0) continue;
                        ImGui::PushID(talent.id.c_str());
                        if (SignUI::BeginRow("talento", rowIndex++)) {
                            const float buttonWidth = talent.unlocked ? 90.0f : SignUI::RowButtonWidth("Aprender");
                            const float rowTop = SignUI::RowBegin();
                            ImGui::PushFont(fonts.monoBold13);
                            ImGui::Text("%s", talent.name.c_str());
                            ImGui::PopFont();
                            ImGui::SameLine();
                            ImGui::TextColored(gold, "· %d punto%s", talent.cost, talent.cost == 1 ? "" : "s");
                            SignUI::RowCluster(rowTop, rowRight(buttonWidth));
                            if (talent.unlocked) {
                                ImGui::SetCursorPosY(rowTop + 7.0f);
                                ImGui::TextColored(ImVec4(0.45f, 0.90f, 0.62f, 1.0f), "APRENDIDO");
                            } else if (SignUI::RowButton("Aprender", talent.available, true)) {
                                m_TalentUnlockRequest = talent.id;
                            }
                            SignUI::RowBelow(rowTop);
                            ImGui::PushStyleColor(ImGuiCol_Text, blueText);
                            ImGui::PushTextWrapPos(rowRight(buttonWidth + 8.0f));
                            ImGui::TextWrapped("%s", talent.description.c_str());
                            ImGui::PopTextWrapPos();
                            ImGui::PopStyleColor();
                            if (!talent.requirement.empty())
                                ImGui::TextColored(ImVec4(1.0f, 0.71f, 0.67f, 1.0f), "Requisito: %s", talent.requirement.c_str());
                        }
                        SignUI::EndRow();
                        ImGui::PopID();
                    }
                    SignUI::EndSection();
                }
                SignUI::BeginFooter();
                if (SignUI::PrimaryButton("Cerrar talentos")) TogglePause();
            } else if (m_PauseSection == 3) {
                ImGui::PushStyleColor(ImGuiCol_Text, blueText);
                ImGui::TextWrapped("%s", companionIsFriend
                    ? "El Chato es tu compañero y te sigue por el barrio."
                    : "El Chato ronda el barrio. Acércate y pulsa E para hacer amistad.");
                ImGui::PopStyleColor();
                ImGui::Spacing();
                if (SignUI::BeginSection("Nombre y pelaje", Icon::Paw)) {
                    ImGui::SetNextItemWidth(-1.0f);
                    if (ImGui::InputText("##CompanionName", m_CompanionName.data(), m_CompanionName.size())) {
                        if (m_CompanionName[0] == '\0') SetCompanionCustomization("El Chato", m_CompanionCoat);
                    }
                    ImGui::Spacing();
                    const std::array<const char*, 4> coatNames{"Canela", "Marrón", "Gris", "Negro"};
                    SignUI::FlexReset();
                    for (int coat = 0; coat < static_cast<int>(coatNames.size()); ++coat) {
                        if (SignUI::Button(coatNames[static_cast<size_t>(coat)], Icon::None, m_CompanionCoat == coat)) m_CompanionCoat = coat;
                    }
                }
                SignUI::EndSection();
                SignUI::FlexReset();
                if (SignUI::Button("Acariciar a El Chato", Icon::Heart) && companionIsFriend) m_PetRequest = true;
                if (!companionIsFriend) SignUI::NoteText("Hazte amigo de El Chato (E) para acariciarlo.");
                SignUI::NoteText("La amistad, el nombre y el pelaje se guardan con la partida.");
                SignUI::BeginFooter();
                if (SignUI::PrimaryButton("Cerrar ficha")) TogglePause();
            } else if (m_PauseSection == 4) {
                ImGui::PushStyleColor(ImGuiCol_Text, blueText);
                ImGui::TextWrapped("Compra al dealer. De noche puedes venderle las sustancias que lleves.");
                ImGui::PopStyleColor();
                ImGui::Spacing();
                int rowIndex = 0;
                for (const DealerOption& option : dealerOptions) {
                    ImGui::PushID(option.id.c_str());
                    if (SignUI::BeginRow("dealer", rowIndex++)) {
                        const float buttonWidth = SignUI::RowButtonWidth("Comprar");
                        const float rowTop = SignUI::RowBegin();
                        ImGui::PushFont(fonts.monoBold14);
                        ImGui::Text("%s", option.name.c_str());
                        ImGui::PopFont();
                        ImGui::SameLine(rowRight(buttonWidth + 110.0f));
                        ImGui::SetCursorPosY(rowTop + 7.0f);
                        ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.0f, 1.0f), "$ %d COP", option.price);
                        SignUI::RowCluster(rowTop, rowRight(buttonWidth));
                        if (SignUI::RowButton("Comprar", option.affordable && option.canCarry, true)) m_DealerPurchaseRequest = option.id;
                        SignUI::RowBelow(rowTop);
                        if (!option.affordable) ImGui::TextColored(ImVec4(1.0f, 0.71f, 0.67f, 1.0f), "No tienes dinero suficiente.");
                        else if (!option.canCarry) ImGui::TextColored(ImVec4(1.0f, 0.71f, 0.67f, 1.0f), "No cabe en el costal.");
                    }
                    SignUI::EndRow();
                    ImGui::PopID();
                }
                SignUI::BeginFooter();
                if (SignUI::PrimaryButton("Cerrar")) TogglePause();
            } else if (m_PauseSection == 5) {
                int rowIndex = 0;
                for (const ShopOption& option : m_ShopOptions) {
                    ImGui::PushID(option.key.c_str());
                    if (SignUI::BeginRow("producto", rowIndex++)) {
                        const char* buttonLabel = option.owned ? "Listo" : "Comprar";
                        const float buttonWidth = SignUI::RowButtonWidth("Comprar");
                        const float rowTop = SignUI::RowBegin();
                        ImGui::PushFont(fonts.monoBold14);
                        ImGui::Text("%s", option.name.c_str());
                        ImGui::PopFont();
                        ImGui::SameLine(rowRight(buttonWidth + 92.0f));
                        ImGui::SetCursorPosY(rowTop + 7.0f);
                        ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.0f, 1.0f), "$ %d", option.price);
                        SignUI::RowCluster(rowTop, rowRight(buttonWidth));
                        if (SignUI::RowButton(buttonLabel, option.enabled, true)) m_ShopPurchaseRequest = option.key;
                        SignUI::RowBelow(rowTop);
                        ImGui::PushStyleColor(ImGuiCol_Text, blueText);
                        ImGui::PushTextWrapPos(rowRight(buttonWidth + 8.0f));
                        ImGui::TextWrapped("%s", option.description.c_str());
                        ImGui::PopTextWrapPos();
                        ImGui::PopStyleColor();
                    }
                    SignUI::EndRow();
                    ImGui::PopID();
                }
                SignUI::BeginFooter();
                if (SignUI::PrimaryButton("Cerrar")) TogglePause();
                footerMoney(true);
            } else if (m_PauseSection == 6) {
                int total = 0;
                for (const RecyclerRow& row : m_RecyclerRows) total += row.totalPrice;
                if (m_RecyclerRows.empty()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, blueText);
                    ImGui::TextWrapped("No traes nada para vender. Busca en las bolsas de basura: latas, botellas, cartón, chatarra y, con suerte, objetos de valor.");
                    ImGui::PopStyleColor();
                }
                int rowIndex = 0;
                for (const RecyclerRow& row : m_RecyclerRows) {
                    ImGui::PushID(rowIndex);
                    if (SignUI::BeginRow("venta", rowIndex++)) {
                        const float rowTop = SignUI::RowBegin();
                        ImGui::PushFont(fonts.monoBold14);
                        ImGui::Text("%s", row.name.c_str());
                        ImGui::PopFont();
                        ImGui::SameLine(rowRight(150.0f));
                        ImGui::SetCursorPosY(rowTop + 7.0f);
                        ImGui::TextColored(ImVec4(1, 1, 1, 1), "× %d", row.quantity);
                        ImGui::SameLine(rowRight(90.0f));
                        ImGui::SetCursorPosY(rowTop + 7.0f);
                        ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.0f, 1.0f), "$ %d", row.totalPrice);
                        SignUI::RowBelow(rowTop - 8.0f);
                        ImGui::PushStyleColor(ImGuiCol_Text, blueText);
                        ImGui::Text("$ %d c/u", row.unitPrice);
                        ImGui::PopStyleColor();
                    }
                    SignUI::EndRow();
                    ImGui::PopID();
                }
                SignUI::BeginFooter();
                char sellLabel[64];
                if (total > 0) std::snprintf(sellLabel, sizeof(sellLabel), "Vender todo · $ %d", total);
                else std::snprintf(sellLabel, sizeof(sellLabel), "Vender todo");
                if (SignUI::PrimaryButton(sellLabel, false, total > 0)) m_RecyclerSellRequest = true;
                footerMoney(false);
            } else {
                // ---- Sonido y volumen ----
                if (SignUI::BeginSection("Sonido y Volumen", Icon::Sound)) {
                    const bool muted = audio.IsMuted();
                    float volume = audio.GetVolume();
                    const float rowTop = ImGui::GetCursorPosY();
                    const float available = ImGui::GetContentRegionAvail().x;
                    const float buttonWidth = 14.0f + 22.0f + SignUI::SpacedTextWidth(fonts.monoBold12, 12.0f,
                        SignUI::Upper(muted ? "Activar" : "Silenciar").c_str(), 1.0f) + 14.0f;
                    ImGui::SetCursorPosY(rowTop + 11.0f);
                    ImGui::TextUnformatted("Volumen maestro:");
                    ImGui::SameLine(0.0f, 12.0f);
                    ImGui::SetCursorPosY(rowTop + 9.0f);
                    const float sliderWidth = std::max(80.0f, available - 118.0f - 12.0f - 12.0f - 44.0f - 12.0f - buttonWidth);
                    if (SignUI::Slider("volumen", &volume, sliderWidth)) audio.SetVolume(volume);
                    ImGui::SameLine(0.0f, 10.0f);
                    ImGui::SetCursorPosY(rowTop + 11.0f);
                    ImGui::PushFont(fonts.monoBold12);
                    ImGui::TextColored(gold, "%d%%", static_cast<int>(volume * 100.0f + 0.5f));
                    ImGui::PopFont();
                    ImGui::SameLine(available - buttonWidth);
                    ImGui::SetCursorPosY(rowTop);
                    SignUI::FlexReset();
                    if (SignUI::Button(muted ? "Activar" : "Silenciar", muted ? Icon::Mute : Icon::Sound)) audio.SetMuted(!muted);
                }
                SignUI::EndSection();

                // ---- Estado actual ----
                if (SignUI::BeginSection("Estado Actual", Icon::Chart)) {
                    const float columnX = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x * 0.5f;
                    const auto pair = [&](const char* label, const std::string& value, bool second) {
                        if (second) ImGui::SameLine(columnX);
                        ImGui::TextColored(ImVec4(0.67f, 0.67f, 0.67f, 1.0f), "%s ", label);
                        ImGui::SameLine(0.0f, 0.0f);
                        ImGui::PushFont(fonts.monoBold13);
                        ImGui::TextUnformatted(value.c_str());
                        ImGui::PopFont();
                    };
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
                    pair("Personaje:", playerName.empty() ? "Parcero" : playerName, false);
                    pair("Dinero:", fmt("$ %.0f", money), true);
                    pair("Tiempo:", fmt("Día %d — %02d:%02d", static_cast<int>(dayProgress) + 1, hour, minute), false);
                    pair("Nivel:", fmt("Nv %d · XP %d", level, xp), true);
                    pair("Salud:", fmt("%.0f%%", needs[3]), false);
                    pair("Sed / Hambre:", fmt("%.0f%% / %.0f%%", needs[1], needs[0]), true);
                    pair("Energía:", fmt("%.0f%%", needs[2]), false);
                    pair("Higiene:", fmt("%.0f%%", needs[4]), true);
                    pair("Ánimo:", fmt("%.0f%%", needs[5]), false);
                    pair("Vicio:", fmt("%.0f%%", needs[6]), true);
                    pair("Costal:", fmt("%.1f / %.0f kg", inventoryWeight, inventoryCapacity), false);
                    pair("Peatones / Tráfico:", fmt("%llu / %llu", static_cast<unsigned long long>(pedestrians),
                                                    static_cast<unsigned long long>(traffic)), true);
                    ImGui::PopStyleVar();
                }
                SignUI::EndSection();

                // ---- Opciones rápidas ----
                if (SignUI::BeginSection("Opciones Rápidas", Icon::Gear)) {
                    SignUI::FlexReset();
                    if (SignUI::Button("Radio transistor (R)", Icon::Radio)) OpenRadioMenu();
                    if (SignUI::Button("Guardar partida ahora", Icon::Save)) m_SaveRequest = true;
                    if (SignUI::Button(m_RadarVisible ? "Ocultar radar (N)" : "Mostrar radar (N)", Icon::Compass)) m_RadarVisible = !m_RadarVisible;
                    if (SignUI::Button(window.IsFullscreen() ? "Modo ventana (F11)" : "Pantalla completa (F11)", Icon::Camera)) window.ToggleFullscreen();
                    if (SignUI::Button("Personaje (P)", Icon::Person)) {
                        m_CharacterCreatorRequest = true;
                        TogglePause();
                    }
                    if (SignUI::Button("Inventario (I)", Icon::Bag)) OpenPauseSection(1);
                    if (SignUI::Button("Talentos (T)", Icon::Star)) OpenPauseSection(2);
                    if (SignUI::Button("El Chato (O)", Icon::Paw)) OpenPauseSection(3);
                    if (SignUI::Button("Empezar partida nueva", Icon::Home, false, true)) ImGui::OpenPopup("ConfirmNewGameModal");
                    if (!m_SaveNotification.empty() && m_SaveNotificationTimer > 0.0f)
                        ImGui::TextColored(ImVec4(0.45f, 0.92f, 0.55f, 1.0f), "%s", m_SaveNotification.c_str());
                }
                SignUI::EndSection();

                if (ImGui::BeginPopupModal("ConfirmNewGameModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::Text("¿Deseas empezar una partida completamente nueva?");
                    ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.20f, 1.0f), "Se reiniciará todo el progreso y se abrirá el Creador de Personaje.");
                    ImGui::Spacing();
                    SignUI::FlexReset();
                    if (SignUI::Button("Sí, iniciar nueva partida", Icon::None, false, true)) {
                        m_NewGameRequest = true;
                        TogglePause();
                        ImGui::CloseCurrentPopup();
                    }
                    if (SignUI::Button("Cancelar")) ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }

                // ---- Guía de controles ----
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1, 1, 1, 0.06f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(1, 1, 1, 0.10f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.88f, 0.60f, 1.0f));
                ImGui::PushFont(fonts.monoBold13);
                const bool guideOpen = ImGui::CollapsingHeader("Guía completa de controles y supervivencia", ImGuiTreeNodeFlags_DefaultOpen);
                ImGui::PopFont();
                ImGui::PopStyleColor(4);
                if (guideOpen) {
                    static const char* const guide[][2] = {
                        {"WASD", "Desplazarse por el barrio."},
                        {"Ratón", "Clic para fijar cámara (o arrastrar) · Rueda para zoom."},
                        {"Flechas", "Girar la cámara."},
                        {"Shift", "Correr."},
                        {"V", "Alternar vista tercera persona / isométrica."},
                        {"E", "Interactuar con basura, tiendas, cambuche o autos."},
                        {"Q", "Recoger objetos, robo directo a peatones o comprar al dealer."},
                        {"F", "Entregar reciclables de la misión en la recicladora."},
                        {"B / L", "Pedir limosna a peatones."},
                        {"Z / X", "Hacer popo / lanzar un mojón."},
                        {"I", "Inventario y acceso rápido."},
                        {"T", "Abrir árbol de talentos."},
                        {"O", "Ficha de El Chato."},
                        {"C", "Abrir mesa de crafteo."},
                        {"P", "Modificar apariencia del personaje."},
                        {"N", "Alternar radar / minimapa urbano."},
                        {"R", "Abrir sintonizador de radio transistor AM/FM."},
                        {"F11", "Alternar pantalla completa."},
                        {"ESC", "Pausar el juego / cerrar paneles."},
                    };
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 3.0f));
                    for (const auto& entry : guide) SignUI::KeyHint(entry[0], entry[1]);
                    ImGui::PopStyleVar();
                }
                SignUI::BeginFooter();
                if (SignUI::PrimaryButton("Continuar jugando")) TogglePause();
                ImGui::SameLine(0.0f, 14.0f);
                SignUI::FlexReset();
                if (SignUI::Button("Guardar y salir del juego", Icon::None, false, true)) m_QuitRequest = true;
            }
        }
        SignUI::EndMenu();
        if (closeRequested) {
            if (m_RadioMenuOpen) CloseRadioMenu();
            else TogglePause();
        }
    }

    {
        using SignUI::Icon;
        const SignUI::Fonts& fonts = SignUI::GetFonts();
        const float displayHeight = ImGui::GetIO().DisplaySize.y;
        const ImGuiWindowFlags plainFlags = hudFlags | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs;

        // Barra superior plana: nombre, día y hora, dinero, nivel y puntos de talento
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_Always);
        if (ImGui::Begin("Topbar", nullptr, plainFlags)) {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImVec2 at = ImGui::GetCursorScreenPos();
            float x = at.x;
            const auto item = [&](const std::string& text, ImU32 color) {
                SignUI::AddShadowText(draw, fonts.mono14, 14.0f, ImVec2(x, at.y), color, text.c_str());
                x += fonts.mono14->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, text.c_str()).x + 24.0f;
            };
            char buffer[96];
            item(playerName.empty() ? "Parcero" : playerName, IM_COL32(222, 184, 64, 255));
            std::snprintf(buffer, sizeof(buffer), "Día %d — %02d:%02d", static_cast<int>(dayProgress) + 1, hour, minute);
            item(buffer, IM_COL32(238, 238, 238, 255));
            std::snprintf(buffer, sizeof(buffer), "$ %.0f", money);
            item(buffer, IM_COL32(238, 238, 238, 255));
            std::snprintf(buffer, sizeof(buffer), "Nv %d", level);
            item(buffer, IM_COL32(238, 238, 238, 255));
            SignUI::DrawIcon(draw, Icon::Star, ImVec2(x + 7.0f, at.y + 8.0f), 15.0f);
            std::snprintf(buffer, sizeof(buffer), "%d", talentPoints);
            SignUI::AddShadowText(draw, fonts.mono14, 14.0f, ImVec2(x + 22.0f, at.y), IM_COL32(255, 209, 102, 255), buffer);
            x += 22.0f + 12.0f;
            ImGui::Dummy(ImVec2(std::max(210.0f, x - at.x), 18.0f));
            ImGui::SetCursorScreenPos(ImVec2(at.x, at.y + 25.0f));
            SignUI::StatBar(std::clamp(static_cast<float>(xp) / static_cast<float>(std::max(1, level * 100)), 0.0f, 1.0f),
                            IM_COL32(39, 174, 96, 255), 200.0f, 6.0f);
        }
        ImGui::End();

        // Barras de necesidades (abajo a la izquierda) con avisos encima
        ImGui::SetNextWindowPos(ImVec2(20, displayHeight - 20), ImGuiCond_Always, ImVec2(0, 1));
        if (ImGui::Begin("Necesidades", nullptr, plainFlags)) {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            if (needs[3] <= 10.0f) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.27f, 1.0f), "Salud crítica: usa vendajes o evita los vehículos.");
            } else if (needs[3] <= 25.0f) {
                ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.24f, 1.0f), "Salud baja: usa vendajes y evita los vehículos.");
            }
            if (boostActive) {
                ImGui::TextColored(ImVec4(0.25f, 0.95f, 1.0f, 1.0f), "ADRENALINA: %.1fs (+35%% vel, +botín)", std::max(0.0f, boostTimer));
            }
            const float withdrawal = needs[6] < 20.0f ? (20.0f - needs[6]) * 5.0f : 0.0f;
            if (withdrawal >= 80.0f) {
                const float flash = 0.5f + 0.5f * static_cast<float>(std::sin(ImGui::GetTime() * 8.0));
                ImGui::TextColored(ImVec4(1.0f, 0.15f * flash, 0.15f * flash, 1.0f), "¡ABSTINENCIA CRÍTICA: PSICOSIS!");
            } else if (withdrawal >= 50.0f) {
                ImGui::TextColored(ImVec4(1.0f, 0.60f, 0.20f, 1.0f), "Síndrome de abstinencia: paranoia");
            }
            static const Icon needIcons[] = {Icon::Bread, Icon::Drop, Icon::Energy, Icon::Pill, Icon::Soap, Icon::Heart, Icon::Flame};
            static const ImU32 needFills[] = {
                IM_COL32(201, 162, 39, 255), IM_COL32(58, 123, 213, 255), IM_COL32(176, 106, 214, 255), IM_COL32(192, 57, 43, 255),
                IM_COL32(48, 168, 204, 255), IM_COL32(142, 68, 173, 255), IM_COL32(224, 122, 30, 255)
            };
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            for (size_t i = 0; i < needs.size(); ++i) {
                const float value = std::clamp(needs[i], 0.0f, 100.0f);
                const bool critical = i == 3 ? value <= 10.0f
                    : i == 4 ? value <= 20.0f
                    : i == 5 ? value <= 10.0f
                    : i == 6 ? value <= 4.0f : value <= 0.0f;
                const float rowY = origin.y + static_cast<float>(i) * 20.0f;
                SignUI::DrawIcon(draw, needIcons[i], ImVec2(origin.x + 8.0f, rowY + 7.0f), 14.0f);
                ImGui::SetCursorScreenPos(ImVec2(origin.x + 32.0f, rowY + 2.0f));
                SignUI::StatBar(value / 100.0f, needFills[i], 160.0f, 10.0f, critical);
            }
            ImGui::SetCursorScreenPos(origin);
            ImGui::Dummy(ImVec2(192.0f, static_cast<float>(needs.size()) * 20.0f));
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

    // Viñeta roja oscura pulsante en los bordes de la pantalla durante psicosis
    if (psychosisSeverity > 0.0f && !m_Paused) {
        const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(ImGui::GetTime() * 4.0));
        const int alphaVal = static_cast<int>((75.0f + 65.0f * pulse) * psychosisSeverity);
        const ImU32 vigColor = IM_COL32(45, 6, 10, std::clamp(alphaVal, 0, 255));
        ImDrawList* bgDraw = ImGui::GetBackgroundDrawList();
        const ImVec2 dSize = ImGui::GetIO().DisplaySize;
        const float thickness = (35.0f + 15.0f * pulse) * psychosisSeverity;
        bgDraw->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(dSize.x, thickness), vigColor, vigColor, 0, 0);
        bgDraw->AddRectFilledMultiColor(ImVec2(0, dSize.y - thickness), ImVec2(dSize.x, dSize.y), 0, 0, vigColor, vigColor);
        bgDraw->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(thickness, dSize.y), vigColor, 0, 0, vigColor);
        bgDraw->AddRectFilledMultiColor(ImVec2(dSize.x - thickness, 0), ImVec2(dSize.x, dSize.y), 0, vigColor, vigColor, 0);

        // Susurros de paranoia flotantes
        static const std::array<const char*, 9> paranoiaWhispers{{
            "¿QUIÉN ESTÁ AHÍ?", "TE ESTÁN SIGUIENDO", "NO MIRES ATRÁS",
            "TODOS TE ESTÁN MIRANDO", "¡VIENEN POR TI!", "ESTÁN EN LAS PAREDES",
            "NO CONFÍES EN NADIE", "¡ESOS CARROS TE VIGILAN!", "¡CORRE!"
        }};
        m_WhisperTimer -= dt;
        if (m_WhisperTimer <= 0.0f) {
            static std::mt19937 whisperRand(777);
            std::uniform_int_distribution<size_t> pick(0, paranoiaWhispers.size() - 1);
            m_ActiveWhisper = paranoiaWhispers[pick(whisperRand)];
            m_ActiveWhisperTimer = 3.2f;
            std::uniform_real_distribution<float> randX(0.20f, 0.75f);
            std::uniform_real_distribution<float> randY(0.20f, 0.70f);
            m_WhisperPosX = randX(whisperRand);
            m_WhisperPosY = randY(whisperRand);
            m_WhisperTimer = 6.0f + std::uniform_real_distribution<float>(0.0f, 7.0f)(whisperRand);
        }
        if (m_ActiveWhisperTimer > 0.0f) {
            m_ActiveWhisperTimer -= dt;
            const float alpha = std::clamp(m_ActiveWhisperTimer / 0.7f, 0.0f, 0.92f);
            const ImVec2 whisperPos(ImGui::GetIO().DisplaySize.x * m_WhisperPosX,
                                    ImGui::GetIO().DisplaySize.y * m_WhisperPosY);
            ImGui::SetNextWindowPos(whisperPos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowBgAlpha(0.75f * alpha);
            if (ImGui::Begin("WhisperOverlay", nullptr, hudFlags | ImGuiWindowFlags_NoInputs)) {
                ImGui::TextColored(ImVec4(1.0f, 0.22f, 0.22f, alpha), "\"%s\"", m_ActiveWhisper.c_str());
            }
            ImGui::End();
        }
    }

    {
        using SignUI::Icon;
        const SignUI::Fonts& fonts = SignUI::GetFonts();
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const ImGuiWindowFlags plainFlags = hudFlags | ImGuiWindowFlags_NoBackground;

        // Botones de vista y paneles de ayuda y objetivo (arriba a la izquierda)
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));
        ImGui::SetNextWindowPos(ImVec2(12, 49), ImGuiCond_Always);
        if (ImGui::Begin("Vista y objetivo", nullptr, plainFlags)) {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const auto hudButton = [&](const char* id, const char* label, Icon icon, float width, const char* tip) {
                ImGui::PushID(id);
                const bool pressed = ImGui::InvisibleButton("##hud", ImVec2(width, 32.0f));
                ImGui::PopID();
                const bool hovered = ImGui::IsItemHovered();
                const ImVec2 min = ImGui::GetItemRectMin();
                const ImVec2 max = ImGui::GetItemRectMax();
                draw->AddRectFilled(min, max, hovered ? IM_COL32(24, 34, 52, 235) : IM_COL32(8, 12, 20, 210), 3.0f);
                draw->AddRect(min, max, hovered ? IM_COL32(150, 165, 190, 255) : IM_COL32(74, 86, 108, 255), 3.0f);
                if (label) draw->AddText(fonts.mono12, 12.0f, ImVec2(min.x + 10.0f, (min.y + max.y) * 0.5f - 7.0f), IM_COL32(208, 214, 226, 255), label);
                else SignUI::DrawIcon(draw, icon, ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f), 16.0f);
                if (hovered && tip) ImGui::SetTooltip("%s", tip);
                return pressed;
            };
            const char* cameraLabel = camera.GetMode() == CameraMode::ThirdPerson ? "Tercera persona · V" : "Isométrica · V";
            if (hudButton("camara", cameraLabel, Icon::None, 150.0f, "Alternar vista")) camera.ToggleMode();
            ImGui::SameLine();
            if (hudButton("inventario", nullptr, Icon::Bag, 38.0f, "Inventario [I]")) OpenPauseSection(1);
            ImGui::SameLine();
            if (hudButton("sonido", nullptr, audio.IsMuted() ? Icon::Mute : Icon::Sound, 38.0f, "Silenciar o activar el audio")) audio.SetMuted(!audio.IsMuted());
            ImGui::SameLine();
            if (hudButton("pausa", nullptr, Icon::Pause, 38.0f, "Pausa [Esc]")) TogglePause();

            const auto panel = [&](const char* id, auto&& content) {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 3.0f);
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0.58f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
                ImGui::BeginChild(id, ImVec2(344.0f, 0.0f), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs);
                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar(2);
                content();
                ImGui::EndChild();
            };
            panel("ayuda", [&] {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.83f, 0.89f, 1.0f));
                ImGui::TextWrapped("Mueve el ratón (o arrastra) para girar la cámara · Rueda: zoom");
                ImGui::PopStyleColor();
            });
            panel("objetivo", [&] {
                ImGui::TextWrapped("%s", objective.c_str());
                if (simulation.IsMissionComplete()) ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.48f, 1.0f), "Misión completada");
                else if (simulation.IsMissionStarted()) ImGui::TextColored(ImVec4(0.96f, 0.77f, 0.0f, 1.0f), "Reciclables: %llu / %llu",
                    static_cast<unsigned long long>(missionProgress), static_cast<unsigned long long>(missionTarget));
                else ImGui::TextDisabled("Acércate al marcador para iniciar la misión");
            });

            // Enfermedades activas: una línea roja por cada una
            if (diseaseSummary != "Sin enfermedades" && !diseaseSummary.empty()) {
                std::istringstream diseases(diseaseSummary);
                std::string part;
                while (std::getline(diseases, part, '|')) {
                    const size_t first = part.find_first_not_of(" \t");
                    const size_t last = part.find_last_not_of(" \t");
                    if (first == std::string::npos) continue;
                    part = part.substr(first, last - first + 1);
                    std::string name = part, remaining;
                    const size_t paren = part.find(" (");
                    if (paren != std::string::npos) {
                        name = part.substr(0, paren);
                        remaining = part.substr(paren + 2, part.size() - paren - 3);
                    }
                    const ImVec2 min = ImGui::GetCursorScreenPos();
                    const ImVec2 max(min.x + 220.0f, min.y + 21.0f);
                    draw->AddRectFilled(min, max, IM_COL32(192, 57, 43, 52));
                    draw->AddRect(min, max, IM_COL32(192, 57, 43, 255));
                    draw->AddText(fonts.mono12, 12.0f, ImVec2(min.x + 8.0f, min.y + 3.0f), IM_COL32(231, 76, 60, 255), name.c_str());
                    const float width = fonts.mono12->CalcTextSizeA(12.0f, FLT_MAX, 0.0f, remaining.c_str()).x;
                    draw->AddText(fonts.mono12, 12.0f, ImVec2(max.x - 8.0f - width, min.y + 3.0f), IM_COL32(136, 136, 136, 255), remaining.c_str());
                    ImGui::Dummy(ImVec2(220.0f, 21.0f));
                }
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);

        if (!m_Paused && Input::IsKeyPressed(GLFW_KEY_N)) m_RadarVisible = !m_RadarVisible;
        const float panelWidth = 175.0f;
        const float panelLeft = display.x - 16.0f - panelWidth;

        // Radar circular (arriba a la derecha)
        if (m_RadarVisible) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowPos(ImVec2(panelLeft, 18.0f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(panelWidth, 182.0f), ImGuiCond_Always);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.03f, 0.06f, 0.09f, 0.86f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.33f, 0.47f, 0.42f, 0.75f));
            if (ImGui::Begin("Radar Bogota [N]", nullptr, (hudFlags & ~ImGuiWindowFlags_AlwaysAutoResize) | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs)) {
                ImDrawList* draw = ImGui::GetWindowDrawList();
                const ImVec2 origin = ImGui::GetWindowPos();
                SignUI::DrawIcon(draw, Icon::Compass, ImVec2(origin.x + 16.0f, origin.y + 12.0f), 13.0f);
                draw->AddText(fonts.monoBold12, 11.0f, ImVec2(origin.x + 28.0f, origin.y + 5.0f), IM_COL32(238, 238, 238, 255), "Radar Bogotá");
                draw->AddRect(ImVec2(origin.x + panelWidth - 30.0f, origin.y + 4.0f), ImVec2(origin.x + panelWidth - 10.0f, origin.y + 19.0f), IM_COL32(220, 225, 235, 180), 2.0f);
                draw->AddText(fonts.monoBold12, 11.0f, ImVec2(origin.x + panelWidth - 24.0f, origin.y + 5.5f), IM_COL32(238, 238, 238, 255), "N");

                const float diameter = 130.0f;
                const float radius = diameter * 0.5f;
                const ImVec2 center(origin.x + panelWidth * 0.5f, origin.y + 28.0f + radius);
                const float range = 42.0f;
                constexpr int samples = 26;
                const float cell = diameter / samples;
                draw->AddCircleFilled(center, radius, IM_COL32(26, 29, 32, 245), 48);
                for (int z = 0; z < samples; ++z) for (int x = 0; x < samples; ++x) {
                    const float localX = (x + 0.5f) / samples * 2.0f - 1.0f;
                    const float localZ = (z + 0.5f) / samples * 2.0f - 1.0f;
                    if (localX * localX + localZ * localZ > 0.94f) continue;
                    const float worldX = playerPos.x + localX * range;
                    const float worldZ = playerPos.z + localZ * range;
                    const int tileX = static_cast<int>(std::floor((worldX + CityLayout::WIDTH) / CityLayout::TILE_SIZE));
                    const int tileZ = static_cast<int>(std::floor((worldZ + CityLayout::HEIGHT) / CityLayout::TILE_SIZE));
                    const TileType tile = city.GetTileAt(tileX, tileZ);
                    ImU32 color = IM_COL32(48, 50, 53, 255);
                    if (tile == TileType::Road) color = IM_COL32(104, 106, 108, 255);
                    else if (tile == TileType::Busway) color = IM_COL32(133, 87, 60, 255);
                    else if (tile == TileType::Sidewalk || tile == TileType::Plaza) color = IM_COL32(115, 112, 105, 255);
                    else if (tile == TileType::Grass) color = IM_COL32(62, 92, 58, 255);
                    const ImVec2 cell0(center.x - radius + x * cell, center.y - radius + z * cell);
                    draw->AddRectFilled(cell0, ImVec2(cell0.x + cell + 0.5f, cell0.y + cell + 0.5f), color);
                }
                draw->AddCircle(center, radius, IM_COL32(120, 140, 170, 255), 48, 1.5f);
                const auto marker = [&](const glm::vec3& position, float size, ImU32 color) {
                    const float dx = (position.x - playerPos.x) / range;
                    const float dz = (position.z - playerPos.z) / range;
                    if (dx * dx + dz * dz <= 0.94f) draw->AddCircleFilled(ImVec2(center.x + dx * radius, center.y + dz * radius), size, color);
                };
                for (const auto& pickup : simulation.GetPickups()) marker(pickup.position, 2.0f, IM_COL32(248, 204, 78, 255));
                if (!simulation.IsMissionComplete()) marker(simulation.GetMissionNpcPosition(), 3.0f, IM_COL32(237, 84, 72, 255));
                marker(simulation.GetDogPosition(), 2.5f, IM_COL32(117, 184, 255, 255));
                draw->AddCircleFilled(center, 4.0f, IM_COL32(49, 220, 115, 255));
                draw->AddCircle(center, 7.0f, IM_COL32(12, 38, 24, 255));
                const ImU32 compassColor = IM_COL32(210, 216, 228, 200);
                draw->AddText(fonts.mono11, 10.0f, ImVec2(center.x - 3.0f, center.y - radius + 3.0f), compassColor, "N");
                draw->AddText(fonts.mono11, 10.0f, ImVec2(center.x + radius - 11.0f, center.y - 5.0f), compassColor, "E");
                draw->AddText(fonts.mono11, 10.0f, ImVec2(center.x - 3.0f, center.y + radius - 13.0f), compassColor, "S");
                draw->AddText(fonts.mono11, 10.0f, ImVec2(center.x - radius + 4.0f, center.y - 5.0f), compassColor, "O");
                char info[64];
                std::snprintf(info, sizeof(info), "El Chato: %.0f m", glm::distance(playerPos, simulation.GetDogPosition()));
                draw->AddText(fonts.mono11, 10.0f, ImVec2(origin.x + 10.0f, origin.y + 28.0f + diameter + 8.0f), IM_COL32(238, 214, 136, 255), info);
            }
            ImGui::End();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
        }

        // Costal: lista de objetos y peso
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
        ImGui::SetNextWindowPos(ImVec2(display.x - 16.0f, 208.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.60f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Begin("Inventario", nullptr, hudFlags | ImGuiWindowFlags_NoInputs)) {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const float width = panelWidth - 16.0f;
            const size_t maxRows = static_cast<size_t>(std::max(3.0f, (display.y - 208.0f - 80.0f) / 19.0f));
            if (inventoryOptions.empty()) {
                ImGui::TextDisabled("Costal vacío");
                ImGui::Dummy(ImVec2(width, 1.0f));
            }
            size_t shown = 0;
            for (const auto& item : inventoryOptions) {
                if (shown++ >= maxRows) break;
                const ImVec2 at = ImGui::GetCursorScreenPos();
                draw->AddText(fonts.mono12, 12.0f, ImVec2(at.x, at.y + 1.0f), IM_COL32(204, 204, 204, 255), item.name.c_str());
                char amount[24];
                std::snprintf(amount, sizeof(amount), "x%d", item.quantity);
                float right = at.x + width;
                if (item.usable) {
                    const float tagWidth = fonts.mono11->CalcTextSizeA(10.0f, FLT_MAX, 0.0f, "[usar]").x;
                    draw->AddText(fonts.mono11, 10.0f, ImVec2(right - tagWidth, at.y + 2.0f), IM_COL32(136, 136, 136, 255), "[usar]");
                    right -= tagWidth + 8.0f;
                }
                const float amountWidth = fonts.mono12->CalcTextSizeA(12.0f, FLT_MAX, 0.0f, amount).x;
                draw->AddText(fonts.mono12, 12.0f, ImVec2(right - amountWidth, at.y + 1.0f), IM_COL32(201, 162, 39, 255), amount);
                draw->AddLine(ImVec2(at.x, at.y + 17.0f), ImVec2(at.x + width, at.y + 17.0f), IM_COL32(34, 34, 34, 255));
                ImGui::Dummy(ImVec2(width, 19.0f));
            }
            if (inventoryOptions.size() > maxRows) ImGui::TextDisabled("+%zu más…", inventoryOptions.size() - maxRows);
            char weight[48];
            std::snprintf(weight, sizeof(weight), "Peso: %.1f/%.0f", inventoryWeight, inventoryCapacity);
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const float weightWidth = fonts.mono11->CalcTextSizeA(11.0f, FLT_MAX, 0.0f, weight).x;
            draw->AddText(fonts.mono11, 11.0f, ImVec2(at.x + width - weightWidth, at.y + 4.0f), IM_COL32(136, 136, 136, 255), weight);
            ImGui::Dummy(ImVec2(width, 20.0f));
        }
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
    }

    std::string interactionPrompt;
    const bool nearMissionNpc = simulation.IsNearMissionNpc(playerPos, 2.0f);
    const size_t nearbyCar = simulation.GetNearbyCar(playerPos, 2.0f);
    const size_t nearbyParkedCar = simulation.GetNearbyParkedCar(playerPos, 2.0f);
    const bool nearDealer = simulation.IsNearDealer(playerPos, 2.2f);
    const bool nearRecycler = simulation.IsNearRecycler(playerPos, 2.5f);
    const size_t nearbyVendor = simulation.GetNearbyVendor(playerPos, 2.0f);
    const size_t nearbyPedestrian = simulation.GetNearbyPedestrian(playerPos, 1.8f);
    const bool nearPickup = std::any_of(simulation.GetPickups().begin(), simulation.GetPickups().end(), [&](const PickupItem& pickup) {
        return pickup.available && !pickup.container && glm::distance(playerPos, pickup.position) < 1.8f;
    });
    const bool nearContainer = std::any_of(simulation.GetPickups().begin(), simulation.GetPickups().end(), [&](const PickupItem& pickup) {
        return pickup.available && pickup.container && glm::distance(playerPos, pickup.position) < 1.8f;
    });
    if (nearMissionNpc && !simulation.IsMissionStarted() && !simulation.IsMissionComplete()) interactionPrompt = "[E] Hablar con el vecino · iniciar mision";
    else if (simulation.IsNearDog(playerPos, 2.0f)) interactionPrompt = simulation.IsDogFriend()
        ? "[E] Acariciar a El Chato" : "[E] Acercarte a El Chato y hacer amistad";
    else if (nearDealer) interactionPrompt = simulation.IsNight()
        ? "[E] Vender sustancias (si llevas) · [Q] Comprar"
        : "[E] / [Q] Comprar sustancias al dealer";
    else if (nearRecycler) interactionPrompt = "[E] Vender reciclables · [F] Entregar objetos de la mision";
    else if (nearbyVendor < simulation.GetStreetVendorCount()) interactionPrompt = "[E] Comprar en " + simulation.GetVendor(nearbyVendor).name;
    else if (nearbyPedestrian < simulation.GetPedestrianCount()) interactionPrompt = "[E] Intentar cosquilleo · [Q] Robo directo";
    else if (nearbyParkedCar < simulation.GetParkedCarCount()) interactionPrompt = "[E] Robar retrovisores del auto estacionado";
    else if (nearbyCar < simulation.GetTrafficCount()) interactionPrompt = "[E] Intentar robar retrovisores del auto";
    else if (simulation.IsNearShop(playerPos, 2.2f)) interactionPrompt = "[E] Tienda · comprar un articulo";
    else if (nearContainer) interactionPrompt = "[E] Revisar caneca · mantén [Q] para clasificar (más botín)";
    else if (nearPickup) interactionPrompt = "[E] Recoger bolsa · mantén [Q] para clasificar (más botín)";
    else if (inventorySummary.find("Pan duro") != std::string::npos || inventorySummary.find("Botella agua") != std::string::npos || inventorySummary.find("Vendaje") != std::string::npos || inventorySummary.find("Curita") != std::string::npos || inventorySummary.find("Tinto") != std::string::npos || inventorySummary.find("Comida enlatada") != std::string::npos || inventorySummary.find("Refresco") != std::string::npos || inventorySummary.find("Tintico") != std::string::npos || inventorySummary.find("Pegante") != std::string::npos || inventorySummary.find("Pastilla") != std::string::npos || inventorySummary.find("Cigarrillo") != std::string::npos || inventorySummary.find("Jabón") != std::string::npos || inventorySummary.find("Cepillo") != std::string::npos || inventorySummary.find("Vacuna") != std::string::npos || inventorySummary.find("Carbón activo") != std::string::npos || inventorySummary.find("Jarabe") != std::string::npos || inventorySummary.find("Pomada") != std::string::npos || inventorySummary.find("Antiparasitario") != std::string::npos || inventorySummary.find("Antibiótico") != std::string::npos) interactionPrompt = "[E] Usar el primer consumible o medicamento del costal";
    if (m_SortProgress >= 0.0f && !m_Paused) {
        const bool complete = m_SortProgress >= 1.0f;
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y - 112.0f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.80f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 10.0f));
        if (ImGui::Begin("Clasificando", nullptr, hudFlags | ImGuiWindowFlags_NoInputs)) {
            ImGui::PushFont(SignUI::GetFonts().mono14);
            ImGui::TextColored(complete ? ImVec4(1.0f, 0.82f, 0.40f, 1.0f) : ImVec4(0.86f, 0.90f, 0.96f, 1.0f), "%s",
                               complete ? "¡Lista! Suelta Q para recoger" : "Clasificando basura… mantén Q");
            ImGui::PopFont();
            SignUI::StatBar(m_SortProgress, complete ? IM_COL32(255, 209, 102, 255) : IM_COL32(39, 174, 96, 255), 260.0f, 10.0f);
        }
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
    }

    if (!interactionPrompt.empty()) {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y - 62.0f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.80f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.79f, 0.64f, 0.15f, 0.85f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 8.0f));
        ImGui::PushFont(SignUI::GetFonts().mono14);
        if (ImGui::Begin("Interaccion", nullptr, hudFlags | ImGuiWindowFlags_NoInputs)) {
            ImGui::TextUnformatted(interactionPrompt.c_str());
        }
        ImGui::End();
        ImGui::PopFont();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
    }

    // Recordatorio de teclas (tenue, al pie de la pantalla)
    {
        const SignUI::Fonts& fonts = SignUI::GetFonts();
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const char* lines[2] = {
            "WASD mover · Shift correr · V cámara · N radar · R radio · E interactuar · Q recoger/dealer · B limosna · X lanzar · Z popó",
            "I inventario · T talentos · O El Chato · C crafteo · P personaje · F11 pantalla completa · ESC pausa"
        };
        for (int line = 0; line < 2; ++line) {
            const float width = fonts.mono11->CalcTextSizeA(11.0f, FLT_MAX, 0.0f, lines[line]).x;
            ImGui::GetBackgroundDrawList()->AddText(fonts.mono11, 11.0f,
                ImVec2((display.x - width) * 0.5f, display.y - 36.0f + static_cast<float>(line) * 14.0f),
                IM_COL32(255, 255, 255, 80), lines[line]);
        }
    }

    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 330, 16), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(314, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Ajustes de Render & Clima")) {
        // Controles de ventana
        if (ImGui::Button(window.IsFullscreen() ? "Modo Ventana [F11]" : "Pantalla Completa [F11]")) {
            window.ToggleFullscreen();
        }
        ImGui::SameLine();
        if (ImGui::Button("Maximizar")) {
            window.Maximize();
        }

        ImGui::Separator();
        auto& lighting = renderer.GetLighting();
        ImGui::SliderFloat("Densidad Niebla", &lighting.fogDensity, 0.001f, 0.06f, "%.3f");
        ImGui::ColorEdit3("Color Niebla", &lighting.fogColor.r);
        ImGui::ColorEdit3("Luz Ambiente", &lighting.ambientColor.r);
        ImGui::ColorEdit3("Color Sol", &lighting.sunColor.r);

        static bool wireframe = false;
        if (ImGui::Checkbox("Modo Wireframe", &wireframe)) {
            renderer.SetWireframe(wireframe);
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.20f, 1.0f), "Controles:");
        ImGui::BulletText("Mouse: Giro 360 continuo (confinado)");
        ImGui::BulletText("Tab / Esc: Mostrar/Ocultar cursor");
        ImGui::BulletText("F11: Pantalla completa");
        ImGui::BulletText("Flechas: Control de vista");
        ImGui::BulletText("W, A, S, D: Moverse");
        ImGui::BulletText("Shift: Correr");
        ImGui::BulletText("V: Tercera Persona / Isometrica");
        ImGui::BulletText("N: Mostrar / ocultar radar");
        ImGui::BulletText("Z: Hacer popo · X: Lanzar mojón si tienes uno");
        ImGui::BulletText("E / Q cerca de peatones: Cosquilleo / robo directo");
    }
    ImGui::End();
}

void UIManager::RenderCambucheMenu(const std::vector<CambuchePieceOption>& pieces, bool hasBase, bool isNight) {
    if (!m_CambucheMenuOpen) return;

    // Liberar el ratón mientras el menú está abierto
    Input::SetCursorLocked(false);

    using SignUI::Icon;
    const SignUI::Fonts& fonts = SignUI::GetFonts();
    const ImVec4 blueText(0.84f, 0.89f, 1.0f, 1.0f);
    SignUI::DrawBackdrop(0.55f);
    bool closeRequested = false;
    if (SignUI::BeginMenu("Cambuche", "Cambuche · Refugio urbano", "Construye tu refugio paso a paso", Icon::Home, &closeRequested)) {
        ImGui::PushStyleColor(ImGuiCol_Text, blueText);
        ImGui::TextWrapped("Acércate a los materiales y recógelos antes de construir.");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        int rowIndex = 0;
        for (const auto& piece : pieces) {
            ImGui::PushID(piece.id.c_str());
            if (SignUI::BeginRow("pieza", rowIndex++)) {
                const float statusWidth = piece.canBuild && !piece.isBuilt && !piece.isLocked ? SignUI::RowButtonWidth("Construir") : 128.0f;
                const ImVec2 at = ImGui::GetCursorScreenPos();
                const float rowTop = ImGui::GetCursorPosY();
                ImDrawList* draw = ImGui::GetWindowDrawList();
                if (piece.isBuilt) {
                    draw->AddCircleFilled(ImVec2(at.x + 10.0f, at.y + 15.0f), 9.0f, IM_COL32(25, 196, 99, 255), 16);
                    SignUI::DrawIcon(draw, Icon::Check, ImVec2(at.x + 10.0f, at.y + 15.0f), 14.0f);
                } else {
                    SignUI::DrawIcon(draw, Icon::Home, ImVec2(at.x + 10.0f, at.y + 15.0f), 18.0f, piece.isLocked ? 0.35f : 1.0f);
                }
                ImGui::SetCursorScreenPos(ImVec2(at.x + 30.0f, at.y + 7.0f));
                ImGui::PushFont(fonts.monoBold14);
                ImGui::TextColored(piece.isLocked ? ImVec4(0.6f, 0.65f, 0.72f, 1.0f) : ImVec4(1, 1, 1, 1), "%s", piece.name.c_str());
                ImGui::PopFont();
                SignUI::RowCluster(rowTop, ImGui::GetWindowWidth() - 10.0f - statusWidth);
                if (piece.isBuilt) {
                    ImGui::SetCursorPosY(rowTop + 7.0f);
                    ImGui::TextColored(ImVec4(0.62f, 0.94f, 0.75f, 1.0f), "Construido");
                } else if (piece.isLocked) {
                    ImGui::SetCursorPosY(rowTop + 7.0f);
                    ImGui::TextDisabled("Bloqueado");
                } else if (piece.canBuild) {
                    if (SignUI::RowButton("Construir", true, true)) m_CambucheBuildRequest = piece.id;
                } else {
                    ImGui::SetCursorPosY(rowTop + 7.0f);
                    ImGui::TextColored(ImVec4(1.0f, 0.71f, 0.67f, 1.0f), "Faltan materiales");
                }
                SignUI::RowBelow(rowTop);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, blueText);
                ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 18.0f - statusWidth);
                ImGui::TextWrapped("%s", piece.desc.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
                if (!piece.reqText.empty()) {
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 30.0f);
                    ImGui::TextColored(piece.canBuild ? ImVec4(0.62f, 0.94f, 0.75f, 1.0f) : ImVec4(1.0f, 0.71f, 0.67f, 1.0f),
                                       "Requiere: %s", piece.reqText.c_str());
                }
            }
            SignUI::EndRow();
            ImGui::PopID();
        }

        ImGui::Spacing();
        if (hasBase) {
            if (!isNight) {
                ImGui::TextDisabled("Puedes dormir aquí de noche para recuperar energía.");
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, blueText);
                ImGui::TextWrapped("Es de noche. Puedes descansar en el cambuche.");
                ImGui::PopStyleColor();
                SignUI::FlexReset();
                if (SignUI::Button("Dormir (recupera energía y salud)", Icon::Heart)) {
                    m_SleepRequest = true;
                    m_CambucheMenuOpen = false;
                    Input::SetCursorLocked(true);
                }
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.45f, 1.0f), "Construye la base para poder descansar aquí.");
        }

        SignUI::BeginFooter();
        if (SignUI::PrimaryButton("Cerrar (Esc)")) closeRequested = true;
    }
    SignUI::EndMenu();
    if (closeRequested) {
        m_CambucheMenuOpen = false;
        Input::SetCursorLocked(true);
    }
}

} // namespace Bazuko
