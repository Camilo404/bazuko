// src/world/props/Banca.js
import * as THREE from 'three';

export class Banca {
  constructor(position, rotation = 0) {
    this.group = new THREE.Group();
    this.group.position.copy(position);
    this.group.rotation.y = rotation;

    const woodMat = new THREE.MeshLambertMaterial({
      color: 0x4a3020,
      flatShading: true
    });
    const metalMat = new THREE.MeshLambertMaterial({
      color: 0x1a1a20,
      flatShading: true
    });
    woodMat.userData.staticMergeKey = 'banca:madera';
    metalMat.userData.staticMergeKey = 'banca:metal';

    // Asiento
    const seatGeo = new THREE.BoxGeometry(1.6, 0.08, 0.5);
    const seat = new THREE.Mesh(seatGeo, woodMat);
    seat.position.y = 0.45;
    seat.castShadow = true;
    seat.receiveShadow = true;
    this.group.add(seat);

    // Respaldo
    const backGeo = new THREE.BoxGeometry(1.6, 0.5, 0.08);
    const back = new THREE.Mesh(backGeo, woodMat);
    back.position.set(0, 0.7, -0.21);
    back.castShadow = true;
    this.group.add(back);

    // Patas
    const legGeo = new THREE.BoxGeometry(0.08, 0.45, 0.4);
    for (const x of [-0.7, 0.7]) {
      const leg = new THREE.Mesh(legGeo, metalMat);
      leg.position.set(x, 0.22, 0);
      leg.castShadow = true;
      this.group.add(leg);
    }
  }
}
