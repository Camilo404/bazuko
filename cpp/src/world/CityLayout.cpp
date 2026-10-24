#include "CityLayout.hpp"
#include "../core/Paths.hpp"
#include <cmath>
#include <algorithm>
#include <array>
#include <random>
#include <cstdint>

namespace Bazuko {

namespace {

std::shared_ptr<Texture> MakeSurfaceTexture(int surface, uint32_t seed) {
    constexpr int size = 128;
    std::mt19937 random(seed);
    std::uniform_int_distribution<int> noise(-10, 10);
    std::vector<uint8_t> pixels(size * size * 4, 255);
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
        int red = 57, green = 59, blue = 62;
        const int variation = noise(random);
        if (surface == 1) {
            const int brickHeight = 12;
            const int brickWidth = 37;
            const int staggeredX = (x + (y / brickHeight % 2) * (brickWidth / 2)) % brickWidth;
            const bool mortar = y % brickHeight < 2 || staggeredX < 2;
            constexpr std::array<std::array<int, 3>, 6> brickTones{{
                {{138,75,50}}, {{125,68,48}}, {{148,85,58}},
                {{110,59,42}}, {{160,95,64}}, {{133,72,47}}
            }};
            const auto& tone = brickTones[static_cast<size_t>((x / brickWidth + y / brickHeight) % brickTones.size())];
            red = mortar ? 90 : tone[0] + variation;
            green = mortar ? 68 : tone[1] + variation;
            blue = mortar ? 55 : tone[2] + variation;
        } else if (surface == 2) {
            const bool grout = x % 32 < 2 || y % 32 < 2;
            red = grout ? 105 : 153 + variation;
            green = grout ? 101 : 149 + variation;
            blue = grout ? 96 : 138 + variation;
        } else {
            red += variation;
            green += variation;
            blue += variation;
        }
        const size_t index = static_cast<size_t>((y * size + x) * 4);
        pixels[index] = static_cast<uint8_t>(std::clamp(red, 0, 255));
        pixels[index + 1] = static_cast<uint8_t>(std::clamp(green, 0, 255));
        pixels[index + 2] = static_cast<uint8_t>(std::clamp(blue, 0, 255));
        pixels[index + 3] = 255;
    }
    return std::make_shared<Texture>(size, size, pixels.data());
}

}

CityLayout::CityLayout(const CityLoadProgress& progressCallback)
    : m_Grid(WIDTH * HEIGHT, TileType::Building) {
    if (progressCallback) progressCallback(1, 7, "Generando trazado urbano y calles de Bogotá...");
    GenerateCityMap();
    BuildMeshes(progressCallback);
}

TileType CityLayout::GetTileAt(int x, int z) const {
    if (x < 0 || x >= WIDTH || z < 0 || z >= HEIGHT) return TileType::Wall;
    return m_Grid[z * WIDTH + x];
}

bool CityLayout::IsWalkable(float worldX, float worldZ) const {
    int tileX = static_cast<int>(std::floor((worldX + (WIDTH * TILE_SIZE * 0.5f)) / TILE_SIZE));
    int tileZ = static_cast<int>(std::floor((worldZ + (HEIGHT * TILE_SIZE * 0.5f)) / TILE_SIZE));

    TileType tile = GetTileAt(tileX, tileZ);
    return tile != TileType::Building && tile != TileType::Wall;
}

bool CityLayout::IsRoad(float worldX, float worldZ) const {
    const int x = static_cast<int>(std::floor((worldX + WIDTH * TILE_SIZE * 0.5f) / TILE_SIZE));
    const int z = static_cast<int>(std::floor((worldZ + HEIGHT * TILE_SIZE * 0.5f) / TILE_SIZE));
    const auto tile = GetTileAt(x, z);
    return tile == TileType::Road || tile == TileType::Busway;
}

bool CityLayout::IsSidewalk(float worldX, float worldZ) const {
    const int x = static_cast<int>(std::floor((worldX + WIDTH * TILE_SIZE * 0.5f) / TILE_SIZE));
    const int z = static_cast<int>(std::floor((worldZ + HEIGHT * TILE_SIZE * 0.5f) / TILE_SIZE));
    const auto tile = GetTileAt(x, z);
    return tile == TileType::Sidewalk || tile == TileType::Plaza || tile == TileType::Grass;
}

bool CityLayout::IsBlocked(float worldX, float worldZ, float radius) const {
    return !IsWalkable(worldX - radius, worldZ - radius) || !IsWalkable(worldX + radius, worldZ - radius)
        || !IsWalkable(worldX - radius, worldZ + radius) || !IsWalkable(worldX + radius, worldZ + radius);
}

glm::vec3 CityLayout::GetSpawnPoint() const {
    // Spawn en un andén espacioso en el centro
    return glm::vec3(0.0f, 0.0f, 0.0f);
}

void CityLayout::GenerateCityMap() {
    // 1. Configurar vías principales (Carreras en X, Calles en Z)
    // Troncal Caracas en el centro
    int caracasBus0 = 57, caracasBus1 = 62;
    int caracasLane0 = 53, caracasLane1 = 56;
    int caracasLane2 = 63, caracasLane3 = 66;

    // Carreras secundarias
    std::vector<std::pair<int, int>> carreras = {
        {18, 23}, {36, 41}, {caracasLane0, caracasLane1}, {caracasLane2, caracasLane3},
        {84, 89}, {106, 111}
    };

    // Calles
    std::vector<std::pair<int, int>> calles = {
        {16, 21}, {38, 43}, {60, 65}, {72, 77}, {94, 99}
    };

    // Asignar troncal TransMilenio
    for (int z = 1; z < HEIGHT - 1; ++z) {
        for (int x = caracasBus0; x <= caracasBus1; ++x) {
            m_Grid[z * WIDTH + x] = TileType::Busway;
        }
    }

    // Asignar carreras (vías)
    for (int z = 1; z < HEIGHT - 1; ++z) {
        for (const auto& [x0, x1] : carreras) {
            for (int x = x0; x <= x1; ++x) {
                m_Grid[z * WIDTH + x] = TileType::Road;
            }
        }
    }

    // Asignar calles (vías)
    for (const auto& [z0, z1] : calles) {
        for (int z = z0; z <= z1; ++z) {
            for (int x = 1; x < WIDTH - 1; ++x) {
                if (m_Grid[z * WIDTH + x] != TileType::Busway) {
                    m_Grid[z * WIDTH + x] = TileType::Road;
                }
            }
        }
    }

    // 2. Generar andenes alrededor de las vías
    auto isStreet = [&](int x, int z) {
        if (x < 0 || x >= WIDTH || z < 0 || z >= HEIGHT) return false;
        TileType t = m_Grid[z * WIDTH + x];
        return t == TileType::Road || t == TileType::Busway;
    };

    for (int z = 1; z < HEIGHT - 1; ++z) {
        for (int x = 1; x < WIDTH - 1; ++x) {
            if (m_Grid[z * WIDTH + x] != TileType::Building) continue;

            bool nearStreet = false;
            for (int dz = -2; dz <= 2 && !nearStreet; ++dz) {
                for (int dx = -2; dx <= 2; ++dx) {
                    if (isStreet(x + dx, z + dz)) {
                        nearStreet = true;
                        break;
                    }
                }
            }

            if (nearStreet) {
                m_Grid[z * WIDTH + x] = TileType::Sidewalk;
            }
        }
    }

    // 3. Parques y Plazas
    // Parque Central
    for (int z = 24; z <= 35; ++z) {
        for (int x = 26; x <= 33; ++x) {
            if (m_Grid[z * WIDTH + x] == TileType::Building) {
                m_Grid[z * WIDTH + x] = (x == 29 || z == 29) ? TileType::Plaza : TileType::Grass;
            }
        }
    }
    const std::array<std::array<int, 4>, 2> additionalParks{{
        {{44, 50, 24, 35}},
        {{70, 81, 46, 57}}
    }};
    for (const auto& park : additionalParks) {
        for (int z = park[2]; z <= park[3]; ++z) {
            for (int x = park[0]; x <= park[1]; ++x) {
                if (m_Grid[z * WIDTH + x] == TileType::Building) m_Grid[z * WIDTH + x] = TileType::Grass;
            }
        }
    }

    // Muros perimetrales
    for (int x = 0; x < WIDTH; ++x) {
        m_Grid[0 * WIDTH + x] = TileType::Wall;
        m_Grid[(HEIGHT - 1) * WIDTH + x] = TileType::Wall;
    }
    for (int z = 0; z < HEIGHT; ++z) {
        m_Grid[z * WIDTH + 0] = TileType::Wall;
        m_Grid[z * WIDTH + (WIDTH - 1)] = TileType::Wall;
    }
}

void CityLayout::BuildMeshes(const CityLoadProgress& progressCallback) {
    m_Buildings.clear();
    if (progressCallback) progressCallback(2, 7, "Cargando arquitectura: Edificio 1 (Only_Building.fbx)...");
    m_BuildingModels[0] = Model::LoadFromFile(Paths::Find("public/models/Edificio 1/Only_Building.fbx"));
    if (progressCallback) progressCallback(3, 7, "Cargando arquitectura: Edificio 3 (highrise.fbx)...");
    m_BuildingModels[1] = Model::LoadFromFile(Paths::Find("public/models/Edificio 3/highrise.fbx"));
    constexpr std::array<const char*, 3> modularBuildingFiles{{
        "Building_LevelModularShell_Bottom.fbx",
        "Building_LevelModularShell_Mid.fbx",
        "Building_LevelModularShell_Top.fbx"
    }};
    for (size_t index = 0; index < modularBuildingFiles.size(); ++index) {
        if (progressCallback) {
            progressCallback(4 + index, 7, "Cargando módulos urbanos (" + std::to_string(index + 1) + "/3: " + modularBuildingFiles[index] + ")...");
        }
        m_ModularBuildingModels[index] = Model::LoadFromFile(Paths::Find(
            std::string("public/models/Edificio 2/Models/FBX/") + modularBuildingFiles[index]));
    }
    if (progressCallback) progressCallback(7, 7, "Construyendo mallas de calzadas, andenes y señalética...");
    const auto availableBuildingVariant = [&](int preferred) {
        for (int offset = 0; offset < 3; ++offset) {
            const int variant = (preferred + offset) % 3;
            const bool available = variant == 1
                ? std::all_of(m_ModularBuildingModels.begin(), m_ModularBuildingModels.end(),
                              [](const auto& model) { return static_cast<bool>(model); })
                : static_cast<bool>(m_BuildingModels[variant == 0 ? 0 : 1]);
            if (available) return variant;
        }
        return -1;
    };
    std::vector<Vertex> groundVertices;
    std::vector<uint32_t> groundIndices;
    std::vector<Vertex> sidewalkVertices;
    std::vector<uint32_t> sidewalkIndices;
    std::vector<Vertex> parkVertices;
    std::vector<uint32_t> parkIndices;
    std::vector<Vertex> markingVertices;
    std::vector<uint32_t> markingIndices;

    float halfWidth = (WIDTH * TILE_SIZE) * 0.5f;
    float halfHeight = (HEIGHT * TILE_SIZE) * 0.5f;

    auto getColorForTile = [](TileType type) -> glm::vec4 {
        switch (type) {
            case TileType::Road:     return glm::vec4(0.20f, 0.21f, 0.23f, 1.0f); // Asfalto
            case TileType::Busway:   return glm::vec4(0.42f, 0.22f, 0.20f, 1.0f); // Carril TransMilenio
            case TileType::Sidewalk: return glm::vec4(0.55f, 0.54f, 0.52f, 1.0f); // Andén bogotano
            case TileType::Grass:    return glm::vec4(0.25f, 0.38f, 0.22f, 1.0f); // Prado
            case TileType::Plaza:    return glm::vec4(0.60f, 0.58f, 0.54f, 1.0f); // Adoquín plaza
            default:                 return glm::vec4(0.30f, 0.30f, 0.30f, 1.0f);
        }
    };

    m_AsphaltTexture = MakeSurfaceTexture(0, 22);
    m_BrickTexture = MakeSurfaceTexture(1, 43);
    m_SidewalkTexture = MakeSurfaceTexture(2, 87);
    std::mt19937 rng(42); // Seed fija para reproducibilidad
    std::uniform_real_distribution<float> heightDist(6.5f, 15.5f);
    std::uniform_int_distribution<int> colorTypeDist(0, 3);

    std::vector<Vertex> buildingVertices;
    std::vector<uint32_t> buildingIndices;
    std::vector<Vertex> detailVertices;
    std::vector<uint32_t> detailIndices;
    std::vector<Vertex> signVertices;
    std::vector<uint32_t> signIndices;
    std::vector<uint8_t> modelOccupied(static_cast<size_t>(WIDTH * HEIGHT), 0);
    std::vector<uint8_t> visitedBuildingTiles(static_cast<size_t>(WIDTH * HEIGHT), 0);
    const auto urbanHash = [](int x, int z) {
        return static_cast<uint32_t>((static_cast<uint32_t>(x + 17) * 73856093u)
            ^ (static_cast<uint32_t>(z + 31) * 19349663u));
    };
    constexpr std::array<glm::vec4, 4> urbanColors{{
        {0.78f, 0.40f, 0.27f, 1.0f}, {0.65f, 0.67f, 0.67f, 1.0f},
        {0.81f, 0.77f, 0.68f, 1.0f}, {0.60f, 0.64f, 0.66f, 1.0f}
    }};
    constexpr std::array<int, 8> blockFloorCounts{{5, 6, 7, 8, 10, 12, 16, 24}};
    std::vector<size_t> componentQueue;
    for (int startZ = 1; startZ < HEIGHT - 1; ++startZ) {
        for (int startX = 1; startX < WIDTH - 1; ++startX) {
            const size_t startIndex = static_cast<size_t>(startZ * WIDTH + startX);
            if (m_Grid[startIndex] != TileType::Building || visitedBuildingTiles[startIndex]) continue;
            componentQueue.clear();
            componentQueue.push_back(startIndex);
            visitedBuildingTiles[startIndex] = 1;
            int minX = startX, maxX = startX, minZ = startZ, maxZ = startZ;
            for (size_t current = 0; current < componentQueue.size(); ++current) {
                const int x = static_cast<int>(componentQueue[current] % WIDTH);
                const int z = static_cast<int>(componentQueue[current] / WIDTH);
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
                minZ = std::min(minZ, z);
                maxZ = std::max(maxZ, z);
                for (const auto [dx, dz] : {std::pair{-1, 0}, std::pair{1, 0},
                                            std::pair{0, -1}, std::pair{0, 1}}) {
                    const int nextX = x + dx, nextZ = z + dz;
                    if (GetTileAt(nextX, nextZ) != TileType::Building) continue;
                    const size_t nextIndex = static_cast<size_t>(nextZ * WIDTH + nextX);
                    if (visitedBuildingTiles[nextIndex]) continue;
                    visitedBuildingTiles[nextIndex] = 1;
                    componentQueue.push_back(nextIndex);
                }
            }
            const int tilesX = maxX - minX + 1;
            const int tilesZ = maxZ - minZ + 1;
            const size_t boundingArea = static_cast<size_t>(tilesX * tilesZ);
            const float coverage = static_cast<float>(componentQueue.size()) / static_cast<float>(boundingArea);
            const uint32_t seed = urbanHash(minX, minZ);
            const int variant = availableBuildingVariant(static_cast<int>(seed % 3u));
            if (variant < 0 || tilesX < 4 || tilesZ < 4 || coverage < 1.0f) continue;
            for (const size_t tile : componentQueue) modelOccupied[tile] = 1;
            const int floors = seed % 17u == 0u ? 24
                : blockFloorCounts[static_cast<size_t>((seed >> 8) % blockFloorCounts.size())];
            const float width = std::max(0.5f, static_cast<float>(tilesX) * TILE_SIZE - 0.5f);
            const float depth = std::max(0.5f, static_cast<float>(tilesZ) * TILE_SIZE - 0.5f);
            const float height = static_cast<float>(floors) * 3.2f;
            const float centerX = (static_cast<float>(minX + maxX + 1) * TILE_SIZE - WIDTH * TILE_SIZE) * 0.5f;
            const float centerZ = (static_cast<float>(minZ + maxZ + 1) * TILE_SIZE - HEIGHT * TILE_SIZE) * 0.5f;
            m_Buildings.push_back({{centerX, height * 0.5f, centerZ},
                                   {width, height, depth},
                                   urbanColors[seed % urbanColors.size()], -1, floors, variant});
        }
    }
    // Rotulos con texto: si no hay fuente se vuelve a los letreros de color liso.
    const bool hasSigns = m_SignAtlas.Build(Paths::Find("assets/fonts/Roboto-Medium.ttf"));
    const auto appendSignQuad = [&](const glm::vec3& center, const glm::vec3& normal, float width, float height,
                                    const SignUV& uv, float angle, float shiftU) {
        const glm::vec3 axisU = normal.z != 0.0f ? glm::vec3(normal.z > 0 ? 1.0f : -1.0f, 0, 0)
                                                : glm::vec3(0, 0, normal.x > 0 ? -1.0f : 1.0f);
        const glm::vec3 axisV(0, 1, 0);
        const glm::vec3 c = center + axisU * shiftU;
        const float ca = std::cos(angle), sa = std::sin(angle);
        const glm::vec3 u = (axisU * ca + axisV * sa) * (width * 0.5f);
        const glm::vec3 v = (axisV * ca - axisU * sa) * (height * 0.5f);
        const glm::vec4 white(1.0f);
        const uint32_t base = static_cast<uint32_t>(signVertices.size());
        signVertices.push_back({c - u - v, normal, {uv.min.x, uv.min.y}, white});
        signVertices.push_back({c + u - v, normal, {uv.max.x, uv.min.y}, white});
        signVertices.push_back({c + u + v, normal, {uv.max.x, uv.max.y}, white});
        signVertices.push_back({c - u + v, normal, {uv.min.x, uv.max.y}, white});
        signIndices.insert(signIndices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});
    };
    const auto appendFacadePanel = [&](const glm::vec3& center, const glm::vec3& normal,
                                       float width, float height, const glm::vec4& color, float offset) {
        const glm::vec3 axisU = normal.z != 0.0f ? glm::vec3(normal.z > 0 ? 1.0f : -1.0f,0,0)
                                                : glm::vec3(0,0,normal.x > 0 ? -1.0f : 1.0f);
        const glm::vec3 axisV(0,1,0);
        const glm::vec3 surface = center + normal * offset;
        const glm::vec3 halfU = axisU * (width * 0.5f);
        const glm::vec3 halfV = axisV * (height * 0.5f);
        const uint32_t base = static_cast<uint32_t>(detailVertices.size());
        detailVertices.push_back({surface-halfU-halfV,normal,{0,0},color});
        detailVertices.push_back({surface+halfU-halfV,normal,{1,0},color});
        detailVertices.push_back({surface+halfU+halfV,normal,{1,1},color});
        detailVertices.push_back({surface-halfU+halfV,normal,{0,1},color});
        detailIndices.insert(detailIndices.end(),{base,base+1,base+2,base+2,base+3,base});
    };

    for (int z = 0; z < HEIGHT; ++z) {
        for (int x = 0; x < WIDTH; ++x) {
            TileType type = m_Grid[z * WIDTH + x];
            const size_t tileIndex = static_cast<size_t>(z * WIDTH + x);
            if (type == TileType::Building && modelOccupied[tileIndex] != 0) continue;

            float x0 = x * TILE_SIZE - halfWidth;
            float z0 = z * TILE_SIZE - halfHeight;
            float x1 = x0 + TILE_SIZE;
            float z1 = z0 + TILE_SIZE;

            if (type == TileType::Building) {
                float bHeight = heightDist(rng);
                int colorVariant = colorTypeDist(rng);
                glm::vec4 bColor;
                if (colorVariant == 0) {
                    bColor = glm::vec4(0.96f, 0.88f, 0.82f, 1.0f);
                } else if (colorVariant == 1) {
                    bColor = glm::vec4(0.82f, 0.86f, 0.88f, 1.0f);
                } else if (colorVariant == 2) {
                    bColor = glm::vec4(0.98f, 0.91f, 0.78f, 1.0f);
                } else {
                    bColor = glm::vec4(0.78f, 0.80f, 0.82f, 1.0f);
                }

                constexpr float buildingWidth = TILE_SIZE;
                const float buildingDepth = buildingWidth;
                m_Buildings.push_back({
                    glm::vec3(x0 + buildingWidth * 0.5f, bHeight * 0.5f,
                              z0 + buildingDepth * 0.5f),
                    glm::vec3(buildingWidth, bHeight, buildingDepth),
                    bColor,
                    -1
                });
                const std::array<std::pair<TileType, glm::vec3>,4> facadeSides{{
                    {GetTileAt(x,z-1),{0,0,-1}}, {GetTileAt(x,z+1),{0,0,1}},
                    {GetTileAt(x-1,z),{-1,0,0}}, {GetTileAt(x+1,z),{1,0,0}}
                }};
                std::array<int, 4> streetSideIndices{};
                int streetSideCount = 0;
                for (size_t i = 0; i < facadeSides.size(); ++i) {
                    const TileType neighbor = facadeSides[i].first;
                    if (neighbor == TileType::Sidewalk || neighbor == TileType::Road || neighbor == TileType::Busway) {
                        streetSideIndices[streetSideCount++] = static_cast<int>(i);
                    }
                }

                int storefrontSide = -1;
                std::uniform_int_distribution<int> storefrontChanceDist(0, 99);
                if (streetSideCount > 0 && storefrontChanceDist(rng) < 30) {
                    std::uniform_int_distribution<int> sideChoiceDist(0, streetSideCount - 1);
                    storefrontSide = streetSideIndices[sideChoiceDist(rng)];
                }

                int sideIndex = -1;
                for (const auto& [neighbor, normal] : facadeSides) {
                    ++sideIndex;
                    if (neighbor != TileType::Sidewalk && neighbor != TileType::Road && neighbor != TileType::Busway) continue;
                    const glm::vec3 wallCenter(x0 + TILE_SIZE*0.5f,0.0f,z0 + TILE_SIZE*0.5f);
                    const float wallOffset = TILE_SIZE * 0.5f + 0.012f;
                    for (float y=4.5f; y<bHeight-0.8f; y+=2.7f) {
                        const glm::vec3 paneCenter = wallCenter + glm::vec3(0,y,0) + normal*wallOffset;
                        appendFacadePanel(paneCenter,normal,0.94f,1.22f,{0.90f,0.82f,0.67f,1.0f},0.0f);
                        appendFacadePanel(paneCenter+normal*0.014f,normal,0.76f,0.98f,{0.10f,0.17f,0.22f,1.0f},0.0f);
                        appendFacadePanel(paneCenter+glm::vec3(0,0.03f,0)+normal*0.03f,normal,0.035f,0.92f,{0.72f,0.67f,0.56f,1.0f},0.0f);
                    }
                    if (sideIndex == storefrontSide) {
                        const float storefrontOffset = wallOffset + 0.012f;
                        appendFacadePanel(wallCenter+glm::vec3(0,1.12f,0)+normal*storefrontOffset,normal,1.62f,2.18f,{0.39f,0.42f,0.41f,1.0f},0.0f);
                        for (int slat = 0; slat < 11; ++slat) {
                            const float y = 0.18f + slat * 0.18f;
                            appendFacadePanel(wallCenter+glm::vec3(0,y,0)+normal*(storefrontOffset+0.012f),normal,1.60f,0.025f,
                                              slat % 2 == 0 ? glm::vec4(0.25f,0.28f,0.28f,1.0f) : glm::vec4(0.55f,0.57f,0.55f,1.0f),0.0f);
                        }
                        if (hasSigns) {
                            // hash estable por fachada: nombre del local, grafiti de la persiana y aviso de cartulina
                            const uint32_t h = static_cast<uint32_t>(x * 73856093) ^ static_cast<uint32_t>(z * 19349663) ^
                                               static_cast<uint32_t>((sideIndex + 1) * 83492791);
                            appendSignQuad(wallCenter + glm::vec3(0, 2.55f, 0) + normal * (storefrontOffset + 0.026f), normal,
                                           1.72f, 0.43f, m_SignAtlas.Shop(h % SignAtlas::kShopSigns), 0.0f, 0.0f);
                            if ((h >> 5) % 100 < 75) {
                                const float tilt = (static_cast<float>((h >> 9) % 100) / 100.0f - 0.5f) * 0.3f;
                                appendSignQuad(wallCenter + glm::vec3(0, 0.95f, 0) + normal * (storefrontOffset + 0.04f), normal,
                                               1.30f, 0.33f, m_SignAtlas.Tag((h >> 3) % SignAtlas::kTags), tilt, 0.0f);
                            }
                            if ((h >> 13) % 100 < 55) {
                                const float shift = (static_cast<float>((h >> 17) % 100) / 100.0f - 0.5f) * 0.7f;
                                appendSignQuad(wallCenter + glm::vec3(0, 1.80f, 0) + normal * (storefrontOffset + 0.045f), normal,
                                               0.72f, 0.18f, m_SignAtlas.Note((h >> 7) % SignAtlas::kNotes), 0.0f, shift);
                            }
                        } else {
                            const std::array<glm::vec4, 4> signColors{{
                                {0.88f,0.13f,0.12f,1.0f}, {0.91f,0.72f,0.12f,1.0f},
                                {0.10f,0.48f,0.27f,1.0f}, {0.16f,0.35f,0.72f,1.0f}
                            }};
                            const size_t signVariant = static_cast<size_t>((x * 7 + z * 11) % static_cast<int>(signColors.size()));
                            appendFacadePanel(wallCenter+glm::vec3(0,2.55f,0)+normal*(storefrontOffset+0.026f),normal,1.72f,0.42f,
                                              signColors[signVariant],0.0f);
                        }
                    }
                }
            } else if (type != TileType::Wall) {
                glm::vec4 col = getColorForTile(type);
                float y = (type == TileType::Sidewalk || type == TileType::Plaza) ? 0.15f : 0.0f;
                auto* vertices = (type == TileType::Sidewalk || type == TileType::Plaza) ? &sidewalkVertices : &parkVertices;
                auto* indices = (type == TileType::Sidewalk || type == TileType::Plaza) ? &sidewalkIndices : &parkIndices;
                if (type == TileType::Road || type == TileType::Busway) { vertices = &groundVertices; indices = &groundIndices; }
                uint32_t base = static_cast<uint32_t>(vertices->size());
                vertices->push_back({{x0, y, z1}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, col});
                vertices->push_back({{x1, y, z1}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, col});
                vertices->push_back({{x1, y, z0}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, col});
                vertices->push_back({{x0, y, z0}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, col});
                indices->insert(indices->end(), {base,base+1,base+2,base+2,base+3,base});
            }
        }
    }

    if (!groundVertices.empty()) {
        m_GroundMesh = std::make_shared<Mesh>(groundVertices, groundIndices);
    }
    if (!sidewalkVertices.empty()) m_SidewalkMesh = std::make_shared<Mesh>(sidewalkVertices, sidewalkIndices);
    if (!parkVertices.empty()) m_NatureMesh = std::make_shared<Mesh>(parkVertices, parkIndices);
    const auto worldX = [&](float tile) { return (tile + 0.5f) * TILE_SIZE - halfWidth; };
    const auto worldZ = [&](float tile) { return (tile + 0.5f) * TILE_SIZE - halfHeight; };
    const auto addMarking = [&](float x, float z, float width, float depth, const glm::vec4& color) {
        const uint32_t base = static_cast<uint32_t>(markingVertices.size());
        markingVertices.push_back({{x-width*0.5f,0.025f,z+depth*0.5f},{0,1,0},{0,1},color});
        markingVertices.push_back({{x+width*0.5f,0.025f,z+depth*0.5f},{0,1,0},{1,1},color});
        markingVertices.push_back({{x+width*0.5f,0.025f,z-depth*0.5f},{0,1,0},{1,0},color});
        markingVertices.push_back({{x-width*0.5f,0.025f,z-depth*0.5f},{0,1,0},{0,0},color});
        markingIndices.insert(markingIndices.end(), {base,base+1,base+2,base+2,base+3,base});
    };
    const std::array<std::pair<int,int>, 6> carreras{{{18,23},{36,41},{53,56},{63,66},{84,89},{106,111}}};
    const std::array<std::pair<int,int>, 5> crucesCarrera{{{18,23},{36,41},{53,66},{84,89},{106,111}}};
    const std::array<std::pair<int,int>, 5> calles{{{16,21},{38,43},{60,65},{72,77},{94,99}}};
    const glm::vec4 yellow(0.94f,0.76f,0.20f,1.0f);
    const glm::vec4 white(0.94f,0.94f,0.90f,1.0f);
    for (const auto& [z0,z1] : calles) {
        const float centerZ = (worldZ(z0) + worldZ(z1)) * 0.5f;
        for (float x=worldX(2); x<worldX(WIDTH-2); x+=6.0f) addMarking(x,centerZ,1.8f,0.12f,yellow);
    }
    for (const auto& [x0,x1] : carreras) {
        const float centerX = (worldX(x0) + worldX(x1)) * 0.5f;
        for (float z=worldZ(2); z<worldZ(HEIGHT-2); z+=6.0f) addMarking(centerX,z,0.12f,1.8f,yellow);
    }
    for (const auto& [za,zb] : calles) for (const auto& [xa,xb] : crucesCarrera) {
        for (float edgeZ : {worldZ(za)-3.0f, worldZ(zb)+3.0f})
            for (float x=worldX(xa)-1.0f; x<worldX(xb)+1.0f; x+=1.8f) addMarking(x,edgeZ,0.52f,3.6f,white);
        for (float edgeX : {worldX(xa)-3.0f, worldX(xb)+3.0f})
            for (float z=worldZ(za)-1.0f; z<worldZ(zb)+1.0f; z+=1.8f) addMarking(edgeX,z,3.6f,0.52f,white);
    }
    if (!markingVertices.empty()) m_MarkingMesh = std::make_shared<Mesh>(markingVertices, markingIndices);
    const auto appendFace = [&](const glm::vec3& center, const glm::vec3& halfSize, const glm::vec4& color,
                                const glm::vec3& normal, const std::array<glm::vec3, 4>& corners) {
        const uint32_t base = static_cast<uint32_t>(buildingVertices.size());
        for (size_t i = 0; i < corners.size(); ++i) {
            const glm::vec3 position = center + corners[i] * halfSize;
            const glm::vec2 uv = normal.y != 0.0f ? glm::vec2(position.x * 0.25f, position.z * 0.25f)
                : glm::vec2((normal.x != 0.0f ? position.z : position.x) * 0.8f, position.y * 0.8f);
            buildingVertices.push_back({position, normal, uv, color});
        }
        buildingIndices.insert(buildingIndices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});
    };
    const auto appendBox = [&](const glm::vec3& center, const glm::vec3& size, const glm::vec4& color) {
        const glm::vec3 half = size * 0.5f;
        appendFace(center, half, color, {0, 0, 1}, {{{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}}});
        appendFace(center, half, color, {0, 0,-1}, {{{1,-1,-1},{-1,-1,-1},{-1,1,-1},{1,1,-1}}});
        appendFace(center, half, color, {-1,0,0}, {{{-1,-1,-1},{-1,-1,1},{-1,1,1},{-1,1,-1}}});
        appendFace(center, half, color, {1,0,0}, {{{1,-1,1},{1,-1,-1},{1,1,-1},{1,1,1}}});
        appendFace(center, half, color, {0,1,0}, {{{-1,1,1},{1,1,1},{1,1,-1},{-1,1,-1}}});
        appendFace(center, half, color, {0,-1,0}, {{{-1,-1,-1},{1,-1,-1},{1,-1,1},{-1,-1,1}}});
    };
    for (const auto& building : m_Buildings) {
        if (building.modelVariant >= 0) continue;
        if (building.urbanDesign >= 0) {
            const float width = building.size.x;
            const float depth = building.size.z;
            const float height = building.size.y;
            const bool steppedTower = building.urbanDesign == 3 && building.floors >= 8;
            const float lowerHeight = steppedTower ? height - 6.4f : height;
            appendBox({building.position.x, lowerHeight * 0.5f, building.position.z},
                      {width, lowerHeight, depth}, building.color);
            if (steppedTower) {
                appendBox({building.position.x, lowerHeight + 3.2f, building.position.z},
                          {width * 0.76f, 6.4f, depth * 0.76f},
                          glm::vec4(building.color.r * 0.92f, building.color.g * 0.94f,
                                    building.color.b * 0.96f, 1.0f));
            }

            const int firstTileX = static_cast<int>(std::floor(
                (building.position.x - width * 0.5f + WIDTH) / TILE_SIZE));
            const int firstTileZ = static_cast<int>(std::floor(
                (building.position.z - depth * 0.5f + HEIGHT) / TILE_SIZE));
            const int tilesX = static_cast<int>(std::round(width / TILE_SIZE));
            const int tilesZ = static_cast<int>(std::round(depth / TILE_SIZE));
            const int centerTileX = firstTileX + tilesX / 2;
            const int centerTileZ = firstTileZ + tilesZ / 2;
            const std::array<std::pair<TileType, glm::vec3>, 4> sides{{
                {GetTileAt(centerTileX, firstTileZ - 1), {0,0,-1}},
                {GetTileAt(centerTileX, firstTileZ + tilesZ), {0,0,1}},
                {GetTileAt(firstTileX - 1, centerTileZ), {-1,0,0}},
                {GetTileAt(firstTileX + tilesX, centerTileZ), {1,0,0}}
            }};
            const auto streetFacing = [](TileType type) {
                return type == TileType::Sidewalk || type == TileType::Road || type == TileType::Busway;
            };
            for (size_t sideIndex = 0; sideIndex < sides.size(); ++sideIndex) {
                const auto& [neighbor, normal] = sides[sideIndex];
                if (!streetFacing(neighbor)) continue;
                const bool alongX = normal.z != 0.0f;
                const float facadeWidth = alongX ? width : depth;
                glm::vec3 wallCenter = building.position + normal * ((alongX ? depth : width) * 0.5f + 0.018f);
                wallCenter.y = 0.0f;
                const float floorHeight = 3.2f;
                const int windowCount = std::max(1, static_cast<int>(std::floor(facadeWidth / 3.2f)));
                const float spacing = facadeWidth / static_cast<float>(windowCount);
                const bool bandedConcrete = building.urbanDesign == 1;
                for (int floor = 0; floor < building.floors; ++floor) {
                        const float baseY = static_cast<float>(floor) * floorHeight;
                        const bool groundFloor = floor == 0;
                        if (groundFloor) {
                            const float storefrontOffset = 0.035f;
                            appendFacadePanel(wallCenter + glm::vec3(0.0f, 1.35f, 0.0f) + normal * storefrontOffset,
                                              normal, std::min(facadeWidth * 0.72f, 4.2f), 2.25f,
                                              {0.13f, 0.20f, 0.24f, 1.0f}, 0.0f);
                            appendFacadePanel(wallCenter + glm::vec3(0.0f, 2.58f, 0.0f) + normal * (storefrontOffset + 0.025f),
                                              normal, std::min(facadeWidth * 0.78f, 4.8f), 0.38f,
                                              {0.25f, 0.29f, 0.30f, 1.0f}, 0.0f);
                        }
                        for (int window = 0; window < windowCount; ++window) {
                            const float shift = (static_cast<float>(window) + 0.5f) * spacing - facadeWidth * 0.5f;
                            const glm::vec3 paneCenter = wallCenter + glm::vec3(0.0f, baseY + 1.85f, 0.0f)
                                + normal * 0.025f
                                + (alongX ? glm::vec3(shift, 0.0f, 0.0f) : glm::vec3(0.0f, 0.0f, shift));
                            const bool brickBalcony = building.urbanDesign == 0;
                        const bool verticalFrame = building.urbanDesign == 2;
                        const bool glassCorner = building.urbanDesign == 4;
                        const glm::vec4 glassColor = glassCorner
                            ? glm::vec4(0.09f, 0.18f, 0.23f, 1.0f)
                            : glm::vec4(0.10f, 0.17f, 0.22f, 1.0f);
                        const float paneWidth = std::min(spacing * (glassCorner ? 0.88f : 0.68f), 2.2f);
                        appendFacadePanel(paneCenter, normal, paneWidth + 0.18f, 1.62f,
                                          {0.82f, 0.76f, 0.66f, 1.0f}, 0.0f);
                        appendFacadePanel(paneCenter + normal * 0.016f, normal, paneWidth, 1.43f,
                                          glassColor, 0.0f);
                        if (brickBalcony) {
                            const float balconyY = baseY + 1.12f;
                            appendFacadePanel(paneCenter + glm::vec3(0.0f, balconyY - (baseY + 1.85f), 0.0f)
                                                  + normal * 0.11f,
                                              normal, std::min(spacing * 0.84f, 2.6f), 0.12f,
                                              {0.48f, 0.43f, 0.37f, 1.0f}, 0.0f);
                            appendFacadePanel(paneCenter + glm::vec3(0.0f, balconyY + 0.43f - (baseY + 1.85f), 0.0f)
                                                  + normal * 0.12f,
                                              normal, std::min(spacing * 0.78f, 2.4f), 0.07f,
                                              {0.72f, 0.70f, 0.65f, 1.0f}, 0.0f);
                        } else if (verticalFrame) {
                            for (float edge : {-0.5f, 0.5f}) {
                                appendFacadePanel(paneCenter + (alongX ? glm::vec3(edge * spacing, 0.0f, 0.0f)
                                                                        : glm::vec3(0.0f, 0.0f, edge * spacing))
                                                      + normal * 0.035f,
                                                  normal, 0.13f, 2.9f, {0.84f, 0.80f, 0.72f, 1.0f}, 0.0f);
                            }
                        }
                    }
                    if (bandedConcrete || floor > 0) {
                        appendFacadePanel(wallCenter + glm::vec3(0.0f, baseY + 0.08f, 0.0f) + normal * 0.045f,
                                          normal, facadeWidth - 0.12f, bandedConcrete ? 0.18f : 0.09f,
                                          bandedConcrete ? glm::vec4(0.79f, 0.79f, 0.76f, 1.0f)
                                                         : glm::vec4(0.54f, 0.54f, 0.52f, 1.0f),
                                          0.0f);
                    }
                }
                if (building.urbanDesign == 4) {
                    appendFacadePanel(wallCenter + glm::vec3(0.0f, height * 0.5f, 0.0f) + normal * 0.055f,
                                      normal, 0.18f, height - 0.4f, {0.78f, 0.82f, 0.82f, 1.0f}, 0.0f);
                }
            }
            continue;
        }
        appendBox(building.position, building.size, building.color);
    }
    if (!buildingVertices.empty()) m_BuildingMesh = std::make_shared<Mesh>(buildingVertices, buildingIndices);
    if (!detailVertices.empty()) m_FacadeDetailMesh = std::make_shared<Mesh>(detailVertices, detailIndices);
    if (!signVertices.empty()) m_SignMesh = std::make_shared<Mesh>(signVertices, signIndices);
}

void CityLayout::Render(Renderer& renderer, const glm::vec3& focusPosition) {
    if (m_GroundMesh) {
        m_AsphaltTexture->BindUnit(0);
        renderer.Submit(*m_GroundMesh, glm::mat4(1.0f), glm::vec4(1.0f), true);
    }
    if (m_SidewalkMesh) {
        m_SidewalkTexture->BindUnit(0);
        renderer.Submit(*m_SidewalkMesh, glm::mat4(1.0f), glm::vec4(1.0f), true);
    }
    if (m_NatureMesh) {
        renderer.Submit(*m_NatureMesh, glm::mat4(1.0f), glm::vec4(1.0f));
    }

    if (m_MarkingMesh) renderer.Submit(*m_MarkingMesh, glm::mat4(1.0f));
    if (m_BuildingMesh) {
        m_BrickTexture->BindUnit(0);
        renderer.Submit(*m_BuildingMesh, glm::mat4(1.0f), glm::vec4(1.0f), true);
    }
    if (m_FacadeDetailMesh) renderer.Submit(*m_FacadeDetailMesh, glm::mat4(1.0f));
    if (m_SignMesh && m_SignAtlas.GetTexture()) {
        m_SignAtlas.GetTexture()->BindUnit(0);
        renderer.Submit(*m_SignMesh, glm::mat4(1.0f), glm::vec4(1.0f), true, false, nullptr, 2); // mezcla alfa: grafitis con fondo transparente
    }
    for (const auto& building : m_Buildings) {
        if (building.modelVariant < 0) continue;
        const glm::vec2 offset(building.position.x - focusPosition.x,
                               building.position.z - focusPosition.z);
        if (glm::dot(offset, offset) > 85.0f * 85.0f) continue;
        const glm::vec3 boundsCenter = building.position;
        const float groundY = boundsCenter.y - building.size.y * 0.5f;
        const glm::vec3 targetFootprint(
            std::max(0.5f, building.size.x - 1.0f),
            building.size.y,
            std::max(0.5f, building.size.z - 1.0f));
        const auto fittedTransform = [&](const Model& model, const glm::vec3& position,
                                         const glm::vec3& targetSize) {
            const glm::vec3 extent = model.GetBoundsMaximum() - model.GetBoundsMinimum();
            if (glm::any(glm::lessThanEqual(extent, glm::vec3(0.0001f)))) return glm::mat4(1.0f);
            return glm::translate(glm::mat4(1.0f), position)
                * glm::scale(glm::mat4(1.0f), targetSize / extent)
                * glm::translate(glm::mat4(1.0f),
                    {-((model.GetBoundsMinimum().x + model.GetBoundsMaximum().x) * 0.5f),
                     -model.GetBoundsMinimum().y,
                     -((model.GetBoundsMinimum().z + model.GetBoundsMaximum().z) * 0.5f)});
        };
        if (building.modelVariant == 1) {
            const float bottomHeight = building.size.y * 0.25f;
            const float middleHeight = building.size.y * 0.5f;
            const float topHeight = building.size.y - bottomHeight - middleHeight;
            const std::array<float, 3> sectionHeights{{bottomHeight, middleHeight, topHeight}};
            float sectionY = groundY;
            for (size_t section = 0; section < m_ModularBuildingModels.size(); ++section) {
                const float height = sectionHeights[section];
                const glm::vec3 targetSize(targetFootprint.x, height, targetFootprint.z);
                m_ModularBuildingModels[section]->Draw(
                    renderer, fittedTransform(*m_ModularBuildingModels[section],
                                              {boundsCenter.x, sectionY, boundsCenter.z}, targetSize));
                sectionY += height;
            }
        } else {
            const auto& model = m_BuildingModels[building.modelVariant == 0 ? 0 : 1];
            if (!model) continue;
            model->Draw(renderer, fittedTransform(*model,
                                                   {boundsCenter.x, groundY, boundsCenter.z},
                                                   targetFootprint));
        }
    }
}

} // namespace Bazuko
