import test from 'node:test';
import assert from 'node:assert/strict';
import { RadioSystem } from '../src/systems/RadioSystem.js';
import { ITEMS, isUsable } from '../src/data/items.js';
import { Game } from '../src/core/game.js';

test('radio combina volumen propio, volumen general, silencio y pausa', () => {
  const radio = new RadioSystem();
  radio._masterGain = { gain: { value: 0 } };
  radio.setVolume(.8);
  radio.setMasterVolume(.5);
  assert.equal(radio._masterGain.gain.value, .4);
  radio.setMasterVolume(.5, true);
  radio.setVolume(.6);
  assert.equal(radio._masterGain.gain.value, 0);
  radio.setMasterVolume(.5, false);
  radio.setPaused(true);
  assert.equal(radio._masterGain.gain.value, 0);
  radio.setPaused(false);
  assert.equal(radio._masterGain.gain.value, .3);
});

test('inventario permite usar los nuevos objetos y conserva sus iconos', () => {
  for (const id of ['radioTransistor', 'pilas', 'popo']) {
    assert.equal(isUsable(ITEMS[id]), true);
    assert.notEqual(ITEMS[id].emoji, '📦');
  }
  assert.equal(isUsable(ITEMS.carton), false);
});

test('radar reutiliza el caché sin congelar la posición del perro ni duplicar el cambuche', () => {
  const game = Object.assign(Object.create(Game.prototype), {
    cambuche: { position: { x: 112.98, z: 94.64 } },
    world: { active: { reserved: { bed: { x: 1, z: 1 } } } },
    dog: { isFriend: false, position: { x: 4, z: 5 } }
  });
  assert.equal(game._getPOIs(1000).length, 1);
  game.dog.isFriend = true;
  const points = game._getPOIs(1010);
  assert.equal(points.length, 2);
  game.dog.position.x = 8;
  assert.equal(game._getPOIs(1020), points);
  assert.equal(points[1].x, 8);
  assert.equal(points[0].x, 112.98);
});
