import * as THREE from 'three';
import { busPathX } from './Colpatria.js';
import { BRIDGE_RISE } from './BridgeElevation.js';

export const LAKE_PARK = Object.freeze({ name: 'Parque del Lago', z0: 222, z1: 318,
  lakeX: 75, lakeZ: 262, radiusX: 50, radiusZ: 26 });
export const LAKE_BRIDGE = Object.freeze({ x0: 68, x1: 82, z0: 220, z1: 318,
  crestStart: 220, crestEnd: 280, rise: BRIDGE_RISE });

export function onLakeBridge(x, z) {
  return z >= LAKE_BRIDGE.z0 && z <= LAKE_BRIDGE.z1 && x >= busPathX(z,LAKE_BRIDGE.x0) && x <= busPathX(z,LAKE_BRIDGE.x1);
}

export function bridgeHeight(z) {
  const b = LAKE_BRIDGE;
  // La entrada norte conecta elevada con el viaducto; solo baja en la orilla sur.
  if (z < b.z0 - 1 || z >= b.z1) return .02;
  const t = z > b.crestEnd ? (b.z1 - z) / (b.z1 - b.crestEnd) : 1;
  return .02 + b.rise * (.5 - .5 * Math.cos(Math.PI * t));
}

export function buildLakeWater(material) {
  const water = new THREE.Mesh(new THREE.CircleGeometry(1, 96), material);
  water.name = 'Agua continua bajo el puente';
  water.rotation.x = -Math.PI / 2;
  water.scale.set(LAKE_PARK.radiusX, LAKE_PARK.radiusZ, 1);
  water.position.set(LAKE_PARK.lakeX, -.18, LAKE_PARK.lakeZ);
  water.receiveShadow = true;
  return water;
}

/** Solo la ampliación; se conserva la posición de todas las calles del centro. */
export function paintLakePark(map) {
  const p = LAKE_PARK, b = LAKE_BRIDGE;
  for (let z = 111; z < map.length - 1; z++) for (let x = 1; x < map[0].length - 1; x++) {
    const wx = x * 2, wz = z * 2;
    const radius = Math.hypot((wx - p.lakeX) / p.radiusX, (wz - p.lakeZ) / p.radiusZ);
    const promenade = Math.abs(radius - 1.14) < .045 || z === 113 || z === 155 || x === 10 || x === 112 ||
      (x === 86 && (wz < 234 || wz > 290));
    map[z][x] = radius < 1 ? 'water' : promenade ? 'plaza' : 'grass';
    if (onLakeBridge(wx,wz)) map[z][x] = 'busway';
  }
}
