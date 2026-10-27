#pragma once

#include "../renderer/Mesh.hpp"
#include "../renderer/Renderer.hpp"
#include "CharacterTypes.hpp"
#include <array>
#include <memory>

namespace Bazuko {

class Ragdoll {
public:
    Ragdoll();

    // Superficie donde cae: altura del suelo y, si hay barandas (tablero del viaducto), el semiancho transitable (0 = sin limite)
    void SetSurface(float groundY, float halfWidth = 0.0f) { m_GroundY = groundY; m_HalfWidth = halfWidth; }
    void Begin(const glm::vec3& position, float yaw, const glm::vec3& impactVelocity,
               const CharacterLook& look);
    void Update(float dt);
    void Draw(Renderer& renderer) const;
    glm::vec3 GetPlayerPosition() const;
    glm::mat4 GetCharacterTransform(float yaw, float recovery = 0.0f) const;

private:
    struct Constraint {
        size_t first = 0;
        size_t second = 0;
        float length = 0.0f;
    };

    std::array<glm::vec3, 16> m_Positions{};
    std::array<glm::vec3, 16> m_Velocities{};
    std::array<float, 16> m_FloorHeights{};
    std::array<Constraint, 15> m_Constraints{};
    std::shared_ptr<Mesh> m_BoneMesh;
    std::shared_ptr<Mesh> m_HeadMesh;
    std::shared_ptr<Mesh> m_JointMesh;
    glm::vec4 m_SkinColor{1.0f};
    glm::vec4 m_ClothesColor{1.0f};
    glm::vec4 m_ShoeColor{1.0f};
    float m_GroundY = 0.0f;
    float m_HalfWidth = 0.0f;
    bool m_Active = false;
};

} // namespace Bazuko
