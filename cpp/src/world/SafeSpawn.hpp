#pragma once
// Regla estricta: el jugador nunca aparece ni se carga dentro de un edificio, un modelo o un recinto cerrado
// donde quede bloqueado. Una posicion es segura si esta libre y desde ella se puede llegar a una zona abierta.
#include <functional>
#include <glm/glm.hpp>

namespace Bazuko {

// `isFree(x, z)`: la posicion es caminable y no esta bloqueada (mismas reglas que el movimiento del jugador).
using FreeCheck = std::function<bool(float, float)>;

// Desde `start` se alcanza una zona abierta (al menos `openRadius` metros de distancia y muchas celdas libres).
bool CanEscapeFrom(const glm::vec2& start, const FreeCheck& isFree, float openRadius = 25.0f);

// Devuelve `wanted` si es segura; si no, la posicion segura mas cercana (anillos crecientes de hasta `searchRadius`);
// si no hay ninguna, `fallback`. Conserva la altura de `wanted`.
glm::vec3 FindSafePlayerPosition(const glm::vec3& wanted, const FreeCheck& isFree, const glm::vec3& fallback,
                                 float searchRadius = 80.0f);

} // namespace Bazuko
