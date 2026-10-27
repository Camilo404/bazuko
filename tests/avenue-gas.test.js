import test from 'node:test';
import assert from 'node:assert/strict';
import * as THREE from 'three';
import { skylineFloors, buildAvenueOverpass, AVENUE_OVERPASS } from '../src/world/bogota/AvenueLandmarks.js';
import { GAS_STATION, GAS_OBSTACLES, buildGasStation } from '../src/world/bogota/GasStation.js';
import { Bogota } from '../src/world/districts/Bogota.js';
import { CollisionSystem } from '../src/systems/CollisionSystem.js';

test('la avenida alterna edificios bajos y altos sin cambiar su altura entre consultas', () => {
  const heights = new Set();
  for (let z = 20; z < 100; z += 5) {
    const floors = skylineFloors(25, z, false, 0);
    heights.add(floors);
    assert.equal(floors, skylineFloors(25, z, false, .99));
  }
  assert.deepEqual([...heights].sort(), [2, 3, 4, 6]);
  assert.equal(skylineFloors(25, 30, true, .9), 2);
});

test('la pasarela deja libre el corredor del bus y sus torres quedan fuera de la calzada', () => {
  const group = buildAvenueOverpass();
  const b = AVENUE_OVERPASS;
  group.updateMatrixWorld(true);
  for (const mesh of group.children) {
    const bounds = new THREE.Box3().setFromObject(mesh);
    if (bounds.max.x > 57 && bounds.min.x < 93) assert.ok(bounds.min.y >= b.clearance - 1e-6);
  }
  assert.equal(group.children.filter(o => o.name === 'Torre de acceso').length, 2);
  assert.ok(new THREE.Box3().setFromObject(group).max.y > 11);
});

test('la gasolinera sustituye el lote indicado sin cerrar calles ni el paso bajo la marquesina', () => {
  const city = new Bogota(); city.map = city._layout();
  assert.equal(city.map[48][77], 'floor');
  assert.equal(city.map[39][77], 'road');
  assert.equal(city.map[48][86], 'road');
  const collisions = new CollisionSystem(); city._buildCollisions(city.map, collisions);
  const occupied = (x, z) => collisions.colliders.some(({ box }) => box.containsPoint(new THREE.Vector3(x, 1, z)));
  assert.equal(occupied(149, 97), true);
  assert.equal(occupied(GAS_STATION.x, 100), false);
  assert.equal(occupied(154, 114), true);
  assert.equal(GAS_OBSTACLES.filter(o => o.h === 2).length, 4);
});

test('el modelo de estación tiene cubierta elevada y permanece dentro del lote', () => {
  const station = buildGasStation({ createSign: () => new THREE.Texture() });
  const bounds = new THREE.Box3().setFromObject(station);
  assert.ok(bounds.min.x >= GAS_STATION.x0 - 1e-6);
  assert.ok(bounds.max.x <= GAS_STATION.x1 + 1e-6);
  assert.ok(bounds.min.z >= GAS_STATION.z0);
  assert.ok(bounds.max.z <= GAS_STATION.z1);
  assert.ok(bounds.max.y > 9);
  assert.equal(station.children.filter(o => o.isLight).length, 0);
});
