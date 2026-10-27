#pragma once
// Viaducto elevado de la troncal y puente del lago (ColpatriaViaduct.js, LakePark.js y BridgeElevation.js del original).
// Un unico tablero de 14 m de ancho recorre el eje del troncal (X 0): una rampa norte sube a 7,82 m despues de la
// primera calle, el tablero sigue a esa cota sobre el centro y el lago, y una rampa sur lo baja hasta la orilla.
// Solo logica de alturas y capas: sin dependencias del motor, para poder probarla aparte.
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
#include <glm/glm.hpp>

namespace Bazuko::Viaduct {

constexpr float kBase = 0.02f;           // cota de la calzada
constexpr float kRise = 7.8f;            // altura del tablero sobre la calzada (7,82 m en total)
constexpr float kHalfWidth = 7.0f;       // el tablero mide 14 m
constexpr float kWalkHalfWidth = 6.45f;  // las barandas dejan 6,45 m de paso a cada lado del eje
constexpr float kThickness = 0.8f;       // canto del tablero
constexpr float kStartZ = -66.0f;        // la rampa norte empieza justo despues de la estacion del bus (Z -67)
constexpr float kRampLength = 23.57f;
constexpr float kCrestStartZ = kStartZ + kRampLength;
constexpr float kCrestEndZ = 170.0f;     // aqui empieza el descenso hacia la orilla sur del lago
constexpr float kEndZ = 208.0f;          // el tablero vuelve a la calzada en el borde sur del mapa
constexpr float kBoardHeight = 0.55f;    // desde el suelo solo se sube donde la rampa esta casi a ras
constexpr float kHeadroom = 2.0f;        // bajo la rampa baja no se puede pasar (la cabeza no cabe)
constexpr float kElevated = 1.6f;        // por encima de esta cota algo esta "sobre" la calle

// Cota del tablero a lo largo del eje: rampa norte con transiciones suaves, cresta y descenso en coseno.
inline float Height(float z) {
    if (z <= kStartZ || z >= kEndZ) return kBase;
    if (z >= kCrestEndZ) {
        const float t = (kEndZ - z) / (kEndZ - kCrestEndZ);
        return kBase + kRise * (0.5f - 0.5f * std::cos(3.14159265f * t));
    }
    if (z >= kCrestStartZ) return kBase + kRise;
    constexpr float ease = 1.0f;
    const float distance = z - kStartZ;
    const float grade = kRise / (kRampLength - ease);
    const auto eased = [&](float d) { return grade * (d / 2.0f - ease * std::sin(3.14159265f * d / ease) / (2.0f * 3.14159265f)); };
    if (distance < ease) return kBase + eased(distance);
    if (distance > kRampLength - ease) return kBase + kRise - eased(kRampLength - distance);
    return kBase + grade * (distance - ease / 2.0f);
}

inline float Slope(float z) { return (Height(z + 0.5f) - Height(z - 0.5f)); }   // metros de subida por metro de recorrido

inline bool Covers(float x, float z) { return std::fabs(x) <= kHalfWidth && z > kStartZ && z < kEndZ; }

// Capa del que camina: `onDeck` es true mientras esta sobre el tablero. Devuelve la nueva capa y fija la altura `y`.
//  - sobre el tablero: sigue su cota hasta salir por un extremo, donde vuelve a la calle;
//  - en la calle: solo sube si entra por un extremo donde la rampa esta a ras; en cualquier otro punto pasa por debajo.
inline bool StepLayer(bool onDeck, float x, float z, float& y) {
    if (onDeck) {
        if (!Covers(x, z)) { y = 0.0f; return false; }
        y = Height(z);
        return true;
    }
    if (Covers(x, z) && Height(z) <= kBoardHeight) { y = Height(z); return true; }
    y = 0.0f;
    return false;
}

// En la calle, el tramo bajo de la rampa es un muro: no se sube desde el costado ni se pasa por debajo
inline bool BlockedOnGround(float x, float z) {
    if (!Covers(x, z)) return false;
    const float height = Height(z);
    return height > kBoardHeight && height < kHeadroom;
}

// Sobre el tablero solo se camina entre las barandas
inline bool BlockedOnDeck(float x, float /*z*/, float radius) { return std::fabs(x) + radius > kWalkHalfWidth; }

// Pilares bajo el tablero (posiciones X, Z): cada ~14 m y sin invadir las calles que lo cruzan.
// `streetBands` son las franjas de Z de las calles transversales.
inline std::vector<glm::vec2> PillarPositions(const std::vector<std::pair<float, float>>& streetBands) {
    std::vector<glm::vec2> pillars;
    float lastZ = -1.0e9f;
    for (float z = kCrestStartZ + 2.0f; z < kCrestEndZ + 12.0f; z += 1.0f) {
        if (z - lastZ < 14.0f) continue;
        const bool inStreet = std::any_of(streetBands.begin(), streetBands.end(),
            [&](const auto& band) { return z > band.first - 2.5f && z < band.second + 2.5f; });
        if (inStreet) continue;
        pillars.emplace_back(-4.5f, z);
        pillars.emplace_back(4.5f, z);
        lastZ = z;
    }
    return pillars;
}

} // namespace Bazuko::Viaduct
