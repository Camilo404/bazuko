// Pruebas del botin de las bolsas de basura (logica pura: sin ventana ni GPU).
#include "game/Harvest.hpp"

#include <cmath>
#include <cstdio>
#include <map>

using namespace Bazuko;

static int g_failures = 0;
#define CHECK(cond, ...)                                                        \
    do {                                                                        \
        if (!(cond)) {                                                          \
            ++g_failures;                                                       \
            std::printf("FALLO (%s:%d): %s -> ", __FILE__, __LINE__, #cond);    \
            std::printf(__VA_ARGS__);                                           \
            std::printf("\n");                                                  \
        }                                                                       \
    } while (0)

// Frecuencia de objeto raro y numero de objetos comunes de `trials` recolecciones
static void Measure(const GameData& data, const HarvestParams& params, int trials, double& rareRate, double& commonAverage) {
    std::mt19937 random(12345);
    int rares = 0;
    long commons = 0;
    const auto fromRareTable = [&](const std::string& id) {
        for (const LootEntry& entry : data.lootRare) if (entry.item == id) return true;
        return false;
    };
    for (int trial = 0; trial < trials; ++trial) {
        const HarvestResult result = RollHarvest(data, params, random);
        int rareRolls = 0;
        for (const std::string& id : result.items) rareRolls += fromRareTable(id) ? 1 : 0;
        CHECK(rareRolls <= 1, "mas de un objeto de la tabla rara");
        rares += rareRolls;
        commons += static_cast<long>(result.items.size()) - rareRolls;
        CHECK(result.items.empty() || result.rare.empty() || data.FindItem(result.rare) != nullptr, "raro desconocido");
    }
    rareRate = static_cast<double>(rares) / trials;
    commonAverage = static_cast<double>(commons) / trials;
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::string error;
    const auto loaded = GameData::LoadFromFile(BAZUKO_GAME_DATA_PATH, &error);
    if (!loaded) {
        std::printf("no cargo: %s\n", error.c_str());
        return 1;
    }
    const GameData& data = *loaded;
    double rare = 0, commons = 0;
    constexpr int kTrials = 40000;

    Measure(data, {false, false, 1.0f, 0}, kTrials, rare, commons);
    std::printf("sin clasificar: %.3f comunes, raro %.3f\n", commons, rare);
    CHECK(std::fabs(commons - 2.0) < 1e-9 && std::fabs(rare - 0.05) < 0.006, "2 tiradas y 5 %% de raro");

    Measure(data, {false, true, 1.0f, 0}, kTrials, rare, commons);
    std::printf("clasificada: %.3f comunes, raro %.3f\n", commons, rare);
    CHECK(std::fabs(commons - 4.0) < 1e-9 && std::fabs(rare - 0.12) < 0.008, "4 tiradas y 12 %% de raro");

    Measure(data, {true, true, 1.0f, 0}, kTrials, rare, commons);
    CHECK(std::fabs(commons - 6.0) < 1e-9 && std::fabs(rare - 0.25) < 0.010, "adrenalina clasificada: 6 tiradas y 25 %% (%.3f)", rare);
    Measure(data, {true, false, 1.0f, 0}, kTrials, rare, commons);
    CHECK(std::fabs(commons - 4.0) < 1e-9 && std::fabs(rare - 0.15) < 0.009, "adrenalina sin clasificar: 4 tiradas y 15 %% (%.3f)", rare);

    Measure(data, {false, true, 1.25f, 1}, kTrials, rare, commons);
    std::printf("Ojo de Aguila + Reciclador: %.3f comunes, raro %.3f\n", commons, rare);
    CHECK(std::fabs(commons - 5.0) < 1e-9 && std::fabs(rare - 0.15) < 0.009, "extra tirada y rareza x1.25");

    // Reparto de la tabla comun: la lata pesa 33 de 105.8
    {
        std::mt19937 random(7);
        std::map<std::string, int> counts;
        for (int roll = 0; roll < 60000; ++roll) ++counts[PickWeighted(data.lootCommon, random)];
        float total = 0;
        for (const LootEntry& entry : data.lootCommon) total += entry.weight;
        for (const LootEntry& entry : data.lootCommon) {
            const double expected = entry.weight / total;
            const double actual = static_cast<double>(counts[entry.item]) / 60000.0;
            CHECK(std::fabs(actual - expected) < 0.01, "%s: esperado %.3f, salio %.3f", entry.item.c_str(), expected, actual);
        }
    }

    // El botin garantizado va primero y un material raro garantizado cuenta como hallazgo
    {
        std::mt19937 random(3);
        const HarvestResult result = RollHarvest(data, {}, random, {"colchon", "carton"});
        CHECK(result.items.size() >= 4 && result.items[0] == "colchon" && result.items[1] == "carton", "garantizado primero");
        CHECK(result.rare == "colchon", "colchon (muy raro) deberia contar como hallazgo, salio '%s'", result.rare.c_str());
    }

    // Reserva del cambuche: todos los materiales, repartidos en lotes
    {
        std::mt19937 random(5);
        const auto bags = DistributeShelterLoot(data, 28, random);
        std::map<std::string, int> totals;
        size_t withLoot = 0;
        for (const auto& bag : bags) {
            if (!bag.empty()) ++withLoot;
            CHECK(bag.size() <= 2, "una bolsa lleva %zu materiales", bag.size());
            for (const std::string& id : bag) ++totals[id];
        }
        std::map<std::string, int> expected;
        for (const CambuchePieceDef& piece : data.cambuche)
            for (const auto& [item, quantity] : piece.req) expected[item] += quantity;
        CHECK(totals == expected, "los materiales repartidos no suman lo que pide el cambuche");
        CHECK(expected["carton"] == 8 && expected["madera"] == 8 && expected["lata"] == 6 && expected["lona"] == 1
                  && expected["colchon"] == 1 && expected["vela"] == 1,
              "requisitos del cambuche");
        std::printf("reserva del cambuche: %zu materiales en %zu bolsas\n", static_cast<size_t>(25), withLoot);
        // Con pocas bolsas nada se pierde
        const auto few = DistributeShelterLoot(data, 3, random);
        std::map<std::string, int> fewTotals;
        for (const auto& bag : few) for (const std::string& id : bag) ++fewTotals[id];
        CHECK(fewTotals == expected, "con 3 bolsas tambien deben repartirse todos los materiales");
    }

    CHECK(std::fabs(InfectionChance(false, 1.0f) - 0.12f) < 1e-6f, "riesgo base");
    CHECK(std::fabs(InfectionChance(true, 1.0f) - 0.216f) < 1e-6f, "riesgo clasificando");
    CHECK(std::fabs(InfectionChance(true, 0.6f * 0.7f) - 0.216f * 0.42f) < 1e-6f, "riesgo con Piel dura y Desplazado");

    if (g_failures == 0) std::printf("Harvest: todo OK\n");
    else std::printf("Harvest: %d fallos\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
