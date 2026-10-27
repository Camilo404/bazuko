// Exporta los datos del juego web (src/data) a JSON para el port de C++.
// Uso: node tools/export-game-data.mjs   ->   cpp/assets/data/game-data.json
// El JSON se regenera desde items.js y talents.js: no se edita a mano.
import { writeFileSync, mkdirSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  ITEMS, SHOP_ITEMS, LOOT_COMMON, LOOT_RARE, LOOT_SUBSTANCE, COSTAL_BONUS, isRecyclable, itemRarityLabel
} from '../src/data/items.js';
import { TALENTS, BRANCHES } from '../src/data/talents.js';
import { CAMBUCHE, SLEEP } from '../src/data/cambuche.js';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const out = resolve(root, 'cpp/assets/data/game-data.json');

// Orden de los objetos en el port: las partidas guardadas referencian el indice, asi que no puede cambiar.
const LEGACY_ORDER = [
  'lata', 'botella', 'carton', 'chatarra', 'cable', 'pan', 'agua', 'reloj', 'vendaje', 'antibiotico',
  'madera', 'pilas', 'radioTransistor', 'retrovisor', 'radio', 'tela', 'pastilla', 'lona', 'colchon', 'vela',
  'popo', 'alambre', 'plastico', 'mochila', 'arma', 'curita', 'rifa', 'cafe', 'lataComida', 'refresco',
  'tintico', 'pegante', 'cigarrillo', 'jabon', 'cepillo', 'vacuna', 'carbon', 'jarabe', 'pomada',
  'antiparasitario', 'moneda', 'anillo'
];

// Objetos que solo existen en el port (recetas de crafteo que el original no define en items.js).
const PORT_ONLY = {
  tela: { name: 'Retazo de tela', weight: 0.2, value: 300, recyclable: false },
  alambre: { name: 'Alambre', weight: 0.1, value: 500, recyclable: false },
  plastico: { name: 'Plástico', weight: 0.1, value: 200, recyclable: false },
  mochila: { name: 'Mochila', weight: 0.8, value: 12000, recyclable: false },
  arma: { name: 'Arma casera', weight: 1.2, value: 0, recyclable: false },
  curita: { name: 'Curita', weight: 0.05, value: 500, recyclable: true },
  rifa: { name: 'Cartón de rifa', weight: 0.05, value: 1000, recyclable: true }
};

const STATS = ['hambre', 'sed', 'energia', 'salud', 'higiene', 'animo', 'vicio'];

function exportItem(id) {
  const source = ITEMS[id];
  if (!source) return { id, portOnly: true, ...PORT_ONLY[id] };
  const item = {
    id,
    name: source.name,
    weight: source.weight,
    value: source.value,
    rarity: source.rarity ?? (source.rare ? 'rare' : ''),
    rarityLabel: itemRarityLabel(id),
    recyclable: isRecyclable(source),
    usable: !!source.usable,
    build: !!source.build,
    substance: !!source.substance,
    cure: source.cure ?? '',
    desc: source.desc ?? ''
  };
  if (source.consumable) item.consumable = { stat: source.consumable, amount: source.amount };
  const effects = {};
  for (const stat of STATS) if (source[stat]) effects[stat] = source[stat];
  if (Object.keys(effects).length) item.effects = effects;
  return item;
}

const ids = [...LEGACY_ORDER, ...Object.keys(ITEMS).filter((id) => !LEGACY_ORDER.includes(id))];
const data = {
  generatedFrom: 'src/data/items.js, src/data/talents.js, src/data/cambuche.js',
  costalBonus: COSTAL_BONUS,
  items: ids.map(exportItem),
  shop: SHOP_ITEMS.map((entry) => ({
    item: entry.item ?? '',
    upgrade: entry.upgrade ?? '',
    price: entry.price,
    qty: entry.qty ?? 1,
    label: entry.label ?? '',
    desc: entry.desc ?? ''
  })),
  lootCommon: LOOT_COMMON,
  lootRare: LOOT_RARE,
  lootSubstance: LOOT_SUBSTANCE,
  cambuche: CAMBUCHE.map(({ id, name, req, needs, desc }) => ({ id, name, req, needs: needs ?? '', desc })),
  sleep: SLEEP,
  branches: BRANCHES.map(({ id, name }) => ({ id, name })),
  talents: Object.values(TALENTS).map((talent) => ({
    id: talent.id,
    name: talent.name,
    desc: talent.desc,
    branch: talent.branch,
    cost: talent.cost,
    requires: talent.requires,
    effects: talent.effects
  }))
};

mkdirSync(dirname(out), { recursive: true });
writeFileSync(out, JSON.stringify(data, null, 2) + '\n');
console.log(`Datos exportados a ${out}: ${data.items.length} objetos, ${data.shop.length} productos, ${data.talents.length} talentos.`);
