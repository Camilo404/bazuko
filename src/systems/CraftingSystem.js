// src/systems/CraftingSystem.js
import { RECIPES } from '../data/recipes.js';
import { ITEMS } from '../data/items.js';

export class CraftingSystem {
  constructor() {
    this.recipes = RECIPES;
  }

  /** ¿Tiene el inventario los ingredientes de la receta? */
  canCraft(recipe, inventory) {
    for (const [id, qty] of Object.entries(recipe.ingredients)) {
      if (inventory.count(id) < qty) return false;
    }
    return true;
  }

  /** Craftea una receta. Devuelve el item resultante o null. */
  craft(recipe, inventory) {
    if (!this.canCraft(recipe, inventory)) return null;

    // Verificar peso del resultado
    const resultItem = ITEMS[recipe.result];
    if (!resultItem) return null;
    if (inventory.totalWeight + resultItem.weight * recipe.qty > inventory.maxWeight) {
      return null;
    }

    // Quitar ingredientes
    for (const [id, qty] of Object.entries(recipe.ingredients)) {
      inventory.remove(id, qty);
    }

    // Añadir resultado
    inventory.add(resultItem, recipe.qty);

    window.dispatchEvent(new CustomEvent('craft', {
      detail: { name: resultItem.name, qty: recipe.qty }
    }));

    return resultItem;
  }

  /** ¿Cabe el resultado en el costal? (misma regla que craft) */
  fits(recipe, inventory) {
    const resultItem = ITEMS[recipe.result];
    return !!resultItem && inventory.totalWeight + resultItem.weight * recipe.qty <= inventory.maxWeight;
  }

  /** Devuelve todas las recetas marcando si son crafteables y si su resultado cabe */
  getAvailable(inventory) {
    return this.recipes.map((r) => ({
      ...r,
      craftable: this.canCraft(r, inventory),
      fits: this.fits(r, inventory)
    }));
  }
}
