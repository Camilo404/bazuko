#pragma once
// Estacion Portal del Centro (TransitTerminal.js del original): isla accesible de 4 m entre los dos carriles del
// troncal al norte del mapa, con una rampa peatonal de 8 m en cada extremo. El bus del norte para en el carril este
// (X 78,5) y el del sur en el oeste (X 71,5).
#include <cmath>

namespace Bazuko::Station {

constexpr float kCenterX = 75.0f;
constexpr float kCenterZ = -30.0f;        // el bus se detiene con su centro aqui
constexpr float kPlatformLength = 40.0f;
constexpr float kRampLength = 8.0f;
constexpr float kHalfWidth = 2.0f;        // isla de 4 m
constexpr float kPlatformHeight = 0.92f;
constexpr float kNorthEnd = kCenterZ - kPlatformLength * 0.5f - kRampLength;   // -58
constexpr float kSouthEnd = kCenterZ + kPlatformLength * 0.5f + kRampLength;   // -2
constexpr float kNorthLaneX = 78.5f;
constexpr float kSouthLaneX = 71.5f;
constexpr float kTerminalNorthZ = -74.0f;

// Cota del suelo sobre la isla: rampa norte, plataforma y rampa sur; fuera de ella, la calzada
inline float Height(float x, float z) {
    if (std::fabs(x - kCenterX) > kHalfWidth || z <= kNorthEnd || z >= kSouthEnd) return 0.0f;
    if (z < kNorthEnd + kRampLength) return kPlatformHeight * (z - kNorthEnd) / kRampLength;
    if (z > kSouthEnd - kRampLength) return kPlatformHeight * (kSouthEnd - z) / kRampLength;
    return kPlatformHeight;
}

} // namespace Bazuko::Station
