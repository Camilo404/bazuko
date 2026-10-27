#pragma once
// Botin de las bolsas y canecas de basura (Pickup.harvest y ShelterLoot del juego web).
#include <random>
#include <string>
#include <vector>

#include "GameData.hpp"

namespace Bazuko {

inline constexpr float kSortSeconds = 1.5f;   // tiempo manteniendo Q para clasificar la basura

struct HarvestParams {
    bool boost = false;            // adrenalina
    bool sorted = false;           // se completo la clasificacion (Q mantenido)
    float rareChanceMult = 1.0f;   // talento Ojo de Aguila
    int extraRolls = 0;            // origen Reciclador
};

struct HarvestResult {
    std::vector<std::string> items;   // ids, empezando por el botin garantizado
    std::string rare;                 // id del objeto raro obtenido ("" si no hay)
};

// Elige un objeto de la tabla segun sus pesos.
std::string PickWeighted(const std::vector<LootEntry>& table, std::mt19937& random);

// Recolecta una bolsa: botin garantizado + tiradas comunes + posible objeto raro.
// Sin clasificar: 2 tiradas (4 con adrenalina) y 5 % de raro (15 %); clasificada: 4 (6) y 12 % (25 %).
HarvestResult RollHarvest(const GameData& data, const HarvestParams& params, std::mt19937& random,
                          const std::vector<std::string>& guaranteed = {});

// Materiales de un cambuche completo repartidos en `bagCount` bolsas: la reserva renovable que permite construirlo.
// Cartones, latas y tablas van en lotes de 2; lona, colchon y vela ocupan una bolsa cada uno.
std::vector<std::vector<std::string>> DistributeShelterLoot(const GameData& data, size_t bagCount, std::mt19937& random);

// Probabilidad de enfermar al rebuscar (12 %, x1.8 si se clasifico, por la resistencia de talentos y origen).
float InfectionChance(bool sorted, float resistance);

} // namespace Bazuko
