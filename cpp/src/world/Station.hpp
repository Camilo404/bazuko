#pragma once
// Estacion Portal del Centro (TransitTerminal.js del original): isla accesible de 4 m entre los dos carriles del
// troncal, con una rampa peatonal en cada extremo. El bus del norte para en el carril este y el del sur en el oeste.
#include <cmath>

namespace Bazuko::Station {

constexpr float kCenterX = 0.0f;
constexpr float kCenterZ = -90.0f;        // el bus se detiene con su centro aqui
constexpr float kPlatformLength = 24.0f;
constexpr float kRampLength = 6.0f;
constexpr float kHalfWidth = 2.0f;        // isla de 4 m
constexpr float kPlatformHeight = 0.92f;
constexpr float kNorthEnd = kCenterZ - kPlatformLength * 0.5f - kRampLength;   // -108
constexpr float kSouthEnd = kCenterZ + kPlatformLength * 0.5f + kRampLength;   // -72

// Cota del suelo sobre la isla: rampa norte, plataforma y rampa sur; fuera de ella, la calzada
inline float Height(float x, float z) {
    if (std::fabs(x - kCenterX) > kHalfWidth || z <= kNorthEnd || z >= kSouthEnd) return 0.0f;
    if (z < kNorthEnd + kRampLength) return kPlatformHeight * (z - kNorthEnd) / kRampLength;
    if (z > kSouthEnd - kRampLength) return kPlatformHeight * (kSouthEnd - z) / kRampLength;
    return kPlatformHeight;
}

} // namespace Bazuko::Station
