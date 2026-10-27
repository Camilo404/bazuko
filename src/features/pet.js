// src/features/pet.js
import * as THREE from 'three';

export const CONFIG = {
  followDist: 1.8,
  teleportDist: 22.0,
  lerpSpeed: 4.5,
  barkCooldown: 6.0,
  storageKey: 'vagabundo.mascota_config',
  defaultName: 'Firulais',
  defaultColor: '#965a28',
  defaultAccessory: 'bufanda',
  defaultSize: 1.0
};

export class Pet {
  // Construye la mascota canina y carga la configuración guardada.
  constructor(scene, player = null) {
    this.scene = scene;
    this.player = player;
    this.state = 'idle';
    this.time = 0;
    this.barkTimer = 0;
    this.position = (player?.position?.clone() ?? new THREE.Vector3(12, 0, 12)).add(new THREE.Vector3(1, 0, 1));
    this.group = new THREE.Group();
    this.group.position.copy(this.position);
    this._loadConfig();
    this._buildMesh();
    if (scene) scene.add(this.group);
    this._buildCustomizationModal();
  }

  // Carga las opciones de personalización desde localStorage.
  _loadConfig() {
    let saved = null;
    try {
      const data = typeof localStorage !== 'undefined' ? localStorage.getItem(CONFIG.storageKey) : null;
      if (data) saved = JSON.parse(data);
    } catch {}
    this.nombre = saved?.nombre ?? CONFIG.defaultName;
    this.color = saved?.color ?? CONFIG.defaultColor;
    this.accesorio = saved?.accesorio ?? CONFIG.defaultAccessory;
    this.tamano = Number(saved?.tamano ?? CONFIG.defaultSize);
  }

  // Guarda las opciones de personalización actuales en localStorage.
  _saveConfig() {
    try {
      if (typeof localStorage !== 'undefined') {
        localStorage.setItem(CONFIG.storageKey, JSON.stringify({
          nombre: this.nombre,
          color: this.color,
          accesorio: this.accesorio,
          tamano: this.tamano
        }));
      }
    } catch {}
  }

  // Construye la geometría 3D básica y partes de la mascota.
  _buildMesh() {
    while (this.group.children.length > 0) {
      const c = this.group.children[0];
      this.group.remove(c);
      c.geometry?.dispose?.();
      c.material?.dispose?.();
    }
    const colorHex = parseInt(this.color.replace('#', '0x'), 16) || 0x965a28;
    const bodyMat = new THREE.MeshLambertMaterial({ color: colorHex, flatShading: true });
    this.body = new THREE.Mesh(new THREE.BoxGeometry(0.32, 0.28, 0.55), bodyMat);
    this.body.position.y = 0.32;
    this.group.add(this.body);

    this.head = new THREE.Mesh(new THREE.BoxGeometry(0.24, 0.22, 0.26), bodyMat);
    this.head.position.set(0, 0.44, 0.32);
    this.group.add(this.head);

    this._addAccessory();
    this.group.scale.setScalar(this.tamano);
  }

  // Añade el accesorio correspondiente sobre el modelo 3D.
  _addAccessory() {
    if (this.accesorio === 'bufanda') {
      const bufanda = new THREE.Mesh(
        new THREE.BoxGeometry(0.34, 0.09, 0.16),
        new THREE.MeshLambertMaterial({ color: 0xc0392b })
      );
      bufanda.position.set(0, 0.38, 0.22);
      this.group.add(bufanda);
    } else if (this.accesorio === 'collar') {
      const collar = new THREE.Mesh(
        new THREE.BoxGeometry(0.33, 0.05, 0.12),
        new THREE.MeshLambertMaterial({ color: 0x2980b9 })
      );
      collar.position.set(0, 0.38, 0.22);
      this.group.add(collar);
    } else if (this.accesorio === 'gorra') {
      const gorra = new THREE.Mesh(
        new THREE.BoxGeometry(0.22, 0.08, 0.28),
        new THREE.MeshLambertMaterial({ color: 0xf39c12 })
      );
      gorra.position.set(0, 0.56, 0.34);
      this.group.add(gorra);
    }
  }

  // Actualiza el seguimiento suave lerp, teletransporte y estados de la mascota.
  update(dt, playerPos, npcs = []) {
    if (!playerPos) return;
    this.time += dt;
    if (this.barkTimer > 0) this.barkTimer -= dt;
    const dist = this.position.distanceTo(playerPos);

    if (dist > CONFIG.teleportDist) {
      this.position.copy(playerPos).add(new THREE.Vector3(1, 0, 1));
      this.group.position.copy(this.position);
      this.state = 'idle';
      return;
    }

    if (dist > CONFIG.followDist) {
      this.state = 'follow';
      const target = playerPos.clone().addScaledVector(
        this.position.clone().sub(playerPos).normalize(),
        CONFIG.followDist
      );
      this.position.lerp(target, Math.min(1, CONFIG.lerpSpeed * dt));
      this.group.position.copy(this.position);
      this.group.lookAt(playerPos.x, this.group.position.y, playerPos.z);
    } else {
      this.state = 'idle';
    }

    this._checkThreats(npcs);
  }

  // Verifica si hay peatones sospechosos cerca para ladrar y alertar.
  _checkThreats(npcs) {
    if (this.barkTimer > 0) return;
    for (const n of npcs) {
      if (n?.position && n.alerta > 0.6 && this.position.distanceTo(n.position) < 3.5) {
        this.state = 'alerta';
        this.barkTimer = CONFIG.barkCooldown;
        if (typeof window !== 'undefined') {
          window.dispatchEvent(new CustomEvent('toast', {
            detail: { text: `🐕 ¡${this.nombre} ladra alertando a un sospechoso!`, type: 'info' }
          }));
        }
        break;
      }
    }
  }

  // Proporciona el bonus de distracción que reduce el riesgo de ser atrapado al robar.
  getDistractionBonus() {
    return 0.10;
  }

  // Aplica y guarda la nueva configuración personalizada con vista previa en vivo.
  applyCustomization({ nombre, color, accesorio, tamano }) {
    if (nombre !== undefined) this.nombre = nombre.trim() || CONFIG.defaultName;
    if (color !== undefined) this.color = color;
    if (accesorio !== undefined) this.accesorio = accesorio;
    if (tamano !== undefined) this.tamano = Math.max(0.6, Math.min(1.6, Number(tamano)));
    this._saveConfig();
    this._buildMesh();
  }

  // Genera el panel modal interactivo de personalización para la mascota.
  _buildCustomizationModal() {
    if (typeof document === 'undefined') return;
    let modal = document.getElementById('pet-customization-modal');
    if (modal) return;
    modal = document.createElement('div');
    modal.id = 'pet-customization-modal';
    modal.style.cssText = 'display:none;position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);width:320px;background:rgba(20,20,24,0.96);border:2px solid #e67e22;border-radius:8px;padding:16px;z-index:110;color:#eee;font-family:monospace;box-shadow:0 0 24px rgba(0,0,0,0.85);';
    modal.innerHTML = `
      <h3 style="margin-bottom:12px;color:#f39c12;text-align:center;">🐕 Personalizar Mascota</h3>
      <label style="display:block;font-size:12px;margin-bottom:4px;">Nombre:</label>
      <input id="pet-name-input" type="text" style="width:100%;margin-bottom:10px;padding:5px;background:#111;color:#fff;border:1px solid #444;border-radius:4px;" />
      <label style="display:block;font-size:12px;margin-bottom:4px;">Color:</label>
      <input id="pet-color-input" type="color" style="width:100%;height:32px;margin-bottom:10px;background:#111;border:1px solid #444;cursor:pointer;" />
      <label style="display:block;font-size:12px;margin-bottom:4px;">Accesorio:</label>
      <select id="pet-acc-select" style="width:100%;margin-bottom:10px;padding:5px;background:#111;color:#fff;border:1px solid #444;border-radius:4px;">
        <option value="ninguno">Ninguno</option>
        <option value="bufanda">Bufanda roja</option>
        <option value="gorra">Gorra bogotana</option>
        <option value="collar">Collar azul</option>
      </select>
      <label style="display:block;font-size:12px;margin-bottom:4px;">Tamaño: <span id="pet-size-val">1.0</span></label>
      <input id="pet-size-input" type="range" min="0.7" max="1.4" step="0.1" style="width:100%;margin-bottom:16px;" />
      <div style="display:flex;justify-content:flex-end;gap:8px;">
        <button id="pet-modal-close" style="padding:6px 14px;background:#e67e22;color:#fff;border:none;border-radius:4px;cursor:pointer;font-weight:bold;">Listo</button>
      </div>
    `;
    document.body.appendChild(modal);

    const nameIn = modal.querySelector('#pet-name-input');
    const colorIn = modal.querySelector('#pet-color-input');
    const accSel = modal.querySelector('#pet-acc-select');
    const sizeIn = modal.querySelector('#pet-size-input');
    const sizeVal = modal.querySelector('#pet-size-val');
    const closeBtn = modal.querySelector('#pet-modal-close');

    const updateLive = () => {
      sizeVal.textContent = sizeIn.value;
      this.applyCustomization({
        nombre: nameIn.value,
        color: colorIn.value,
        accesorio: accSel.value,
        tamano: parseFloat(sizeIn.value)
      });
    };

    nameIn.addEventListener('input', updateLive);
    colorIn.addEventListener('input', updateLive);
    accSel.addEventListener('change', updateLive);
    sizeIn.addEventListener('input', updateLive);

    closeBtn.addEventListener('click', () => {
      modal.style.display = 'none';
      if (window.__game) window.__game._petModalOpen = false;
    });
  }

  // Alterna la visibilidad del modal de personalización con los valores actuales.
  toggleCustomizationMenu() {
    if (typeof document === 'undefined') return;
    const modal = document.getElementById('pet-customization-modal');
    if (!modal) return;
    const isOpen = modal.style.display === 'block';
    if (isOpen) {
      modal.style.display = 'none';
      if (window.__game) window.__game._petModalOpen = false;
    } else {
      modal.querySelector('#pet-name-input').value = this.nombre;
      modal.querySelector('#pet-color-input').value = this.color;
      modal.querySelector('#pet-acc-select').value = this.accesorio;
      modal.querySelector('#pet-size-input').value = this.tamano;
      modal.querySelector('#pet-size-val').textContent = this.tamano;
      modal.style.display = 'block';
      if (window.__game) window.__game._petModalOpen = true;
    }
  }

  // Libera las geometrías y materiales asociados a la mascota.
  dispose() {
    if (this.group && this.scene) {
      this.scene.remove(this.group);
      this.group.traverse(o => {
        o.geometry?.dispose?.();
        o.material?.dispose?.();
      });
    }
  }
}
