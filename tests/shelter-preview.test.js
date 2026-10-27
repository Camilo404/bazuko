import assert from 'node:assert/strict';
import { test } from 'node:test';
import * as THREE from 'three';
import { Cambuche } from '../src/entities/Cambuche.js';
import { cloneFinishedShelter } from '../src/ui/ShelterPreview.js';

test('la foto muestra el refugio armado sin construir ni mover el cambuche real', () => {
  const scene = new THREE.Scene(), shelter = new Cambuche(scene, new THREE.Vector3(112.98, 0, 94.64));
  shelter.model = new THREE.Mesh(new THREE.BoxGeometry(3.9, 2.1, 2.8), new THREE.MeshStandardMaterial());
  shelter.group.add(shelter.model); shelter.setBuilt({});
  const built = structuredClone(shelter.built), realPosition = shelter.position.clone();
  const preview = cloneFinishedShelter(shelter);
  assert.equal(shelter.model.visible, false);
  assert.equal(shelter.markerPile.visible, true);
  assert.deepEqual(shelter.built, built); assert.deepEqual(shelter.position, realPosition);
  assert.deepEqual(preview.position.toArray(), [0, 0, 0]);
  assert.equal(preview.children.at(-1).visible, true);
  assert.equal(preview.children.at(-1).geometry, shelter.model.geometry);
  const root = preview.children[0];
  assert.equal(root.children[0].visible, false);
  const visiblePieces = root.children.slice(1).filter(node => node.visible);
  assert.equal(visiblePieces.length, 2, 'Solo fogón y vela complementan el GLB completo');
  assert.equal(shelter.fireLight.visible, true);
  preview.traverse(node => { if (node.isLight) assert.equal(node.visible, false); });
  assert.equal(scene.children.length, 1);
});

test('la vista procedural también muestra todas las mejoras sin alterar las piezas reales', () => {
  const shelter = new Cambuche(new THREE.Scene(), new THREE.Vector3());
  shelter.setBuilt({ base: true });
  const preview = cloneFinishedShelter(shelter), root = preview.children[0];
  assert.equal(root.children.slice(1).filter(node => node.visible).length, 6);
  assert.equal(shelter.pieces.techo.visible, false);
  assert.equal(shelter.pieces.fogon.visible, false);
});
