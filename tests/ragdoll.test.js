import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import * as THREE from 'three';
import { Player } from '../src/entities/Player.js';
import { CollisionSystem } from '../src/systems/CollisionSystem.js';

const playerAtOrigin = stats => {
  const player = new Player(new THREE.Scene(), { axis: { x: 0, z: 0 }, isRunning: false }, stats);
  player.position.set(0, 0, 0); player.root.position.copy(player.position);
  return player;
};

test('el ragdoll usa once cajas y diez articulaciones, lanza lejos y libera su mundo físico', () => {
  const player = playerAtOrigin();
  assert.equal(player.ragdoll.world, null);
  player.receiveVehicleImpact(new THREE.Vector3(1, 0, 0), 12);
  assert.equal(player.ragdoll.parts.length, 11);
  assert.equal(player.ragdoll.world.constraints.length, 10);
  assert.ok(player.ragdoll.parts.every(part => part.body.shapes.length === 1 && part.body.shapes[0].constructor.name === 'Box'));
  let maxHeight = 0;
  for (let frame = 0; frame < 360; frame++) {
    player.update(1 / 60, null, null);
    maxHeight = Math.max(maxHeight, player.position.y);
    assert.ok(player.position.toArray().every(Number.isFinite));
  }
  assert.ok(maxHeight > .8);
  assert.ok(player.position.x > 10 && player.position.x < 35);
  assert.equal(player.ragdoll.world, null);
  assert.equal(player.isStunned, false);
  assert.equal(player.ragdoll.parts.length, 0);
  player.dispose();
});

test('un ragdoll fatal termina la caída sin levantarse y se restaura al reaparecer', () => {
  const player = playerAtOrigin({ salud: 0 });
  const head = player.character.ragdollRig.head;
  const original = head.quaternion.clone();
  player.receiveVehicleImpact(new THREE.Vector3(1, 0, 0), 10);
  for (let frame = 0; frame < 360; frame++) player.update(1 / 60, null, null);
  assert.equal(player.isRagdollActive, false);
  assert.equal(player.ragdoll.world, null);
  assert.ok(head.getWorldPosition(new THREE.Vector3()).y < .8, 'El personaje fallecido permanece caído');
  player.clearVehicleImpact();
  assert.ok(head.quaternion.angleTo(original) < 1e-7);
  player.dispose();
});

test('las cajas de los vehículos impiden atravesarlos a pie y se actualizan sin crear mallas', () => {
  const collisions = new CollisionSystem();
  const car = { position: new THREE.Vector3(5, 0, 0), group: new THREE.Group(), length: 4.3, width: 1.76 };
  collisions.setVehicles([car]);
  const box = collisions.dynamicColliders[0].box;
  assert.equal(collisions.dynamicColliders.length, 1);
  assert.ok(collisions.resolveMove(new THREE.Vector3(0, 0, 0), new THREE.Vector3(20, 0, 0)).x < 2.5);
  assert.ok(collisions.resolveMove(new THREE.Vector3(5, 0, -5), new THREE.Vector3(0, 0, 10)).z < -1.2);
  car.position.x = 15; car.group.rotation.y = Math.PI / 2;
  collisions.setVehicles([car]);
  assert.equal(collisions.dynamicColliders[0].box, box);
  assert.ok(Math.abs(box.max.x - box.min.x - car.width) < 1e-8);
  assert.ok(Math.abs(box.max.z - box.min.z - car.length) < 1e-8);
  assert.equal(car.group.children.length, 0);
  const bus = { position: new THREE.Vector3(30, 0, 0), group: new THREE.Group(), length: 18.2, width: 2.52 };
  collisions.setVehicles([car], bus);
  assert.equal(collisions.dynamicColliders.length, 2);
  assert.ok(Math.abs(collisions.dynamicColliders[1].box.max.z - collisions.dynamicColliders[1].box.min.z - 18.2) < 1e-8);
  collisions.setVehicles([]);
  assert.equal(collisions.dynamicColliders.length, 0);
  assert.equal(collisions._vehicles.size, 0);
});

test('el ragdoll conserva la caja orientada del bus en vez de su envolvente diagonal', () => {
  const player = playerAtOrigin();
  const collisions = new CollisionSystem();
  const group = new THREE.Group(); group.rotation.set(.15, .7, 0, 'YXZ');
  const bus = { position: new THREE.Vector3(5, 0, 0), group, length: 18.2, width: 2.52 };
  collisions.setVehicles([], bus);
  player.receiveVehicleImpact(new THREE.Vector3(1,0,0), 6, { collisions });
  player.update(1 / 60, null, collisions);
  const entry = collisions.dynamicColliders[0], body = player.ragdoll.environment.get(entry);
  assert.ok(body);
  assert.ok(Math.abs(body.shapes[0].halfExtents.x - 1.26) < 1e-8);
  assert.ok(Math.abs(body.shapes[0].halfExtents.z - 9.1) < 1e-8);
  assert.ok(Math.abs(body.quaternion.y - group.quaternion.y) < 1e-8);
  assert.ok(Math.abs(body.quaternion.x - group.quaternion.x) < 1e-8);
  player.dispose();
});

test('el ragdoll respeta la caja simple de un carro estacionado al aterrizar', () => {
  const player = playerAtOrigin();
  const collisions = new CollisionSystem();
  const car = { position: new THREE.Vector3(5, 0, 0), group: new THREE.Group(), length: 4.3, width: 1.76 };
  collisions.setVehicles([car]);
  player.receiveVehicleImpact(new THREE.Vector3(1, 0, 0), 6, { collisions });
  let collided = false;
  for (let frame = 0; frame < 300; frame++) {
    player.update(1 / 60, null, collisions);
    if (player.ragdoll.world?.contacts.length) collided = true;
    assert.ok(player.position.toArray().every(Number.isFinite));
  }
  assert.ok(collided);
  assert.ok(player.position.x < 5, 'La caja del coche frena o desvía el cuerpo');
  player.dispose();
});

test('el esqueleto real de MakeHuman conserva su pose al activar y limpiar el ragdoll', () => {
  // Se leen jerarquía y transformaciones reales; no se necesitan imágenes ni GPU.
  const json = JSON.parse(readFileSync(new URL('../public/models/personaje/hombre-joven/cuerpo.gltf', import.meta.url), 'utf8'));
  const joints = new Set(json.skins.flatMap(skin => skin.joints));
  const nodes = json.nodes.map((data, index) => {
    const node = joints.has(index) ? new THREE.Bone() : new THREE.Object3D();
    node.name = data.name ?? '';
    if (data.translation) node.position.fromArray(data.translation);
    if (data.rotation) node.quaternion.fromArray(data.rotation);
    if (data.scale) node.scale.fromArray(data.scale);
    if (data.matrix) new THREE.Matrix4().fromArray(data.matrix).decompose(node.position, node.quaternion, node.scale);
    return node;
  });
  json.nodes.forEach((data, index) => data.children?.forEach(child => nodes[index].add(nodes[child])));
  const group = new THREE.Group(); group.scale.setScalar(1.08);
  for (const index of json.scenes[json.scene ?? 0].nodes) group.add(nodes[index]);
  const player = playerAtOrigin(); player.root.remove(player.character.group);
  player.character = { group, update() {} }; player.root.add(group);
  const originals = nodes.filter(node => node.isBone).map(node => ({ node, position: node.position.clone(), quaternion: node.quaternion.clone() }));
  player.receiveVehicleImpact(new THREE.Vector3(1, 0, 0), 12);
  assert.equal(player.ragdoll.parts.length, 11);
  for (let frame = 0; frame < 90; frame++) player.update(1 / 60, null, null);
  assert.ok(player.position.x > 5);
  assert.ok(player.ragdoll.parts.every(part => Number.isFinite(part.body.position.y)));
  player.clearVehicleImpact();
  for (const { node, position, quaternion } of originals) {
    assert.ok(node.position.distanceTo(position) < 1e-7);
    assert.deepEqual(node.quaternion.toArray(), quaternion.toArray());
  }
  player.dispose();
});
