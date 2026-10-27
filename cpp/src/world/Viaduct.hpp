#pragma once
// Viaducto elevado de la troncal y puente del lago (ColpatriaViaduct.js, LakePark.js, BridgeElevation.js y Colpatria.js
// del original). Un unico tablero de 14 m recorre el eje del troncal: una rampa norte sube a 7,82 m despues de la
// calle 10, el tablero sigue a esa cota (rodeando la torre Colpatria con una curva de 44 m hacia el este) y cruza el
// lago, y una rampa sur lo baja hasta la orilla. Todas las cotas y el trazado son los del original, en su marco de
// coordenadas. Solo logica de alturas y capas: sin dependencias del motor, para poder probarla aparte.
#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>
#include <glm/glm.hpp>

namespace Bazuko::Viaduct {

constexpr float kBase = 0.02f;           // cota de la calzada
constexpr float kRise = 7.8f;            // altura del tablero sobre la calzada (7,82 m en total)
constexpr float kHalfWidth = 7.0f;       // el tablero mide 14 m
constexpr float kWalkHalfWidth = 6.45f;  // las barandas dejan 6,45 m de paso a cada lado del eje
constexpr float kThickness = 0.71f;      // canto del tablero (0,06 de losa + 0,65 de concreto)
constexpr float kStartZ = 15.43f;        // el tablero empieza a subir aqui (antes de la calle 10)
constexpr float kCrestStartZ = 39.0f;    // y llega a la cota maxima aqui
constexpr float kRampLength = kCrestStartZ - kStartZ;
constexpr float kJoinZ = 220.0f;         // desde aqui sigue el perfil del puente del lago
constexpr float kCrestEndZ = 280.0f;     // aqui empieza el descenso hacia la orilla sur
constexpr float kEndZ = 318.0f;          // el tablero vuelve a la calzada en el borde sur del mapa
constexpr float kBoardHeight = 0.55f;    // desde el suelo solo se sube donde la rampa esta casi a ras
constexpr float kHeadroom = 2.0f;        // bajo la rampa baja no se puede pasar (la cabeza no cabe)
constexpr float kElevated = 1.6f;        // por encima de esta cota algo esta sobre la calle
constexpr float kAxisX = 75.0f;          // eje recto de la troncal (centro del carril exclusivo)

// ---- trazado: el eje se desplaza hacia el este para rodear Colpatria (Colpatria.js) ----
constexpr float kDetourStart = 80.0f, kDetourEnd = 260.0f, kDetourOffset = 44.0f;

inline float BridgeCenter(float z) {
    const float t = std::clamp(z < 154.0f ? (z - kDetourStart) / (154.0f - kDetourStart) : (kDetourEnd - z) / (kDetourEnd - 182.0f), 0.0f, 1.0f);
    const float ease = t * t * t * (10.0f + t * (-15.0f + 6.0f * t));
    return kAxisX + kDetourOffset * ease;
}

// Normal del eje en el plano (x, z): x = 1/escala, z = -dx/escala
inline glm::vec2 BridgeNormal(float z) {
    const float dx = (BridgeCenter(z + 0.01f) - BridgeCenter(z - 0.01f)) / 0.02f;
    const float scale = std::hypot(1.0f, dx);
    return {1.0f / scale, -dx / scale};
}

// X del carril `laneX` (78,5 hacia el norte, 71,5 hacia el sur) a la altura Z: desplazamiento perpendicular del eje
inline float BusPathX(float z, float laneX = 78.5f) {
    if (z <= kDetourStart - 5.0f || z >= kDetourEnd + 5.0f) return laneX;
    const float offset = laneX - kAxisX;
    float lo = z - 10.0f, hi = z + 10.0f;
    for (int i = 0; i < 28; ++i) {
        const float t = (lo + hi) * 0.5f;
        if (t + BridgeNormal(t).y * offset < z) lo = t; else hi = t;
    }
    const float t = (lo + hi) * 0.5f;
    return BridgeCenter(t) + BridgeNormal(t).x * offset;
}

// Direccion de la marcha (x, z) del carril: `direction` -1 hacia el norte y +1 hacia el sur
inline glm::vec2 BusHeading(float z, float laneX, float direction) {
    const float dx = BusPathX(z + direction * 0.01f, laneX) - BusPathX(z - direction * 0.01f, laneX);
    const float scale = std::hypot(dx, 0.02f);
    return {dx / scale, direction * 0.02f / scale};
}

// Borde izquierdo (-1) o derecho (+1) del tablero a la altura Z del eje: (x, z)
inline glm::vec2 Edge(float z, float side) {
    const glm::vec2 normal = BridgeNormal(z);
    const float offset = side * kHalfWidth;
    return {BridgeCenter(z) + normal.x * offset, z + normal.y * offset};
}

// ---- alturas ----
inline float LakeBridgeHeight(float z) {
    constexpr float z0 = 220.0f;
    if (z < z0 - 1.0f || z >= kEndZ) return kBase;
    const float t = z > kCrestEndZ ? (kEndZ - z) / (kEndZ - kCrestEndZ) : 1.0f;
    return kBase + kRise * (0.5f - 0.5f * std::cos(3.14159265f * t));
}

// Cota del tablero a lo largo del eje: rampa norte con transiciones suaves, cresta y descenso en coseno
inline float Height(float z) {
    if (z <= kStartZ) return kBase;
    if (z >= kJoinZ) return LakeBridgeHeight(z);
    if (z >= kCrestStartZ) return kBase + kRise;
    constexpr float ease = 1.0f;
    const float distance = z - kStartZ;
    const float grade = kRise / (kRampLength - ease);
    const auto eased = [&](float d) { return grade * (d / 2.0f - ease * std::sin(3.14159265f * d / ease) / (2.0f * 3.14159265f)); };
    if (distance < ease) return kBase + eased(distance);
    if (distance > kRampLength - ease) return kBase + kRise - eased(kRampLength - distance);
    return kBase + grade * (distance - ease / 2.0f);
}

inline float Slope(float z) { return Height(z + 0.5f) - Height(z - 0.5f); }   // metros de subida por metro de recorrido

// Distancia lateral con signo de (x, z) al eje del tablero y la Z del eje que le corresponde
inline float Lateral(float x, float z, float* axisZ = nullptr) {
    float axis = z, distance = 0.0f;
    for (int i = 0; i < 4; ++i) {
        const glm::vec2 normal = BridgeNormal(axis);
        distance = (x - BridgeCenter(axis)) / normal.x;
        axis = z - normal.y * distance;
    }
    if (axisZ) *axisZ = axis;
    return distance;
}

inline bool Covers(float x, float z) {
    if (z <= kStartZ - 8.0f || z >= kEndZ + 8.0f) return false;
    float axis = z;
    const float lateral = Lateral(x, z, &axis);
    return std::fabs(lateral) <= kHalfWidth && axis > kStartZ && axis < kEndZ;
}

// Altura del tablero bajo (x, z) (la de su punto del eje)
inline float DeckHeightAt(float x, float z) {
    float axis = z;
    Lateral(x, z, &axis);
    return Height(axis);
}

// Capa del que camina: `onDeck` es true mientras esta sobre el tablero. Devuelve la nueva capa y fija la altura `y`.
//  - sobre el tablero: sigue su cota hasta salir por un extremo, donde vuelve a la calle;
//  - en la calle: solo sube si entra por un extremo donde la rampa esta a ras; en cualquier otro punto pasa por debajo.
inline bool StepLayer(bool onDeck, float x, float z, float& y) {
    if (onDeck) {
        if (!Covers(x, z)) { y = 0.0f; return false; }
        y = DeckHeightAt(x, z);
        return true;
    }
    if (Covers(x, z) && DeckHeightAt(x, z) <= kBoardHeight) { y = DeckHeightAt(x, z); return true; }
    y = 0.0f;
    return false;
}

// En la calle, el tramo bajo de la rampa es un muro: no se sube desde el costado ni se pasa por debajo
inline bool BlockedOnGround(float x, float z) {
    if (!Covers(x, z)) return false;
    const float height = DeckHeightAt(x, z);
    return height > kBoardHeight && height < kHeadroom;
}

// Sobre el tablero solo se camina entre las barandas
inline bool BlockedOnDeck(float x, float z, float radius) { return std::fabs(Lateral(x, z)) + radius > kWalkHalfWidth; }

// Pilares bajo el tablero (x, z) como en el original: cada 14 m desde Z 50, 4 m al oeste del eje y fuera de las calzadas
inline std::vector<glm::vec2> PillarPositions(const std::function<bool(float, float)>& isRoad) {
    std::vector<glm::vec2> pillars;
    for (float z = 50.0f; z < kEndZ - 8.0f; z += 14.0f) {
        const float x = BusPathX(z) - 4.0f;
        if (isRoad(x, z)) continue;
        pillars.emplace_back(x, z);
    }
    return pillars;
}

} // namespace Bazuko::Viaduct
