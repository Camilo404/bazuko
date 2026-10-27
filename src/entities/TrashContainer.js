import { Pickup } from './Pickup.js';

/** Caneca permanente; comparte el sistema de botín y conserva los objetos que no caben. */
export class TrashContainer extends Pickup {
  constructor(scene, position) {
    super(scene, position, { modelKind: 'contenedor', cooldown: 120 });
  }

  harvest(...args) {
    const result = super.harvest(...args);
    this.mesh.visible = true;
    return result;
  }
}
