#pragma once
#include <vector>
#include <array>
#include <memory>
#include <string>
#include <functional>
#include <glm/glm.hpp>
#include "../renderer/Mesh.hpp"
#include "../renderer/Model.hpp"
#include "../renderer/Renderer.hpp"
#include "../renderer/Texture.hpp"
#include "SignAtlas.hpp"

namespace Bazuko {

enum class TileType : uint8_t {
    Grass,
    Road,
    Busway,
    Sidewalk,
    Building,
    Plaza,
    Wall
};

struct BuildingInstance {
    glm::vec3 position;
    glm::vec3 size;
    glm::vec4 color;
    int urbanDesign = -1;
    int floors = 0;
    int modelVariant = -1;
};

class CityLayout {
public:
    static constexpr int WIDTH = 120;
    static constexpr int HEIGHT = 111;
    static constexpr float TILE_SIZE = 2.0f; // 2 metros por baldosa (240x222m total)

    using CityLoadProgress = std::function<void(size_t completed, size_t total, const std::string& stage)>;
    explicit CityLayout(const CityLoadProgress& progressCallback = nullptr);
    ~CityLayout() = default;

    void Render(Renderer& renderer, const glm::vec3& focusPosition);

    TileType GetTileAt(int x, int z) const;
    bool IsWalkable(float worldX, float worldZ) const;
    bool IsRoad(float worldX, float worldZ) const;
    bool IsSidewalk(float worldX, float worldZ) const;
    bool IsBlocked(float worldX, float worldZ, float radius = 0.0f) const;

    glm::vec3 GetSpawnPoint() const;

private:
    void GenerateCityMap();
    void BuildMeshes(const CityLoadProgress& progressCallback = nullptr);

    std::vector<TileType> m_Grid;
    std::shared_ptr<Mesh> m_GroundMesh;
    std::shared_ptr<Mesh> m_SidewalkMesh;
    std::shared_ptr<Mesh> m_NatureMesh;
    std::shared_ptr<Mesh> m_MarkingMesh;
    std::shared_ptr<Mesh> m_BuildingMesh;
    std::shared_ptr<Mesh> m_FacadeDetailMesh;
    std::shared_ptr<Mesh> m_SignMesh;
    std::array<std::shared_ptr<Model>, 2> m_BuildingModels;
    std::array<std::shared_ptr<Model>, 3> m_ModularBuildingModels;
    SignAtlas m_SignAtlas;
    std::shared_ptr<Texture> m_AsphaltTexture;
    std::shared_ptr<Texture> m_SidewalkTexture;
    std::shared_ptr<Texture> m_BrickTexture;
    std::vector<BuildingInstance> m_Buildings;
};

} // namespace Bazuko
