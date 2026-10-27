#include "Harvest.hpp"

#include <algorithm>
#include <map>

namespace Bazuko {

std::string PickWeighted(const std::vector<LootEntry>& table, std::mt19937& random) {
    if (table.empty()) return {};
    float total = 0.0f;
    for (const LootEntry& entry : table) total += entry.weight;
    float roll = std::uniform_real_distribution<float>(0.0f, total)(random);
    for (const LootEntry& entry : table) {
        roll -= entry.weight;
        if (roll <= 0.0f) return entry.item;
    }
    return table.front().item;
}

HarvestResult RollHarvest(const GameData& data, const HarvestParams& params, std::mt19937& random,
                          const std::vector<std::string>& guaranteed) {
    HarvestResult result;
    result.items = guaranteed;
    int rolls;
    float rareChance;
    if (params.sorted) {
        rolls = params.boost ? 6 : 4;
        rareChance = (params.boost ? 0.25f : 0.12f) * params.rareChanceMult;
    } else {
        rolls = params.boost ? 4 : 2;
        rareChance = (params.boost ? 0.15f : 0.05f) * params.rareChanceMult;
    }
    rolls += params.extraRolls;
    for (int roll = 0; roll < rolls; ++roll) result.items.push_back(PickWeighted(data.lootCommon, random));
    if (std::uniform_real_distribution<float>(0.0f, 1.0f)(random) < rareChance) {
        result.rare = PickWeighted(data.lootRare, random);
        result.items.push_back(result.rare);
    }
    if (result.rare.empty()) {
        // Un material raro garantizado (lona, colchon, vela) tambien cuenta como hallazgo
        for (const std::string& id : result.items) {
            const ItemDef* item = data.FindItem(id);
            if (item && (item->rarity == "rare" || item->rarity == "veryRare")) {
                result.rare = id;
                break;
            }
        }
    }
    return result;
}

std::vector<std::vector<std::string>> DistributeShelterLoot(const GameData& data, size_t bagCount, std::mt19937& random) {
    std::vector<std::vector<std::string>> bags(bagCount);
    if (bagCount == 0) return bags;
    std::map<std::string, int> requirements;   // map: orden estable para repetir la misma reparticion con la misma semilla
    for (const CambuchePieceDef& piece : data.cambuche)
        for (const auto& [item, quantity] : piece.req) requirements[item] += quantity;
    std::vector<size_t> order(bagCount);
    for (size_t index = 0; index < bagCount; ++index) order[index] = index;
    std::shuffle(order.begin(), order.end(), random);
    size_t cursor = 0;
    for (const auto& [item, quantity] : requirements) {
        const int stack = (item == "carton" || item == "lata" || item == "madera") ? 2 : 1;
        for (int remaining = quantity; remaining > 0; remaining -= stack) {
            std::vector<std::string>& bag = bags[order[cursor++ % bagCount]];
            for (int unit = 0; unit < std::min(stack, remaining); ++unit) bag.push_back(item);
        }
    }
    return bags;
}

float InfectionChance(bool sorted, float resistance) {
    float chance = 0.12f;
    if (sorted) chance *= 1.8f;
    return chance * resistance;
}

} // namespace Bazuko
