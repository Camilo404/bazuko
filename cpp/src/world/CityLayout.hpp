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
#include "Station.hpp"

namespace Bazuko {

enum class TileType : uint8_t {
    Grass,
    Road,
    Busway,
    Sidewalk,
    Building,
    Plaza,
    Wall,
    Water,   // lago: no transitable
    Floor,   // suelo duro de un lote (gasolinera), transitable como un anden
    Solid    // edificio unico (tienda D69): bloquea el paso pero no genera la caja generica
};

// Caja solida de un hito urbano (surtidores, columnas, torres de la pasarela...)
struct StaticBox {
    glm::vec3 center;
    glm::vec3 half;
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
    static constexpr float TILE_SIZE = 2.0f;
    // El mapa mide 120 x 161 baldosas (240 x 322 m): el centro original de 120 x 111 mas el Parque del Lago al sur.
    // El origen del mundo sigue anclado al centro original (ORIGIN_Z) para que las partidas guardadas y todas las
    // posiciones fijas conserven sus coordenadas al ampliar el mapa.
    static constexpr int BASE_HEIGHT = 111;
    static constexpr int HEIGHT = 161;
    static constexpr float ORIGIN_X = WIDTH * TILE_SIZE * 0.5f;
    static constexpr float ORIGIN_Z = BASE_HEIGHT * TILE_SIZE * 0.5f;

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

    // Hitos con nombre para el radar: gasolinera, tienda D69, pasarela y Parque del Lago
    struct Landmark {
        std::string name;
        glm::vec3 position;
    };
    const std::vector<Landmark>& GetLandmarks() const { return m_Landmarks; }
    const std::vector<StaticBox>& GetObstacles() const { return m_Obstacles; }
    // Cota del suelo a nivel de calle: sube sobre la isla de la estacion y es 0 en el resto
    float GetGroundHeight(float worldX, float worldZ) const { return Station::Height(worldX, worldZ); }
    // 0..1: las luces de Colpatria se encienden de noche
    void SetNightGlow(float glow) { m_NightGlow = glow; }

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
    std::shared_ptr<Mesh> m_ColpatriaLightMesh;   // franjas de colores de la torre (brillan de noche)
    std::shared_ptr<Mesh> m_GlassMesh;            // vidrios de la estacion (mezcla alfa)
    float m_NightGlow = 0.0f;
    std::shared_ptr<Mesh> m_LandmarkMesh;   // cajas de color de la gasolinera, la tienda D69 y la pasarela
    std::vector<Landmark> m_Landmarks;
    std::vector<StaticBox> m_Obstacles;
    std::array<std::shared_ptr<Model>, 2> m_BuildingModels;
    std::array<std::shared_ptr<Model>, 3> m_ModularBuildingModels;
    SignAtlas m_SignAtlas;
    std::shared_ptr<Texture> m_AsphaltTexture;
    std::shared_ptr<Texture> m_SidewalkTexture;
    std::shared_ptr<Texture> m_BrickTexture;
    std::vector<BuildingInstance> m_Buildings;
};

} // namespace Bazuko
