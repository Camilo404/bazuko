import * as THREE from 'three';

export const AVENUE_OVERPASS = Object.freeze({ x0: 55, x1: 95, z: 8, clearance: 6.4 });

export function skylineFloors(x, z, ruin, random = Math.random()) {
  if (ruin) return random < .3 ? 3 : 2;
  if (x >= 23 && x <= 53) {
    const rhythm = [2, 3, 2, 4, 3, 6, 2, 4];
    return rhythm[(Math.floor(z / 5) + Math.floor(x / 3) * 3) % rhythm.length];
  }
  return 2 + Math.floor(Math.min(.999999, Math.max(0, random)) * 3);
}

/** Pasarela entre torres de acceso: referencia visual, sin acceso público a la planta superior. */
export function buildAvenueOverpass() {
  const group = new THREE.Group(); group.name = 'Pasarela urbana de la Caracas';
  const b = AVENUE_OVERPASS;
  const concrete = new THREE.MeshStandardMaterial({ color: '#a4a99d', roughness: .9 });
  const steel = new THREE.MeshStandardMaterial({ color: '#315d5a', roughness: .65, metalness: .3 });
  const dark = new THREE.MeshStandardMaterial({ color: '#243439', roughness: .6 });
  const glass = new THREE.MeshStandardMaterial({ color: '#84a5ad', roughness: .45, metalness: .2 });
  const box = (w, h, d, x, y, z, material) => {
    const mesh = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), material);
    mesh.position.set(x, y, z); mesh.castShadow = mesh.receiveShadow = true;
    group.add(mesh); return mesh;
  };
  for (const x of [b.x0, b.x1]) {
    const tower = box(1.4, 11.4, 4.6, x, 5.7, b.z, concrete);
    tower.name = 'Torre de acceso';
    box(1.8, .3, 5, x, 11.55, b.z, steel);
    box(1.1, 2.2, .06, x, 1.2, b.z - 2.34, dark);
    for (const y of [4, 7.4, 9.5]) box(1.3, 1.25, .06, x, y, b.z - 2.34, glass);
  }
  box(b.x1 - b.x0, .4, 3.5, 75, b.clearance + .2, b.z, concrete);
  for (const z of [b.z - 1.7, b.z + 1.7]) {
    box(40, .18, .16, 75, 10, z, steel);
    box(42, .18, .16, 75, 7, z, steel);
    for (let x = b.x0; x < b.x1; x += 6) {
      box(.16, 3, .16, x, 8.5, z, steel);
      const diagonal = box(Math.hypot(6, 3), .13, .13, x + 3, 8.5, z, steel);
      diagonal.rotation.z = Math.atan2((Math.round((x - b.x0) / 6) % 2 ? -1 : 1) * 3, 6);
    }
  }
  return group;
}
