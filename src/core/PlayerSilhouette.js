import * as THREE from 'three';

/** Segunda pasada del mismo personaje animado, solo en píxeles ocultos por profundidad. */
export class PlayerSilhouette {
  constructor() {
    this._lightLayers = [];
    this._overlayObjects = [];
    this.material = new THREE.MeshBasicMaterial({
      color: 0x73e5ef, transparent: true, opacity: 0.65,
      depthTest: true, depthFunc: THREE.GreaterDepth, depthWrite: false,
      polygonOffset: true, polygonOffsetFactor: -1, polygonOffsetUnits: -1,
      toneMapped: false
    });
  }

  render(renderer, scene, camera, playerRoot, lights = scene.children) {
    // También recoge modelos que terminan de cargarse o se cambian durante la partida.
    playerRoot.traverse(o => { if (o.isMesh) o.layers.enable(1); });
    const mask = camera.layers.mask;
    const override = scene.overrideMaterial;
    const background = scene.background;
    const autoClear = renderer.autoClear;
    const shadowUpdate = renderer.shadowMap.autoUpdate;
    const worldUpdate = scene.matrixWorldAutoUpdate;
    const children = scene.children;
    const overlayObjects = this._overlayObjects;
    overlayObjects.length = 0;
    overlayObjects.push(playerRoot);
    const lightLayers = this._lightLayers;
    lightLayers.length = 0;
    for (const o of lights) {
      if (o.isLight && o.visible && o.layers.test(camera.layers)) {
        lightLayers.push(o, o.layers.mask);
        overlayObjects.push(o);
        o.layers.enable(1);
      }
    }
    try {
      camera.layers.set(1);
      scene.overrideMaterial = this.material;
      scene.background = null;
      renderer.autoClear = false;
      renderer.shadowMap.autoUpdate = false;
      // Conserva padre y matrices mundiales. Solo visita jugador y luces en estas pasadas;
      // recorrer otra vez toda la ciudad no aporta ningún píxel a la silueta.
      scene.children = overlayObjects;
      scene.matrixWorldAutoUpdate = false;
      renderer.render(scene, camera);
      // La ropa y el cuerpo pueden ocultarse entre sí. Repintar las superficies visibles
      // elimina esa falsa silueta, conservándola solo donde las tapa el entorno.
      scene.overrideMaterial = override;
      renderer.render(scene, camera);
    } finally {
      scene.children = children;
      scene.matrixWorldAutoUpdate = worldUpdate;
      camera.layers.mask = mask;
      scene.overrideMaterial = override;
      scene.background = background;
      renderer.autoClear = autoClear;
      renderer.shadowMap.autoUpdate = shadowUpdate;
      for (let i = 0; i < lightLayers.length; i += 2) lightLayers[i].layers.mask = lightLayers[i + 1];
    }
  }
}
