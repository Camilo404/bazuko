const HEARING_DISTANCE = 55;
const MAX_NEARBY_VEHICLES = 8;

/** Buffers compartidos; identificadores, mezcla y claxon independientes por vehículo. */
export class VehicleAudio {
  constructor(sounds, random = Math.random, driverSounds = []) {
    this.sounds = sounds;
    this.random = random;
    this.driverSounds = driverSounds;
    this.instances = new Map();
    this.paused = false;
  }

  _stop(state) {
    for (const key of ['idle', 'drive', 'horn', 'remark']) {
      if (state[key]) state[key].sound.stop(state[key].id);
      state[key] = null;
    }
    state.idleVolume = state.driveVolume = 0;
  }

  clear() {
    for (const state of this.instances.values()) this._stop(state);
    this.instances.clear();
  }

  setPaused(paused) {
    if (this.paused === paused) return;
    this.paused = paused;
    for (const state of this.instances.values()) {
      for (const key of ['idle', 'drive']) {
        const voice = state[key];
        if (voice) paused ? voice.sound.pause(voice.id) : voice.sound.play(voice.id);
      }
      // Un claxon interrumpido por un menú no se repite al volver.
      if (paused && state.horn) { state.horn.sound.stop(state.horn.id); state.horn = null; }
      if (paused && state.remark) { state.remark.sound.stop(state.remark.id); state.remark = null; }
    }
  }

  _loop(state, key, sound) {
    if (!state[key] && sound.state() === 'loaded') {
      const id = sound.play();
      state[key] = { sound, id };
      sound.volume(0, id);
      sound.seek(state.phase * sound.duration(), id);
    }
    return state[key];
  }

  update(dt, position, vehicles = [], bus = null) {
    const present = new Set(vehicles.map(vehicle => vehicle.car));
    const buses = [bus].flat().filter(Boolean);
    for (const vehicle of buses) present.add(vehicle);
    for (const [car, state] of this.instances) if (!present.has(car)) {
      this._stop(state); this.instances.delete(car);
    }
    if (this.paused) return;
    const entries = vehicles.map(vehicle => ({ source: vehicle, car: vehicle.car, bus: false }));
    for (const vehicle of buses) entries.push({ source: vehicle, car: vehicle, bus: true });
    for (const entry of entries) {
      const dx = entry.car.position.x - position.x, dz = entry.car.position.z - position.z;
      entry.distance = Math.hypot(dx, dz);
      entry.voiceDistance = Math.hypot(dx, entry.car.position.y - position.y, dz);
    }
    // La visibilidad del render no determina si un vehículo se puede oír.
    const nearby = new Set(entries.filter(entry => entry.distance < HEARING_DISTANCE)
      .sort((a, b) => a.distance - b.distance).slice(0, MAX_NEARBY_VEHICLES).map(entry => entry.car));
    for (const entry of entries) {
      let state = this.instances.get(entry.car);
      if (!state) {
        state = { phase: this.random(), pitch: .96 + this.random() * .08,
          hornIndex: Math.floor(this.random() * 3), patience: .7 + this.random() * .8,
          blockedFor: 0, cooldown: 0, idleVolume: 0, driveVolume: 0 };
        this.instances.set(entry.car, state);
      }
      state.cooldown = Math.max(0, state.cooldown - dt);
      state.blockedFor = entry.source.personBlocked ? state.blockedFor + dt : 0;
      if (!nearby.has(entry.car)) { this._stop(state); continue; }
      const idle = this._loop(state, 'idle', this.sounds[entry.bus ? 'busIdle' : 'carIdle']);
      const drive = this._loop(state, 'drive', this.sounds[entry.bus ? 'busDrive' : 'carDrive']);
      const mix = Math.min(1, Math.max(0, entry.source.speed) / 2.5);
      const smoothing = 1 - Math.exp(-dt * 7);
      state.idleVolume += ((1 - mix) * (entry.bus ? .38 : .34) - state.idleVolume) * smoothing;
      state.driveVolume += (mix * (entry.bus ? .42 : .40) - state.driveVolume) * smoothing;
      for (const [voice, volume, rate] of [[idle, state.idleVolume, state.pitch],
        [drive, state.driveVolume, state.pitch * (.85 + Math.min(entry.source.speed, 5) * .065)]]) {
        if (!voice) continue;
        voice.sound.volume(volume, voice.id);
        voice.sound.rate(rate, voice.id);
      }
      if (state.blockedFor >= state.patience && state.cooldown === 0) {
        const sound = this.sounds[`horn${state.hornIndex + 1}`];
        if (sound.state() === 'loaded') {
          if (state.horn) state.horn.sound.stop(state.horn.id);
          const id = sound.play('honk');
          state.horn = { sound, id };
          sound.volume(.48, id);
          sound.rate(state.pitch, id);
          if (!entry.bus && entry.voiceDistance < 8 && this.driverSounds.length) {
            const index = Math.min(this.driverSounds.length - 1, Math.floor(this.random() * this.driverSounds.length));
            const remark = this.driverSounds[index];
            if (remark.state() === 'loaded') {
              if (state.remark) state.remark.sound.stop(state.remark.id);
              state.remark = { sound: remark, id: remark.play() };
            }
          }
          state.cooldown = 6 + this.random() * 5;
        }
      }
      if (state.remark) {
        const { sound, id } = state.remark;
        if (!sound.playing(id)) state.remark = null;
        else sound.volume(.7 * Math.max(0, Math.min(1, (8 - entry.voiceDistance) / 6)), id);
      }
    }
  }
}
