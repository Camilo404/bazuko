/** Ventanas largas e histéresis para no cambiar resolución por un tirón aislado. */
export class AdaptiveResolution {
  constructor(deviceRatio = 1) {
    this.max = Math.min(deviceRatio || 1, 1.5);
    this.min = Math.min(this.max, 0.75);
    this.ratio = this.max;
    this.pressure = 0;
    this.resetWindow();
  }

  resetWindow() { this.elapsed = 0; this.frames = 0; this.fastWindows = 0; }

  setLimit(max) {
    this.max = max;
    this.min = Math.min(max, .75);
    this.ratio = max;
    this.pressure = 0;
    this.resetWindow();
  }

  sample(frameMs) {
    if (!Number.isFinite(frameMs) || frameMs <= 0) return false;
    this.elapsed += frameMs;
    this.frames++;
    const fps = this.frames * 1000 / this.elapsed;
    // Ocho cuadros lentos como mínimo: un tirón aislado no cambia la calidad.
    const severe = fps < 30 && this.elapsed >= 1000 && this.frames >= 8;
    if (this.elapsed < 3000 && !severe) return false;
    this.elapsed = this.frames = 0;
    const before = this.ratio;
    const previousPressure = this.pressure;
    if (fps < 45) {
      if (this.ratio > this.min) this.ratio = Math.max(this.min, Math.round((this.ratio - (severe ? .3 : .15)) * 100) / 100);
      else this.pressure = Math.min(2, this.pressure + 1);
      this.fastWindows = 0;
    } else if (fps > 57) {
      if (++this.fastWindows >= 4) {
        if (this.pressure) this.pressure--;
        else this.ratio = Math.min(this.max, Math.round((this.ratio + .15) * 100) / 100);
        this.fastWindows = 0;
      }
    } else this.fastWindows = 0;
    return this.ratio !== before || this.pressure !== previousPressure;
  }
}
