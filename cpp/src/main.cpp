#include "core/Window.hpp"
#include "core/Input.hpp"
#include "core/Clock.hpp"
#include "core/Log.hpp"
#include "renderer/Renderer.hpp"
#include "renderer/Camera.hpp"
#include "renderer/Mesh.hpp"
#include "renderer/Model.hpp"
#include "ui/UIManager.hpp"
#include "audio/AudioManager.hpp"
#include "world/CityLayout.hpp"
#include "world/Simulation.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <filesystem>
#include <array>
#include <algorithm>
#include <random>
#include <fstream>
#include <nlohmann/json.hpp>

#if defined(_WIN32)
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace fs = std::filesystem;

static std::string FindAssetPath(const std::string& relPath) {
    std::vector<std::string> searchPaths = {
        relPath,
        "../" + relPath,
        "../../" + relPath,
        "../../../" + relPath,
        "../../../../" + relPath,
        "public/" + relPath
    };
    for (const auto& p : searchPaths) {
        if (fs::exists(p)) return p;
    }
    return relPath;
}

int main(int /*argc*/, char** /*argv*/) {
    using namespace Bazuko;

    LogInfo("Iniciando Bazuko - Motor C++ / OpenGL 4.5 Core DSA...");

    Window window({"Bazuko - Bogota 3D (C++ OpenGL 4.5 DSA)", 1280, 720, false, true});
    Input::Init(window.GetNativeWindow());

    Renderer renderer;
    AudioManager audio;
    UIManager ui(window.GetNativeWindow());
    Camera camera(55.0f, window.GetAspectRatio());

    window.SetResizeCallback([&](int width, int height) {
        if (height > 0) {
            camera.SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));
            renderer.SetViewport(0, 0, width, height);
        }
    });

    CityLayout city;
    Simulation simulation(city);
    glm::vec3 playerPos = city.GetSpawnPoint();
    playerPos.y = 0.0f;
    float playerYaw = 0.0f;
    struct Item { std::string id; std::string name; float weight; int value; int consumableNeed = -1; float amount = 0.0f; };
    const std::array<Item, 27> lootTable{{
        {"lata", "Lata vacía", 0.2f, 250}, {"botella", "Botella PET", 0.3f, 150},
        {"carton", "Cartón", 0.5f, 200}, {"chatarra", "Chatarra", 1.0f, 900},
        {"cable", "Cable cobre", 0.4f, 2400}, {"pan", "Pan duro", 0.3f, 1000, 0, 25.0f},
        {"agua", "Botella agua", 0.5f, 1500, 1, 35.0f}, {"reloj", "Reloj oxidado", 0.1f, 8000},
        {"vendaje", "Vendaje", 0.1f, 3000, 3, 30.0f}, {"antibiotico", "Antibiótico", 0.1f, 18000},
        {"madera", "Tablas", 2.0f, 1500}, {"pilas", "Pilas AA", 0.1f, 2000}, {"radioTransistor", "Radio transistor", 0.6f, 18000},
        {"retrovisor", "Retrovisor", 0.8f, 15000}, {"radio", "Radio de auto", 1.5f, 35000},
        {"tela", "Retazo de tela", 0.2f, 300}, {"pastilla", "Pastilla", 0.05f, 8000},
        {"lona", "Lona azul", 2.0f, 4000}, {"colchon", "Colchón viejo", 6.0f, 5000}, {"vela", "Vela", 0.05f, 500},
        {"popo", "Mojón de popó", 0.3f, 50}, {"alambre", "Alambre", 0.1f, 500},
        {"plastico", "Plástico", 0.1f, 200}, {"mochila", "Mochila", 0.8f, 12000},
        {"arma", "Arma casera", 1.2f, 0}, {"curita", "Curita", 0.05f, 500},
        {"rifa", "Cartón de rifa", 0.05f, 1000}
    }};
    const std::array<Item, 6> shopCatalog{{
        {"pan", "Pan duro", 0.3f, 1000, 0, 25.0f}, {"agua", "Botella agua", 0.5f, 1500, 1, 35.0f},
        {"vendaje", "Vendaje", 0.1f, 3000, 3, 30.0f}, {"madera", "Tablas", 2.0f, 1500},
        {"tela", "Retazo de tela", 0.2f, 300}, {"pilas", "Pilas AA", 0.1f, 2000}
    }};
    struct InventoryEntry { size_t itemIndex; int quantity; };
    std::vector<InventoryEntry> inventory;
    float inventoryWeight = 0.0f;
    float money = 0.0f;
    int xp = 0;
    int level = 1;
    int itemsCollected = 0;
    int itemsSold = 0;
    size_t missionProgress = 0;
    std::array<float, 7> needs{100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 80.0f};
    std::string objective = "Recoge 5 objetos para empezar";
    struct DiseaseState { std::string id; float remaining; float healthDrain; float mentalDrain; };
    std::vector<DiseaseState> diseases;
    float infectionCooldown = 0.0f;
    const fs::path savePath = "savegame.json";
    {
        std::ifstream save(savePath);
        if (save) {
            try {
                nlohmann::json state;
                save >> state;
                auto savedNeeds = state.value("needs", std::vector<float>{});
                for (size_t i = 0; i < std::min(needs.size(), savedNeeds.size()); ++i) needs[i] = std::clamp(savedNeeds[i], 0.0f, 100.0f);
                money = state.value("money", 0.0f);
                xp = state.value("xp", 0);
                level = std::max(1, state.value("level", 1));
                itemsCollected = state.value("itemsCollected", 0);
                itemsSold = state.value("itemsSold", 0);
                missionProgress = state.value("missionProgress", size_t{0});
                simulation.SetMissionProgress(missionProgress);
                if (state.value("missionStarted", false)) simulation.StartMission();
                if (state.value("missionComplete", false)) simulation.CompleteMission();
                objective = state.value("objective", objective);
                for (const auto& savedDisease : state.value("diseases", nlohmann::json::array())) {
                    DiseaseState disease{savedDisease.value("id", std::string{}), savedDisease.value("remaining", 0.0f), savedDisease.value("healthDrain", 0.0f), savedDisease.value("mentalDrain", 0.0f)};
                    if (disease.remaining > 0.0f && disease.healthDrain > 0.0f) diseases.push_back(std::move(disease));
                }
                if (state.contains("dayProgress")) simulation.SetDayProgress(state.value("dayProgress", 0.30f));
                playerPos.x = state.value("playerX", playerPos.x);
                playerPos.z = state.value("playerZ", playerPos.z);
                for (const auto& entry : state.value("inventory", nlohmann::json::array())) {
                    size_t itemIndex = entry.value("item", lootTable.size());
                    int quantity = entry.value("quantity", 0);
                    if (itemIndex < lootTable.size() && quantity > 0 && inventoryWeight + lootTable[itemIndex].weight * quantity <= 20.0f) {
                        inventory.push_back({itemIndex, quantity});
                        inventoryWeight += lootTable[itemIndex].weight * quantity;
                    }
                }
            } catch (const std::exception& error) { LogWarn("Partida guardada inválida: {}", error.what()); }
        }
    }
    std::mt19937 lootRandom(std::random_device{}());
    float survivalSaveTimer = 0.0f;
    auto collectNearbyPickup = [&](float radius) {
        size_t nearest = simulation.GetPickups().size();
        float nearestDistance = radius;
        for (size_t index = 0; index < simulation.GetPickups().size(); ++index) {
            const float distance = glm::distance(playerPos, simulation.GetPickups()[index].position);
            if (distance < nearestDistance) { nearest = index; nearestDistance = distance; }
        }
        PickupItem pickup;
        if (nearest >= simulation.GetPickups().size() || !simulation.CollectPickup(nearest, pickup)) return false;
        const Item& item = lootTable[pickup.itemIndex];
        if (inventoryWeight + item.weight > 20.0f) {
            simulation.RestorePickup(pickup);
            audio.Play(AudioCue::Full);
            return false;
        }
        auto entry = std::find_if(inventory.begin(), inventory.end(), [&](const InventoryEntry& value) { return value.itemIndex == pickup.itemIndex; });
        if (entry == inventory.end()) inventory.push_back({pickup.itemIndex, 1}); else ++entry->quantity;
        inventoryWeight += item.weight;
        ++itemsCollected;
        xp += 3;
        while (xp >= 100 * level) { xp -= 100 * level; ++level; }
        audio.Play(AudioCue::Pickup);
        return true;
    };

    // Cargar modelo 3D realista de personaje MakeHuman/Mixamo
    std::string cuerpoPath = FindAssetPath("public/models/personaje/hombre-joven/cuerpo.gltf");
    std::string ropaPath = FindAssetPath("public/models/personaje/hombre-joven/chaqueta.gltf");
    std::string calzadoPath = FindAssetPath("public/models/personaje/hombre-joven/tenis-rotos.gltf");
    std::string peloPath = FindAssetPath("public/models/personaje/hombre-joven/corto.gltf");

    std::shared_ptr<Model> charBody = Model::LoadFromFile(cuerpoPath);
    std::shared_ptr<Model> charJacket = Model::LoadFromFile(ropaPath);
    std::shared_ptr<Model> charShoes = Model::LoadFromFile(calzadoPath);
    std::shared_ptr<Model> charHair = Model::LoadFromFile(peloPath);
    const std::string animationPath = FindAssetPath("public/models/animaciones-prueba.glb");
    const std::shared_ptr<Model> animationLibrary = Model::LoadFromFile(animationPath);
    if (animationLibrary) {
        for (const auto& characterPart : {charBody, charJacket, charShoes, charHair}) {
            if (characterPart) characterPart->SetAnimationSource(*animationLibrary);
        }
    }
    const int walkClip = charBody ? charBody->FindAnimationClip("Walk") : -1;
    const int runClip = charBody ? charBody->FindAnimationClip("Run") : -1;
    const int idleClip = charBody ? charBody->FindAnimationClip("Idle") : -1;

    auto fallbackBody = Mesh::CreateCube(glm::vec3(0.6f, 1.2f, 0.4f), glm::vec4(0.20f, 0.45f, 0.65f, 1.0f));
    auto fallbackHead = Mesh::CreateCube(glm::vec3(0.4f, 0.4f, 0.4f), glm::vec4(0.85f, 0.70f, 0.55f, 1.0f));

    Clock clock;
    LogInfo("Motor iniciado con exito. Entrando al bucle principal.");

    while (!window.ShouldClose()) {
        // 1. Snapshot de estado previo
        Input::BeginFrame();

        // 2. Procesar eventos del sistema
        window.PollEvents();

        // 3. Post-poll para asegurar deltas de mouse
        Input::PostPoll();

        float dt = clock.Tick();
        if (dt > 0.1f) dt = 0.1f;

        // 4. Teclas de acción
        if (Input::IsKeyPressed(GLFW_KEY_ESCAPE)) ui.HandleEscape();
        if (ui.IsPaused()) dt = 0.0f;
        if (Input::IsKeyPressed(GLFW_KEY_C)) ui.ToggleCraftMenu();

        if (!ui.IsPaused() && Input::IsKeyPressed(GLFW_KEY_TAB)) {
            Input::ToggleCursorLock();
        }

        if (!ui.IsPaused() && Input::IsKeyPressed(GLFW_KEY_V)) {
            camera.ToggleMode();
        }

        if (Input::IsKeyPressed(GLFW_KEY_F11)) {
            window.ToggleFullscreen();
        }

        // 5. Control de cursor y cámara
        ImGuiIO& io = ImGui::GetIO();

        // Si el usuario hace click en la pantalla cuando el cursor está libre, volver a bloquearlo
        if (!ui.IsPaused() && !Input::IsCursorLocked() && !io.WantCaptureMouse) {
            if (Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) || Input::IsMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
                Input::SetCursorLocked(true);
            }
        }

        // Si el cursor está bloqueado (oculto) O si el usuario arrastra con click:
        bool shouldRotate = !ui.IsPaused() && (Input::IsCursorLocked() ||
                           ((Input::IsMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT) || Input::IsMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT)) && !io.WantCaptureMouse));

        if (shouldRotate) {
            glm::vec2 delta = Input::GetMouseDelta();
            if (std::abs(delta.x) > 0.0001f || std::abs(delta.y) > 0.0001f) {
                camera.Orbit(delta.x, delta.y);
            }
        }

        // Control de vista con flechas del teclado
        float arrowX = ui.IsPaused() ? 0.0f : (Input::IsKeyDown(GLFW_KEY_RIGHT) ? 1.0f : 0.0f) - (Input::IsKeyDown(GLFW_KEY_LEFT) ? 1.0f : 0.0f);
        float arrowY = ui.IsPaused() ? 0.0f : (Input::IsKeyDown(GLFW_KEY_DOWN) ? 1.0f : 0.0f) - (Input::IsKeyDown(GLFW_KEY_UP) ? 1.0f : 0.0f);
        if (std::abs(arrowX) > 0.01f || std::abs(arrowY) > 0.01f) {
            camera.Orbit(arrowX * dt * 250.0f, arrowY * dt * 180.0f);
        }

        // Zoom de cámara con la rueda
        float scroll = Input::GetScrollDelta();
        if (!ui.IsPaused() && std::abs(scroll) > 0.01f && !io.WantCaptureMouse) {
            camera.Zoom(-scroll * 0.5f);
        }

        // 6. Movimiento del personaje (WASD)
        bool isRunning = !ui.IsPaused() && (Input::IsKeyDown(GLFW_KEY_LEFT_SHIFT) || Input::IsKeyDown(GLFW_KEY_RIGHT_SHIFT));
        float moveSpeed = (isRunning ? 6.0f : 3.0f) * (needs[5] < 20.0f ? 0.75f : 1.0f);

        glm::vec3 moveDir(0.0f);
        if (!ui.IsPaused() && camera.GetMode() == CameraMode::ThirdPerson) {
            float camYaw = camera.GetYaw();
            glm::vec3 forward = glm::normalize(glm::vec3(std::sin(camYaw), 0.0f, std::cos(camYaw)));
            glm::vec3 right = glm::normalize(glm::vec3(std::cos(camYaw), 0.0f, -std::sin(camYaw)));

            if (Input::IsKeyDown(GLFW_KEY_W)) moveDir -= forward;
            if (Input::IsKeyDown(GLFW_KEY_S)) moveDir += forward;
            if (Input::IsKeyDown(GLFW_KEY_A)) moveDir -= right;
            if (Input::IsKeyDown(GLFW_KEY_D)) moveDir += right;
        } else if (!ui.IsPaused()) {
            if (Input::IsKeyDown(GLFW_KEY_W)) moveDir += glm::vec3(-1.0f, 0.0f, -1.0f);
            if (Input::IsKeyDown(GLFW_KEY_S)) moveDir += glm::vec3( 1.0f, 0.0f,  1.0f);
            if (Input::IsKeyDown(GLFW_KEY_A)) moveDir += glm::vec3(-1.0f, 0.0f,  1.0f);
            if (Input::IsKeyDown(GLFW_KEY_D)) moveDir += glm::vec3( 1.0f, 0.0f, -1.0f);
        }

        float currentSpeed = 0.0f;
        if (glm::length(moveDir) > 0.001f) {
            moveDir = glm::normalize(moveDir);
            currentSpeed = moveSpeed;

            glm::vec3 nextPos = playerPos + moveDir * (moveSpeed * dt);
            if (city.IsWalkable(nextPos.x, nextPos.z) && !simulation.IsBlocked(nextPos, 0.35f)) {
                playerPos = nextPos;
            } else if (city.IsWalkable(nextPos.x, playerPos.z) && !simulation.IsBlocked({nextPos.x, 0.0f, playerPos.z}, 0.35f)) {
                playerPos.x = nextPos.x;
            } else if (city.IsWalkable(playerPos.x, nextPos.z) && !simulation.IsBlocked({playerPos.x, 0.0f, nextPos.z}, 0.35f)) {
                playerPos.z = nextPos.z;
            }

            playerYaw = std::atan2(moveDir.x, moveDir.z);
        }
        audio.Update(currentSpeed > 0.25f, isRunning);

        needs[0] = std::max(0.0f, needs[0] - (100.0f / 7200.0f) * dt);
        needs[1] = std::max(0.0f, needs[1] - (100.0f / 5400.0f) * dt);
        needs[2] = std::max(0.0f, needs[2] - (100.0f / 5400.0f) * dt - (isRunning && currentSpeed > 0.0f ? 0.01f * dt : 0.0f));
        needs[4] = std::max(0.0f, needs[4] - (100.0f / 21600.0f) * dt);
        needs[5] = std::max(0.0f, needs[5] - (100.0f / 10800.0f + (simulation.IsNight() ? 100.0f / 360.0f : 0.0f)) * dt);
        needs[6] = std::max(0.0f, needs[6] - (60.0f / 7200.0f) * dt);
        if (needs[0] <= 0.0f) needs[3] = std::max(0.0f, needs[3] - (100.0f / 1800.0f) * dt);
        if (needs[1] <= 0.0f) needs[3] = std::max(0.0f, needs[3] - (100.0f / 1200.0f) * dt);
        for (auto disease = diseases.begin(); disease != diseases.end();) {
            disease->remaining -= dt;
            needs[3] = std::max(0.0f, needs[3] - disease->healthDrain * dt);
            needs[5] = std::max(0.0f, needs[5] - disease->mentalDrain * dt);
            if (disease->remaining <= 0.0f) disease = diseases.erase(disease); else ++disease;
        }
        infectionCooldown = std::max(0.0f, infectionCooldown - dt);
        if (infectionCooldown <= 0.0f && needs[4] < 30.0f && std::uniform_real_distribution<float>(0.0f, 1.0f)(lootRandom) < 0.00012f * dt && diseases.size() < 3) {
            const std::array<DiseaseState, 3> types{{{"gripe", 45.0f, 0.12f, 0.01f}, {"infeccion", 54.0f, 0.216f, 0.02f}, {"intoxicacion", 36.0f, 0.096f, 0.03f}}};
            const auto& type = types[static_cast<size_t>(std::uniform_int_distribution<int>(0, 2)(lootRandom))];
            if (std::none_of(diseases.begin(), diseases.end(), [&](const DiseaseState& disease) { return disease.id == type.id; })) diseases.push_back(type);
            infectionCooldown = 8.0f;
        }
        if (!ui.IsPaused() && Input::IsKeyPressed(GLFW_KEY_Q)) collectNearbyPickup(1.8f);
        if (!ui.IsPaused() && Input::IsKeyPressed(GLFW_KEY_F)) {
            int soldNow = 0;
            const bool atRecycler = simulation.IsNearRecycler(playerPos, 3.0f);
            for (auto entry = inventory.begin(); entry != inventory.end();) {
                const Item& item = lootTable[entry->itemIndex];
                if (item.consumableNeed >= 0 || item.id == "antibiotico" || item.id == "radioTransistor" || item.id == "pilas" || item.id == "tela" || item.id == "pastilla" || item.id == "madera" || item.id == "lona" || item.id == "colchon" || item.id == "vela" || item.id == "popo" || item.id == "alambre" || item.id == "plastico" || item.id == "mochila" || item.id == "arma" || item.id == "curita") { ++entry; continue; }
                money += item.value * entry->quantity;
                itemsSold += entry->quantity;
                if (atRecycler && simulation.IsMissionStarted() && !simulation.IsMissionComplete()) {
                    for (int delivered = 0; delivered < entry->quantity; ++delivered) {
                        if (!simulation.CompleteMissionItem()) break;
                        ++missionProgress;
                    }
                }
                soldNow += entry->quantity;
                xp += 2 * entry->quantity;
                while (xp >= 100 * level) { xp -= 100 * level; ++level; }
                inventoryWeight -= item.weight * entry->quantity;
                entry = inventory.erase(entry);
            }
            if (atRecycler && simulation.IsMissionStarted() && !simulation.IsMissionComplete() && missionProgress >= simulation.GetMissionTarget()) {
                money += 5000.0f;
                xp += 20;
                while (xp >= 100 * level) { xp -= 100 * level; ++level; }
                missionProgress = simulation.GetMissionTarget();
                simulation.CompleteMission();
            }
            if (soldNow > 0) {
                needs[5] = std::min(100.0f, needs[5] + std::min(5.0f, static_cast<float>(soldNow)) * 0.4f);
                audio.Play(AudioCue::Sell);
            } else audio.Play(AudioCue::Empty);
        }
        if (!ui.IsPaused() && (Input::IsKeyPressed(GLFW_KEY_B) || Input::IsKeyPressed(GLFW_KEY_L)) && !simulation.IsNearShop(playerPos, 2.0f)) {
            money += simulation.IsNight() ? 70.0f : 120.0f;
            needs[5] = std::min(100.0f, needs[5] + 0.6f);
        }
        if (!ui.IsPaused() && Input::IsKeyPressed(GLFW_KEY_E)) {
            size_t carIndex = simulation.GetNearbyCar(playerPos, 2.0f);
            bool nearShop = simulation.IsNearShop(playerPos, 2.2f);
            if (simulation.IsNearMissionNpc(playerPos, 2.0f) && !simulation.IsMissionStarted() && !simulation.IsMissionComplete()) {
                simulation.StartMission();
            } else if (simulation.StealCar(carIndex)) {
                const bool caught = std::uniform_real_distribution<float>(0.0f, 1.0f)(lootRandom) < 0.25f;
                if (caught) {
                    needs[3] = std::max(0.0f, needs[3] - 25.0f);
                    money = std::max(0.0f, money - 5000.0f);
                    audio.Play(AudioCue::Empty);
                } else {
                    xp += 8;
                    while (xp >= 100 * level) { xp -= 100 * level; ++level; }
                    for (const std::string id : {"retrovisor", "retrovisor"}) {
                        auto item = std::find_if(lootTable.begin(), lootTable.end(), [&](const Item& entry) { return entry.id == id; });
                        if (item != lootTable.end()) {
                            size_t itemIndex = static_cast<size_t>(item - lootTable.begin());
                            if (inventoryWeight + item->weight <= 20.0f) { inventory.push_back({itemIndex, 1}); inventoryWeight += item->weight; }
                        }
                    }
                    if (std::uniform_real_distribution<float>(0.0f, 1.0f)(lootRandom) < 0.2f) {
                        size_t radioIndex = static_cast<size_t>(std::find_if(lootTable.begin(), lootTable.end(), [](const Item& item) { return item.id == "radio"; }) - lootTable.begin());
                        if (inventoryWeight + lootTable[radioIndex].weight <= 20.0f) { inventory.push_back({radioIndex, 1}); inventoryWeight += lootTable[radioIndex].weight; }
                    }
                    audio.Play(AudioCue::Treasure);
                }
            } else if (nearShop) {
                const Item& product = shopCatalog[std::uniform_int_distribution<size_t>(0, shopCatalog.size() - 1)(lootRandom)];
                size_t itemIndex = static_cast<size_t>(std::find_if(lootTable.begin(), lootTable.end(), [&](const Item& item) { return item.id == product.id; }) - lootTable.begin());
                if (itemIndex < lootTable.size() && money >= product.value && inventoryWeight + product.weight <= 20.0f) {
                    money -= product.value;
                    auto entry = std::find_if(inventory.begin(), inventory.end(), [&](const InventoryEntry& item) { return item.itemIndex == itemIndex; });
                    if (entry == inventory.end()) inventory.push_back({itemIndex, 1}); else ++entry->quantity;
                    inventoryWeight += product.weight;
                    audio.Play(AudioCue::Pickup);
                }
            } else if (collectNearbyPickup(1.8f)) {
            } else {
                auto consumable = std::find_if(inventory.begin(), inventory.end(), [&](const InventoryEntry& entry) { return lootTable[entry.itemIndex].consumableNeed >= 0; });
                if (consumable != inventory.end()) {
                    const Item& item = lootTable[consumable->itemIndex];
                    const size_t needIndex = static_cast<size_t>(item.consumableNeed);
                    needs[needIndex] = std::min(100.0f, needs[needIndex] + item.amount);
                    inventoryWeight -= item.weight;
                    if (--consumable->quantity == 0) inventory.erase(consumable);
                    audio.Play(AudioCue::Pickup);
                } else {
                    auto medicine = std::find_if(inventory.begin(), inventory.end(), [&](const InventoryEntry& entry) {
                        const std::string& id = lootTable[entry.itemIndex].id;
                        return id == "vendaje" || id == "antibiotico" || id == "curita";
                    });
                    if (medicine != inventory.end()) {
                        const Item& item = lootTable[medicine->itemIndex];
                        if (item.id == "vendaje" || item.id == "curita") needs[3] = std::min(100.0f, needs[3] + (item.id == "curita" ? 15.0f : 30.0f)); else diseases.clear();
                        inventoryWeight -= item.weight;
                        if (--medicine->quantity == 0) inventory.erase(medicine);
                        audio.Play(AudioCue::Pickup);
                    }
                }
            }
        }
        const std::string craftRequest = ui.ConsumeCraftRequest();
        if (!craftRequest.empty()) {
            auto countItem = [&](const std::string& id) {
                int quantity = 0;
                for (const auto& entry : inventory) if (lootTable[entry.itemIndex].id == id) quantity += entry.quantity;
                return quantity;
            };
            std::string resultId;
            std::array<std::pair<std::string, int>, 3> ingredients;
            if (craftRequest == "radioTransistor" && countItem("chatarra") >= 1 && countItem("cable") >= 2 && countItem("pilas") >= 1 && inventoryWeight + 0.6f <= 20.0f) {
                resultId = "radioTransistor";
                ingredients = {{{"chatarra", 1}, {"cable", 2}, {"pilas", 1}}};
            } else if (craftRequest == "antibiotico" && countItem("pastilla") >= 1 && countItem("botella") >= 1 && inventoryWeight + 0.1f <= 20.0f) {
                resultId = "antibiotico";
                ingredients = {{{"pastilla", 1}, {"botella", 1}, {"", 0}}};
            } else if (craftRequest == "arma" && countItem("chatarra") >= 2 && countItem("alambre") >= 1 && countItem("madera") >= 1 && inventoryWeight + 1.2f <= 20.0f) {
                resultId = "arma";
                ingredients = {{{"chatarra", 2}, {"alambre", 1}, {"madera", 1}}};
            } else if (craftRequest == "mochila" && countItem("tela") >= 3 && countItem("alambre") >= 1 && inventoryWeight + 0.8f <= 20.0f) {
                resultId = "mochila";
                ingredients = {{{"tela", 3}, {"alambre", 1}, {"", 0}}};
            } else if (craftRequest == "curita" && countItem("tela") >= 1 && countItem("plastico") >= 1 && inventoryWeight + 0.1f <= 20.0f) {
                resultId = "curita";
                ingredients = {{{"tela", 1}, {"plastico", 1}, {"", 0}}};
            } else if (craftRequest == "rifa" && countItem("carton") >= 2 && countItem("lata") >= 1 && inventoryWeight + 0.05f <= 20.0f) {
                resultId = "rifa";
                ingredients = {{{"carton", 2}, {"lata", 1}, {"", 0}}};
            }
            if (!resultId.empty()) {
                for (const auto& [id, quantity] : ingredients) {
                    for (int removed = 0; removed < quantity; ++removed) {
                        auto entry = std::find_if(inventory.begin(), inventory.end(), [&](const InventoryEntry& item) { return lootTable[item.itemIndex].id == id; });
                        if (entry != inventory.end()) {
                            inventoryWeight -= lootTable[entry->itemIndex].weight;
                            if (--entry->quantity == 0) inventory.erase(entry);
                        }
                    }
                }
                size_t resultIndex = static_cast<size_t>(std::find_if(lootTable.begin(), lootTable.end(), [&](const Item& item) { return item.id == resultId; }) - lootTable.begin());
                auto result = std::find_if(inventory.begin(), inventory.end(), [&](const InventoryEntry& entry) { return entry.itemIndex == resultIndex; });
                const int resultQuantity = resultId == "curita" ? 2 : 1;
                if (result == inventory.end()) inventory.push_back({resultIndex, resultQuantity}); else result->quantity += resultQuantity;
                inventoryWeight += lootTable[resultIndex].weight;
                xp += 10;
                while (xp >= 100 * level) { xp -= 100 * level; ++level; }
                audio.Play(AudioCue::Treasure);
            }
        }

        if (simulation.IsMissionComplete()) objective = "Mision completada: recibiste $5000 y 20 XP";
        else if (simulation.IsMissionStarted()) objective = "Mision: entrega 5 reciclables en El Progreso [F] (" + std::to_string(simulation.GetMissionProgress()) + "/5)";
        else if (itemsCollected < 5) objective = "Rebusca y recoge 5 objetos [Q]";
        else if (itemsSold < 3) objective = "Recicla 3 objetos del costal [F]";
        else if (level < 2) objective = "Sigue rebuscando para alcanzar nivel 2";
        else if (level >= 2) objective = "Sobrevive en Bogotá y mejora tu vida";

        simulation.Update(dt, playerPos);

        // 7. Actualizar cámara
        camera.Follow(playerPos, dt, currentSpeed, isRunning);

        // 8. Renderizado 3D con OpenGL 4.5 DSA
        renderer.BeginScene(camera, clock.GetElapsedTime());

        // Renderizar ciudad de Bogotá
        city.Render(renderer);
        simulation.Render(renderer, clock.GetElapsedTime(), playerPos);

        // Renderizar personaje
        {
            glm::mat4 playerModel = glm::translate(glm::mat4(1.0f), playerPos);
            playerModel = glm::rotate(playerModel, playerYaw, glm::vec3(0.0f, 1.0f, 0.0f));

            if (charBody) {
                glm::mat4 mScale = glm::scale(playerModel, glm::vec3(1.08f));
                const bool moving = currentSpeed > 0.25f;
                const int animationClip = moving ? (isRunning && runClip >= 0 ? runClip : walkClip) : idleClip;
                const float animationTime = clock.GetElapsedTime();
                charBody->Draw(renderer, mScale, glm::vec4(0.85f, 0.72f, 0.62f, 1.0f), animationTime, animationClip >= 0, animationClip);
                if (charJacket) charJacket->Draw(renderer, mScale, glm::vec4(0.25f, 0.40f, 0.55f, 1.0f), animationTime, animationClip >= 0, animationClip);
                if (charShoes)  charShoes->Draw(renderer, mScale, glm::vec4(0.30f, 0.28f, 0.26f, 1.0f), animationTime, animationClip >= 0, animationClip);
                if (charHair)   charHair->Draw(renderer, mScale, glm::vec4(0.15f, 0.12f, 0.10f, 1.0f), animationTime, animationClip >= 0, animationClip);
            } else {
                glm::mat4 bodyModel = glm::translate(playerModel, glm::vec3(0.0f, 0.6f, 0.0f));
                renderer.Submit(*fallbackBody, bodyModel);

                glm::mat4 headModel = glm::translate(playerModel, glm::vec3(0.0f, 1.4f, 0.0f));
                renderer.Submit(*fallbackHead, headModel);
            }
        }

        renderer.EndScene();

        // 9. Interfaz gráfica (Dear ImGui)
        ui.BeginFrame();
        std::string inventorySummary = inventory.empty() ? "Vacío" : "";
        for (const auto& entry : inventory) {
            if (!inventorySummary.empty()) inventorySummary += "  |  ";
            inventorySummary += lootTable[entry.itemIndex].name + " x" + std::to_string(entry.quantity);
        }
        std::string diseaseSummary = diseases.empty() ? "Sin enfermedades" : "";
        for (const auto& disease : diseases) {
            if (!diseaseSummary.empty()) diseaseSummary += "  |  ";
            diseaseSummary += disease.id + " (" + std::to_string(static_cast<int>(disease.remaining)) + " s)";
        }
        const auto inventoryCount = [&](const std::string& id) {
            int count = 0;
            for (const auto& entry : inventory) if (lootTable[entry.itemIndex].id == id) count += entry.quantity;
            return count;
        };
        const std::vector<CraftingOption> craftingOptions{
            {"radioTransistor", "Radio transistor AM/FM x1", "Chatarra (" + std::to_string(inventoryCount("chatarra")) + "/1) · Cable (" + std::to_string(inventoryCount("cable")) + "/2) · Pilas (" + std::to_string(inventoryCount("pilas")) + "/1)", inventoryCount("chatarra") >= 1 && inventoryCount("cable") >= 2 && inventoryCount("pilas") >= 1 && inventoryWeight + 0.6f <= 20.0f},
            {"arma", "Arma casera x1", "Chatarra (" + std::to_string(inventoryCount("chatarra")) + "/2) · Alambre (" + std::to_string(inventoryCount("alambre")) + "/1) · Tablas (" + std::to_string(inventoryCount("madera")) + "/1)", inventoryCount("chatarra") >= 2 && inventoryCount("alambre") >= 1 && inventoryCount("madera") >= 1 && inventoryWeight + 1.2f <= 20.0f},
            {"mochila", "Mochila x1", "Tela (" + std::to_string(inventoryCount("tela")) + "/3) · Alambre (" + std::to_string(inventoryCount("alambre")) + "/1)", inventoryCount("tela") >= 3 && inventoryCount("alambre") >= 1 && inventoryWeight + 0.8f <= 20.0f},
            {"curita", "Curita x2", "Tela (" + std::to_string(inventoryCount("tela")) + "/1) · Plastico (" + std::to_string(inventoryCount("plastico")) + "/1)", inventoryCount("tela") >= 1 && inventoryCount("plastico") >= 1 && inventoryWeight + 0.1f <= 20.0f},
            {"antibiotico", "Antibiotico x1", "Pastilla (" + std::to_string(inventoryCount("pastilla")) + "/1) · Botella (" + std::to_string(inventoryCount("botella")) + "/1)", inventoryCount("pastilla") >= 1 && inventoryCount("botella") >= 1 && inventoryWeight + 0.1f <= 20.0f},
            {"rifa", "Carton de rifa x1", "Carton (" + std::to_string(inventoryCount("carton")) + "/2) · Lata (" + std::to_string(inventoryCount("lata")) + "/1)", inventoryCount("carton") >= 2 && inventoryCount("lata") >= 1 && inventoryWeight + 0.05f <= 20.0f},
            {"hierba", "Hierba seca x2", "Tela (" + std::to_string(inventoryCount("tela")) + "/1) · Tablas (" + std::to_string(inventoryCount("madera")) + "/1) · Resultado sin definir", false}
        };
        ui.RenderHUD(dt, playerPos, camera, renderer, window, city, simulation, audio, needs, money, inventoryWeight, 20.0f,
                     simulation.GetDayProgress(), simulation.GetPedestrianCount(), simulation.GetTrafficCount(), inventorySummary, diseaseSummary, level, xp, objective,
                     simulation.GetMissionProgress(), simulation.GetMissionTarget(), craftingOptions);
        ui.EndFrame();

        window.SwapBuffers();
        if (ui.ConsumeSaveRequest()) survivalSaveTimer = 5.0f;
        survivalSaveTimer += dt;
        if (survivalSaveTimer >= 5.0f) {
            nlohmann::json state;
            state["needs"] = needs;
            state["money"] = money;
            state["xp"] = xp;
            state["level"] = level;
            state["itemsCollected"] = itemsCollected;
            state["itemsSold"] = itemsSold;
            state["missionProgress"] = simulation.GetMissionProgress();
            state["missionStarted"] = simulation.IsMissionStarted();
            state["missionComplete"] = simulation.IsMissionComplete();
            state["objective"] = objective;
            state["dayProgress"] = simulation.GetDayProgress();
            state["diseases"] = nlohmann::json::array();
            for (const auto& disease : diseases) state["diseases"].push_back({{"id", disease.id}, {"remaining", disease.remaining}, {"healthDrain", disease.healthDrain}, {"mentalDrain", disease.mentalDrain}});
            state["playerX"] = playerPos.x;
            state["playerZ"] = playerPos.z;
            state["inventory"] = nlohmann::json::array();
            for (const auto& entry : inventory) state["inventory"].push_back({{"item", entry.itemIndex}, {"quantity", entry.quantity}});
            std::ofstream save(savePath);
            if (save) save << state.dump(2);
            survivalSaveTimer = 0.0f;
        }
    }

    Input::Shutdown();
    LogInfo("Cerrando aplicacion con normalidad.");
    return 0;
}
